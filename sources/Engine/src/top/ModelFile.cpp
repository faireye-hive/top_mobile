#include "top/ModelFile.h"
#include <algorithm>
#include <cmath>

#include <cstdio>
#include <cstring>

namespace top {
namespace {

constexpr std::uint32_t kFvfNormal = 0x010;
constexpr std::uint32_t kFvfDiffuse = 0x040;
constexpr std::uint32_t kFvfTex1 = 0x100;
constexpr std::uint32_t kFvfTex2 = 0x200;
constexpr std::uint32_t kFvfTex3 = 0x300;
constexpr std::uint32_t kFvfTex4 = 0x400;
constexpr std::uint32_t kFvfLastBeta = 0x1000;
constexpr int kGeomHeader = 116;
constexpr int kMtlRecord = 1004;
constexpr int kMtl0001 = 1164;
constexpr int kMtl0000 = 1028;
constexpr std::uint32_t kHelperDummy = 0x0001;

struct Cursor {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t pos = 0;
    bool ok = true;

    bool need(std::size_t n) {
        if (!ok || n > size - pos) {
            ok = false;
            return false;
        }
        return true;
    }

    std::uint32_t u32() {
        if (!need(4)) return 0;
        const std::uint32_t v = static_cast<std::uint32_t>(data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24));
        pos += 4;
        return v;
    }

    float f32() {
        const std::uint32_t bits = u32();
        float value = 0.f;
        std::memcpy(&value, &bits, sizeof value);
        return value;
    }

    void skip(std::size_t n) {
        if (!need(n)) return;
        pos += n;
    }

