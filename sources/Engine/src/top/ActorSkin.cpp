#include "top/ActorSkin.h"

#include "top/Image.h"
#include "top/ModelFile.h"
#include "top/SceneWorld.h"
#include "top/TexturePaths.h"
#include "top/WeaponPose.h"
#include "top/ActionPose.h"
#include "top/CharacterPoseBinding.h"
#include "top/ActorFrameCache.h"
#include "top/HistoricalCharacterHeight.h"
#include "top/PopulationIdentity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <dirent.h>
#include <unordered_map>

namespace top {
namespace {

constexpr float kPi = 3.14159265f;

struct PoseSpan {
    int start = 0;
    int end = 0;
    int impact = 0;
    bool ok() const { return end > start; }
};

struct BoneClip {
    struct Dummy {
        std::uint32_t id = 0;
        std::uint32_t parent = 0xffffffffu;
        float matrix[16] = {};
    };
    int boneNum = 0;
    int frameNum = 0;
    int keyType = 0;
    std::vector<std::uint32_t> parent;
    std::vector<float> inv;
    std::vector<float> mat43;
    std::vector<float> pos;
    std::vector<float> quat;
    std::vector<std::string> names;
    std::vector<Dummy> dummies;

    bool load(const std::string& path);
    void matrices(int frame, std::vector<float>& out, bool skin = true) const;
    bool dummyMatrix(int frame, int id, float* out) const;
    bool dummyMatrixFromPose(const std::vector<float>& pose, int id, float* out) const;
};

bool readFile(const std::string& path, std::vector<std::uint8_t>& out, std::size_t limit) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long length = std::ftell(file);
    if (length <= 0 || static_cast<std::size_t>(length) > limit) {
        std::fclose(file);
        return false;
    }
    out.resize(static_cast<std::size_t>(length));
    std::fseek(file, 0, SEEK_SET);
    const bool ok = std::fread(out.data(), 1, out.size(), file) == out.size();
    std::fclose(file);
    return ok;
}

std::uint32_t ru32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

float rf32(const std::uint8_t* p) {
    float value = 0.f;
    std::memcpy(&value, p, 4);
    return value;
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

bool inverseAffine(const float* m, float* out) {
    const float a00 = m[0], a01 = m[1], a02 = m[2];
    const float a10 = m[4], a11 = m[5], a12 = m[6];
    const float a20 = m[8], a21 = m[9], a22 = m[10];
    const float det = a00 * (a11 * a22 - a12 * a21) -
                      a01 * (a10 * a22 - a12 * a20) +
                      a02 * (a10 * a21 - a11 * a20);
    if (std::fabs(det) < 1e-8f) return false;
    const float invDet = 1.f / det;
    identity(out);
    out[0] = (a11 * a22 - a12 * a21) * invDet;
    out[1] = (a02 * a21 - a01 * a22) * invDet;
    out[2] = (a01 * a12 - a02 * a11) * invDet;
    out[4] = (a12 * a20 - a10 * a22) * invDet;
    out[5] = (a00 * a22 - a02 * a20) * invDet;
    out[6] = (a02 * a10 - a00 * a12) * invDet;
    out[8] = (a10 * a21 - a11 * a20) * invDet;
    out[9] = (a01 * a20 - a00 * a21) * invDet;
    out[10] = (a00 * a11 - a01 * a10) * invDet;
    out[12] = -(m[12] * out[0] + m[13] * out[4] + m[14] * out[8]);
    out[13] = -(m[12] * out[1] + m[13] * out[5] + m[14] * out[9]);
    out[14] = -(m[12] * out[2] + m[13] * out[6] + m[14] * out[10]);
    return true;
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

// Mesmo lwMatrixCtrl::UpdateYawPitchRoll dos predios e do CCharacter.
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

void quatToMat(float* m, float x, float y, float z, float w, float px, float py, float pz) {
    const float x2 = x + x;
    const float y2 = y + y;
    const float z2 = z + z;
    const float xx = x * x2;
    const float xy = x * y2;
    const float xz = x * z2;
    const float yy = y * y2;
    const float yz = y * z2;
    const float zz = z * z2;
    const float wx = w * x2;
    const float wy = w * y2;
    const float wz = w * z2;
    m[0] = 1.f - (yy + zz);
    m[1] = xy + wz;
    m[2] = xz - wy;
    m[3] = 0.f;
    m[4] = xy - wz;
    m[5] = 1.f - (xx + zz);
    m[6] = yz + wx;
    m[7] = 0.f;
    m[8] = xz + wy;
    m[9] = yz - wx;
    m[10] = 1.f - (xx + yy);
    m[11] = 0.f;
    m[12] = px;
    m[13] = py;
    m[14] = pz;
    m[15] = 1.f;
}

bool BoneClip::load(const std::string& path) {
    std::vector<std::uint8_t> file;
    if (!readFile(path, file, 32 * 1024 * 1024) || file.size() < 20) return false;
    const std::uint32_t version = ru32(file.data());
    // lwAnimDataBone::Load(const char*) recusa versao abaixo de 0x1000.
    if (version < 0x1000 || version > 0x1005) return false;
    boneNum = static_cast<int>(ru32(file.data() + 4));
    frameNum = static_cast<int>(ru32(file.data() + 8));
    const int dummyNum = static_cast<int>(ru32(file.data() + 12));
    keyType = static_cast<int>(ru32(file.data() + 16));
    if (boneNum <= 0 || boneNum > 256 || frameNum <= 0 || frameNum > 20000 || dummyNum < 0 || dummyNum > 256) return false;
    if (keyType < 1 || keyType > 3) return false;
    std::size_t cursor = 20;
    const std::size_t headerBytes = static_cast<std::size_t>(boneNum) * 72 + static_cast<std::size_t>(boneNum) * 64 +
                                    static_cast<std::size_t>(dummyNum) * 72;
    if (cursor + headerBytes > file.size()) return false;
    parent.resize(static_cast<std::size_t>(boneNum));
    names.resize(static_cast<std::size_t>(boneNum));
    for (int i = 0; i < boneNum; ++i) {
        const std::uint8_t* p = file.data() + cursor;
        const char* text = reinterpret_cast<const char*>(p);
        names[static_cast<std::size_t>(i)].assign(text, strnlen(text, 64));
        parent[static_cast<std::size_t>(i)] = ru32(p + 68);
        cursor += 72;
    }
    inv.resize(static_cast<std::size_t>(boneNum) * 16);
    for (int i = 0; i < boneNum * 16; ++i) inv[static_cast<std::size_t>(i)] = rf32(file.data() + cursor + static_cast<std::size_t>(i) * 4);
    cursor += static_cast<std::size_t>(boneNum) * 64;
    dummies.resize(static_cast<std::size_t>(dummyNum));
    for (int i = 0; i < dummyNum; ++i) {
        Dummy& dummy = dummies[static_cast<std::size_t>(i)];
        dummy.id = ru32(file.data() + cursor);
        dummy.parent = ru32(file.data() + cursor + 4);
        for (int n = 0; n < 16; ++n) dummy.matrix[n] = rf32(file.data() + cursor + 8 + static_cast<std::size_t>(n) * 4);
        cursor += 72;
    }
    if (keyType == 1) {
        const std::size_t n = static_cast<std::size_t>(boneNum) * static_cast<std::size_t>(frameNum) * 12;
        if (cursor + n * 4 > file.size()) return false;
        mat43.resize(n);
        for (std::size_t i = 0; i < n; ++i) mat43[i] = rf32(file.data() + cursor + i * 4);
    } else if (keyType == 2) {
        const std::size_t n = static_cast<std::size_t>(boneNum) * static_cast<std::size_t>(frameNum) * 16;
        if (cursor + n * 4 > file.size()) return false;
        mat43.resize(n);
        for (std::size_t i = 0; i < n; ++i) mat43[i] = rf32(file.data() + cursor + i * 4);
    } else if (version >= 0x1003) {
        const std::size_t posN = static_cast<std::size_t>(boneNum) * static_cast<std::size_t>(frameNum) * 3;
        const std::size_t quatN = static_cast<std::size_t>(boneNum) * static_cast<std::size_t>(frameNum) * 4;
        if (cursor + (posN + quatN) * 4 > file.size()) return false;
        pos.resize(posN);
        quat.resize(quatN);
        // Por osso: todos os positions, depois todos os quaternions.
        std::size_t at = cursor;
        for (int bone = 0; bone < boneNum; ++bone) {
            for (int frame = 0; frame < frameNum; ++frame) {
                const std::size_t dst = (static_cast<std::size_t>(bone) * frameNum + frame) * 3;
                pos[dst] = rf32(file.data() + at);
                pos[dst + 1] = rf32(file.data() + at + 4);
                pos[dst + 2] = rf32(file.data() + at + 8);
                at += 12;
            }
            for (int frame = 0; frame < frameNum; ++frame) {
                const std::size_t dst = (static_cast<std::size_t>(bone) * frameNum + frame) * 4;
                quat[dst] = rf32(file.data() + at);
                quat[dst + 1] = rf32(file.data() + at + 4);
                quat[dst + 2] = rf32(file.data() + at + 8);
                quat[dst + 3] = rf32(file.data() + at + 12);
                at += 16;
            }
        }
    } else {
        pos.assign(static_cast<std::size_t>(boneNum) * frameNum * 3, 0.f);
        quat.resize(static_cast<std::size_t>(boneNum) * frameNum * 4);
        std::size_t at = cursor;
        for (int bone = 0; bone < boneNum; ++bone) {
            const int posCount = parent[static_cast<std::size_t>(bone)] == 0xffffffffu ? frameNum : 1;
            if (at + static_cast<std::size_t>(posCount) * 12 + static_cast<std::size_t>(frameNum) * 16 > file.size()) return false;
            float px = 0.f;
            float py = 0.f;
            float pz = 0.f;
            for (int frame = 0; frame < posCount; ++frame) {
                px = rf32(file.data() + at);
                py = rf32(file.data() + at + 4);
                pz = rf32(file.data() + at + 8);
                const std::size_t dst = (static_cast<std::size_t>(bone) * frameNum + frame) * 3;
                pos[dst] = px;
                pos[dst + 1] = py;
                pos[dst + 2] = pz;
                at += 12;
            }
            for (int frame = posCount; frame < frameNum; ++frame) {
                const std::size_t dst = (static_cast<std::size_t>(bone) * frameNum + frame) * 3;
                pos[dst] = px;
                pos[dst + 1] = py;
                pos[dst + 2] = pz;
            }
            for (int frame = 0; frame < frameNum; ++frame) {
                const std::size_t dst = (static_cast<std::size_t>(bone) * frameNum + frame) * 4;
                quat[dst] = rf32(file.data() + at);
                quat[dst + 1] = rf32(file.data() + at + 4);
                quat[dst + 2] = rf32(file.data() + at + 8);
                quat[dst + 3] = rf32(file.data() + at + 12);
                at += 16;
            }
        }
    }
    return true;
}

void BoneClip::matrices(int frame, std::vector<float>& out, bool skin) const {
    if (frame < 0) frame = 0;
    if (frame >= frameNum) frame = frameNum - 1;
    out.assign(static_cast<std::size_t>(boneNum) * 16, 0.f);
    for (int i = 0; i < boneNum; ++i) {
        float* m = out.data() + static_cast<std::size_t>(i) * 16;
        if (keyType == 1) {
            const float* s = mat43.data() + (static_cast<std::size_t>(i) * frameNum + frame) * 12;
            m[0] = s[0];
            m[1] = s[1];
            m[2] = s[2];
            m[4] = s[3];
            m[5] = s[4];
            m[6] = s[5];
            m[8] = s[6];
            m[9] = s[7];
            m[10] = s[8];
            m[12] = s[9];
            m[13] = s[10];
            m[14] = s[11];
            m[15] = 1.f;
        } else if (keyType == 2) {
            const float* s = mat43.data() + (static_cast<std::size_t>(i) * frameNum + frame) * 16;
            std::memcpy(m, s, sizeof(float) * 16);
        } else {
            const std::size_t p = (static_cast<std::size_t>(i) * frameNum + frame) * 3;
            const std::size_t q = (static_cast<std::size_t>(i) * frameNum + frame) * 4;
            quatToMat(m, quat[q], quat[q + 1], quat[q + 2], quat[q + 3], pos[p], pos[p + 1], pos[p + 2]);
        }
        const std::uint32_t parentId = parent[static_cast<std::size_t>(i)];
        if (parentId != 0xffffffffu && parentId < static_cast<std::uint32_t>(boneNum)) {
            float combined[16];
            multiply(m, out.data() + static_cast<std::size_t>(parentId) * 16, combined);
            std::memcpy(m, combined, sizeof combined);
        }
    }
    if (!skin) return;
    for (int i = 0; i < boneNum; ++i) {
        float combined[16];
        multiply(inv.data() + static_cast<std::size_t>(i) * 16, out.data() + static_cast<std::size_t>(i) * 16, combined);
        std::memcpy(out.data() + static_cast<std::size_t>(i) * 16, combined, sizeof combined);
    }
}

bool BoneClip::dummyMatrixFromPose(const std::vector<float>& pose, int id, float* out) const {
    const Dummy* wanted = nullptr;
    for (const Dummy& dummy : dummies) {
        if (static_cast<int>(dummy.id) == id) { wanted = &dummy; break; }
    }
    if (!wanted || wanted->parent >= static_cast<std::uint32_t>(boneNum) ||
        pose.size() < static_cast<std::size_t>(boneNum) * 16) return false;
    // The historical dummy uses the SKIN matrix, not the absolute bone pose;
    // otherwise weapon grips drift away from the hands (05-D regression).
    multiply(wanted->matrix, pose.data() + static_cast<std::size_t>(wanted->parent) * 16, out);
    return true;
}

bool BoneClip::dummyMatrix(int frame, int id, float* out) const {
    std::vector<float> pose;
    matrices(frame, pose, true);
    return dummyMatrixFromPose(pose, id, out);
}

std::string lowerCopy(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

void indexDir(const std::string& dir, std::unordered_map<std::string, std::string>& out) {
    DIR* handle = opendir(dir.c_str());
    if (!handle) return;
    while (dirent* ent = readdir(handle)) {
        if (ent->d_name[0] == '.') continue;
        const std::string name = ent->d_name;
        const std::string path = dir + "/" + name;
        DIR* child = opendir(path.c_str());
        if (child) {
            closedir(child);
            indexDir(path, out);
        } else {
            out[lowerCopy(name)] = path;
        }
    }
    closedir(handle);
}

std::vector<std::string> tabs(const std::string& line) {
    std::vector<std::string> out;
    std::size_t cursor = 0;
    while (cursor <= line.size()) {
        const std::size_t tab = line.find('\t', cursor);
        if (tab == std::string::npos) {
            out.push_back(line.substr(cursor));
            break;
        }
        out.push_back(line.substr(cursor, tab - cursor));
        cursor = tab + 1;
    }
    return out;
}

int field(const std::vector<std::string>& cols, int index) {
    if (index < 0 || index >= static_cast<int>(cols.size()) || cols[static_cast<std::size_t>(index)].empty()) return 0;
    return std::atoi(cols[static_cast<std::size_t>(index)].c_str());
}

struct ChaRow {
    int id = 0;
    int modal = 0;
    int model = 0;
    int suit = 0;
    int skin[5] = {};
    int shadow = 0;
    float length = 0.8f;
    float width = 0.8f;
};

bool loadTable(const std::string& path, std::unordered_map<int, std::vector<std::string>>& rows) {
    std::vector<std::uint8_t> file;
    if (!readFile(path, file, 40 * 1024 * 1024)) return false;
    std::size_t cursor = 0;
    while (cursor < file.size()) {
        std::size_t end = cursor;
        while (end < file.size() && file[end] != '\n') ++end;
        std::string line(reinterpret_cast<const char*>(file.data() + cursor), end - cursor);
        cursor = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 2 || line[0] == '/') continue;
        const int id = std::atoi(line.c_str());
        if (id <= 0) continue;
        rows[id] = tabs(line);
    }
    return !rows.empty();
}

// CharacterPoseInfo.txt define 7 grupos de 54 acoes. O antigo chamava
// _BigPose2SmallPose ao mudar o armamento; agora preservamos todas as variantes.
struct PoseBank {
    // action[weaponMode][baseAction] inclui as tres variacoes de ataque e
    // as acoes de habilidades 11..15, sem perder as poses 05-D ja usadas.
    std::array<std::array<PoseSpan, kWeaponPoseStride + 1>, kWeaponPoseGroups> action{};
};

bool loadPoses(const std::string& path, const int* ids, int idCount,
               std::unordered_map<int, PoseBank>& out) {
    std::vector<std::uint8_t> file;
    if (!readFile(path, file, 8 * 1024 * 1024)) return false;
    int current = -1;
    bool want = false;
    std::size_t cursor = 0;
    while (cursor < file.size()) {
        std::size_t end = cursor;
        while (end < file.size() && file[end] != '\n') ++end;
        std::string line(reinterpret_cast<const char*>(file.data() + cursor), end - cursor);
        cursor = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '/') continue;
        if (line[0] == '\t' || line[0] == ' ') {
            if (!want) continue;
            const std::vector<std::string> cols = tabs(line);
            std::vector<int> nums;
            for (const std::string& col : cols) {
                if (col.empty()) continue;
                nums.push_back(std::atoi(col.c_str()));
            }
            if (nums.size() < 3) continue;
            const int action = nums[0];
            if (action < 1 || action > kWeaponPoseGroups * kWeaponPoseStride) continue;
            const int mode = weaponPoseGroup(action);
            const int base = weaponPoseBase(action);
            PoseSpan span;
            span.start = nums[1];
            span.end = nums[2];
            span.impact = nums.size() > 3 ? nums[3] : span.start;
            if (span.start < 0 || span.end <= span.start) continue;
            if (span.impact < span.start || span.impact > span.end) span.impact = span.start;
            out[current].action[static_cast<std::size_t>(mode)][static_cast<std::size_t>(base)] = span;
        } else {
            current = std::atoi(line.c_str());
            want = false;
            for (int i = 0; i < idCount; ++i) {
                if (ids[i] == current) want = true;
            }
        }
    }
    for (const auto& entry : out) {
        if (entry.second.action[0][1].ok()) return true;
    }
    return false;
}

std::string resolveName(const std::string& dir, const std::string& name, std::unordered_map<std::string, std::string>& index, bool& scanned) {
    const std::string direct = dir + "/" + name;
    if (dir.find("/texture/") != std::string::npos) {
        const std::string mapped = resolveTexturePath(direct);
        if (!mapped.empty()) return mapped;
    }
    FILE* file = std::fopen(direct.c_str(), "rb");
    if (file) {
        std::fclose(file);
        return direct;
    }
    if (!scanned) {
        indexDir(dir, index);
        scanned = true;
    }
    const auto it = index.find(lowerCopy(name));
    if (it == index.end()) return {};
    return it->second;
}

void skinVertex(const ModelGeom& geom, const std::vector<float>& mats, std::size_t vertex, float& x, float& y, float& z) {
    x = geom.position[vertex * 3];
    y = geom.position[vertex * 3 + 1];
    z = geom.position[vertex * 3 + 2];
    if (geom.bonePalette.empty() || geom.blendWeight.size() < (vertex + 1) * 4) return;
    float ox = 0.f;
    float oy = 0.f;
    float oz = 0.f;
    float sum = 0.f;
    for (int k = 0; k < 4; ++k) {
        const float weight = geom.blendWeight[vertex * 4 + k];
        if (weight < 1e-5f) continue;
        const unsigned slot = geom.blendIndex[vertex * 4 + k];
        if (slot >= geom.bonePalette.size()) continue;
        const std::uint32_t bone = geom.bonePalette[slot];
        if (bone >= mats.size() / 16) continue;
        float px = 0.f;
        float py = 0.f;
        float pz = 0.f;
        transformPoint(mats.data() + static_cast<std::size_t>(bone) * 16, x, y, z, px, py, pz);
        ox += px * weight;
        oy += py * weight;
        oz += pz * weight;
        sum += weight;
    }
    if (sum < 1e-4f) return;
    x = ox;
    y = oy;
    z = oz;
}

void skinNormal(const ModelGeom& geom, const std::vector<float>& mats, std::size_t vertex, float& x, float& y, float& z) {
    x = y = z = 0.f;
    if (geom.normal.size() < (vertex + 1) * 3) return;
    const float sx = geom.normal[vertex * 3];
    const float sy = geom.normal[vertex * 3 + 1];
    const float sz = geom.normal[vertex * 3 + 2];
    if (geom.bonePalette.empty() || geom.blendWeight.size() < (vertex + 1) * 4 ||
        geom.blendIndex.size() < (vertex + 1) * 4) {
        x = sx;
        y = sy;
        z = sz;
        return;
    }
    float sum = 0.f;
    for (int k = 0; k < 4; ++k) {
        const float weight = geom.blendWeight[vertex * 4 + k];
        if (weight < 1e-5f) continue;
        const unsigned slot = geom.blendIndex[vertex * 4 + k];
        if (slot >= geom.bonePalette.size()) continue;
        const std::uint32_t bone = geom.bonePalette[slot];
        if (bone >= mats.size() / 16) continue;
        float nx = 0.f;
        float ny = 0.f;
        float nz = 0.f;
        transformDir(mats.data() + static_cast<std::size_t>(bone) * 16, sx, sy, sz, nx, ny, nz);
        x += nx * weight;
        y += ny * weight;
        z += nz * weight;
        sum += weight;
    }
    if (sum < 1e-4f) {
        x = sx;
        y = sy;
        z = sz;
    }
}

int sampleFrame(const PoseSpan& pose, float cursor, int frameNum) {
    if (!pose.ok() || frameNum <= 1) return 0;
    int span = pose.end - pose.start;
    if (span < 1) span = 1;
    float local = cursor;
    while (local < 0.f) local += static_cast<float>(span);
    while (local > static_cast<float>(span)) local -= static_cast<float>(span);
    int frame = pose.start + static_cast<int>(local);
    if (frame >= frameNum) frame = frameNum - 1;
    if (frame < 0) frame = 0;
    return frame;
}

bool stepLand(MapFile& map, float& x, float& y) {
    const int ox = static_cast<int>(x);
    const int oy = static_cast<int>(y);
    for (int ring = 0; ring <= 12; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::abs(dx) != ring && std::abs(dy) != ring) continue;
                const float sx = static_cast<float>(ox + dx) + 0.5f;
                const float sy = static_cast<float>(oy + dy) + 0.5f;
                if (!map.blockedAt(sx, sy)) {
                    x = sx;
                    y = sy;
                    return true;
                }
            }
        }
    }
    return false;
}

