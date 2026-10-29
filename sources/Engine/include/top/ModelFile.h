#pragma once

// Leitor de .lmo / .lgo no layout Win32 de lwModelObjInfo::Load
// (sources/Engine/src/lwExpObj.cpp). Ponteiro tem 4 bytes nesse arquivo.
// Nao da para fread a struct de 64 bits.

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>

namespace top {

struct ModelSubset {
    std::uint32_t startIndex = 0;
    std::uint32_t indexCount = 0;
    std::string texture;
    int blend = 0;
    float r = 1.f;
    float g = 1.f;
    float b = 1.f;
    std::vector<float> uvAnimation;
    int uvFrames = 0;
    // Pares key/frame, valor; corresponde a lwKeyFloat.
    std::vector<float> opacityAnimation;
    std::vector<std::string> imageAnimation;
};

struct ModelGeom {
    float local[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::uint32_t parent = 0xffffffffu;
    std::vector<float> position;
    std::vector<float> normal;
    std::vector<float> uv;
    std::vector<std::uint8_t> color;
    // lwBlendInfo: 4 indices de paleta e 4 pesos por vertice. A paleta
    // aponta para o osso do .lab (lwMeshInfo::bone_index_seq).
    std::vector<std::uint8_t> blendIndex;
    std::vector<float> blendWeight;
    std::vector<std::uint32_t> bonePalette;
    std::vector<std::uint32_t> indices;
    std::vector<ModelSubset> subsets;
    // Controlador ANIM_CTRL_TYPE_MAT embutido no bloco anim_data do modelo.
    // Cada quadro e uma lwMatrix43 expandida para a matriz 4x4 do MindPower.
    std::vector<float> matrixAnimation;
    int matrixFrames = 0;
    // Preserve the embedded lwAnimDataBone block. Effect meshes decode it
    // separately; character/world readers keep their existing LAB policy.
    std::uint32_t embeddedBoneVersion = 0;
    std::vector<std::uint8_t> embeddedBoneData;
    int effectBoneCount = 0, effectBoneFrames = 0;
    std::vector<float> effectBoneMatrices; // frame-major inverse-bind * pose
};

struct Model {
    std::vector<ModelGeom> geoms;
    // Dummy 0 do proprio item. O cliente original usa o inverso desta
    // matriz para alinhar o ponto de empunhadura ao dummy da mao.
    float itemDummy[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    bool hasItemDummy = false;
    // SK2.15-A: all authored item dummy transforms (not just grip dummy 0).
    std::unordered_map<int,std::array<float,16>> itemDummies;

    bool load(const std::string& path);
    // .lgo de personagem: um lwGeomObjInfo, nao o envelope lwModelObjInfo.
    bool loadGeom(const std::string& path);
};

}  // namespace top