    std::string nameAt(std::size_t offset) {
        if (!ok || offset >= size) return {};
        std::size_t end = offset;
        const std::size_t stop = offset + 64 < size ? offset + 64 : size;
        while (end < stop && data[end] != 0) ++end;
        std::string name(reinterpret_cast<const char*>(data + offset), end - offset);
        const std::size_t slash = name.find_last_of("\\/");
        if (slash != std::string::npos) name = name.substr(slash + 1);
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
        return name;
    }
};

int texSets(std::uint32_t fvf) {
    // O mesmo if/else de lwMeshInfo_Load. TEX3 cai no TEX1 porque 0x300 tem o bit 0x100.
    if (fvf & kFvfTex1) return 1;
    if (fvf & kFvfTex2) return 2;
    if (fvf & kFvfTex3) return 3;
    if (fvf & kFvfTex4) return 4;
    return 0;
}

bool readMesh(Cursor& file, std::size_t meshPos, std::size_t meshSize, std::uint32_t fileVersion, ModelGeom& geom) {
    if (meshPos + meshSize > file.size || meshSize < 24) return false;
    Cursor mesh;
    mesh.data = file.data + meshPos;
    mesh.size = meshSize;
    std::uint32_t version = fileVersion;
    if (fileVersion == 0) version = mesh.u32();

    std::uint32_t fvf = 0;
    std::uint32_t pt = 0;
    std::uint32_t vertexNum = 0;
    std::uint32_t indexNum = 0;
    std::uint32_t subsetNum = 0;
    std::uint32_t boneNum = 0;
    bool modern = false;
    if (version >= 0x1004) {
        modern = true;
        fvf = mesh.u32();
        pt = mesh.u32();
        vertexNum = mesh.u32();
        indexNum = mesh.u32();
        subsetNum = mesh.u32();
        boneNum = mesh.u32();
        mesh.u32();
        const std::uint32_t elements = mesh.u32();
        mesh.skip(96);
        if (elements > 64) return false;
        mesh.skip(static_cast<std::size_t>(elements) * 8);
    } else if (version == 0) {
        fvf = mesh.u32();
        pt = mesh.u32();
        vertexNum = mesh.u32();
        indexNum = mesh.u32();
        subsetNum = mesh.u32();
        boneNum = mesh.u32();
        mesh.skip(128);
    } else if (version == 1 || (version >= 0x1000 && version < 0x1004)) {
        fvf = mesh.u32();
        pt = mesh.u32();
        vertexNum = mesh.u32();
        indexNum = mesh.u32();
        subsetNum = mesh.u32();
        boneNum = mesh.u32();
        mesh.skip(96);
    } else {
        return false;
    }
    if (!mesh.ok || pt != 4 || vertexNum == 0 || vertexNum > 200000 || indexNum > 600000 || subsetNum > 64) return false;

    auto readSubsets = [&]() {
        geom.subsets.resize(subsetNum);
        for (std::uint32_t i = 0; i < subsetNum; ++i) {
            const std::uint32_t prim = mesh.u32();
            geom.subsets[i].startIndex = mesh.u32();
            mesh.u32();
            mesh.u32();
            geom.subsets[i].indexCount = prim * 3;
        }
    };
    if (!modern) readSubsets();

    geom.position.resize(static_cast<std::size_t>(vertexNum) * 3);
    for (std::size_t i = 0; i < geom.position.size(); ++i) geom.position[i] = mesh.f32();
    if (fvf & kFvfNormal) {
        geom.normal.resize(static_cast<std::size_t>(vertexNum) * 3);
        for (std::size_t i = 0; i < geom.normal.size(); ++i) geom.normal[i] = mesh.f32();
    }
    const int sets = texSets(fvf);
    if (sets > 0) {
        geom.uv.resize(static_cast<std::size_t>(vertexNum) * 2);
        for (std::size_t i = 0; i < geom.uv.size(); ++i) geom.uv[i] = mesh.f32();
        if (sets > 1) mesh.skip(static_cast<std::size_t>(sets - 1) * vertexNum * 8);
    }
    if (fvf & kFvfDiffuse) {
        geom.color.resize(static_cast<std::size_t>(vertexNum) * 3);
        for (std::uint32_t i = 0; i < vertexNum; ++i) {
            const std::uint32_t color = mesh.u32();
            geom.color[i * 3] = static_cast<std::uint8_t>((color >> 16) & 255);
            geom.color[i * 3 + 1] = static_cast<std::uint8_t>((color >> 8) & 255);
            geom.color[i * 3 + 2] = static_cast<std::uint8_t>(color & 255);
        }
    }
    if ((modern && boneNum > 0) || (!modern && (fvf & kFvfLastBeta) && boneNum > 0)) {
        geom.blendIndex.resize(static_cast<std::size_t>(vertexNum) * 4);
        geom.blendWeight.resize(static_cast<std::size_t>(vertexNum) * 4);
        for (std::uint32_t v = 0; v < vertexNum; ++v) {
            for (int k = 0; k < 4; ++k) {
                if (!mesh.need(1)) return false;
                geom.blendIndex[static_cast<std::size_t>(v) * 4 + k] = mesh.data[mesh.pos++];
            }
            for (int k = 0; k < 4; ++k) geom.blendWeight[static_cast<std::size_t>(v) * 4 + k] = mesh.f32();
        }
        geom.bonePalette.resize(boneNum);
        if (modern) {
            for (std::uint32_t i = 0; i < boneNum; ++i) geom.bonePalette[i] = mesh.u32();
        } else {
            for (std::uint32_t i = 0; i < boneNum; ++i) {
                if (!mesh.need(1)) return false;
                geom.bonePalette[i] = mesh.data[mesh.pos++];
            }
        }
    }
    if (indexNum > 0) {
        geom.indices.resize(indexNum);
        for (std::uint32_t i = 0; i < indexNum; ++i) geom.indices[i] = mesh.u32();
    }
    if (modern) readSubsets();
    return mesh.ok && mesh.pos == mesh.size;
}

struct MtlView {
    std::string texture;
    int blend = 0;
    float r = 1.f;
    float g = 1.f;
    float b = 1.f;
};

void readDiffuse(Cursor& file, std::size_t offset, MtlView& mtl) {
    if (offset + 12 > file.size) return;
    std::memcpy(&mtl.r, file.data + offset, 4);
    std::memcpy(&mtl.g, file.data + offset + 4, 4);
    std::memcpy(&mtl.b, file.data + offset + 8, 4);
    if (mtl.r + mtl.g + mtl.b < 0.12f) {
        mtl.r = mtl.g = mtl.b = 1.f;
    }
}

bool readMaterials(Cursor& file, std::size_t mtlPos, std::size_t mtlSize, std::uint32_t fileVersion, std::vector<MtlView>& out) {
    if (mtlSize == 0) return true;
    if (mtlPos + mtlSize > file.size || mtlSize > 100000) return false;
    Cursor mtl;
    mtl.data = file.data + mtlPos;
    mtl.size = mtlSize;
    std::uint32_t version = fileVersion;
    if (fileVersion == 0) version = mtl.u32();
    const std::uint32_t count = mtl.u32();
    if (!mtl.ok || count > 64) return false;
    out.resize(count);
    if (fileVersion >= 0x1000 || version >= 0x1000 || version == 2) {
        if (4u + static_cast<std::uint64_t>(count) * kMtlRecord > mtlSize && fileVersion != 0) return false;
        if (fileVersion == 0 && 8u + static_cast<std::uint64_t>(count) * kMtlRecord > mtlSize) return false;
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t rec = mtl.pos;
            mtl.skip(4);
            const std::uint32_t kind = mtl.u32();
            out[i].blend = (kind >= 1 && kind <= 4) ? 1 : 0;
            readDiffuse(file, mtlPos + rec + 8, out[i]);
            out[i].texture = file.nameAt(mtlPos + rec + 216);
            mtl.skip(kMtlRecord - 8);
        }
    } else if (version == 1) {
        if (8u + static_cast<std::uint64_t>(count) * kMtl0001 > mtlSize) return false;
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t rec = mtl.pos;
            mtl.skip(4);
            const std::uint32_t kind = mtl.u32();
            out[i].blend = (kind >= 1 && kind <= 4) ? 1 : 0;
            readDiffuse(file, mtlPos + rec + 8, out[i]);
            out[i].texture = file.nameAt(mtlPos + rec + 248);
            mtl.skip(kMtl0001 - 8);
        }
    } else if (version == 0) {
        if (8u + static_cast<std::uint64_t>(count) * kMtl0000 > mtlSize) return false;
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t rec = mtl.pos;
            readDiffuse(file, mtlPos + rec, out[i]);
            out[i].texture = file.nameAt(mtlPos + rec + 212);
            mtl.skip(kMtl0000);
        }
    } else {
        return false;
    }
    return mtl.ok;
}