void pushVertex(std::vector<MeshVertex>& verts, float x, float y, float z, float r, float g, float b, float u, float v,
                float nx = 0.f, float ny = 0.f, float nz = 0.f) {
    MeshVertex vertex{};
    vertex.x = x;
    vertex.y = y;
    vertex.z = z;
    vertex.r = r;
    vertex.g = g;
    vertex.b = b;
    vertex.a = 1.f;
    vertex.u = u;
    vertex.v = v;
    vertex.nx = nx;
    vertex.ny = ny;
    vertex.nz = nz;
    verts.push_back(vertex);
}

// SHOWRSIZE do cliente antigo. A janela do radar tem 128 px e mostra 40 m.
// A camera do minimapa olha para baixo com up (0, -1, 0): +X vai para a
// direita da tela e +Y desce. O norte desenhado no quadro fica em -Y.
constexpr float kShowR = 40.f;
constexpr int kRadar = 128;

struct ScreenBox {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
};

ScreenBox funBarBox(int screenW, int screenH) {
    return {static_cast<float>(screenW) - 215.f, static_cast<float>(screenH) - 30.f, 215.f, 30.f};
}

// Barra de atalhos de texture/ui/MUI/em-uso/skill/new4.tga. A textura so tem F1 a F8: a faixa
// de cima traz F1-F7 e metade do F8, e a ponta do F8 esta na faixa de baixo.
// Desenhada: 256 + 39 = 295 px de largura. As caixas tem 32 px e ficam a
// cada 34 px, a primeira em x=5 (medido na textura).
constexpr float kKeyBarW = 295.f;
constexpr float kKeyBarH = 44.f;
constexpr float kKeyFirst = 5.f;
constexpr float kKeyPitch = 34.f;
constexpr float kKeyCell = 32.f;
constexpr int kKeySlots = 8;

ScreenBox skillBarBox(int screenW, int screenH) {
    // floor: 295 e impar, e meio pixel deixaria a textura borrada.
    return {std::floor((static_cast<float>(screenW) - kKeyBarW) * 0.5f),
            static_cast<float>(screenH) - kKeyBarH - 6.f, kKeyBarW, kKeyBarH};
}

ScreenBox miniBox(int screenW) {
    // frmMinimap 140x155, margem direita 4, topo 15. A altura extra cobre
    // o nome e a coordenada, que o script desenha um pouco abaixo da moldura.
    return {static_cast<float>(screenW) - 144.f, 15.f, 140.f, 172.f};
}

struct FunIcon {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
    float cropX = 0.f;
    float cropY = 0.f;
    int which = -1;
};

void funIcons(FunIcon* out, int& count) {
    // frmMainFun em main.clu. which segue togglePanel: 0 bolsa, 1 status, 2 skill, 3 missao.
    const FunIcon icons[] = {
        {7.f, 4.f, 23.f, 22.f, 0.f, 0.f, 1},
        {31.f, 4.f, 23.f, 22.f, 0.f, 22.f, 0},
        {55.f, 4.f, 23.f, 22.f, 0.f, 44.f, 2},
        {79.f, 4.f, 23.f, 22.f, 0.f, 66.f, 3},
        {103.f, 4.f, 23.f, 22.f, 0.f, 88.f, -1},
        {127.f, 4.f, 23.f, 22.f, 0.f, 110.f, -1},
        {151.f, 4.f, 23.f, 22.f, 0.f, 132.f, -1},
        {175.f, 4.f, 37.f, 22.f, 0.f, 154.f, -1},
    };
    count = 8;
    for (int i = 0; i < count; ++i) out[i] = icons[i];
}

void radarPixel(float playerX, float playerY, float worldX, float worldY, int& px, int& py) {
    const float ppm = static_cast<float>(kRadar) / kShowR;
    px = static_cast<int>(std::floor((worldX - playerX) * ppm + static_cast<float>(kRadar) * 0.5f));
    py = static_cast<int>(std::floor((worldY - playerY) * ppm + static_cast<float>(kRadar) * 0.5f));
}

}  // namespace

struct ActorSkin::Body {
    int id = 0;
    int territory = 0;          // characterinfo.txt chTerritory, DX9
    int seaHeightCm = 0;        // characterinfo.txt sSeaHeight (centimeters)
    bool player = false;
    bool passive = false;
    std::uint64_t spawnKey = 0;
    int respawnSeconds = 0;
    float spawnFacing = 0.f; // graus do CreateCha, independente da matriz visual
    float spawnX = 0.f, spawnY = 0.f; // stable location even while AI moves
    BoneClip bone;
    std::vector<Model> parts;
    int cachedPoseFrame = -1;
    int cachedDrawFrame = -1;
    int cachedDrawPose = -1;
    std::vector<float> cachedPoseMatrices;
    std::vector<std::vector<actor_frame_cache::IndexedVertices>> cachedGeometry;
    // Slot visual correspondente a cada modelo. 0..4 sao partes do corpo;
    // 6/9 sao armas anexadas ao mesmo esqueleto.
    std::vector<int> partSlots;
    std::vector<Model> landParts;
    std::vector<int> landPartSlots;
    PoseSpan idle;
    PoseSpan run;
    PoseSpan attack;
    PoseSpan die;
    PoseSpan seat;
    PoseBank variants;
    int poseMode = 0;
    int activeAction = 0;
    unsigned long long activeActionCycle = 0;
    int leftWeaponBackDummy = -1;
    int rightWeaponBackDummy = -1;
    PoseSpan actionPose(int base) const {
        if (base < 1 || base > kWeaponPoseStride || bone.frameNum <= 1) return {};
        PoseSpan picked = variants.action[static_cast<std::size_t>(poseMode)][static_cast<std::size_t>(base)];
        if (!picked.ok() || picked.start >= bone.frameNum || picked.end <= picked.start)
            picked = variants.action[0][static_cast<std::size_t>(base)];
        if (!picked.ok() || picked.start >= bone.frameNum || picked.end <= picked.start) return {};
        picked.end = std::min(picked.end, bone.frameNum - 1);
        picked.impact = std::clamp(picked.impact, picked.start, picked.end);
        return picked;
    }
    void usePoseMode(int requested) {
        const int mode = requested >= 0 && requested < kWeaponPoseGroups ? requested : 0;
        if (poseMode != mode) { cursor = 0.f; activeAction = 0; }
        poseMode = mode;
        idle = actionPose(1);
        if (!idle.ok()) idle = actionPose(4); // alternate standing pose
        run = actionPose(5);
        if (!run.ok()) run = actionPose(6);
        attack = actionPose(7);
        if (!attack.ok()) attack = actionPose(8);
        if (!attack.ok()) attack = actionPose(9);
        die = actionPose(17);
        seat = actionPose(16);
    }
    float cursor = 8.f;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float yawDeg = 180.f;
    bool moving = false;
    int pose = 0;
    bool hide = false;
    float hp01 = -1.f;
    int shadow = 0;
    float shadowLength = 0.8f;
    float shadowWidth = 0.8f;
};

ActorSkin::ActorSkin() = default;
ActorSkin::~ActorSkin() = default;

