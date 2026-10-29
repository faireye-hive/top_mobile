#pragma once

// Lote e anotacao do HUD: contrato de dados entre UI e backend.
// Sem dependencia do estado 3D (ActorSkin) nem do dispositivo GL.
#include "top/VertexTypes.h"
#include <string>
#include <vector>
namespace top {
struct ActorBatch {
    int mode = 3;
    std::string texture;
    std::vector<MeshVertex> verts;
};

struct HudNote {
    std::string text;
    float x = 0.f;
    float y = 0.f;
    float r = 1.f;
    float g = 1.f;
    float b = 1.f;
};

} // namespace top