bool readItemDummy(Cursor& file, std::size_t helperPos, std::size_t helperSize,
                   std::uint32_t version, float* out,
                   std::unordered_map<int,std::array<float,16>>& dummies) {
    if (helperSize < 8 || helperPos + helperSize > file.size) return false;
    Cursor helper;
    helper.data = file.data + helperPos;
    helper.size = helperSize;
    // Apenas a versao 0 grava outra mascara de versao dentro do bloco.
    if (version == 0) helper.u32();
    const std::uint32_t type = helper.u32();
    if (!helper.ok || (type & kHelperDummy) == 0) return false;
    const std::uint32_t count = helper.u32();
    if (!helper.ok || count == 0 || count > 256) return false;
    const std::size_t recordSize = version >= 0x1001 ? 140u : 68u;
    if (static_cast<std::uint64_t>(count) * recordSize > helper.size - helper.pos) return false;
    std::unordered_map<int,std::array<float,16>> parsed;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t id = helper.u32();
        std::array<float,16> matrix{};
        for (float& value : matrix) value = helper.f32();
        helper.skip(recordSize - 68u);
        if (!helper.ok) return false;
        if(id<=255 && std::all_of(matrix.begin(),matrix.end(),
                                  [](float f){return std::isfinite(f);}))
            parsed[static_cast<int>(id)]=matrix;
    }
    auto grip=parsed.find(0);
    if(grip==parsed.end())return false;
    std::memcpy(out,grip->second.data(),sizeof(float)*16);
    dummies=std::move(parsed);
    return true;
}