bool ActorSkin::selfTest() {
    // Item 255 (Smile 1), pose de bind: y vai de -0.07 a +0.21. A cara sai para +Y.
    // Girar 180 em cima disso mostrou a nuca. O yaw do passo e atan2(dx, dy).
    // O angulo de jogo 180 - esse yaw deixa o +Y da malha no passo.
    float node[16];
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    nodeMatrix(node, 180.f, 10.f, 20.f, 3.f);
    transformPoint(node, 0.f, 1.f, 2.f, x, y, z);
    if (std::fabs(x - 10.f) > 1e-3f || std::fabs(y - 21.f) > 1e-3f || std::fabs(z - 5.f) > 1e-3f) return false;
    nodeMatrix(node, 90.f, 0.f, 0.f, 0.f);
    transformPoint(node, 0.f, 1.f, 2.f, x, y, z);
    if (std::fabs(x - 1.f) > 1e-3f || std::fabs(y) > 1e-3f || std::fabs(z - 2.f) > 1e-3f) return false;
    const float moveYaw = kPi * 0.5f;
    const float yawDeg = 180.f - moveYaw * (180.f / kPi);
    nodeMatrix(node, yawDeg, 0.f, 0.f, 0.f);
    transformPoint(node, 0.f, 1.f, 0.f, x, y, z);
    if (std::fabs(x - std::sin(moveYaw)) > 1e-3f || std::fabs(y - std::cos(moveYaw)) > 1e-3f) return false;
    // 10 m numa janela de 40 m e 128 px sao 32 px. +X e a direita. -Y sobe.
    int px = 0;
    int py = 0;
    radarPixel(1000.f, 2000.f, 1010.f, 2000.f, px, py);
    if (px != 96 || py != 64) {
        std::fprintf(stderr, "minimapa: +X nao vai para a direita (%d,%d)\n", px, py);
        return false;
    }
    radarPixel(1000.f, 2000.f, 1000.f, 1990.f, px, py);
    if (px != 64 || py != 32) {
        std::fprintf(stderr, "minimapa: -Y nao sobe na tela (%d,%d)\n", px, py);
        return false;
    }
    // Exercise the renderer clock as well as the gameplay timers, without GL.
    ActorSkin animated;
    Body fixture;
    fixture.player = true;
    fixture.bone.frameNum = 200;
    fixture.variants.action[0][1] = {0, 10, 0};
    fixture.variants.action[0][7] = {20, 50, 35};
    fixture.variants.action[0][5] = {51, 70, 60};
    fixture.variants.action[0][12] = {80, 104, 89};
    fixture.variants.action[1][7] = {60, 75, 65};
    fixture.usePoseMode(0);
    animated.bodies_.push_back(fixture);
    fixture.player = false;
    animated.bodies_.push_back(fixture);
    MapFile emptyMap;
    animated.setPlayerPose(2);
    animated.setPlayerActionBase(7);
    animated.setPlayerActionOnce(true);
    animated.setPlayerActionPlaybackRate(2.f);
    animated.setPlayerActionCycle(1);
    animated.setPlayer(0.f, 0.f, 0.f, 0.f, false, .1f);
    animated.setMonster(0, 0.f, 0.f, 0.f, false, 2, false, 1.f, 12, 1.f, 1);
    animated.build(emptyMap);
    if (animated.bodies_[0].cachedPoseFrame != 26 || animated.bodies_[1].cachedPoseFrame != 83) return false;
    animated.setPlayer(0.f, 0.f, 0.f, 0.f, false, 1.f);
    animated.build(emptyMap);
    if (animated.bodies_[0].cachedPoseFrame != 50 || animated.bodies_[1].cachedPoseFrame != 104) {
        std::fprintf(stderr, "animacao: golpe do jogador/monstro repetiu em vez de parar no fim\n");
        return false;
    }
    animated.setPlayerActionCycle(2);
    animated.setMonster(0, 0.f, 0.f, 0.f, false, 2, false, 1.f, 12, 1.f, 2);
    animated.setPlayer(0.f, 0.f, 0.f, 0.f, false, 0.f);
    animated.build(emptyMap);
    if (animated.bodies_[0].cachedPoseFrame != 20 || animated.bodies_[1].cachedPoseFrame != 80) return false;
    // SK2.13-B: walking overrides the casting pose immediately, and the
    // interrupted cast does not resume its frozen last frame on stopping.
    animated.setPlayerPose(5);
    animated.setPlayerActionBase(12);
    animated.setPlayerActionOnce(true);
    animated.setPlayerActionPlaybackRate(.25f); // must NOT slow run frames
    animated.setPlayerActionCycle(4);
    animated.setPlayer(0,0,0,0,false,1.f/30.f);
    animated.build(emptyMap);
    if (animated.bodies_[0].pose != 5) return false;
    animated.setPlayer(1,0,0,0,true,1.f/30.f);
    animated.build(emptyMap);
    if (animated.bodies_[0].pose != 1 || animated.bodies_[0].cachedPoseFrame != 52) {
        std::fprintf(stderr, "SK2.13-B: caminhada nao interrompeu a pose de conjuracao\n");
        return false;
    }
    animated.setPlayer(2,0,0,0,true,1.f/30.f);
    animated.build(emptyMap);
    if (animated.bodies_[0].cachedPoseFrame != 53) return false;
    animated.setPlayer(2,0,0,0,false,1.f/30.f);
    animated.build(emptyMap);
    if (animated.bodies_[0].pose != 0) return false; // no casting restart
    animated.setPlayerActionCycle(5);
    animated.setPlayer(2,0,0,0,false,1.f/30.f);
    animated.build(emptyMap);
    if (animated.bodies_[0].pose != 5) return false; // new cast works
    animated.setPlayerActionPlaybackRate(2.f); // restore original test setup
    const auto monsterTiming = animated.monsterActionTiming(0, 12);
    if (std::fabs(monsterTiming.duration - .8f) > .001f ||
        std::fabs(monsterTiming.impact - .3f) > .001f) return false;
    // Illusion Slash has a real action 14 in the sword bank. The unarmed
    // group does not contain it; the generic attack is NOT a substitute.
    if (animated.playerActionDuration(14) != 0.f) return false;
    animated.bodies_[0].variants.action[1][14] = {110, 140, 119};
    animated.bodies_[0].usePoseMode(1);
    if (std::fabs(animated.playerActionDuration(14) - 1.f) > .001f ||
        std::fabs(animated.playerActionWindup(14) - .3f) > .001f) {
        std::fprintf(stderr, "animacao: skill 81 nao escolheu a pose 14 da espada\n");
        return false;
    }
    animated.setPlayerActionBase(14);
    animated.setPlayerActionCycle(3);
    animated.setPlayer(0.f, 0.f, 0.f, 0.f, false, .1f);
    animated.build(emptyMap);
    if (animated.bodies_[0].cachedPoseFrame != 116) {
        std::fprintf(stderr, "animacao: skill 81 desenhou outra acao em vez da 14 (frame %d pose %d)\n", animated.bodies_[0].cachedPoseFrame, animated.bodies_[0].pose);
        return false;
    }
    if (std::fabs(animated.playerActionDuration(7) - .5f) > .001f ||
        std::fabs(animated.playerActionWindup(7) - 5.f / 30.f) > .001f ||
        std::fabs(animated.playerActionDuration(54) - .5f) > .001f) {
        std::fprintf(stderr, "animacao: tempos da arma ou fallback diferem da pose renderizada\n");
        return false;
    }
    // Skills such as Berserk (base action 12) must attach both swords to
    // animated hands even when the safe-area idle policy sheathes them.
    ActorSkin swords;
    Body wielder;
    wielder.player=true;
    wielder.bone.boneNum=1;wielder.bone.frameNum=6;wielder.bone.keyType=1;
    wielder.bone.parent={0xffffffffu};wielder.bone.inv.resize(16);
    identity(wielder.bone.inv.data());
    for(int f=0;f<6;++f) {
        // lwMatrix43, row-vector translation in the last three floats.
        const float m[12]={1,0,0,0,1,0,0,0,1,static_cast<float>(f),0,0};
        wielder.bone.mat43.insert(wielder.bone.mat43.end(),m,m+12);
    }
    for(int id:{6,9,21,22}) {
        BoneClip::Dummy dummy;dummy.id=id;dummy.parent=0;identity(dummy.matrix);
        dummy.matrix[12]=id>=21?10.f:0.f;
        wielder.bone.dummies.push_back(dummy);
    }
    wielder.leftWeaponBackDummy=21;wielder.rightWeaponBackDummy=22;
    wielder.variants.action[0][1]={0,5,0};wielder.variants.action[0][12]={0,5,1};
    wielder.usePoseMode(0);
    Model sword;ModelGeom geom;
    geom.position={0,0,0, .1f,0,0, 0,.1f,0};geom.indices={0,1,2};
    ModelSubset subset;subset.indexCount=3;subset.r=1;subset.g=0;subset.b=0;
    geom.subsets.push_back(subset);sword.geoms.push_back(geom);
    wielder.parts={sword,sword};wielder.partSlots={6,9};
    swords.bodies_.push_back(wielder);swords.setPlayerWeaponsDrawn(false);
    swords.setPlayerPose(5);swords.setPlayerActionBase(12);swords.setPlayerActionOnce(true);
    swords.setPlayerActionCycle(1);swords.setPlayer(0,0,0,0,false,.1f);
    swords.build(emptyMap);
    auto bladesAt=[&](float expected) {
        for(const auto& batch:swords.batches_) if(batch.mode==3&&batch.verts.size()>=6)
            return std::fabs(batch.verts[0].x-expected)<.001f &&
                   std::fabs(batch.verts[3].x-expected)<.001f;
        return false;
    };
    if(!bladesAt(3.f))return false; // both animated hands, not back +10
    const auto poseA=swords.playerDummyWorldMatrices();
    if(!poseA.count(6) || !poseA.count(9) ||
       std::fabs(poseA.at(6)[12]-3.f)>.001f ||
       std::fabs(poseA.at(9)[12]-3.f)>.001f) return false;
    swords.setPlayer(0,0,0,0,false,1.f/30.f);swords.build(emptyMap);
    if(!bladesAt(4.f))return false; // both weapons follow the next .lab frame
    const auto poseB=swords.playerDummyWorldMatrices();
    if(!poseB.count(6) || std::fabs(poseB.at(6)[12]-4.f)>.001f) return false;
    swords.setPlayerPose(0);swords.setPlayer(0,0,0,0,false,0);swords.build(emptyMap);
    if(!bladesAt(10.f))return false; // idle safely returns to the back
    ActorSkin targetActor;
    Body targetBody;
    targetBody.player=false;
    targetBody.bone.boneNum=1;targetBody.bone.frameNum=6;targetBody.bone.keyType=1;
    targetBody.bone.parent={0xffffffffu};targetBody.bone.inv.resize(16);
    identity(targetBody.bone.inv.data());
    for(int f=0;f<6;++f) {
        const float m[12]={1,0,0,0,1,0,0,0,1,static_cast<float>(f),0,0};
        targetBody.bone.mat43.insert(targetBody.bone.mat43.end(),m,m+12);
    }
    BoneClip::Dummy hitDummy;hitDummy.id=55;hitDummy.parent=0;identity(hitDummy.matrix);
    hitDummy.matrix[12]=2.f;targetBody.bone.dummies.push_back(hitDummy);
    targetBody.variants.action[0][1]={0,5,0};targetBody.variants.action[0][12]={0,5,1};
    targetBody.usePoseMode(0);targetActor.bodies_.push_back(targetBody);
    targetActor.setPlayer(0,0,0,0,false,.1f);
    targetActor.setMonster(0,5,7,0,false,2,false,1.f,12,1.f,1);
    targetActor.build(emptyMap);
    const auto targetPose=targetActor.monsterDummyWorldMatrices(0);
    if(!targetPose.count(55) || std::fabs(targetPose.at(55)[12]-10.f)>.001f ||
       std::fabs(targetPose.at(55)[13]-7.f)>.001f ||
       std::fabs(targetPose.at(55)[14])>.001f) return false;
    ActorSkin hud;
    const int stateX = 1280 - 215 + 7 + 4;
    const int stateY = 720 - 30 + 4 + 4;
    if (hud.shortcutHit(stateX, stateY, 1280, 720) != 1 || !hud.uiBlocks(stateX, stateY, 1280, 720)) {
        std::fprintf(stderr, "atalho: status\n");
        return false;
    }
    const int bagX = 1280 - 215 + 31 + 4;
    if (hud.shortcutHit(bagX, stateY, 1280, 720) != 0) {
        std::fprintf(stderr, "atalho: bolsa\n");
        return false;
    }
    const int skillX = 1280 - 215 + 55 + 4;
    if (hud.shortcutHit(skillX, stateY, 1280, 720) != 2) {
        std::fprintf(stderr, "atalho: skill\n");
        return false;
    }
    const int questX = 1280 - 215 + 79 + 4;
    if (hud.shortcutHit(questX, stateY, 1280, 720) != 3 || !hud.uiBlocks(questX, stateY, 1280, 720)) {
        std::fprintf(stderr, "atalho: missao\n");
        return false;
    }
    // Barra F1-F8: o centro de cada caixa desenhada tem que acertar o slot,
    // e a area de clique nao pode passar do que aparece na tela.
    const ScreenBox keys = skillBarBox(1280, 720);
    for (int slot = 0; slot < kKeySlots; ++slot) {
        const int cx = static_cast<int>(keys.x + kKeyFirst + static_cast<float>(slot) * kKeyPitch + kKeyCell * 0.5f);
        const int cy = static_cast<int>(keys.y) + 22;
        if (hud.shortcutHit(cx, cy, 1280, 720) != 10 + slot) {
            std::fprintf(stderr, "atalho: F%d nao acertou a caixa desenhada\n", slot + 1);
            return false;
        }
    }
    const int pastX = static_cast<int>(keys.x + keys.w) + 6;
    const int pastY = static_cast<int>(keys.y) + 22;
    if (hud.shortcutHit(pastX, pastY, 1280, 720) != -1 || hud.uiBlocks(pastX, pastY, 1280, 720)) {
        std::fprintf(stderr, "atalho: a barra de skills passa do que foi desenhado\n");
        return false;
    }
    const int mapX = 1280 - 144 + 20;
    const int mapY = 15 + 20;
    if (!hud.uiBlocks(mapX, mapY, 1280, 720) || hud.shortcutHit(mapX, mapY, 1280, 720) != -1) {
        std::fprintf(stderr, "minimapa: o clique nao pode andar\n");
        return false;
    }
    if (hud.uiBlocks(640, 360, 1280, 720)) {
        std::fprintf(stderr, "atalho: o centro da tela ficou bloqueado\n");
        return false;
    }
    hud.togglePanel(0);
    // Inventary_my em (214, 64): colar no centro do pescoço (slot 5) e da primeira célula.
    if (hud.invDropHit(248, 108, 1280, 720) != 105 || hud.invDropHit(254, 288, 1280, 720) != 250) {
        std::fprintf(stderr, "inventario: destinos de arraste nao correspondem ao desenho\n");
        return false;
    }
    // 04-D: os hits consultam somente o modelo da InventoryUI.
    hud.setBagView({{0, 42, 1, 5, true, {}, "Sword"},
                    {1, 77, 2, -1, false, {}, "Potion"}}, {{5, 42}}, {}, 123);
    if (hud.invHit(214 + 24, 64 + 32, 1280, 720) != 105 ||
        hud.invHit(214 + 27, 64 + 213, 1280, 720) != 201 ||
        !hud.bagWheel(214 + 25, 64 + 213, 1280, 720)) {
        std::fprintf(stderr, "04-D: equipamento/bolsa nao responderam ao hit-test\n");
        return false;
    }
    hud.setItemDrag(201, 640, 360, true);
    if (!hud.ui_.inventory.itemDragActive() || !hud.ui_.inventory.draggedRow() ||
        hud.ui_.inventory.draggedRow()->id != 77) {
        std::fprintf(stderr, "04-D: arraste perdeu o item de origem\n");
        return false;
    }
    hud.togglePanel(4);
    if (hud.invDropHit(450, 120, 1280, 720) != 350) {
        std::fprintf(stderr, "inventario: bolsa temporaria nao aceita item arrastado\n");
        return false;
    }
    hud.togglePanel(4);
    hud.closeInv();
    if (hud.ui_.inventory.itemDragActive() || hud.invHit(214 + 27, 64 + 213, 1280, 720) != -1) {
        std::fprintf(stderr, "04-D: fechar a bolsa nao limpou o arraste\n");
        return false;
    }
    hud.setSkillView({{41, "Skill 1", {}, 3, true, false},
                      {42, "Skill 2", {}, 3, true, false},
                      {43, "Skill 3", {}, 3, true, false},
                      {44, "Skill 4", {}, 3, true, false},
                      {45, "Skill 5", {}, 3, true, false},
                      {46, "Skill 6", {}, 3, true, false}}, 2);
    hud.togglePanel(2);
    if (hud.skillHit(1280 - 206 + 20, 64 + 70, 1280) != 41 ||
        !hud.skillWheel(1280 - 206 + 20, 64 + 70, 1280, 1) ||
        hud.skillHit(1280 - 206 + 20, 64 + 70, 1280) != 46) {
        std::fprintf(stderr, "04-D: habilidade/scroll nao mantiveram os indices\n");
        return false;
    }
    hud.setSkillView({{47, "Unica", {}, 3, true, false}}, 1);
    if (hud.skillHit(1280 - 206 + 20, 64 + 70, 1280) != 47 ||
        hud.ui_.skills.page() != 0 || hud.ui_.skills.points() != 1) {
        std::fprintf(stderr, "04-D: reduzir a lista de habilidades nao ajustou a pagina\n");
        return false;
    }
    hud.togglePanel(2);
    if (hud.skillHit(1280 - 206 + 20, 64 + 70, 1280) != -1) {
        std::fprintf(stderr, "04-D: janela de habilidade fechada ainda recebe cliques\n");
        return false;
    }
    hud.setNpcWindow(true, "Goldie", "Teste de conversa persistente.");
    if (!hud.uiBlocks(640, 600, 1280, 720)) {
        std::fprintf(stderr, "dialogo: janela nao bloqueia clique no mundo\n");
        return false;
    }
    hud.setNpcWindow(false);
    hud.setShopWindow(true, "Goldie", {"27"}, 0, 1, 27);
    if (!hud.uiBlocks(965, 142, 1280, 720)) {
        std::fprintf(stderr, "loja: janela nao bloqueia clique no mundo\n");
        return false;
    }
    hud.setShopWindow(false);
    hud.setCharacterRoster({"Lance — Lance — Argent City", "Carsise — Carsise — Shaitan City",
                            "Phyllis — Phyllis — Icicle Castle", "Ami — Ami — Argent City"}, 0);
    hud.setCharacterWindow(true);
    if (hud.characterHit(320, 290, 1280, 720) != 1 || hud.characterHit(830, 290, 1280, 720) != 4 ||
        hud.characterHit(400, 500, 1280, 720) != -1 || hud.characterHit(892, 500, 1280, 720) != -4 ||
        !hud.uiBlocks(640, 360, 1280, 720)) {
        std::fprintf(stderr, "personagem: selecao dos quatro perfis nao respondeu\n");
        return false;
    }
    hud.beginCharacterCreation();
    if (hud.characterHit(320, 290, 1280, 720) != 101) {
        std::fprintf(stderr, "personagem: etapa de escolha de raca nao respondeu\n");
        return false;
    }
    hud.chooseCharacterRace(3);
    hud.editCharacterCreateName("Eliel 2026", false);
    hud.editCharacterCreateName("", true);
    if (hud.characterCreateRace() != 3 || hud.characterCreateName() != "Eliel 202" ||
        hud.characterHit(400, 290, 1280, 720) != 201) {
        std::fprintf(stderr, "personagem: etapa de escolha do nascimento nao respondeu\n");
        return false;
    }
    hud.cancelCharacterCreation();
    hud.beginCharacterDeletion();
    if (hud.characterHit(500, 500, 1280, 720) != -5 || hud.characterHit(700, 500, 1280, 720) != -6) {
        std::fprintf(stderr, "personagem: confirmacao de exclusao nao respondeu\n");
        return false;
    }
    hud.cancelCharacterCreation();
    hud.setCharacterWindow(false);
    hud.toggleSystemWindow();
    if (!hud.systemWindow() || hud.systemHit(600, 275, 1280, 720) != 1 ||
        hud.systemHit(600, 430, 1280, 720) != 6 || !hud.uiBlocks(640, 360, 1280, 720)) {
        std::fprintf(stderr, "sistema: menu, configuracao ou sair nao respondeu\n");
        return false;
    }
    hud.closeSystemWindow();
    hud.toggleBoatWindow();
    if (hud.boatHit(500, 260, 1280, 720) != 1 || hud.boatHit(680, 260, 1280, 720) != 10 ||
        hud.boatHit(812, 180, 1280, 720) != 0 || !hud.uiBlocks(640, 360, 1280, 720)) {
        std::fprintf(stderr, "barco: estaleiro e selecao dos cascos nao responderam\n");
        return false;
    }
    hud.closeBoatWindow();
    hud.togglePanel(1);
    if (hud.stateHit(8 + 180, 64 + 6) != 1) {
        std::fprintf(stderr, "status: o fechar nao acertou\n");
        return false;
    }
    if (hud.stateHit(8 + 66, 64 + 180) != 2 || hud.stateHit(8 + 172, 64 + 196) != 5) {
        std::fprintf(stderr, "status: o + nao acertou\n");
        return false;
    }
    if (hud.stateHit(8 + 160, 64 + 48) != 8 || hud.stateHit(8 + 160, 64 + 64) != 10) {
        std::fprintf(stderr, "status: a classe nao acertou\n");
        return false;
    }
    hud.beginStateDrag(28, 72);
    hud.dragState(120, 90, true, 1280, 720);
    if (hud.stateHit(8 + 180, 64 + 6) != -1 || hud.stateHit(120 + 160, 90 - 8 + 6) != 1) {
        std::fprintf(stderr, "status: arrastar nao moveu a janela\n");
        return false;
    }
    hud.closeState();
    if (hud.stateHit(120, 90) != -1) {
        std::fprintf(stderr, "status: fechar ainda bloqueia o chao\n");
        return false;
    }
    return true;
}

