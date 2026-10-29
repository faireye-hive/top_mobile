#include "top/SceneWorld.h"
#include "top/TexturePaths.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>

namespace top {
namespace {

constexpr float kPi = 3.14159265f;

std::uint16_t ru16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t ru32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

std::int32_t ri32(const std::uint8_t* p) {
    return static_cast<std::int32_t>(ru32(p));
}

std::int16_t ri16(const std::uint8_t* p) {
    return static_cast<std::int16_t>(ru16(p));
}

bool fileExists(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::fclose(file);
    return true;
}

std::string lowerCopy(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

void identity(float* m) {
    std::memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

void multiply(const float* a, const float* b, float* out) {
    float r[16];
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r[row * 4 + col] = a[row * 4 + 0] * b[0 * 4 + col] + a[row * 4 + 1] * b[1 * 4 + col] +
                               a[row * 4 + 2] * b[2 * 4 + col] + a[row * 4 + 3] * b[3 * 4 + col];
        }
    }
    std::memcpy(out, r, sizeof r);
}

void transformPoint(const float* m, float x, float y, float z, float& ox, float& oy, float& oz) {
    ox = x * m[0] + y * m[4] + z * m[8] + m[12];
    oy = x * m[1] + y * m[5] + z * m[9] + m[13];
    oz = x * m[2] + y * m[6] + z * m[10] + m[14];
}

void transformDir(const float* m, float x, float y, float z, float& ox, float& oy, float& oz) {
    ox = x * m[0] + y * m[4] + z * m[8];
    oy = x * m[1] + y * m[5] + z * m[9];
    oz = x * m[2] + y * m[6] + z * m[10];
}

// lwMatrixCtrl::UpdateYawPitchRoll: rotacao Z de (-yaw + pi), depois a translacao.
// yaw em graus, o mesmo Angle2Radian de CSceneObj::_UpdateYaw.
void nodeMatrix(float* m, float yawDeg, float x, float y, float z) {
    const float theta = -yawDeg * (kPi / 180.f) + kPi;
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    std::memset(m, 0, sizeof(float) * 16);
    m[0] = c;
    m[1] = -s;
    m[4] = s;
    m[5] = c;
    m[10] = 1.f;
    m[12] = x;
    m[13] = y;
    m[14] = z;
    m[15] = 1.f;
}

void geomWorld(const Model& model, std::size_t index, const float* node, float frame, float* out) {
    float chain[16];
    std::memcpy(chain, node, sizeof chain);
    std::uint32_t parent = model.geoms[index].parent;
    int guard = 0;
    while (parent != 0xffffffffu && parent < model.geoms.size() && guard < 16) {
        const ModelGeom& parentGeom = model.geoms[parent];
        float parentLocal[16];
        if (parentGeom.matrixFrames > 0 && !parentGeom.matrixAnimation.empty()) {
            const int frameIndex = std::max(0, std::min(parentGeom.matrixFrames - 1, static_cast<int>(frame) % parentGeom.matrixFrames));
            std::memcpy(parentLocal, parentGeom.matrixAnimation.data() + static_cast<std::size_t>(frameIndex) * 16u, sizeof parentLocal);
        } else {
            std::memcpy(parentLocal, parentGeom.local, sizeof parentLocal);
        }
        float next[16];
        multiply(parentLocal, chain, next);
        std::memcpy(chain, next, sizeof chain);
        const std::uint32_t step = model.geoms[parent].parent;
        if (step == parent) break;
        parent = step;
        ++guard;
    }
    const ModelGeom& current = model.geoms[index];
    if (current.matrixFrames > 0 && !current.matrixAnimation.empty()) {
        const int frameIndex = std::max(0, std::min(current.matrixFrames - 1, static_cast<int>(frame) % current.matrixFrames));
        multiply(current.matrixAnimation.data() + static_cast<std::size_t>(frameIndex) * 16u, chain, out);
    } else {
        multiply(current.local, chain, out);
    }
}

float groundHeight(MapFile& map, float x, float y) {
    return map.heightAt(x, y);
}

float sceneAnimationRate(int id) {
    switch (id) {
    case 184: case 188: case 183: case 175: case 83: case 187:
    case 126: case 269: case 215: case 461: case 448:
        return 1.f;
    default:
        return .5f;
    }
}

void animatedUv(const ModelSubset& subset, float frame, float u, float v, float& outU, float& outV) {
    outU = u;
    outV = v;
    if (subset.uvFrames <= 0 || subset.uvAnimation.empty()) return;
    const int index = std::max(0, std::min(subset.uvFrames - 1, static_cast<int>(frame) % subset.uvFrames));
    const float* m = subset.uvAnimation.data() + static_cast<std::size_t>(index) * 16u;
    outU = u * m[0] + v * m[4] + m[12];
    outV = u * m[1] + v * m[5] + m[13];
}

float animatedOpacity(const ModelSubset& subset, float frame) {
    if (subset.opacityAnimation.size() < 2) return 1.f;
    const std::size_t count = subset.opacityAnimation.size() / 2u;
    if (frame <= subset.opacityAnimation[0]) return subset.opacityAnimation[1];
    for (std::size_t i = 1; i < count; ++i) {
        const float k0 = subset.opacityAnimation[(i - 1) * 2u];
        const float v0 = subset.opacityAnimation[(i - 1) * 2u + 1];
        const float k1 = subset.opacityAnimation[i * 2u];
        const float v1 = subset.opacityAnimation[i * 2u + 1];
        if (frame <= k1) {
            const float t = k1 > k0 ? (frame - k0) / (k1 - k0) : 0.f;
            return std::max(0.f, std::min(1.f, v0 + (v1 - v0) * t));
        }
    }
    return std::max(0.f, std::min(1.f, subset.opacityAnimation[count * 2u - 1u]));
}

void modelBounds(const Model& model, float& minX, float& minY, float& minZ, float& maxX, float& maxY, float& maxZ) {
    float ident[16];
    identity(ident);
    bool any = false;
    minX = minY = minZ = 1e9f;
    maxX = maxY = maxZ = -1e9f;
    for (std::size_t g = 0; g < model.geoms.size(); ++g) {
        if (model.geoms[g].position.empty()) continue;
        float space[16];
        geomWorld(model, g, ident, 0.f, space);
        const std::vector<float>& pos = model.geoms[g].position;
        for (std::size_t i = 0; i + 2 < pos.size(); i += 3) {
            float x = 0.f;
            float y = 0.f;
            float z = 0.f;
            transformPoint(space, pos[i], pos[i + 1], pos[i + 2], x, y, z);
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            minZ = std::min(minZ, z);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            maxZ = std::max(maxZ, z);
            any = true;
        }
    }
    if (!any) minX = minY = minZ = maxX = maxY = maxZ = 0.f;
}

void addWalk(SceneMesh& out, const float* x, const float* y, const float* z) {
    const float ex1 = x[1] - x[0];
    const float ey1 = y[1] - y[0];
    const float ez1 = z[1] - z[0];
    const float ex2 = x[2] - x[0];
    const float ey2 = y[2] - y[0];
    const float ez2 = z[2] - z[0];
    const float nup = ex1 * ey2 - ey1 * ex2;
    const float nx = ey1 * ez2 - ez1 * ey2;
    const float ny = ez1 * ex2 - ex1 * ez2;
    const float len2 = nup * nup + nx * nx + ny * ny;
    if (len2 < 1e-8f || std::fabs(nup) < 0.65f * std::sqrt(len2)) return;
    float minX = x[0];
    float maxX = x[0];
    float minY = y[0];
    float maxY = y[0];
    for (int k = 1; k < 3; ++k) {
        minX = std::min(minX, x[k]);
        maxX = std::max(maxX, x[k]);
        minY = std::min(minY, y[k]);
        maxY = std::max(maxY, y[k]);
    }
    if (maxX - minX > 40.f || maxY - minY > 40.f) return;
    const int index = static_cast<int>(out.walk.size() / 9);
    for (int k = 0; k < 3; ++k) {
        out.walk.push_back(x[k]);
        out.walk.push_back(y[k]);
        out.walk.push_back(z[k]);
    }
    auto cell = [](float v) { return static_cast<int>(std::floor(v / 4.f)); };
    for (int cy = cell(minY); cy <= cell(maxY); ++cy) {
        for (int cx = cell(minX); cx <= cell(maxX); ++cx) {
            const std::uint64_t key = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32) |
                                      static_cast<std::uint32_t>(cy);
            out.walkCell[key].push_back(index);
        }
    }
}

}  // namespace