bool readMatrixAnimation(Cursor& file, std::size_t animPos, std::size_t animSize,
                         std::uint32_t version, ModelGeom& geom) {
    if (animSize == 0) return true;
    if (animPos + animSize > file.size || animSize > 32 * 1024 * 1024) return false;
    Cursor anim;
    anim.data = file.data + animPos;
    anim.size = animSize;
    if (version == 0) anim.u32();
    const std::uint32_t boneSize = anim.u32();
    const std::uint32_t matrixSize = anim.u32();
    std::uint32_t opacitySize[16] = {};
    std::uint32_t uvSize[16][4] = {};
    std::uint32_t imageSize[16][4] = {};
    // 16 subsets. Opacity passou a existir na versao 0x1005.
    if (version >= 0x1005) {
        for (std::uint32_t& size : opacitySize) size = anim.u32();
    }
    // UV e troca de imagem: 16 subsets x 4 texture stages cada.
    for (auto& row : uvSize) for (std::uint32_t& size : row) size = anim.u32();
    for (auto& row : imageSize) for (std::uint32_t& size : row) size = anim.u32();
    if (!anim.ok || boneSize > anim.size - anim.pos) return false;
    if(boneSize>0) {
        geom.embeddedBoneVersion=version;
        geom.embeddedBoneData.assign(anim.data+anim.pos,anim.data+anim.pos+boneSize);
    }
    anim.skip(boneSize);
    if (matrixSize > anim.size - anim.pos || (matrixSize > 0 && matrixSize < 4)) return false;
    if (matrixSize > 0) {
        const std::size_t matrixEnd = anim.pos + matrixSize;
        const std::uint32_t frames = anim.u32();
        if (!anim.ok || frames == 0 || frames > 10000 || 4u + static_cast<std::uint64_t>(frames) * 48u > matrixSize)
            return false;
        geom.matrixFrames = static_cast<int>(frames);
        geom.matrixAnimation.resize(static_cast<std::size_t>(frames) * 16u);
        for (std::uint32_t frame = 0; frame < frames; ++frame) {
            float* out = geom.matrixAnimation.data() + static_cast<std::size_t>(frame) * 16u;
            const float s[12] = {anim.f32(), anim.f32(), anim.f32(), anim.f32(), anim.f32(), anim.f32(),
                                 anim.f32(), anim.f32(), anim.f32(), anim.f32(), anim.f32(), anim.f32()};
            out[0] = s[0]; out[1] = s[1]; out[2] = s[2]; out[3] = 0.f;
            out[4] = s[3]; out[5] = s[4]; out[6] = s[5]; out[7] = 0.f;
            out[8] = s[6]; out[9] = s[7]; out[10] = s[8]; out[11] = 0.f;
            out[12] = s[9]; out[13] = s[10]; out[14] = s[11]; out[15] = 1.f;
        }
        if (!anim.ok || anim.pos > matrixEnd) return false;
        anim.pos = matrixEnd;
    }

    auto blockCursor = [&](std::uint32_t size, Cursor& block) -> bool {
        if (size == 0) return true;
        if (size > anim.size - anim.pos) return false;
        block.data = anim.data + anim.pos;
        block.size = size;
        block.pos = 0;
        block.ok = true;
        return true;
    };
    // Opacidade: lwAnimDataMtlOpacity grava num seguido de pares DWORD/float.
    for (int subset = 0; subset < 16; ++subset) {
        const std::uint32_t size = opacitySize[subset];
        if (size == 0) continue;
        Cursor block;
        if (!blockCursor(size, block)) return false;
        const std::uint32_t keys = block.u32();
        if (!block.ok || keys > 10000 || 4u + static_cast<std::uint64_t>(keys) * 8u > size) return false;
        if (subset < static_cast<int>(geom.subsets.size())) {
            geom.subsets[static_cast<std::size_t>(subset)].opacityAnimation.resize(static_cast<std::size_t>(keys) * 2u);
            for (std::uint32_t k = 0; k < keys; ++k) {
                geom.subsets[static_cast<std::size_t>(subset)].opacityAnimation[k * 2] = static_cast<float>(block.u32());
                geom.subsets[static_cast<std::size_t>(subset)].opacityAnimation[k * 2 + 1] = block.f32();
            }
        }
        anim.skip(size);
    }
    // UV e imagem mantem a ordem original por subset e stage. O GLSL atual
    // usa o stage 0; os demais continuam sendo consumidos para preservar o
    // alinhamento do bloco seguinte.
    for (int subset = 0; subset < 16; ++subset) {
        for (int stage = 0; stage < 4; ++stage) {
            const std::uint32_t size = uvSize[subset][stage];
            if (size == 0) continue;
            Cursor block;
            if (!blockCursor(size, block)) return false;
            const std::uint32_t uvFrames = block.u32();
            if (!block.ok || uvFrames > 10000 || 4u + static_cast<std::uint64_t>(uvFrames) * 64u > size) return false;
            if (stage == 0 && subset < static_cast<int>(geom.subsets.size())) {
                ModelSubset& material = geom.subsets[static_cast<std::size_t>(subset)];
                material.uvFrames = static_cast<int>(uvFrames);
                material.uvAnimation.resize(static_cast<std::size_t>(uvFrames) * 16u);
                for (std::uint32_t frame = 0; frame < uvFrames; ++frame) {
                    for (int n = 0; n < 16; ++n) material.uvAnimation[static_cast<std::size_t>(frame) * 16u + n] = block.f32();
                }
            }
            anim.skip(size);
        }
    }
    for (int subset = 0; subset < 16; ++subset) {
        for (int stage = 0; stage < 4; ++stage) {
            const std::uint32_t size = imageSize[subset][stage];
            if (size == 0) continue;
            if (size > anim.size - anim.pos) return false;
            Cursor block;
            block.data = anim.data + anim.pos;
            block.size = size;
            block.pos = 0;
            block.ok = true;
            const std::uint32_t count = block.u32();
            if (block.ok && count > 0 && count <= 10000 && count <= (size - 4u) / 4u) {
                const std::size_t recordSize = (size - 4u) / count;
                // lwTexInfo is a 32-bit serialized structure. The file name
                // starts after the ten DWORD header fields and color key.
                constexpr std::size_t fileNameOffset = 44;
                if (stage == 0 && subset < static_cast<int>(geom.subsets.size()) &&
                    recordSize >= fileNameOffset + 1) {
                    ModelSubset& material = geom.subsets[static_cast<std::size_t>(subset)];
                    for (std::uint32_t frame = 0; frame < count; ++frame) {
                        const std::size_t off = 4u + static_cast<std::size_t>(frame) * recordSize + fileNameOffset;
                        if (off >= size) break;
                        const std::size_t maxLen = std::min<std::size_t>(64u, size - off);
                        std::string name(reinterpret_cast<const char*>(anim.data + anim.pos + off), maxLen);
                        const std::size_t nul = name.find('\0');
                        if (nul != std::string::npos) name.resize(nul);
                        if (!name.empty()) material.imageAnimation.push_back(name);
                    }
                }
            }
            anim.skip(size);
        }
    }
    return anim.ok;
}

}  // namespace

