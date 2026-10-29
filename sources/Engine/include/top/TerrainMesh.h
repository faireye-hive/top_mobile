#pragma once

// Malha do chao com o mesmo agrupamento de MPMap::_FillVB:
// camada 0 opaca, camadas 1..3 com o atlas texture/terrain/alpha/total.tga.
// UV repete a cada 4 tiles (0.25), como em _FillVB.

#include "top/GfxDevice.h"
#include "top/MapFile.h"

#include <vector>

namespace top {

struct TerrainBatch {
    int mode = 0;
    int texId = 0;
    std::vector<MeshVertex> verts;
};

struct TerrainMesh {
    std::vector<TerrainBatch> batches;
};

void buildTerrainMesh(MapFile& map, int originX, int originY, int span, TerrainMesh& out);

}  // namespace top