bool slabHit(const SceneSlab& slab, float x, float y, float& top) {
    const float theta = -slab.yawDeg * (3.14159265f / 180.f) + 3.14159265f;
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const float dx = x - slab.x;
    const float dy = y - slab.y;
    const float lx = dx * c - dy * s;
    const float ly = dx * s + dy * c;
    if (lx < slab.minX - 0.05f || lx > slab.maxX + 0.05f || ly < slab.minY - 0.05f || ly > slab.maxY + 0.05f)
        return false;
    top = slab.z + slab.top;
    return true;
}

bool SceneWorld::selfTest() {
    float local[16];
    float node[16];
    float world[16];
    identity(local);
    nodeMatrix(node, 0.f, 10.f, 20.f, 3.f);
    multiply(local, node, world);
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    transformPoint(world, 1.f, 0.f, 2.f, x, y, z);
    if (std::fabs(x - 9.f) > 1e-3f || std::fabs(y - 20.f) > 1e-3f || std::fabs(z - 5.f) > 1e-3f) return false;
    nodeMatrix(node, 90.f, 0.f, 0.f, 0.f);
    multiply(local, node, world);
    transformPoint(world, 1.f, 0.f, 2.f, x, y, z);
    if (std::fabs(x) > 1e-3f || std::fabs(y + 1.f) > 1e-3f || std::fabs(z - 2.f) > 1e-3f) return false;
    SceneMesh mesh;
    const float triX[3] = {0.f, 2.f, 0.f};
    const float triY[3] = {0.f, 0.f, 2.f};
    const float triZ[3] = {1.f, 1.f, 1.f};
    addWalk(mesh, triX, triY, triZ);
    // Degrau de 1 m. O terreno em 0 nao substitui o piso, e um telhado em 5 nao entra no passo.
    if (std::fabs(mesh.surfaceAt(0.4f, 0.4f, 0.6f, 0.f) - 1.f) > 1e-3f) return false;
    if (std::fabs(mesh.surfaceAt(0.4f, 0.4f, 0.2f, 0.f) - 0.f) > 1e-3f) return false;
    SceneSlab wall;
    wall.x = 10.f;
    wall.y = 10.f;
    wall.solid = true;
    wall.minX = -1.f;
    wall.maxX = 1.f;
    wall.minY = -0.5f;
    wall.maxY = 0.5f;
    wall.top = 1.2f;
    mesh.slabs.push_back(wall);
    if (!mesh.solidAt(10.f, 10.f, 0.4f) || mesh.solidAt(14.f, 10.f, 0.4f)) return false;
    return true;
}