bool Model::load(const std::string& path) {
    geoms.clear();
    hasItemDummy = false;
    itemDummies.clear();
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long length = std::ftell(file);
    if (length < 8 || length > 32 * 1024 * 1024) {
        std::fclose(file);
        return false;
    }
    std::fseek(file, 0, SEEK_SET);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) {
        std::fclose(file);
        return false;
    }
    std::fclose(file);

    Cursor cursor;
    cursor.data = bytes.data();
    cursor.size = bytes.size();
    const std::uint32_t version = cursor.u32();
    const std::uint32_t objNum = cursor.u32();
    if (!cursor.ok || objNum == 0 || objNum > 64) return false;
    if (version > 0x1005) return false;
    struct Header {
        std::uint32_t type;
        std::uint32_t addr;
        std::uint32_t size;
    };
    std::vector<Header> headers(objNum);
    for (std::uint32_t i = 0; i < objNum; ++i) {
        headers[i].type = cursor.u32();
        headers[i].addr = cursor.u32();
        headers[i].size = cursor.u32();
    }
    if (!cursor.ok) return false;

    bool any = false;
    for (const Header& header : headers) {
        if (header.type != 1) continue;
        ModelGeom geom;
        if (static_cast<std::size_t>(header.addr) + header.size > bytes.size()) {
            geoms.push_back(std::move(geom));
            continue;
        }
        std::size_t base = header.addr;
        if (version == 0) base += 4;
        if (base + kGeomHeader > bytes.size()) {
            geoms.push_back(std::move(geom));
            continue;
        }
        geom.parent = static_cast<std::uint32_t>(bytes[base + 4] | (bytes[base + 5] << 8) | (bytes[base + 6] << 16) | (bytes[base + 7] << 24));
        std::memcpy(geom.local, bytes.data() + base + 12, sizeof geom.local);
        const std::uint32_t mtlSize = static_cast<std::uint32_t>(bytes[base + 100] | (bytes[base + 101] << 8) | (bytes[base + 102] << 16) | (bytes[base + 103] << 24));
        const std::uint32_t meshSize = static_cast<std::uint32_t>(bytes[base + 104] | (bytes[base + 105] << 8) | (bytes[base + 106] << 16) | (bytes[base + 107] << 24));
        const std::uint32_t helperSize = static_cast<std::uint32_t>(bytes[base + 108] | (bytes[base + 109] << 8) | (bytes[base + 110] << 16) | (bytes[base + 111] << 24));
        const std::uint32_t animSize = static_cast<std::uint32_t>(bytes[base + 112] | (bytes[base + 113] << 8) | (bytes[base + 114] << 16) | (bytes[base + 115] << 24));
        const std::size_t body = base + kGeomHeader;
        std::vector<MtlView> materials;
        const bool fits = body + static_cast<std::size_t>(mtlSize) + meshSize + helperSize + animSize <= bytes.size();
        const bool mtlOk = fits && (mtlSize == 0 || readMaterials(cursor, body, mtlSize, version, materials));
        const bool meshOk = mtlOk && meshSize > 0 && readMesh(cursor, body + mtlSize, meshSize, version, geom);
        const bool animOk = meshOk && (animSize == 0 || readMatrixAnimation(
            cursor, body + static_cast<std::size_t>(mtlSize) + meshSize + helperSize, animSize, version, geom));
        if (meshOk && animOk) {
            any = true;
            for (std::size_t i = 0; i < geom.subsets.size(); ++i) {
                if (i >= materials.size()) continue;
                geom.subsets[i].texture = materials[i].texture;
                geom.subsets[i].blend = materials[i].blend;
                geom.subsets[i].r = materials[i].r;
                geom.subsets[i].g = materials[i].g;
                geom.subsets[i].b = materials[i].b;
            }
        } else {
            geom.subsets.clear();
            geom.indices.clear();
        }
        geoms.push_back(std::move(geom));
    }
    if (!any) geoms.clear();
    return any;
}