bool ActorSkin::open(const std::string& clientRoot, const std::string& mapName, const std::vector<MonsterSlot>& monsters,
                     int playerId) {
    root_ = clientRoot;
    mapName_ = mapName;
    playerId_ = playerId >= 1 && playerId <= 4 ? playerId : 1;
    roster_ = monsters;
    // Finishing the previous background task is only needed for a full
    // map/character transition; regular sector refreshes never wait here.
    populationPreload_ = {};
    populationPreloadIds_.clear();
    populationPreloadFailures_.clear();
    lastPrefetchStats_ = {};
    bodies_.clear();
    cachedPopulationTypes_.clear();
    lastPopulationUpdate_ = {};
    drives_.clear();
    placed_ = false;
    equipLines_.clear();
    wornVisual_.clear();
    boatMode_ = false;
    playerFramework_ = 0;
    for (int& skin : playerDefaultSkin_) skin = 0;
    ui_.content.statLines_.clear();
    ui_.skills.clearLines();
    report_ = ActorReport{};
    font_.open();
    // Nenhum monstro proximo e valido: nao fabricar monstros iniciais em outros mapas.
    const std::vector<MonsterSlot>& roster = monsters;
    std::vector<int> ids;
    ids.reserve(roster.size() + 1);
    ids.push_back(playerId_);
    for (const MonsterSlot& monster : roster) ids.push_back(monster.id);
    std::unordered_map<int, PoseBank> poses;
    std::unordered_map<int, std::vector<std::string>> characters;
    std::unordered_map<int, std::vector<std::string>> items;
    if (!loadTable(clientRoot + "/scripts/table/characterinfo.txt", characters)) return false;
    loadTable(clientRoot + "/scripts/table/iteminfo.txt", items);
    // The historical pose table is keyed by Characterinfo.Action ID (column 20),
    // not by Characterinfo.ID. A monster often shares a pose bank with dozens
    // of different character IDs. Include both keys to support incomplete mods.
    std::vector<int> poseIds = ids;
    for (int id : ids) {
        const auto row = characters.find(id);
        if (row != characters.end()) poseIds.push_back(characterPoseSetId(id, row->second));
    }
    if (!loadPoses(clientRoot + "/scripts/txt/CharacterAction.tx", poseIds.data(),
                   static_cast<int>(poseIds.size()), poses)) return false;

    std::unordered_map<std::string, std::string> modelIndex;
    std::unordered_map<std::string, std::string> texIndex;
    bool modelScanned = false;
    bool texScanned = false;
    const std::string modelDir = clientRoot + "/model/character";
    const std::string texDir = clientRoot + "/texture/character";
    const std::string animDir = clientRoot + "/animation";
    std::string historicalAnimDir;
    std::unordered_map<std::string, std::string> historicalAnimIndex;
    bool historicalAnimScanned = false;

    std::size_t actorIndex = 0;
    for (int id : ids) {
        const std::size_t thisActor = actorIndex++;
        const auto rowIt = characters.find(id);
        if (rowIt == characters.end()) {
            ++report_.partFail;
            continue;
        }
        const std::vector<std::string>& cols = rowIt->second;
        ChaRow row;
        row.id = id;
        row.modal = field(cols, 3);
        row.model = field(cols, 5);
        row.suit = field(cols, 6);
        for (int i = 0; i < 5; ++i) row.skin[i] = field(cols, 8 + i);
        row.shadow = field(cols, 19);
        if (cols.size() > 30) {
            row.length = std::max(0.35f, static_cast<float>(std::atof(cols[29].c_str())));
            row.width = std::max(0.35f, static_cast<float>(std::atof(cols[30].c_str())));
        }
        if (thisActor == 0) {
            playerFramework_ = row.model;
            for (int i = 0; i < 5; ++i) playerDefaultSkin_[i] = row.skin[i];
        }
        char labName[32];
        std::snprintf(labName, sizeof labName, "%04d.lab", row.model);
        Body body;
        body.id = id;
        // Same columns already used by our historical ship renderer. Do not
        // infer water status from terrain color/model name or negative ground.
        body.territory = field(cols, 26);
        body.seaHeightCm = field(cols, 27);
        body.shadow = row.shadow;
        body.shadowLength = row.length;
        body.shadowWidth = row.width;
        body.player = thisActor == 0;
        if (!body.player && thisActor - 1 < roster.size()) {
            body.x = roster[thisActor - 1].x;
            body.y = roster[thisActor - 1].y;
            body.passive = roster[thisActor - 1].passive;
            body.spawnKey = roster[thisActor - 1].spawnKey;
            body.respawnSeconds = roster[thisActor - 1].respawnSeconds;
            body.spawnFacing = roster[thisActor - 1].yaw;
            body.spawnX = roster[thisActor - 1].x;
            body.spawnY = roster[thisActor - 1].y;
            // npc.txt guarda graus no mesmo eixo de CSceneObj::_UpdateYaw.
            // 180 - angulo virava de costas quem foi gravado em 180.
            if (body.passive) body.yawDeg = roster[thisActor - 1].yaw;
        }
        const int requestedActionId = characterPoseSetId(id, cols);
        // Prefer the Action ID bank; if a mod has a broken reference, fall
        // back to the character-ID bank instead of using an arbitrary frame 0.
        auto findValid = [&](int animationId) -> const PoseBank* {
            const auto it = poses.find(animationId);
            if (it == poses.end()) return nullptr;
            const auto& actions = it->second.action[0];
            if (!actions[1].ok() && !actions[4].ok() && !actions[5].ok()) return nullptr;
            return &it->second;
        };
        const PoseBank* bank = findValid(requestedActionId);
        if (!bank && requestedActionId != id) {
            bank = findValid(id);
            if (!body.player) ++report_.monsterPoseFallbacks;
        }
        if (bank) {
            body.variants = *bank;
            if (!body.player) {
                ++report_.monsterPoseReady;
                if (requestedActionId != id && bank == findValid(requestedActionId))
                    ++report_.monsterPoseRemaps;
            }
        } else if (!body.player) {
            ++report_.monsterPoseMissing;
        }
        std::string labPath = resolveName(animDir, labName, modelIndex, modelScanned);
        if (labPath.empty() && !historicalAnimDir.empty())
            labPath = resolveName(historicalAnimDir, labName, historicalAnimIndex, historicalAnimScanned);
        if (labPath.empty() || !body.bone.load(labPath)) {
            ++report_.partFail;
            continue;
        }
        body.usePoseMode(0);
        report_.bones += body.bone.boneNum;

        for (int part = 0; part < 5; ++part) {
            if (row.skin[part] == 0) continue;
            std::string fileName;
            if (row.modal == 1) {
                const auto item = items.find(row.skin[part]);
                const int column = 4 + row.model;
                if (item == items.end() || column >= static_cast<int>(item->second.size())) continue;
                fileName = item->second[static_cast<std::size_t>(column)];
                if (fileName.empty() || fileName == "0") continue;
                fileName += ".lgo";
            } else {
                char name[32];
                const int fileId = row.model * 1000000 + row.suit * 10000 + part;
                std::snprintf(name, sizeof name, "%010d.lgo", fileId);
                fileName = name;
            }
            const std::string path = resolveName(modelDir, fileName, modelIndex, modelScanned);
            Model model;
            if (path.empty() || !model.loadGeom(path)) {
                ++report_.partFail;
                continue;
            }
            for (ModelGeom& geom : model.geoms) {
                for (ModelSubset& subset : geom.subsets) {
                    if (subset.texture.empty()) continue;
                    std::string tex = resolveName(texDir, subset.texture, texIndex, texScanned);
                    if (tex.empty()) {
                        std::string dds = subset.texture;
                        const std::size_t dot = dds.find_last_of('.');
                        if (dot != std::string::npos) dds.replace(dot, std::string::npos, ".dds");
                        tex = resolveName(texDir, dds, texIndex, texScanned);
                    }
                    if (tex.empty() && std::find(report_.missingTextures.begin(), report_.missingTextures.end(), subset.texture) ==
                                           report_.missingTextures.end())
                        report_.missingTextures.push_back(subset.texture);
                    subset.texture = tex;
                }
            }
            body.parts.push_back(std::move(model));
            body.partSlots.push_back(part);
            ++report_.parts;
        }
        if (!body.parts.empty()) bodies_.push_back(std::move(body));
    }
    report_.actors = static_cast<int>(bodies_.size());

    const auto cha = characters.find(playerId_);
    if (cha != characters.end()) {
        const std::vector<std::string>& cols = cha->second;
        auto col = [&](int index) -> std::string {
            if (index < 0 || index >= static_cast<int>(cols.size())) return {};
            return cols[static_cast<std::size_t>(index)];
        };
        // Ordem fixa, desenhada nos campos do State1 (preperty.clu).
        static const char* names[] = {"", "Lance", "Carsise", "Phyllis", "Ami"};
        ui_.content.statLines_.push_back(names[playerId_]);
        ui_.content.statLines_.push_back(col(60));
        ui_.content.statLines_.push_back(col(61));
        ui_.content.statLines_.push_back(col(63));
        ui_.content.statLines_.push_back(col(80));
        ui_.content.statLines_.push_back(col(81));
        ui_.content.statLines_.push_back(col(82));
        ui_.content.statLines_.push_back(col(83));
        ui_.content.statLines_.push_back(col(84));
        ui_.content.statLines_.push_back(col(85));
        while (ui_.content.statLines_.size() < 20) ui_.content.statLines_.push_back({});
        for (int part = 0; part < 5; ++part) {
            const int id = field(cols, 8 + part);
            if (id == 0) continue;
            const auto item = items.find(id);
            if (item == items.end() || item->second.size() < 2 || item->second[1].empty()) equipLines_.push_back(std::to_string(id));
            else equipLines_.push_back(item->second[1]);
        }
        std::unordered_map<int, std::string> skillName;
        std::vector<std::uint8_t> skillFile;
        if (readFile(clientRoot + "/scripts/table/magicsingleinfo.txt", skillFile, 8 * 1024 * 1024)) {
            std::size_t at = 0;
            while (at < skillFile.size()) {
                std::size_t end = at;
                while (end < skillFile.size() && skillFile[end] != '\n') ++end;
                std::string line(reinterpret_cast<const char*>(skillFile.data() + at), end - at);
                at = end + 1;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.size() < 2 || line[0] == '/') continue;
                const std::vector<std::string> skillCols = tabs(line);
                const int id = std::atoi(skillCols.empty() ? "" : skillCols[0].c_str());
                if (id > 0 && skillCols.size() > 1) skillName[id] = skillCols[1];
            }
        }
        const std::string list = col(44);
        std::string token;
        for (std::size_t i = 0; i <= list.size(); ++i) {
            if (i == list.size() || list[i] == ',') {
                if (!token.empty()) {
                    const int id = std::atoi(token.c_str());
                    std::string name = skillName.count(id) ? skillName[id] : std::string{};
                    std::string ascii;
                    for (unsigned char ch : name) {
                        if (ch >= 32 && ch < 127) ascii.push_back(static_cast<char>(ch));
                        else if (!ascii.empty() && ascii.back() != ' ') ascii.push_back(' ');
                    }
                    while (!ascii.empty() && ascii.back() == ' ') ascii.pop_back();
                    if (ascii.size() < 3) ui_.skills.appendLine("Skill " + std::to_string(id));
                    else ui_.skills.appendLine(ascii);
                }
                token.clear();
            } else if (list[i] != ' ') {
                token.push_back(list[i]);
            }
        }
    }

    for (Body& body : bodies_) {
        if (!body.player || body.parts.empty() || body.bone.boneNum == 0) continue;
        const int frameA = sampleFrame(body.idle, 0.f, body.bone.frameNum);
        const int frameB = sampleFrame(body.idle, 12.f, body.bone.frameNum);
        std::vector<float> matA;
        std::vector<float> matB;
        body.bone.matrices(frameA, matA);
        body.bone.matrices(frameB, matB);
        // A primeira peca e o cabelo. O corpo inteiro e que tem pe perto do chao.
        float minZ = 1e9f;
        float maxZ = -1e9f;
        float moved = 0.f;
        for (const Model& model : body.parts) {
            for (const ModelGeom& geom : model.geoms) {
                const std::size_t count = geom.position.size() / 3;
                for (std::size_t v = 0; v < count; ++v) {
                    float ax = 0.f;
                    float ay = 0.f;
                    float az = 0.f;
                    float bx = 0.f;
                    float by = 0.f;
                    float bz = 0.f;
                    skinVertex(geom, matA, v, ax, ay, az);
                    skinVertex(geom, matB, v, bx, by, bz);
                    const float delta = std::fabs(ax - bx) + std::fabs(ay - by) + std::fabs(az - bz);
                    if (delta > moved) moved = delta;
                    if (az < minZ) minZ = az;
                    if (az > maxZ) maxZ = az;
                }
            }
        }
        report_.animMoves = moved > 0.002f;
        report_.bodyMinZ = minZ;
        report_.bodyMaxZ = maxZ;
        break;
    }
    openMinimap();
    return report_.parts > 0 && report_.bones > 0;
}