bool SceneWorld::open(const std::string& clientRoot, const std::string& mapName, const std::string& mapRoot) {
    root_ = clientRoot;
    sectionsX_ = 0;
    sectionsY_ = 0;
    sectionW_ = 8;
    sectionH_ = 8;
    obj_.clear();
    records_.clear();
    models_.clear();
    texByLower_.clear();
    texScanned_ = false;
    records_.clear();
    models_.clear();
    texByLower_.clear();
    texScanned_ = false;
    obj_.clear();

    const std::string tablePath = clientRoot + "/scripts/table/sceneobjinfo.txt";
    FILE* table = std::fopen(tablePath.c_str(), "rb");
    if (!table) return false;
    if (std::fseek(table, 0, SEEK_END) != 0) {
        std::fclose(table);
        return false;
    }
    const long tableLen = std::ftell(table);
    if (tableLen <= 0 || tableLen > 8 * 1024 * 1024) {
        std::fclose(table);
        return false;
    }
    std::string text(static_cast<std::size_t>(tableLen), '\0');
    std::fseek(table, 0, SEEK_SET);
    if (std::fread(text.data(), 1, text.size(), table) != text.size()) {
        std::fclose(table);
        return false;
    }
    std::fclose(table);
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        std::size_t end = text.find('\n', cursor);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(cursor, end - cursor);
        cursor = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 3 || line[0] == '/') continue;
        std::vector<std::string> cols;
        std::size_t at = 0;
        while (at <= line.size()) {
            const std::size_t end = line.find('\t', at);
            cols.push_back(line.substr(at, end == std::string::npos ? std::string::npos : end - at));
            if (end == std::string::npos) break;
            at = end + 1;
        }
        if (cols.size() < 2) continue;
        std::string file = cols[1];
        while (!file.empty() && (file.back() == ' ' || file.back() == '\t')) file.pop_back();
        if (file.empty()) continue;
        const int id = std::atoi(cols[0].c_str());
        if (id < 0 || id > 100000) continue;
        if (static_cast<int>(records_.size()) <= id) records_.resize(static_cast<std::size_t>(id) + 1);
        SceneRecord record;
        record.file = file;
        record.kind = cols.size() > 3 ? std::atoi(cols[3].c_str()) : 0;
        if (record.kind == 2 && cols.size() > 2) {
            std::string name = cols[2];
            for (char& ch : name) {
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            }
            if (name.find("impass") != std::string::npos) record.solid = true;
            else record.passable = true;
        }
        records_[static_cast<std::size_t>(id)] = std::move(record);
    }

    const std::string mapFolder = (mapRoot.empty() ? clientRoot : mapRoot) + "/map";
    std::string objPath = mapFolder + "/" + mapName + ".obj";
    if (!fileExists(objPath)) {
        // O cliente Windows abre mapas em um filesystem sem diferenca de caixa
        // (CTF.obj e solicitado como ctf.obj). Reproduz isso no Linux.
        const std::string wanted = lowerCopy(mapName + ".obj");
        if (DIR* dir = opendir(mapFolder.c_str())) {
            while (dirent* ent = readdir(dir)) {
                if (lowerCopy(ent->d_name) == wanted) {
                    objPath = mapFolder + "/" + ent->d_name;
                    break;
                }
            }
            closedir(dir);
        }
    }
    FILE* obj = std::fopen(objPath.c_str(), "rb");
    // CSceneObjFile::Init criava uma camada vazia quando o .obj nao existia.
    // Alguns mapas distribuidos dependem disso; terreno sem objetos e valido.
    if (!obj) return !records_.empty();
    if (std::fseek(obj, 0, SEEK_END) != 0) {
        std::fclose(obj);
        return false;
    }
    const long objLen = std::ftell(obj);
    if (objLen < 64 || objLen > 64 * 1024 * 1024) {
        std::fclose(obj);
        return false;
    }
    obj_.resize(static_cast<std::size_t>(objLen));
    std::fseek(obj, 0, SEEK_SET);
    if (std::fread(obj_.data(), 1, obj_.size(), obj) != obj_.size()) {
        std::fclose(obj);
        obj_.clear();
        return false;
    }
    std::fclose(obj);
    if (std::memcmp(obj_.data(), "HF Object File!", 15) != 0) return false;
    if (ri32(obj_.data() + 16) != 600) return false;
    sectionsX_ = ri32(obj_.data() + 24);
    sectionsY_ = ri32(obj_.data() + 28);
    sectionW_ = ri32(obj_.data() + 32);
    sectionH_ = ri32(obj_.data() + 36);
    if (sectionsX_ <= 0 || sectionsY_ <= 0 || sectionW_ <= 0 || sectionH_ <= 0) return false;
    const std::size_t indexBytes = static_cast<std::size_t>(sectionsX_) * static_cast<std::size_t>(sectionsY_) * 8;
    if (44 + indexBytes > obj_.size()) return false;
    return !records_.empty();
}