bool Model::loadGeom(const std::string& path) {
    geoms.clear();
    hasItemDummy = false;
    itemDummies.clear();
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long length = std::ftell(file);
    if (length < 120 || length > 32 * 1024 * 1024) {
        std::fclose(file);
        return false;
    }
    std::fseek(file, 0, SEEK_SET);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) {
        std::fclose(file);
        return false;
    }
    std::fclose(file);

    Cursor cursor;
    cursor.data = bytes.data();
    cursor.size = bytes.size();
    const std::uint32_t version = cursor.u32();
    if (!cursor.ok || version > 0x1005) return false;
    // lwGeomObjInfo::Load le o header logo depois da versao. O dword extra
    // do arquivo 0 fica dentro da malha, nao antes deste header.
    const std::size_t base = 4;
    if (base + kGeomHeader > bytes.size()) return false;
    ModelGeom geom;
    geom.parent = static_cast<std::uint32_t>(bytes[base + 4] | (bytes[base + 5] << 8) | (bytes[base + 6] << 16) | (bytes[base + 7] << 24));
    std::memcpy(geom.local, bytes.data() + base + 12, sizeof geom.local);
    const std::uint32_t mtlSize = static_cast<std::uint32_t>(bytes[base + 100] | (bytes[base + 101] << 8) | (bytes[base + 102] << 16) | (bytes[base + 103] << 24));
    const std::uint32_t meshSize = static_cast<std::uint32_t>(bytes[base + 104] | (bytes[base + 105] << 8) | (bytes[base + 106] << 16) | (bytes[base + 107] << 24));
    const std::uint32_t helperSize = static_cast<std::uint32_t>(bytes[base + 108] | (bytes[base + 109] << 8) | (bytes[base + 110] << 16) | (bytes[base + 111] << 24));
    const std::uint32_t animSize = static_cast<std::uint32_t>(bytes[base + 112] | (bytes[base + 113] << 8) | (bytes[base + 114] << 16) | (bytes[base + 115] << 24));
    const std::size_t body = base + kGeomHeader;
    if (body + static_cast<std::size_t>(mtlSize) + meshSize + helperSize + animSize > bytes.size()) return false;
    std::vector<MtlView> materials;
    const bool mtlOk = mtlSize == 0 || readMaterials(cursor, body, mtlSize, version, materials);
    const bool meshOk = mtlOk && meshSize > 0 && readMesh(cursor, body + mtlSize, meshSize, version, geom);
    if (!meshOk) return false;
    if (helperSize > 0) {
        hasItemDummy = readItemDummy(cursor, body + static_cast<std::size_t>(mtlSize) + meshSize,
                                     helperSize, version, itemDummy, itemDummies);
    }
    if (animSize > 0 && !readMatrixAnimation(cursor, body + static_cast<std::size_t>(mtlSize) + meshSize + helperSize,
                                              animSize, version, geom)) return false;
    for (std::size_t i = 0; i < geom.subsets.size(); ++i) {
        if (i >= materials.size()) continue;
        geom.subsets[i].texture = materials[i].texture;
        geom.subsets[i].blend = materials[i].blend;
        geom.subsets[i].r = materials[i].r;
        geom.subsets[i].g = materials[i].g;
        geom.subsets[i].b = materials[i].b;
    }
    geoms.push_back(std::move(geom));
    return true;
}

}  // namespace top