std::unordered_map<int, std::array<float, 16>> ActorSkin::playerDummyWorldMatrices() const {
    std::unordered_map<int, std::array<float, 16>> result;
    if (boatMode_) return result;
    for (const Body& body : bodies_) {
        if (!body.player || body.cachedPoseMatrices.size() !=
                static_cast<std::size_t>(body.bone.boneNum) * 16) continue;
        float node[16];
        nodeMatrix(node, body.yawDeg, body.x, body.y, body.z);
        for (const auto& dummy : body.bone.dummies) {
            if (dummy.id > 255) continue;
            float boneDummy[16];
            if (!body.bone.dummyMatrixFromPose(body.cachedPoseMatrices,
                    static_cast<int>(dummy.id), boneDummy)) continue;
            std::array<float, 16> world{};
            multiply(boneDummy, node, world.data());
            if (std::all_of(world.begin(), world.end(), [](float value) {
                    return std::isfinite(value);
                })) result[static_cast<int>(dummy.id)] = world;
        }
        break;
    }
    return result;
}

std::unordered_map<int, std::array<float, 16>> ActorSkin::monsterDummyWorldMatrices(int index) const {
    std::unordered_map<int, std::array<float, 16>> result;
    if (index < 0 || boatMode_) return result;
    int seen = 0;
    for (const Body& body : bodies_) {
        if (body.player || body.passive) continue;
        if (seen++ != index) continue;
        if (body.cachedDrawFrame < 0 || body.bone.boneNum <= 0) return result;
        std::vector<float> sampledPose;
        const std::vector<float>* pose = &body.cachedPoseMatrices;
        if (body.cachedPoseFrame != body.cachedDrawFrame ||
            body.cachedPoseMatrices.size() != static_cast<std::size_t>(body.bone.boneNum) * 16) {
            body.bone.matrices(body.cachedDrawFrame, sampledPose);
            pose = &sampledPose;
        }
        if (pose->size() != static_cast<std::size_t>(body.bone.boneNum) * 16) return result;
        float node[16];
        nodeMatrix(node, body.yawDeg, body.x, body.y, body.z);
        for (const auto& dummy : body.bone.dummies) {
            if (dummy.id > 255) continue;
            float boneDummy[16];
            if (!body.bone.dummyMatrixFromPose(*pose, static_cast<int>(dummy.id), boneDummy)) continue;
            std::array<float, 16> world{};
            multiply(boneDummy, node, world.data());
            if (std::all_of(world.begin(), world.end(), [](float value) {
                    return std::isfinite(value);
                })) result[static_cast<int>(dummy.id)] = world;
        }
        return result;
    }
    return result;
}

std::array<float, 3> ActorSkin::monsterWorldRoot(int index) const {
    int seen = 0;
    for (const Body& body : bodies_) {
        if (body.player || body.passive) continue;
        if (seen++ == index) return {body.x, body.z, body.y};
    }
    return {};
}

std::unordered_map<int,std::array<float,16>> ActorSkin::playerItemDummyWorldMatrices() const {
    return actorItemDummyWorldMatrices(-1);
}

std::unordered_map<int,std::array<float,16>> ActorSkin::actorItemDummyWorldMatrices(int index) const {
    std::unordered_map<int,std::array<float,16>> result;
    if(boatMode_)return result;
    int seen=0;
    for(const Body& body:bodies_){
        if(index==-1 ? !body.player : (body.player || body.passive || seen++!=index))continue;
        if(body.cachedDrawPose<0 ||
           body.cachedDrawFrame<0)continue;
        std::vector<float> sampled;
        const auto* pose=&body.cachedPoseMatrices;
        if(body.cachedPoseFrame!=body.cachedDrawFrame || pose->size()!=static_cast<std::size_t>(body.bone.boneNum)*16) {
            body.bone.matrices(body.cachedDrawFrame,sampled);pose=&sampled;
        }
        if(pose->size()!=static_cast<std::size_t>(body.bone.boneNum)*16)continue;
        // Right weapon is the historical primary item for ordinary item cast
        // events. A left-only weapon is accepted when no right weapon exists.
        for(int requestedSlot:{9,6}){
            for(std::size_t i=0;i<body.parts.size()&&i<body.partSlots.size();++i){
                if(body.partSlots[i]!=requestedSlot)continue;
                const Model& model=body.parts[i];
                if(!model.hasItemDummy || model.itemDummies.empty())continue;
                const int back=requestedSlot==9?body.rightWeaponBackDummy:body.leftWeaponBackDummy;
                const int attachId=weaponAttachmentDummy(requestedSlot,back,
                                                           body.player?playerWeaponsDrawn_:true,body.cachedDrawPose);
                float attachment[16];
                if(!body.bone.dummyMatrixFromPose(*pose,attachId,attachment))continue;
                float inverseGrip[16];
                if(!inverseAffine(model.itemDummy,inverseGrip))continue;
                float itemToBone[16],node[16],itemToWorld[16];
                multiply(inverseGrip,attachment,itemToBone);
                nodeMatrix(node,body.yawDeg,body.x,body.y,body.z);
                multiply(itemToBone,node,itemToWorld);
                for(const auto& kv:model.itemDummies){
                    std::array<float,16> world{};
                    multiply(kv.second.data(),itemToWorld,world.data());
                    if(std::all_of(world.begin(),world.end(),
                                   [](float x){return std::isfinite(x);}))result[kv.first]=world;
                }
                if(!result.empty())return result;
            }
        }
        break;
    }
    return result;
}

void ActorSkin::setPlayer(float x, float y, float yawRad, float height, bool moving, float dt) {
    playerX_ = x;
    playerY_ = y;
    playerZ_ = height;
    playerYaw_ = yawRad;
    playerMoving_ = moving;
    playerDt_ = dt;
}

void ActorSkin::setPlayerPose(int pose) { playerPose_ = pose; }
void ActorSkin::setPlayerActionOnce(bool once) { playerActionOnce_ = once; }
void ActorSkin::setPlayerActionPlaybackRate(float rate) {
    playerActionPlaybackRate_ = std::clamp(rate, 0.05f, 10.f);
}
void ActorSkin::setPlayerActionCycle(unsigned long long cycle) {
    // Enable with TOP_TRACE_POSES=1 ./build/top_gl. Print only when a new
    // action starts; this is not a per-frame logging path.
    if (cycle != playerActionCycle_ && playerActionBase_ == 14 && std::getenv("TOP_TRACE_POSES")) {
        for (const Body& body : bodies_) {
            if (!body.player) continue;
            const int realId = weaponPoseAction(14, body.poseMode);
            const PoseSpan direct = body.variants.action[static_cast<std::size_t>(body.poseMode)][14];
            const PoseSpan chosen = body.actionPose(14);
            const bool available = chosen.ok() && chosen.start < body.bone.frameNum;
            std::fprintf(stderr,
                "[pose] Illusion Slash skill=81 action=14 real=%d character=%d mode=%d "
                "frames=%d..%d key=%d labFrames=%d %s%s\n",
                realId, body.id, body.poseMode,
                chosen.start, chosen.end, chosen.impact, body.bone.frameNum,
                available ? "OK" : "MISSING (no attack-7 substitution)",
                available && !direct.ok() ? " [using unarmed variant]" : "");
            break;
        }
    }
    playerActionCycle_ = cycle;
}
void ActorSkin::setPlayerWeaponsDrawn(bool drawn) {
    const bool changed = playerWeaponsDrawn_ != drawn;
    playerWeaponsDrawn_ = drawn;
    for (Body& body : bodies_) {
        if (!body.player) continue;
        body.idle = body.actionPose(drawn ? 4 : 1);
        if (!body.idle.ok()) body.idle = body.actionPose(drawn ? 1 : 4);
        body.run = body.actionPose(drawn ? 6 : 5);
        if (!body.run.ok()) body.run = body.actionPose(drawn ? 5 : 6);
        if (changed) {
            body.cursor = 0.f;
            body.pose = -1;
        }
    }
}
void ActorSkin::setPlayerActionBase(int action) {
    playerActionBase_ = action >= 1 && action <= kWeaponPoseStride ? action : 7;
    report_.playerActionBase = playerActionBase_;
}

float ActorSkin::playerActionDuration(int actionBase) const {
    for (const Body& body : bodies_) {
        if (!body.player) continue;
        PoseSpan pose = body.actionPose(actionBase);
        if (!pose.ok() && actionBase != 14) pose = body.attack;
        return pose.ok() ? static_cast<float>(pose.end - pose.start) / 30.f : 0.f;
    }
    return 0.f;
}

float ActorSkin::playerActionWindup(int actionBase) const {
    for (const Body& body : bodies_) {
        if (!body.player) continue;
        PoseSpan pose = body.actionPose(actionBase);
        if (!pose.ok() && actionBase != 14) pose = body.attack;
        return pose.ok() ? static_cast<float>(std::max(0, pose.impact - pose.start)) / 30.f : 0.f;
    }
    return 0.f;
}

void ActorSkin::ensurePlaced(MapFile& map) {
    if (placed_) return;
    const float spot[][2] = {{3.f, 1.5f}, {-3.2f, 2.f}, {1.2f, 4.f}, {-2.4f, -3.f}, {4.f, -1.6f}};
    int spotIndex = 0;
    for (Body& body : bodies_) {
        if (body.player) continue;
        if (body.x == 0.f && body.y == 0.f) {
            body.x = playerX_ + spot[spotIndex % 5][0] + static_cast<float>(spotIndex / 5) * 1.5f;
            body.y = playerY_ + spot[spotIndex % 5][1] - static_cast<float>(spotIndex / 5) * 1.5f;
        }
        stepLand(map, body.x, body.y);
        ++spotIndex;
    }
    placed_ = true;
}

bool ActorSkin::hasPopulationType(int id) const {
    if (cachedPopulationTypes_.count(id)) return true;
    for (const Body& body : bodies_)
        if (!body.player && body.id == id) return true;
    return false;
}

bool ActorSkin::pollPopulationPrefetch() {
    if (!populationPreload_.valid()) return false;
    const auto state = populationPreload_.wait_for(std::chrono::seconds(0));
    if (state != std::future_status::ready && state != std::future_status::deferred) return false;
    lastPrefetchStats_ = {};
    lastPrefetchStats_.requested = static_cast<int>(populationPreloadIds_.size());
    lastPrefetchStats_.elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - populationPreloadStart_).count();
    std::unique_ptr<ActorSkin> loaded;
    try { loaded = populationPreload_.get(); }
    catch (...) { /* Bad assets/failed I/O must not crash the renderer. */ }
    std::unordered_set<int> completed;
    if (loaded) {
        constexpr std::size_t kLimit = 48; // Identical to PERF-01 cache cap.
        for (Body& body : loaded->bodies_) {
            if (body.player || !body.id || completed.count(body.id)) continue;
            completed.insert(body.id);
            if (hasPopulationType(body.id)) continue;
            if (cachedPopulationTypes_.size() >= kLimit) continue;
            const int id = body.id;
            body.cachedPoseMatrices.clear();
            body.cachedGeometry.clear();
            cachedPopulationTypes_.emplace(id, std::make_shared<Body>(std::move(body)));
            ++lastPrefetchStats_.loaded;
        }
    }
    for (int id : populationPreloadIds_) {
        if (completed.count(id) == 0 && !hasPopulationType(id)) {
            populationPreloadFailures_.insert(id);
            ++lastPrefetchStats_.failed;
        }
    }
    populationPreloadIds_.clear();
    return true;
}