void SceneWorld::build(MapFile& map, float centerX, float centerY, float radius, SceneMesh& out, Pass pass) {
    out = SceneMesh{};
    if (obj_.empty() || sectionsX_ <= 0) return;
    const int minTileX = static_cast<int>(centerX - radius);
    const int maxTileX = static_cast<int>(centerX + radius);
    const int minTileY = static_cast<int>(centerY - radius);
    const int maxTileY = static_cast<int>(centerY + radius);
    int sec0x = minTileX / sectionW_;
    int sec1x = maxTileX / sectionW_;
    int sec0y = minTileY / sectionH_;
    int sec1y = maxTileY / sectionH_;
    if (sec0x < 0) sec0x = 0;
    if (sec0y < 0) sec0y = 0;
    if (sec1x >= sectionsX_) sec1x = sectionsX_ - 1;
    if (sec1y >= sectionsY_) sec1y = sectionsY_ - 1;
    const int playerSecX = static_cast<int>(centerX) / sectionW_;
    const int playerSecY = static_cast<int>(centerY) / sectionH_;

    struct Sec {
        int x;
        int y;
        int rank;
    };
    std::vector<Sec> sections;
    for (int sy = sec0y; sy <= sec1y; ++sy) {
        for (int sx = sec0x; sx <= sec1x; ++sx) {
            const int dx = sx - playerSecX;
            const int dy = sy - playerSecY;
            sections.push_back({sx, sy, dx * dx + dy * dy});
        }
    }
    std::sort(sections.begin(), sections.end(), [](const Sec& a, const Sec& b) { return a.rank < b.rank; });

    std::unordered_map<std::string, std::size_t> batchOf;
    auto batchFor = [&](int mode, const std::string& texture) -> SceneBatch& {
        const std::string key = std::to_string(mode) + "\n" + texture;
        const auto it = batchOf.find(key);
        if (it != batchOf.end()) return out.batches[it->second];
        batchOf[key] = out.batches.size();
        SceneBatch batch;
        batch.mode = mode;
        batch.texture = texture;
        out.batches.push_back(std::move(batch));
        return out.batches.back();
    };

    constexpr int kTriCap = 350000;
    for (const Sec& sec : sections) {
        if (out.triangles >= kTriCap) break;
        const int secNo = sec.y * sectionsX_ + sec.x;
        const std::uint8_t* index = obj_.data() + 44 + static_cast<std::size_t>(secNo) * 8;
        const std::int32_t pos = ri32(index);
        int count = ri32(index + 4);
        if (pos <= 0 || count <= 0) continue;
        if (count > 25) count = 25;
        if (static_cast<std::size_t>(pos) + static_cast<std::size_t>(count) * 20 > obj_.size()) continue;
        for (int i = 0; i < count; ++i) {
            if (out.triangles >= kTriCap) break;
            const std::uint8_t* rec = obj_.data() + pos + i * 20;
            const std::uint16_t typeId = ru16(rec);
            const int kind = typeId >> 14;
            const int id = typeId & 0x3fff;
            const std::uint32_t nx = ru32(rec + 4);
            const std::uint32_t ny = ru32(rec + 8);
            const float mx = (static_cast<std::uint32_t>(sec.x * sectionW_ * 100) + nx) / 100.f;
            const float my = (static_cast<std::uint32_t>(sec.y * sectionH_ * 100) + ny) / 100.f;
            if (std::fabs(mx - centerX) > radius || std::fabs(my - centerY) > radius) continue;
            if (kind == 1) {
                if (pass != Pass::Animated && historical_map_scene_effects::enabled() && id > 0) {
                    const std::int16_t heightOff = ri16(rec + 12);
                    const std::int16_t yaw = ri16(rec + 14);
                    MapSceneEffectPlacement placement;
                    placement.key = (static_cast<std::uint64_t>(secNo) << 32) |
                                    static_cast<std::uint32_t>(i);
                    placement.effectId = id;
                    placement.x = mx;
                    placement.y = my;
                    placement.height = groundHeight(map, mx, my) + static_cast<float>(heightOff) / 100.f;
                    placement.yaw = static_cast<float>(yaw) / 100.f;
                    out.mapEffects.push_back(placement);
                }
                continue;
            }
            if (kind != 0) continue;
            if (id < 0 || id >= static_cast<int>(records_.size()) || records_[static_cast<std::size_t>(id)].file.empty()) {
                ++out.modelFail;
                continue;
            }
            const SceneRecord& info = records_[static_cast<std::size_t>(id)];
            // 1 sentar, 2 passagem, 3 luz e 6 som sao volumes do editor.
            // Desenha-los cobre o mapa de cubos verde, vermelho e azul.
            const bool helper = info.kind != 0;
            bool failed = false;
            const Model* model = modelFor(info.file, failed);
            if (!model) {
                if (failed) ++out.modelFail;
                continue;
            }
            // Check animation before any tile interpolation or transform.
            // Animated pass skips ~95% of static scene objects immediately.
            CachedModel& modelCache = models_[info.file];
            if (!helper) {
                if (!modelCache.animationChecked) {
                    for (const ModelGeom& geom : model->geoms) {
                        if (geom.matrixFrames > 0 && !geom.matrixAnimation.empty())
                            modelCache.containsAnimation = true;
                        for (const ModelSubset& subset : geom.subsets)
                            if (subset.uvFrames > 0 || !subset.opacityAnimation.empty() ||
                                !subset.imageAnimation.empty()) modelCache.containsAnimation = true;
                    }
                    modelCache.animationChecked = true;
                }
                if (pass == Pass::Animated && !modelCache.containsAnimation) continue;
            }
            const std::int16_t heightOff = ri16(rec + 12);
            const std::int16_t yaw = ri16(rec + 14);
            const float base = groundHeight(map, mx, my) + static_cast<float>(heightOff) / 100.f;
            float node[16];
            nodeMatrix(node, static_cast<float>(yaw), mx, my, base);
            if (helper) {
                if (pass == Pass::Animated) continue;
                CachedModel& cache = models_[info.file];
                if (!cache.boundsReady) {
                    modelBounds(cache.model, cache.minX, cache.minY, cache.minZ, cache.maxX, cache.maxY, cache.maxZ);
                    cache.boundsReady = true;
                }
                if (info.passable || info.solid) {
                    SceneSlab slab;
                    slab.x = mx;
                    slab.y = my;
                    slab.z = base;
                    slab.yawDeg = static_cast<float>(yaw);
                    slab.minX = cache.minX;
                    slab.maxX = cache.maxX;
                    slab.minY = cache.minY;
                    slab.maxY = cache.maxY;
                    slab.top = cache.maxZ;
                    slab.solid = info.solid;
                    out.slabs.push_back(slab);
                    if (slab.solid && (slab.maxX - slab.minX) > 0.8f && (slab.maxY - slab.minY) > 0.8f) {
                        ++out.staticShadows;
                        SceneBatch& shadow = batchFor(6, {});
                        constexpr int segments = 12;
                        const float cx = (slab.minX + slab.maxX) * 0.5f;
                        const float cz = (slab.minY + slab.maxY) * 0.5f;
                        const float rx = (slab.maxX - slab.minX) * 0.32f;
                        const float rz = (slab.maxY - slab.minY) * 0.32f;
                        for (int segment = 0; segment < segments; ++segment) {
                            const float a0 = 6.2831853f * static_cast<float>(segment) / segments;
                            const float a1 = 6.2831853f * static_cast<float>(segment + 1) / segments;
                            const float floor = slab.z + 0.025f;
                            auto shadowVertex = [floor](float x, float z, float u, float v, float alpha) {
                                MeshVertex vertex{};
                                vertex.x = x; vertex.y = floor; vertex.z = z;
                                vertex.r = 0.015f; vertex.g = 0.02f; vertex.b = 0.025f;
                                vertex.a = alpha; vertex.u = u; vertex.v = v;
                                return vertex;
                            };
                            shadow.verts.push_back(shadowVertex(cx, cz, .5f, .5f, .22f));
                            shadow.verts.push_back(shadowVertex(cx + std::cos(a0) * rx, cz + std::sin(a0) * rz, .0f, .0f, .06f));
                            shadow.verts.push_back(shadowVertex(cx + std::cos(a1) * rx, cz + std::sin(a1) * rz, 1.f, .0f, .06f));
                        }
                    }
                }
                continue;
            }
            const bool renderObject = pass == Pass::All ||
                (pass == Pass::Static && !modelCache.containsAnimation) ||
                (pass == Pass::Animated && modelCache.containsAnimation);
            // Keep the original collision snapshot for animated furniture and
            // moving scenery in the static mesh. No collision is recomputed on
            // the 15 Hz visual update. The original offline collision logic
            // also uses one scene snapshot between sector refreshes.
            const bool buildCollision = pass != Pass::Animated;
            if (!renderObject && !buildCollision) continue;
            if (pass != Pass::Animated) ++out.placed;
            const float objectFrame = animationTime_ * 30.f * sceneAnimationRate(id);
            for (std::size_t g = 0; g < model->geoms.size(); ++g) {
                const ModelGeom& geom = model->geoms[g];
                if (geom.indices.empty() || geom.position.empty()) continue;
                if (geom.matrixFrames > 0 && !geom.matrixAnimation.empty()) {
                    ++out.animatedParts;
                }
                float world[16];
                geomWorld(*model, g, node, objectFrame, world);
                const int vertexCount = static_cast<int>(geom.position.size() / 3);
                for (const ModelSubset& subset : geom.subsets) {
                    if (subset.indexCount < 3) continue;
                    if (subset.uvFrames > 0) ++out.animatedUv;
                    if (!subset.opacityAnimation.empty()) ++out.animatedOpacity;
                    if (!subset.imageAnimation.empty()) ++out.animatedImages;
                    const std::uint32_t last = subset.startIndex + subset.indexCount;
                    if (last > geom.indices.size()) continue;
                    // Non-rendered animated objects are processed only for
                    // their initial collision triangles in the static pass.
                    std::string imageName = renderObject ? subset.texture : std::string{};
                    if (renderObject && !subset.imageAnimation.empty()) {
                        const std::size_t frame = static_cast<std::size_t>(std::max(0, static_cast<int>(objectFrame))) % subset.imageAnimation.size();
                        imageName = subset.imageAnimation[frame];
                    }
                    std::string path = renderObject ? texturePath(imageName) : std::string{};
                    if (renderObject && path.empty() && imageName != subset.texture) path = texturePath(subset.texture);
                    if (renderObject && path.empty()) {
                        if (subset.texture.empty()) {
                            out.untexturedTriangles += static_cast<int>(subset.indexCount / 3);
                        } else if (std::find(out.missingTextures.begin(), out.missingTextures.end(), subset.texture) ==
                                   out.missingTextures.end()) {
                            out.missingTextures.push_back(subset.texture);
                        }
                    }
                    const int mode = path.empty() ? 0 : (subset.blend == 1 ? 4 :
                                                         (!subset.opacityAnimation.empty() ? 8 : 3));
                    SceneBatch* batch = renderObject ? &batchFor(mode, mode == 0 ? std::string() : path) : nullptr;
                    for (std::uint32_t n = subset.startIndex; n + 2 < last; n += 3) {
                        if (out.triangles >= kTriCap) break;
                        const std::uint32_t corner[3] = {geom.indices[n], geom.indices[n + 1], geom.indices[n + 2]};
                        if (corner[0] >= static_cast<std::uint32_t>(vertexCount) || corner[1] >= static_cast<std::uint32_t>(vertexCount) ||
                            corner[2] >= static_cast<std::uint32_t>(vertexCount)) {
                            continue;
                        }
                        float wx[3];
                        float wy[3];
                        float wz[3];
                        for (int k = 0; k < 3; ++k) {
                            const std::uint32_t vi = corner[k];
                            float px, py, pz;
                            transformPoint(world, geom.position[vi * 3], geom.position[vi * 3 + 1], geom.position[vi * 3 + 2], px, py, pz);
                            wx[k] = px;
                            wy[k] = py;
                            wz[k] = pz;
                            if (batch) {
                                float nxv = 0.f;
                                float nyv = 0.f;
                                float nzv = 0.f;
                                if (geom.normal.size() >= geom.position.size()) {
                                    transformDir(world, geom.normal[vi * 3], geom.normal[vi * 3 + 1],
                                                 geom.normal[vi * 3 + 2], nxv, nyv, nzv);
                                    const float nl = std::sqrt(nxv * nxv + nyv * nyv + nzv * nzv);
                                    if (nl > 1e-4f) {
                                        nxv /= nl;
                                        nyv /= nl;
                                        nzv /= nl;
                                    }
                                }
                                float cr = subset.r;
                                float cg = subset.g;
                                float cb = subset.b;
                                if (geom.color.size() >= static_cast<std::size_t>(vi + 1) * 3) {
                                    cr *= geom.color[vi * 3] / 255.f;
                                    cg *= geom.color[vi * 3 + 1] / 255.f;
                                    cb *= geom.color[vi * 3 + 2] / 255.f;
                                }
                                MeshVertex vertex{};
                                vertex.x = px;
                                vertex.y = pz;
                                vertex.z = py;
                                vertex.r = cr;
                                vertex.g = cg;
                                vertex.b = cb;
                                vertex.a = 1.f;
                                vertex.nx = nxv;
                                vertex.ny = nzv;
                                vertex.nz = nyv;
                                if (geom.uv.size() >= static_cast<std::size_t>(vi + 1) * 2) {
                                    const float baseU = geom.uv[vi * 2];
                                    const float baseV = geom.uv[vi * 2 + 1];
                                    animatedUv(subset, objectFrame, baseU, baseV, vertex.u, vertex.v);
                                }
                                vertex.a = animatedOpacity(subset, objectFrame);
                                batch->verts.push_back(vertex);
                            }
                        }
                        if (buildCollision) addWalk(out, wx, wy, wz);
                        if (batch) ++out.triangles;
                    }
                }
            }
            if (modelCache.containsAnimation && pass != Pass::Animated) ++out.animatedObjects;
        }
    }
    std::stable_partition(out.batches.begin(), out.batches.end(), [](const SceneBatch& batch) { return batch.mode == 6; });
    std::stable_partition(out.batches.begin(), out.batches.end(), [](const SceneBatch& batch) { return batch.mode != 4; });
}