int ActorSkin::beginPopulationPrefetch(const std::vector<MonsterSlot>& candidates, int maxTypes) {
#ifdef __EMSCRIPTEN__
    // Spread deferred model parsing over frames; retain the current roster
    // until ensurePopulationPrefetched has finished all required species.
    maxTypes = std::min(maxTypes, 1);
#endif
    if (root_.empty() || bodies_.empty() || maxTypes <= 0 || populationPreload_.valid()) return 0;
    constexpr std::size_t kLimit = 48;
    if (cachedPopulationTypes_.size() >= kLimit) return 0;
    std::unordered_set<int> chosen;
    std::vector<MonsterSlot> subset;
    const auto available = kLimit - cachedPopulationTypes_.size();
    const auto capacity = std::min<std::size_t>(available, static_cast<std::size_t>(maxTypes));
    for (const MonsterSlot& slot : candidates) {
        if (slot.id <= 0 || hasPopulationType(slot.id) || populationPreloadFailures_.count(slot.id) ||
            !chosen.insert(slot.id).second) continue;
        subset.push_back(slot);
        if (subset.size() >= capacity) break;
    }
    if (subset.empty()) return 0;
    populationPreloadIds_.clear();
    for (const MonsterSlot& slot : subset) populationPreloadIds_.push_back(slot.id);
    populationPreloadStart_ = std::chrono::steady_clock::now();
    // All captured values are immutable. No live actor, Play, OpenGL context,
    // texture cache, or renderer is accessed from the worker.
    const std::string clientRoot = root_;
    const std::string mapName = mapName_;
    const int playerId = playerId_;
    populationPreload_ = std::async(
#ifdef __EMSCRIPTEN__
        std::launch::deferred, // No pthread/COOP requirement for the web build.
#else
        std::launch::async,
#endif
        [clientRoot, mapName, playerId, subset = std::move(subset)]() mutable -> std::unique_ptr<ActorSkin> {
            auto loader = std::make_unique<ActorSkin>();
            if (!loader->open(clientRoot, mapName, subset, playerId)) return {};
            return loader;
        });
    return static_cast<int>(populationPreloadIds_.size());
}

ActorSkin::PrefetchState ActorSkin::ensurePopulationPrefetched(const std::vector<MonsterSlot>& required) {
    pollPopulationPrefetch();
    std::unordered_set<int> missingIds;
    std::vector<MonsterSlot> missing;
    for (const MonsterSlot& slot : required) {
        if (slot.id <= 0 || hasPopulationType(slot.id) || !missingIds.insert(slot.id).second) continue;
        if (populationPreloadFailures_.count(slot.id)) return PrefetchState::Fallback;
        missing.push_back(slot);
    }
    if (missing.empty()) return PrefetchState::Ready;
    // Avoid occupying the cache with unrelated types before the required ones.
    if (missing.size() + cachedPopulationTypes_.size() > 48) return PrefetchState::Fallback;
    if (populationPreload_.valid()) return PrefetchState::Pending;
    return beginPopulationPrefetch(missing, static_cast<int>(missing.size())) > 0 ?
        PrefetchState::Pending : PrefetchState::Fallback;
}

bool ActorSkin::updatePopulation(const std::vector<MonsterSlot>& monsters) {
    if (root_.empty() || bodies_.empty()) return false;
    // Body can outlive the original tile. Match stable historical spawn keys,
    // falling back to the original source coordinates for keyless GM/NPC slots.
    auto matches = [](const Body& body, const MonsterSlot& slot) {
        return population_identity::same(
            {body.id, body.spawnX, body.spawnY, body.passive, body.spawnKey},
            {slot.id, slot.x, slot.y, slot.passive, slot.spawnKey});
    };
    std::vector<bool> taken(bodies_.size(), false);
    std::vector<int> oldIndex(monsters.size(), -1);
    // A parsed type can be reused when walking back to an earlier sector.
    // Avoid caching every NPC in the map: at most 48 types per open/map.
    constexpr std::size_t kPopulationTypeLimit = 48;
    for (const Body& body : bodies_) {
        if (body.player || cachedPopulationTypes_.count(body.id) ||
            cachedPopulationTypes_.size() >= kPopulationTypeLimit) continue;
        Body prototype = body;
        prototype.cachedPoseMatrices.clear();
        prototype.cachedGeometry.clear();
        cachedPopulationTypes_[body.id] = std::make_shared<Body>(std::move(prototype));
    }
    std::unordered_map<int, const Body*> prototypes;
    for (const auto& entry : cachedPopulationTypes_)
        prototypes.emplace(entry.first, entry.second.get());
    std::vector<MonsterSlot> loadTypes;
    for (std::size_t i = 0; i < monsters.size(); ++i) {
        const MonsterSlot& slot = monsters[i];
        for (std::size_t j = 0; j < bodies_.size(); ++j) {
            if (!taken[j] && !bodies_[j].player && matches(bodies_[j], slot)) {
                oldIndex[i] = static_cast<int>(j);
                taken[j] = true;
                break;
            }
        }
        if (oldIndex[i] >= 0 || prototypes.count(slot.id)) continue;
        bool known = false;
        for (const Body& body : bodies_) {
            if (!body.player && body.id == slot.id) {
                prototypes.emplace(slot.id, &body);
                known = true;
                break;
            }
        }
        if (!known && std::none_of(loadTypes.begin(), loadTypes.end(), [&](const MonsterSlot& candidate) {
                return candidate.id == slot.id;
            })) loadTypes.push_back(slot);
    }
    const int newTypeCount = static_cast<int>(loadTypes.size());
    if (!loadTypes.empty()) {
        // Load only new character IDs; leave the live scene untouched if I/O
        // fails. The temporary renderer is never exposed to the active Play.
        ActorSkin loader;
        if (!loader.open(root_, mapName_, loadTypes, playerId_)) return false;
        for (const Body& body : loader.bodies_)
            if (!body.player && !prototypes.count(body.id)) {
                if (cachedPopulationTypes_.size() < kPopulationTypeLimit) {
                    Body prototype = body;
                    prototype.cachedPoseMatrices.clear();
                    prototype.cachedGeometry.clear();
                    auto stored = std::make_shared<Body>(std::move(prototype));
                    prototypes.emplace(body.id, stored.get());
                    cachedPopulationTypes_.emplace(body.id, std::move(stored));
                } else {
                    // New type beyond cache capacity: the caller can use
                    // its existing full-open path, without partial mutation.
                    return false;
                }
            }
    }
    // Keep the old animation drives attached to their original spawn identity.
    std::vector<int> oldDriveIndex(bodies_.size(), -1);
    int driveNo = 0;
    for (std::size_t j = 0; j < bodies_.size(); ++j)
        if (!bodies_[j].player && !bodies_[j].passive) oldDriveIndex[j] = driveNo++;
    std::vector<Body> next;
    std::vector<MobDrive> nextDrives;
    next.reserve(monsters.size() + 1);
    nextDrives.reserve(monsters.size());
    // Player and its equipment/boat must never be rebuilt for streaming.
    for (Body& body : bodies_)
        if (body.player) { next.push_back(std::move(body)); break; }
    if (next.empty()) return false;
    int retained = 0;
    int created = 0;
    for (std::size_t i = 0; i < monsters.size(); ++i) {
        const MonsterSlot& slot = monsters[i];
        Body body;
        MobDrive drive;
        if (oldIndex[i] >= 0) {
            const int index = oldIndex[i];
            body = std::move(bodies_[static_cast<std::size_t>(index)]);
            const int oldDrive = oldDriveIndex[static_cast<std::size_t>(index)];
            if (oldDrive >= 0 && static_cast<std::size_t>(oldDrive) < drives_.size())
                drive = drives_[static_cast<std::size_t>(oldDrive)];
            ++retained;
        } else {
            const auto it = prototypes.find(slot.id);
            if (it == prototypes.end()) continue; // same rule as open(): invalid character skipped
            body = *it->second;
            ++created;
            body.x = slot.x;
            body.y = slot.y;
            body.cursor = 8.f;
            body.cachedPoseFrame = body.cachedDrawFrame = body.cachedDrawPose = -1;
            body.cachedPoseMatrices.clear();
            body.cachedGeometry.clear();
        }
        body.spawnKey = slot.spawnKey;
        body.respawnSeconds = slot.respawnSeconds;
        body.spawnFacing = slot.yaw;
        body.spawnX = slot.x;
        body.spawnY = slot.y;
        body.passive = slot.passive;
        if (slot.passive) body.yawDeg = slot.yaw;
        next.push_back(std::move(body));
        if (!slot.passive) nextDrives.push_back(std::move(drive));
    }
    bodies_ = std::move(next);
    drives_ = std::move(nextDrives);
    roster_ = monsters;
    report_.actors = static_cast<int>(bodies_.size());
    report_.parts = report_.bones = report_.monsterPoseReady = report_.monsterPoseMissing = 0;
    for (const Body& body : bodies_) {
        report_.parts += static_cast<int>(body.parts.size());
        report_.bones += body.bone.boneNum;
        if (!body.player) {
            if (body.idle.ok() || body.run.ok()) ++report_.monsterPoseReady;
            else ++report_.monsterPoseMissing;
        }
    }
    // Newly materialized bodies need placement, but do not touch the player's
    // cached geometry/pose, boat mesh, equipped items or UI state.
    placed_ = false;
    lastPopulationUpdate_ = {retained, created, newTypeCount,
                             static_cast<int>(cachedPopulationTypes_.size())};
    return true;
}

int ActorSkin::monsterCount() const {
    int count = 0;
    for (const Body& body : bodies_) {
        if (!body.player && !body.passive) ++count;
    }
    return count;
}

int ActorSkin::appendMonsterModel(int id, bool duplicate) {
    if (id <= 0 || root_.empty()) return -1;
    if (!duplicate) {
        for (int i = 0; i < monsterCount(); ++i)
            if (monsterSlot(i).id == id) return i;
    }
    const bool restoreBoat = boatMode_;
    const int savedBoatId = boatId_;
    std::vector<MonsterSlot> expanded = roster_;
    MonsterSlot slot;
    slot.id = id;
    slot.passive = false;
    slot.x = playerX_ + 1.f;
    slot.y = playerY_ + 1.f;
    slot.yaw = 145.f;
    expanded.push_back(slot);
    if (!open(root_, mapName_, expanded, playerId_)) return -1;
    if (restoreBoat) setBoatMode(true, savedBoatId);
    const int last = monsterCount() - 1;
    return last >= 0 && monsterSlot(last).id == id ? last : -1;
}

float ActorSkin::playerAttackWindup() const {
    for (const Body& body : bodies_) {
        if (!body.player || !body.attack.ok()) continue;
        return static_cast<float>(std::max(0, body.attack.impact - body.attack.start)) / 30.f;
    }
    return 7.f / 30.f;
}

float ActorSkin::playerAttackDuration() const {
    for (const Body& body : bodies_) {
        if (!body.player || !body.attack.ok()) continue;
        return static_cast<float>(std::max(1, body.attack.end - body.attack.start)) / 30.f;
    }
    return .7f;
}

MonsterSlot ActorSkin::monsterSlot(int index) const {
    int seen = 0;
    for (const Body& body : bodies_) {
        if (body.player || body.passive) continue;
        if (seen == index) return {body.id, body.x, body.y, false, body.spawnFacing,
                                   body.spawnKey, body.respawnSeconds};
        ++seen;
    }
    return {};
}

ActorActionTiming ActorSkin::monsterActionTiming(int index, int actionBase) const {
    int seen = 0;
    for (const Body& body : bodies_) {
        if (body.player || body.passive) continue;
        if (seen++ != index) continue;
        PoseSpan pose = body.actionPose(actionBase);
        if (!pose.ok()) pose = body.attack;
        return pose.ok() ? ActorActionTiming{static_cast<float>(pose.end - pose.start) / 30.f,
            static_cast<float>(pose.impact - pose.start) / 30.f} : ActorActionTiming{};
    }
    return {};
}

void ActorSkin::setMonster(int index, float x, float y, float yawRad, bool moving, int pose, bool hide, float hp01,
                           int action, float playbackRate, unsigned long long cycle) {
    if (index < 0) return;
    if (static_cast<int>(drives_.size()) <= index) drives_.resize(static_cast<std::size_t>(index) + 1);
    MobDrive& drive = drives_[static_cast<std::size_t>(index)];
    drive.used = true;
    drive.x = x;
    drive.y = y;
    drive.yaw = yawRad;
    drive.moving = moving;
    drive.pose = pose;
    drive.hide = hide;
    drive.hp01 = hp01;
    drive.action = action;
    drive.playbackRate = std::clamp(playbackRate, .05f, 10.f);
    drive.cycle = cycle;
}

void ActorSkin::setVitals(float hp01, float sp01, float exp01) {
    hp01_ = hp01;
    sp01_ = sp01;
    exp01_ = exp01;
}