float SceneMesh::surfaceAt(float x, float y, float hint, float terrain) const {
    constexpr float kStep = 0.72f;
    float best = terrain;
    auto consider = [&](float height) {
        if (height <= hint + kStep && height > best) best = height;
    };
    for (const SceneSlab& slab : slabs) {
        if (slab.solid) continue;
        float top = 0.f;
        if (slabHit(slab, x, y, top)) consider(top);
    }
    auto cell = [](float v) { return static_cast<int>(std::floor(v / 4.f)); };
    const int cx = cell(x);
    const int cy = cell(y);
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            const std::uint64_t key = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx + ox)) << 32) |
                                      static_cast<std::uint32_t>(cy + oy);
            const auto found = walkCell.find(key);
            if (found == walkCell.end()) continue;
            for (const int index : found->second) {
                const float* tri = walk.data() + static_cast<std::size_t>(index) * 9;
                const float x0 = tri[0];
                const float y0 = tri[1];
                const float z0 = tri[2];
                const float x1 = tri[3];
                const float y1 = tri[4];
                const float z1 = tri[5];
                const float x2 = tri[6];
                const float y2 = tri[7];
                const float z2 = tri[8];
                const float den = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
                if (std::fabs(den) < 1e-5f) continue;
                const float w0 = ((x1 - x) * (y2 - y) - (y1 - y) * (x2 - x)) / den;
                const float w1 = ((x - x0) * (y2 - y0) - (y - y0) * (x2 - x0)) / den;
                const float w2 = 1.f - w0 - w1;
                if (w0 < -0.04f || w1 < -0.04f || w2 < -0.04f) continue;
                consider(w0 * z0 + w1 * z1 + w2 * z2);
            }
        }
    }
    return best;
}