bool ActorSkin::setEquipment(const std::unordered_map<int, int>& worn) {
    if (worn == wornVisual_) return true;
    if (boatMode_) return true;
    Body* player = nullptr;
    for (Body& body : bodies_) {
        if (body.player) {
            player = &body;
            break;
        }
    }
    if (!player) return false;

    std::unordered_map<int, std::vector<std::string>> items;
    if (!loadTable(root_ + "/scripts/table/iteminfo.txt", items)) return false;
    // The animation bank depends on the historical item type, not on whether
    // the cosmetic .lgo loaded. A missing weapon mesh must not turn a sword
    // skill into an unarmed attack.
    const auto typeAt = [&](int slot) {
        const auto equipped = worn.find(slot);
        if (equipped == worn.end()) return 0;
        const auto item = items.find(equipped->second);
        return item != items.end() && item->second.size() > 10
            ? std::max(0, std::atoi(item->second[10].c_str())) : 0;
    };
    const int newPoseMode = weaponPoseMode(typeAt(6), typeAt(9));
    player->usePoseMode(newPoseMode);
    report_.playerPoseMode = player->poseMode;
    std::unordered_map<int, int> desired;
    for (int slot = 0; slot < 5; ++slot) desired[slot] = playerDefaultSkin_[slot];
    // As cinco roupas substituem a aparencia inicial. Armas sao pecas extras
    // deformadas pelo mesmo esqueleto; os demais slots nao possuem malha corporal.
    for (const auto& entry : worn) {
        if ((entry.first >= 0 && entry.first <= 4) || entry.first == 6 || entry.first == 9)
            desired[entry.first] = entry.second;
    }

    std::vector<Model> nextParts;
    std::vector<int> nextSlots;
    std::unordered_map<std::string, std::string> modelIndex;
    std::unordered_map<std::string, std::string> itemModelIndex;
    std::unordered_map<std::string, std::string> texIndex;
    std::unordered_map<std::string, std::string> itemTexIndex;
    bool modelScanned = false;
    bool itemModelScanned = false;
    bool texScanned = false;
    bool itemTexScanned = false;
    const std::string modelDir = root_ + "/model/character";
    const std::string itemModelDir = root_ + "/model/item";
    const std::string texDir = root_ + "/texture/character";
    const std::string itemTexDir = root_ + "/texture/item";
    bool failedEquipped = false;
    for (int slot : {0, 1, 2, 3, 4, 6, 9}) {
        const auto wanted = desired.find(slot);
        if (wanted == desired.end() || wanted->second <= 0) continue;
        const auto item = items.find(wanted->second);
        const int column = 4 + playerFramework_;
        if (item == items.end() || column >= static_cast<int>(item->second.size())) {
            if (worn.count(slot)) failedEquipped = true;
            continue;
        }
        std::string fileName = item->second[static_cast<std::size_t>(column)];
        if (fileName.empty() || fileName == "0") {
            if (worn.count(slot)) failedEquipped = true;
            continue;
        }
        fileName += ".lgo";
        std::string path = resolveName(modelDir, fileName, modelIndex, modelScanned);
        if (path.empty()) path = resolveName(itemModelDir, fileName, itemModelIndex, itemModelScanned);
        Model model;
        if (path.empty() || !model.loadGeom(path)) {
            if (worn.count(slot)) failedEquipped = true;
            continue;
        }
        for (ModelGeom& geom : model.geoms) {
            for (ModelSubset& subset : geom.subsets) {
                if (subset.texture.empty()) continue;
                std::string tex = resolveName(texDir, subset.texture, texIndex, texScanned);
                if (tex.empty()) tex = resolveName(itemTexDir, subset.texture, itemTexIndex, itemTexScanned);
                if (tex.empty()) {
                    std::string dds = subset.texture;
                    const std::size_t dot = dds.find_last_of('.');
                    if (dot != std::string::npos) dds.replace(dot, std::string::npos, ".dds");
                    tex = resolveName(texDir, dds, texIndex, texScanned);
                    if (tex.empty()) tex = resolveName(itemTexDir, dds, itemTexIndex, itemTexScanned);
                }
                if (tex.empty() && std::find(report_.missingTextures.begin(), report_.missingTextures.end(), subset.texture) ==
                                       report_.missingTextures.end())
                    report_.missingTextures.push_back(subset.texture);
                subset.texture = tex;
            }
        }
        nextParts.push_back(std::move(model));
        nextSlots.push_back(slot);
    }
    // Nao troca um corpo valido por uma composicao quebrada. Isto tambem deixa
    // visivel no teste quando um item historico aponta para um arquivo ausente.
    // O conjunto fica anotado mesmo na falha: sem isso o Play repete esta
    // funcao todo quadro e ela relia o iteminfo.txt inteiro a cada vez.
    if (failedEquipped || nextParts.empty()) {
        wornVisual_ = worn;
        return false;
    }
    report_.parts -= static_cast<int>(player->parts.size());
    player->parts = std::move(nextParts);
    player->partSlots = std::move(nextSlots);
    player->cachedGeometry.clear();
    report_.parts += static_cast<int>(player->parts.size());
    const auto backDummy = [](int type, bool left) {
        if (left) {
            if (type == 2 || type == 5 || type == 9 || type == 10) return 21;
            if (type == 1 || type == 3 || type == 11) return 22;
            if (type == 4 || type == 7 || type == 8) return 23;
        } else {
            if (type == 1 || type == 2 || type == 5 || type == 9 || type == 10) return 21;
            if (type == 3 || type == 11 || type == 18 || type == 19) return 22;
            if (type == 4 || type == 7 || type == 8) return 23;
        }
        return -1;
    };
    player->leftWeaponBackDummy = backDummy(typeAt(6), true);
    player->rightWeaponBackDummy = backDummy(typeAt(9), false);
    report_.playerPoseMode = player->poseMode;
    report_.weaponAttached = false;
    report_.weaponMoves = false;
    report_.weaponGripCorrect = false;
    report_.weaponHeight = 0.f;
    for (std::size_t part = 0; part < player->partSlots.size(); ++part) {
        const int slot = player->partSlots[part];
        if (slot != 6 && slot != 9) continue;
        float start[16];
        if (!player->bone.dummyMatrix(player->idle.start, slot, start)) continue;
        report_.weaponAttached = true;
        report_.weaponHeight = start[14];
        if (part < player->parts.size() && player->parts[part].hasItemDummy) {
            float inverseDummy[16];
            float itemToHand[16];
            float aligned[16];
            if (inverseAffine(player->parts[part].itemDummy, inverseDummy)) {
                multiply(inverseDummy, start, itemToHand);
                multiply(player->parts[part].itemDummy, itemToHand, aligned);
                float error = 0.f;
                for (int n = 0; n < 16; ++n) error += std::fabs(aligned[n] - start[n]);
                report_.weaponGripCorrect = report_.weaponGripCorrect || error < 0.002f;
            }
        }
        for (int frame = player->attack.start; frame <= player->attack.end && !report_.weaponMoves; ++frame) {
            float current[16];
            if (!player->bone.dummyMatrix(frame, slot, current)) continue;
            float delta = 0.f;
            for (int n = 0; n < 16; ++n) delta += std::fabs(start[n] - current[n]);
            report_.weaponMoves = delta > 0.001f;
        }
    }
    wornVisual_ = worn;
    return true;
}

bool ActorSkin::setBoatMode(bool enabled, int shipId) {
    if (shipId > 0) boatId_ = std::max(1, std::min(17, shipId));
    if (enabled == boatMode_) return true;
    Body* player = nullptr;
    for (Body& body : bodies_) if (body.player) { player = &body; break; }
    if (!player) return false;
    if (!enabled) {
        if (player->landParts.empty()) return false;
        report_.parts -= static_cast<int>(player->parts.size());
        player->parts = std::move(player->landParts);
        player->partSlots = std::move(player->landPartSlots);
        player->cachedGeometry.clear();
        report_.parts += static_cast<int>(player->parts.size());
        boatMode_ = false;
        boatMoveSpeed_ = 0.f;
        return true;
    }

    std::string dataRoot;
    for (const std::string& candidate : {root_}) {
        FILE* probe = std::fopen((candidate + "/scripts/table/shipinfo.txt").c_str(), "rb");
        if (!probe) continue;
        std::fclose(probe);
        dataRoot = candidate;
        break;
    }
    if (dataRoot.empty()) return false;
    std::unordered_map<int, std::vector<std::string>> ships;
    std::unordered_map<int, std::vector<std::string>> parts;
    std::unordered_map<int, std::vector<std::string>> characters;
    if (!loadTable(dataRoot + "/scripts/table/shipinfo.txt", ships) ||
        !loadTable(dataRoot + "/scripts/table/shipiteminfo.txt", parts)) return false;
    const auto ship = ships.find(boatId_);
    if (ship == ships.end()) return false;
    float seaHeight = 0.f;
    if (loadTable(dataRoot + "/scripts/table/characterinfo.txt", characters)) {
        const int characterId = field(ship->second, 3);
        const auto character = characters.find(characterId);
        // CCharacter::_UpdateHeight uses SeaHeight/100 for SEA territory.
        // The source table stores these fields at columns 26 and 27.
        if (character != characters.end() && field(character->second, 26) == 1)
            seaHeight = static_cast<float>(field(character->second, 27)) / 100.f;
    }
    const int hullId = field(ship->second, 5);
    const auto hull = parts.find(hullId);
    if (hull == parts.end()) return false;
    // CCharBoat::Create selects the first authored option for each component.
    // ShipSet sets byIsUpdate=1, so GetData sums hull, fore, engine and its
    // motors, cannon and equipment into ATTR_BMSPD.
    auto firstId = [](const std::vector<std::string>& row, int column) {
        if (column < 0 || column >= static_cast<int>(row.size())) return 0;
        const std::string& value = row[static_cast<std::size_t>(column)];
        const std::size_t comma = value.find(',');
        return std::atoi(value.substr(0, comma).c_str());
    };
    int shipSpeed = field(ship->second, 25);
    auto addPartSpeed = [&](int id) {
        const auto part = parts.find(id);
        if (id > 0 && part != parts.end()) shipSpeed += field(part->second, 21);
    };
    addPartSpeed(hullId);
    addPartSpeed(firstId(ship->second, 7)); // fore
    const int engineId = firstId(ship->second, 6); // mobility
    addPartSpeed(engineId);
    const auto engine = parts.find(engineId);
    if (engine != parts.end()) {
        for (int column = 3; column <= 6; ++column) {
            const int motorId = field(engine->second, column);
            if (motorId <= 0) break;
            addPartSpeed(motorId);
        }
    }
    addPartSpeed(firstId(ship->second, 8)); // artillery
    addPartSpeed(firstId(ship->second, 9)); // component/equipment
    int modelId = field(hull->second, 2);
    if (modelId <= 0) return false;
    char fileName[32];
    std::snprintf(fileName, sizeof fileName, "%010d.lgo", modelId);
    std::unordered_map<std::string, std::string> modelIndex;
    std::unordered_map<std::string, std::string> texIndex;
    bool modelScanned = false;
    bool texScanned = false;
    const std::string modelDir = root_ + "/model/character";
    const std::string texDir = root_ + "/texture/character";
    std::string path = resolveName(modelDir, fileName, modelIndex, modelScanned);
    // Os registros 7..10 do cliente original são placeholders e apontam para
    // cascos 200406..200409 que não existem nem no acervo DX9. Mantemos essas
    // entradas selecionáveis, usando o casco Guppy como visual de segurança.
    if (path.empty() && boatId_ >= 7 && boatId_ <= 10) {
        const auto fallbackShip = ships.find(1);
        if (fallbackShip != ships.end()) {
            const auto fallbackHull = parts.find(field(fallbackShip->second, 5));
            if (fallbackHull != parts.end()) {
                modelId = field(fallbackHull->second, 2);
                std::snprintf(fileName, sizeof fileName, "%010d.lgo", modelId);
                path = resolveName(modelDir, fileName, modelIndex, modelScanned);
            }
        }
    }
    Model model;
    if (path.empty() || !model.loadGeom(path)) return false;
    for (ModelGeom& geom : model.geoms) {
        for (ModelSubset& subset : geom.subsets) {
            if (subset.texture.empty()) continue;
            std::string tex = resolveName(texDir, subset.texture, texIndex, texScanned);
            if (tex.empty()) {
                std::string dds = subset.texture;
                const std::size_t dot = dds.find_last_of('.');
                if (dot != std::string::npos) dds.replace(dot, std::string::npos, ".dds");
                tex = resolveName(texDir, dds, texIndex, texScanned);
            }
            if (tex.empty() && std::find(report_.missingTextures.begin(), report_.missingTextures.end(), subset.texture) ==
                                   report_.missingTextures.end())
                report_.missingTextures.push_back(subset.texture);
            subset.texture = tex;
        }
    }
    player->landParts = std::move(player->parts);
    player->landPartSlots = std::move(player->partSlots);
    report_.parts -= static_cast<int>(player->landParts.size());
    player->parts.clear();
    player->parts.push_back(std::move(model));
    player->partSlots = {-1};
    player->cachedGeometry.clear();
    report_.parts += 1;
    boatSeaHeight_ = seaHeight;
    boatMoveSpeed_ = static_cast<float>(std::max(0, shipSpeed)) / 100.f;
    boatMode_ = true;
    ui_.boat.close();
    return true;
}

void ActorSkin::setMarks(std::vector<GroundMark> marks) { marks_ = std::move(marks); }

void ActorSkin::addMarks(const std::vector<GroundMark>& marks) { marks_.insert(marks_.end(), marks.begin(), marks.end()); }

void ActorSkin::setFrustum(const float* viewProjection, bool enabled) {
    frustumEnabled_ = enabled && frustum_.setFromClipMatrix(viewProjection);
}