bool SceneMesh::solidAt(float x, float y, float feet) const {
    for (const SceneSlab& slab : slabs) {
        if (!slab.solid) continue;
        float top = 0.f;
        if (!slabHit(slab, x, y, top)) continue;
        if (feet < top + 1.5f && feet + 1.6f > top - 0.15f) return true;
    }
    return false;
}

const Model* SceneWorld::modelFor(const std::string& fileName, bool& failed) {
    failed = false;
    CachedModel& slot = models_[fileName];
    if (!slot.tried) {
        slot.tried = true;
        slot.ok = slot.model.load(root_ + "/model/scene/" + fileName);
        if (!slot.ok) {
            DIR* dir = opendir((root_ + "/model/scene").c_str());
            if (dir) {
                const std::string want = lowerCopy(fileName);
                while (const dirent* ent = readdir(dir)) {
                    if (lowerCopy(ent->d_name) == want) {
                        slot.ok = slot.model.load(root_ + "/model/scene/" + ent->d_name);
                        break;
                    }
                }
                closedir(dir);
            }
        }
    }
    if (!slot.ok) {
        failed = true;
        return nullptr;
    }
    return &slot.model;
}

std::string SceneWorld::texturePath(const std::string& fileName) {
    if (fileName.empty()) return {};
    auto withExt = [](std::string name, const char* ext) {
        const std::size_t dot = name.find_last_of('.');
        if (dot == std::string::npos) return name + ext;
        return name.substr(0, dot) + ext;
    };
    const std::string folder = root_ + "/texture/scene/";
    const std::string ddsName = withExt(fileName, ".dds");
    // Preferir recurso correspondente ao nome original, mesmo se convertido para PNG.
    const std::string original = resolveTexturePath(folder + fileName);
    if (!original.empty()) return original;
    const std::string mappedDds = resolveTexturePath(folder + ddsName);
    if (!mappedDds.empty()) return mappedDds;
    if (!texScanned_) {
        texScanned_ = true;
        DIR* dir = opendir(folder.c_str());
        if (dir) {
            while (const dirent* ent = readdir(dir)) {
                texByLower_[lowerCopy(ent->d_name)] = folder + ent->d_name;
            }
            closedir(dir);
        }
    }
    const auto dds = texByLower_.find(lowerCopy(ddsName));
    if (dds != texByLower_.end()) return dds->second;
    const auto orig = texByLower_.find(lowerCopy(fileName));
    if (orig != texByLower_.end()) return orig->second;
    return {};
}

}  // namespace top