void ActorSkin::build(MapFile& map, const SceneMesh* ground) {
    batches_.clear();
    report_.triangles = 0;
    report_.frustumTested = 0;
    report_.frustumVisible = 0;
    report_.frustumCulled = 0;
    report_.skeletonUpdates = 0;
    report_.skeletonReuses = 0;
    report_.skinnedUniqueVertices = 0;
    report_.transformedUniqueVertices = 0;
    report_.shadows = 0;
    report_.untexturedTriangles = 0;
    ensurePlaced(map);

    std::unordered_map<std::string, std::size_t> batchOf;
    ActorBatch shadows;
    shadows.mode = 6;
    ActorBatch bars;
    bars.mode = 0;
    int mobIndex = 0;
    for (Body& body : bodies_) {
        int drivenPose = -1;
        int drivenAction = 7;
        float drivenRate = 1.f;
        unsigned long long drivenCycle = 0;
        if (!body.player && !body.passive) {
            if (mobIndex < static_cast<int>(drives_.size()) && drives_[static_cast<std::size_t>(mobIndex)].used) {
                const MobDrive& drive = drives_[static_cast<std::size_t>(mobIndex)];
                body.x = drive.x;
                body.y = drive.y;
                // O mesmo 180 - atan2 do jogador. O +Y da malha segue o passo.
                body.yawDeg = 180.f - drive.yaw * (180.f / kPi);
                body.moving = drive.moving;
                body.hide = drive.hide;
                body.hp01 = drive.hp01;
                drivenPose = drive.pose;
                drivenAction = drive.action;
                drivenRate = drive.playbackRate;
                drivenCycle = drive.cycle;
            }
            ++mobIndex;
        }
        if (!body.player && body.hide) continue;

        int want = 0;
        PoseSpan selected;
        int action = 0;
        if (body.player) {
            // DX9 locomotion can interrupt the one-shot body pose without
            // cancelling ongoing skill effects. Never restart the same cast
            // when the character stops walking a moment later.
            if (playerMoving_ && playerPose_ == 5) {
                playerCastPoseInterrupted_ = true;
                playerCastPoseCycle_ = playerActionCycle_;
            }
            const bool interruptedCast = playerCastPoseInterrupted_ &&
                playerCastPoseCycle_ == playerActionCycle_;
            if (playerPose_ == 3 && body.die.ok()) want = 3;
            else if (playerPose_ == 2) {
                action = playerActionBase_;
                selected = body.actionPose(action);
                // Illusion Slash (skill 81) explicitly asks for action 14.
                // Replacing a missing 14 with the normal attack 7 hid the
                // real problem and always drew the wrong movement. Keep
                // legacy fallback for other actions until their banks are
                // audited independently.
                if (!selected.ok() && action != 14) selected = body.attack;
                if (selected.ok()) want = 2;
            } else if (playerPose_ == 5 && !interruptedCast) {
                action = playerActionBase_;
                selected = body.actionPose(action);
                if (!selected.ok()) selected = body.attack;
                if (selected.ok()) want = 5;
            } else if (playerPose_ == 4 && body.seat.ok()) want = 4;
            else if (playerMoving_ && body.run.ok()) want = 1;
        } else if (drivenPose >= 0) {
            if (drivenPose == 3 && body.die.ok()) want = 3;
            else if (drivenPose == 2) {
                action = drivenAction;
                selected = body.actionPose(action);
                if (!selected.ok()) selected = body.attack;
                if (selected.ok()) want = 2;
            }
            else if (body.moving && body.run.ok()) want = 1;
        }
        // Ataques consecutivos podem manter want==2. Reinicia a pose quando
        // o ID real da acao troca (7->8->9), nao apenas ao mudar de estado.
        const unsigned long long actionCycle = body.player ? playerActionCycle_ : drivenCycle;
        if (shouldRestartActionPose(want, body.pose, action, body.activeAction,
                                    actionCycle, body.activeActionCycle)) {
            // Idle, run, and one-shot actions have unrelated frame ranges.
            // Reusing the old cursor made the armed run start in the middle
            // of whichever idle/attack clip had been playing before movement.
            body.cursor = 0.f;
            body.pose = want;
            body.activeAction = action;
            body.activeActionCycle = actionCycle;
        }
        body.cachedDrawPose = want;
        const PoseSpan pose = (want == 2 || want == 5) && selected.ok() ? selected :
            want == 4 ? body.seat : want == 3 ? body.die : want == 2 ? body.attack : want == 1 ? body.run : body.idle;
        const float playbackRate = body.player && playerActionOnce_ && (want == 2 || want == 5)
            ? playerActionPlaybackRate_ : drivenPose == 2 ? drivenRate : 1.f;
        body.cursor += 30.f * playerDt_ * playbackRate;
        int frame = 0;
        if (((want == 3 || want == 4) ||
             (body.player && playerActionOnce_ && (want == 2 || want == 5)) ||
             (drivenPose == 2 && want == 2)) && pose.ok()) {
            // Death, sitting, and authored skill actions finish at their final
            // keyframe instead of looping and appearing to restart/cut off.
            frame = poseFrameOnce(pose.start, pose.end, body.cursor, body.bone.frameNum);
        } else {
            frame = sampleFrame(pose.ok() ? pose : body.idle, body.cursor, body.bone.frameNum);
        }
        body.cachedDrawFrame = frame;
        // Atualize sempre pose, posicao e altura, mesmo fora da camera.
        // Somente a montagem visual (matrizes, skinning e sombras) e descartada;
        // a IA, combate, spawns e colisao continuam no Play/NpcSystem.
        if (body.player) {
            body.x = playerX_;
            body.y = playerY_;
            body.z = playerZ_;
            body.moving = playerMoving_;
            body.yawDeg = 180.f - playerYaw_ * (180.f / kPi);
        } else {
            const float terrain = map.heightAt(body.x, body.y);
            const float groundHeight = ground ?
                ground->surfaceAt(body.x, body.y, terrain, terrain) : terrain;
            body.z = historical_character_height::baseHeight(
                body.territory, body.seaHeightCm, groundHeight);
            if (body.territory == 1) {
                // Opt-in, once per character ID: prove which historical
                // character records are actually visible in the OpenGL scene.
                const char* trace = std::getenv("TOP_TRACE_SEA_ACTORS");
                if (trace && trace[0] == '1') {
                    static std::unordered_map<int, bool> seen;
                    if (seen.size() < 128 && seen.emplace(body.id, true).second)
                        std::fprintf(stderr,
                            "[sea-actor] id=%d territory=%d sea_cm=%d model_z=%.3f terrain_z=%.3f\n",
                            body.id, body.territory, body.seaHeightCm, body.z, groundHeight);
                }
            }
            if (drivenPose < 0 && !body.passive) {
                const float dx = playerX_ - body.x;
                const float dy = playerY_ - body.y;
                body.yawDeg = 180.f - std::atan2(dx, dy) * (180.f / kPi);
            }
        }
        ++report_.frustumTested;
        // Bounding sphere deliberately oversized for historical monsters,
        // big weapons, animated bones and shadows. Player never culled.
        const float radius = std::max(8.f, 2.f * std::hypot(body.shadowLength, body.shadowWidth) + 2.f);
        if (frustumEnabled_ && !body.player &&
            !frustum_.visibleSphere(body.x, body.z + 1.5f, body.y, radius)) {
            ++report_.frustumCulled;
            continue;
        }
        ++report_.frustumVisible;
        // A .lab is indexed by integer keyframe. At FPS > 30, several draws
        // use the same frame; do not rebuild all bone matrices unnecessarily.
        if (body.cachedPoseFrame != frame ||
            body.cachedPoseMatrices.size() != static_cast<std::size_t>(body.bone.boneNum) * 16) {
            body.bone.matrices(frame, body.cachedPoseMatrices);
            ++report_.skeletonUpdates;
            body.cachedPoseFrame = frame;
        } else ++report_.skeletonReuses;
        const std::vector<float>& mats = body.cachedPoseMatrices;
        float node[16];
        nodeMatrix(node, body.yawDeg, body.x, body.y, body.z);
        // O cliente DX9 usava SCENENODE_SHADOW no chao para personagens em
        // estado normal. Mantemos a mesma sombra eliptica, agora pelo shader
        // GLSL, sem ela aparecer no casco, em mortos ou em areas sem terreno.
        const Tile under = map.at(static_cast<int>(body.x), static_cast<int>(body.y));
        const bool dead = want == 3;
        const bool sailing = body.player && boatMode_;
        if (body.shadow > 0 && !dead && !sailing && under.valid && !under.blocked) {
            constexpr int segments = 20;
            const float rx = std::max(0.28f, body.shadowLength * 0.42f);
            const float ry = std::max(0.24f, body.shadowWidth * 0.32f);
            const float z = body.z + 0.025f;
            for (int segment = 0; segment < segments; ++segment) {
                const float a0 = 2.f * kPi * static_cast<float>(segment) / static_cast<float>(segments);
                const float a1 = 2.f * kPi * static_cast<float>(segment + 1) / static_cast<float>(segments);
                pushVertex(shadows.verts, body.x, z, body.y, 0.015f, 0.02f, 0.025f, 0.5f, 0.5f);
                shadows.verts.back().a = 0.34f;
                pushVertex(shadows.verts, body.x + std::cos(a0) * rx, z, body.y + std::sin(a0) * ry,
                           0.015f, 0.02f, 0.025f, 0.f, 0.f);
                shadows.verts.back().a = 0.08f;
                pushVertex(shadows.verts, body.x + std::cos(a1) * rx, z, body.y + std::sin(a1) * ry,
                           0.015f, 0.02f, 0.025f, 1.f, 0.f);
                shadows.verts.back().a = 0.08f;
            }
            ++report_.shadows;
        }
        body.cachedGeometry.resize(body.parts.size());
        for (std::size_t partIndex = 0; partIndex < body.parts.size(); ++partIndex) {
            const Model& model = body.parts[partIndex];
            const int visualSlot = partIndex < body.partSlots.size() ? body.partSlots[partIndex] : -1;
            body.cachedGeometry[partIndex].resize(model.geoms.size());
            float attachment[16];
            const int backDummy = visualSlot == 6 ? body.leftWeaponBackDummy :
                                  visualSlot == 9 ? body.rightWeaponBackDummy : -1;
            const int attachmentDummy = body.player
                ? weaponAttachmentDummy(visualSlot, backDummy, playerWeaponsDrawn_, want)
                : visualSlot;
            const bool attached = (visualSlot == 6 || visualSlot == 9) &&
                                  body.bone.dummyMatrixFromPose(mats, attachmentDummy, attachment);
            for (std::size_t g = 0; g < model.geoms.size(); ++g) {
                const ModelGeom& geom = model.geoms[g];
                if (geom.indices.size() < 3 || geom.position.size() < 3) continue;
                float world[16];
                if (attached) {
                    float itemToHand[16];
                    if (model.hasItemDummy) {
                        float inverseDummy[16];
                        if (inverseAffine(model.itemDummy, inverseDummy)) multiply(inverseDummy, attachment, itemToHand);
                        else std::memcpy(itemToHand, attachment, sizeof itemToHand);
                    } else {
                        std::memcpy(itemToHand, attachment, sizeof itemToHand);
                    }
                    float localToHand[16];
                    multiply(geom.local, itemToHand, localToHand);
                    multiply(localToHand, node, world);
                } else {
                    multiply(geom.local, node, world);
                }
                // Skin each referenced vertex once, not once for every triangle
                // sharing the vertex. World-space conversion is still rebuilt
                // on every draw, so moving/turning actors remain responsive.
                auto& vertexCache = body.cachedGeometry[partIndex][g];
                vertexCache.begin(geom.position.size() / 3, frame);
                const auto skinLocal = [&](std::size_t index, actor_frame_cache::Vertex& out) {
                    ++report_.skinnedUniqueVertices;
                    skinVertex(geom, mats, index, out.x, out.y, out.z);
                    skinNormal(geom, mats, index, out.nx, out.ny, out.nz);
                };
                const auto worldVertex = [&](const actor_frame_cache::Vertex& local,
                                            actor_frame_cache::Vertex& out) {
                    ++report_.transformedUniqueVertices;
                    transformPoint(world, local.x, local.y, local.z, out.x, out.y, out.z);
                    transformDir(world, local.nx, local.ny, local.nz, out.nx, out.ny, out.nz);
                    const float normalLength = std::sqrt(out.nx * out.nx + out.ny * out.ny + out.nz * out.nz);
                    if (normalLength > 1e-4f) {
                        out.nx /= normalLength;
                        out.ny /= normalLength;
                        out.nz /= normalLength;
                    }
                };
                for (const ModelSubset& subset : geom.subsets) {
                    if (subset.indexCount < 3) continue;
                    if (subset.texture.empty()) report_.untexturedTriangles += static_cast<int>(subset.indexCount / 3);
                    const std::string key = std::to_string(subset.blend == 1 ? 4 : 3) + "\n" + subset.texture;
                    if (!batchOf.count(key)) {
                        batchOf[key] = batches_.size();
                        ActorBatch batch;
                        batch.mode = subset.blend == 1 ? 4 : 3;
                        batch.texture = subset.texture;
                        batches_.push_back(std::move(batch));
                    }
                    ActorBatch& batch = batches_[batchOf[key]];
                    const std::uint32_t begin = subset.startIndex;
                    const std::uint32_t end = std::min(subset.startIndex + subset.indexCount, static_cast<std::uint32_t>(geom.indices.size()));
                    for (std::uint32_t i = begin; i + 2 < end; i += 3) {
                        // Validate the complete triangle before emitting vertices.
                        const std::uint32_t i0 = geom.indices[i];
                        const std::uint32_t i1 = geom.indices[i + 1];
                        const std::uint32_t i2 = geom.indices[i + 2];
                        if (i0 >= vertexCache.size() || i1 >= vertexCache.size() || i2 >= vertexCache.size())
                            continue;
                        for (const std::uint32_t index : {i0, i1, i2}) {
                            const auto& v = vertexCache.get(index, skinLocal, worldVertex);
                            const bool hasUv = geom.uv.size() >= (static_cast<std::size_t>(index) + 1) * 2;
                            const float u = hasUv ? geom.uv[static_cast<std::size_t>(index) * 2] : 0.f;
                            const float vv = hasUv ? geom.uv[static_cast<std::size_t>(index) * 2 + 1] : 0.f;
                            pushVertex(batch.verts, v.x, v.z, v.y, subset.r, subset.g, subset.b,
                                       u, vv, v.nx, v.nz, v.ny);
                        }
                        ++report_.triangles;
                    }
                }
            }
        }
        if (!body.player && body.hp01 >= 0.f) {
            const float fill = std::max(0.f, std::min(1.f, body.hp01));
            const float half = 0.45f;
            const float z0 = body.z + 2.45f;
            const float z1 = z0 + 0.12f;
            const float yb = body.y + 0.04f;
            auto quad = [&](float x0, float x1, float r, float g, float b) {
                pushVertex(bars.verts, x0, z0, yb, r, g, b, 0.f, 0.f);
                pushVertex(bars.verts, x1, z0, yb, r, g, b, 1.f, 0.f);
                pushVertex(bars.verts, x0, z1, yb, r, g, b, 0.f, 1.f);
                pushVertex(bars.verts, x1, z0, yb, r, g, b, 1.f, 0.f);
                pushVertex(bars.verts, x1, z1, yb, r, g, b, 1.f, 1.f);
                pushVertex(bars.verts, x0, z1, yb, r, g, b, 0.f, 1.f);
            };
            const float split = body.x - half + 0.9f * fill;
            // Verde e vermelho lado a lado. Antes os dois quads eram
            // coplanares e disputavam a mesma profundidade, piscando e
            // alternando a cor conforme a camera se movia.
            if (fill > 0.f) quad(body.x - half, split, 0.25f, 0.82f, 0.28f);
            if (fill < 1.f) quad(split, body.x + half, 0.32f, 0.05f, 0.05f);
        }
    }
    for (const GroundMark& mark : marks_) {
        const float ground = map.heightAt(mark.x, mark.y);
        if (mark.kind == 0) {
            // 05-W1: marcador maior de drop, visivel de angulo baixo.
            // Arte de drop original continua pendente; nao inventar modelo.
            const float z = ground + 0.34f;
            const float s = 0.48f;
            auto tip = [&](float x, float y, float r, float g, float b) {
                pushVertex(bars.verts, x, z, y, r, g, b, 0.f, 0.f);
            };
            tip(mark.x, mark.y + s, 0.95f, 0.82f, 0.2f);
            tip(mark.x + s, mark.y, 0.95f, 0.7f, 0.15f);
            tip(mark.x, mark.y - s, 0.75f, 0.5f, 0.1f);
            tip(mark.x, mark.y + s, 0.95f, 0.82f, 0.2f);
            tip(mark.x, mark.y - s, 0.75f, 0.5f, 0.1f);
            tip(mark.x - s, mark.y, 0.9f, 0.65f, 0.12f);
        } else if (mark.kind == 1) {
            const float z0 = ground + 0.05f;
            const float z1 = z0 + 1.55f;
            const float yb = mark.y + 0.04f;
            const float half = 0.16f;
            auto post = [&](float x0, float x1, float r, float g, float b) {
                pushVertex(bars.verts, x0, z0, yb, r, g, b, 0.f, 0.f);
                pushVertex(bars.verts, x1, z0, yb, r, g, b, 1.f, 0.f);
                pushVertex(bars.verts, x0, z1, yb, r, g, b, 0.f, 1.f);
                pushVertex(bars.verts, x1, z0, yb, r, g, b, 1.f, 0.f);
                pushVertex(bars.verts, x1, z1, yb, r, g, b, 1.f, 1.f);
                pushVertex(bars.verts, x0, z1, yb, r, g, b, 0.f, 1.f);
            };
            post(mark.x - half, mark.x + half, 0.55f, 0.32f, 0.12f);
        } else if (mark.kind == 2) {
            const float z0 = ground + 0.08f;
            const float z1 = z0 + 2.2f;
            const float s = 0.38f;
            auto tri = [&](float x0, float z, float y0, float x1, float y1, float x2, float y2) {
                pushVertex(bars.verts, x0, z, y0, 0.25f, 0.75f, 1.f, 0.f, 0.f);
                pushVertex(bars.verts, x1, z, y1, 0.45f, 0.9f, 1.f, 1.f, 0.f);
                pushVertex(bars.verts, x2, z, y2, 0.15f, 0.45f, 1.f, 0.5f, 1.f);
            };
            tri(mark.x - s, z0, mark.y, mark.x + s, mark.y, mark.x, mark.y + s);
            tri(mark.x - s, z1, mark.y, mark.x, mark.y + s, mark.x + s, mark.y);
        } else {
            // NPC carregado da tabela do mapa: losango verde no chao ate o
            // modelo dinamico de cada ficha entrar no ActorSkin.
            const float z = ground + 0.12f;
            const float s = 0.32f;
            auto tip = [&](float x, float y, float r, float g, float b) {
                pushVertex(bars.verts, x, z, y, r, g, b, 0.f, 0.f);
            };
            const bool merchant = mark.kind == 4;
            const float r0 = merchant ? 1.f : 0.35f;
            const float g0 = merchant ? 0.78f : 1.f;
            const float b0 = merchant ? 0.18f : 0.45f;
            tip(mark.x, mark.y + s, r0, g0, b0);
            tip(mark.x + s, mark.y, merchant ? 0.9f : 0.15f, merchant ? 0.48f : 0.75f, 0.12f);
            tip(mark.x, mark.y - s, merchant ? 0.65f : 0.08f, merchant ? 0.28f : 0.5f, 0.08f);
            tip(mark.x, mark.y + s, r0, g0, b0);
            tip(mark.x, mark.y - s, merchant ? 0.65f : 0.08f, merchant ? 0.28f : 0.5f, 0.08f);
            tip(mark.x - s, mark.y, merchant ? 0.9f : 0.15f, merchant ? 0.48f : 0.75f, 0.12f);
        }
    }
    if (!shadows.verts.empty()) batches_.insert(batches_.begin(), std::move(shadows));
    if (!bars.verts.empty()) batches_.push_back(std::move(bars));
}

}  // namespace top
