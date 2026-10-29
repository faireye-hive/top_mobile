#include "top/TerrainMesh.h"

#include <unordered_map>

namespace top {
namespace {

// Mesma tabela de MPMap.cpp (AlphaNo2UV). Cada alpha ocupa um quarto do atlas.
constexpr float kAlphaUv[16][2] = {
    {0.00f, 0.00f}, {0.00f, 0.00f}, {0.25f, 0.00f}, {0.50f, 0.00f},
    {0.75f, 0.00f}, {0.00f, 0.25f}, {0.25f, 0.25f}, {0.50f, 0.25f},
    {0.75f, 0.25f}, {0.00f, 0.50f}, {0.25f, 0.50f}, {0.50f, 0.50f},
    {0.75f, 0.50f}, {0.00f, 0.75f}, {0.25f, 0.75f}, {0.50f, 0.75f},
};

constexpr int kCorner[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};

float repeatUv(int tile) {
    int m = tile % 4;
    if (m < 0) m += 4;
    return static_cast<float>(m) * 0.25f;
}

void tintOf(const Tile& tile, float& r, float& g, float& b) {
    r = tile.r / 255.f;
    g = tile.g / 255.f;
    b = tile.b / 255.f;
    if (r + g + b < 0.12f) {
        r = g = b = 1.f;
    }
}

MeshVertex makeVertex(float x, float height, float y, float r, float g, float b, float a, float u, float v, float u2, float v2) {
    MeshVertex vertex{};
    vertex.x = x;
    vertex.y = height;
    vertex.z = y;
    vertex.r = r;
    vertex.g = g;
    vertex.b = b;
    vertex.a = a;
    vertex.u = u;
    vertex.v = v;
    vertex.u2 = u2;
    vertex.v2 = v2;
    return vertex;
}

void emitQuad(std::vector<MeshVertex>& dst, const MeshVertex corner[4]) {
    // MPMap::_FillVB copia os vertices 0,1,2 e depois 3,2,1.
    dst.push_back(corner[0]);
    dst.push_back(corner[1]);
    dst.push_back(corner[2]);
    dst.push_back(corner[3]);
    dst.push_back(corner[2]);
    dst.push_back(corner[1]);
}

}  // namespace

void buildTerrainMesh(MapFile& map, int originX, int originY, int span, TerrainMesh& out) {
    out.batches.clear();
    std::unordered_map<int, std::size_t> baseIndex;
    std::unordered_map<int, std::size_t> overlayIndex;
    TerrainBatch holes;
    holes.mode = 0;
    TerrainBatch water;
    water.mode = 7;
    bool seaNeeded = false;

    auto batchFor = [&](std::unordered_map<int, std::size_t>& index, int mode, int tex) -> TerrainBatch& {
        const auto it = index.find(tex);
        if (it != index.end()) return out.batches[it->second];
        index[tex] = out.batches.size();
        TerrainBatch batch;
        batch.mode = mode;
        batch.texId = tex;
        out.batches.push_back(std::move(batch));
        return out.batches.back();
    };

    for (int y = 0; y < span; ++y) {
        for (int x = 0; x < span; ++x) {
            const int tx = originX + x;
            const int ty = originY + y;
            const Tile tile = map.at(tx, ty);
            // Secoes sem dados sao tiles submarinos validos, como GetTile
            // historico. Somente leitura invalida/fora do mapa fica sem malha.
            if (!tile.valid) continue;
            if (tile.height < -.05f) seaNeeded = true;

            const float u = repeatUv(tx);
            const float v = repeatUv(ty);
            MeshVertex corner[4];
            for (int i = 0; i < 4; ++i) {
                const int cx = tx + kCorner[i][0];
                const int cy = ty + kCorner[i][1];
                const Tile sample = map.at(cx, cy);
                const Tile& colorTile = sample.valid ? sample : tile;
                float r, g, b;
                tintOf(colorTile, r, g, b);
                const float height = sample.valid ? sample.height : tile.height;
                corner[i] = makeVertex(static_cast<float>(cx), height, static_cast<float>(cy), r, g, b, 1.f, u, v, 0.f, 0.f);
                if (i == 1 || i == 3) corner[i].u = u + 0.25f;
                if (i == 2 || i == 3) corner[i].v = v + 0.25f;
            }

            if (tile.layer[0].tex == 0 || tile.layer[0].alpha != 15) {
                for (int i = 0; i < 4; ++i) {
                    corner[i].u = corner[i].v = 0.f;
                }
                emitQuad(holes.verts, corner);
            } else {
                emitQuad(batchFor(baseIndex, 1, tile.layer[0].tex).verts, corner);
                for (int layer = 1; layer < 4; ++layer) {
                    const Layer& texLayer = tile.layer[layer];
                    if (texLayer.tex == 0) break;
                    if (texLayer.alpha == 0 || texLayer.alpha > 15) continue;
                    const float au = kAlphaUv[texLayer.alpha][0];
                    const float av = kAlphaUv[texLayer.alpha][1];
                    constexpr float kInset = 0.01f;
                    MeshVertex blend[4];
                    for (int i = 0; i < 4; ++i) {
                        blend[i] = corner[i];
                        const float tu = (i == 1 || i == 3) ? au + 0.25f - kInset : au + kInset;
                        const float tv = (i == 2 || i == 3) ? av + 0.25f - kInset : av + kInset;
                        blend[i].u = tu;
                        blend[i].v = tv;
                        blend[i].u2 = corner[i].u;
                        blend[i].v2 = corner[i].v;
                    }
                    emitQuad(batchFor(overlayIndex, 2, texLayer.tex).verts, blend);
                }
            }

        }
    }

    // MPMap::RenderSea usa quadras de 4x4 m no nivel 0. Cada vertice fica
    // transparente quando o terreno esta a menos de 50 cm da superficie;
    // assim a agua encontra a praia sem cobrir o chao.
    if (seaNeeded) {
        constexpr int kSeaTile = 4;
        auto aligned = [](int value) {
            int rest = value % kSeaTile;
            if (rest < 0) rest += kSeaTile;
            return value - rest;
        };
        const int seaStartX = aligned(originX);
        const int seaStartY = aligned(originY);
        const int seaEndX = originX + span;
        const int seaEndY = originY + span;
        for (int sy = seaStartY; sy < seaEndY; sy += kSeaTile) {
            for (int sx = seaStartX; sx < seaEndX; sx += kSeaTile) {
                // Mesmo limite por origem de quadra de MPMap::RenderSea.
                if (sx < 0 || sy < 0 || sx >= map.width() || sy >= map.height()) continue;
                MeshVertex sheet[4];
                for (int i = 0; i < 4; ++i) {
                    const int cx = sx + kCorner[i][0] * kSeaTile;
                    const int cy = sy + kCorner[i][1] * kSeaTile;
                    const Tile shore = map.at(cx, cy);
                    // MPMap::GetTile returns _pDefaultTile (-2 m) outside
                    // the map. RenderSea samples it for corner alpha, so the
                    // shoreline should remain opaque at the outer map edge.
                    // A corrupted section is different: keep its safe alpha=0
                    // instead of inventing valid world data.
                    const bool outsideMap = cx < 0 || cy < 0 || cx >= map.width() || cy >= map.height();
                    const bool underwater = shore.valid ? shore.height <= -.5f : outsideMap;
                    const float alpha = underwater ? 207.f / 255.f : 0.f;
                    // MPMap::_dwSeaDefaultColor = ARGB(207, 140, 140, 220).
                    // Cada quadra de 4 m repete um quadro ocean_h inteiro.
                    sheet[i] = makeVertex(static_cast<float>(cx), 0.f, static_cast<float>(cy),
                                          140.f / 255.f, 140.f / 255.f, 220.f / 255.f, alpha,
                                          static_cast<float>(kCorner[i][0]), static_cast<float>(kCorner[i][1]), 0.f, 0.f);
                }
                emitQuad(water.verts, sheet);
            }
        }
    }

    // Camada opaca primeiro, agua e buracos depois, blend por cima.
    // Os batches de textura ja foram inseridos na ordem em que apareceram.
    // Agua e buracos entram depois da base, antes dos overlays: modo 1,
    // depois modo 0/7 e por ultimo modo 2.
    std::vector<TerrainBatch> ordered;
    ordered.reserve(out.batches.size() + 2);
    for (TerrainBatch& batch : out.batches) {
        if (batch.mode == 1 && !batch.verts.empty()) ordered.push_back(std::move(batch));
    }
    if (!holes.verts.empty()) ordered.push_back(std::move(holes));
    if (!water.verts.empty()) ordered.push_back(std::move(water));
    for (TerrainBatch& batch : out.batches) {
        if (batch.mode == 2 && !batch.verts.empty()) ordered.push_back(std::move(batch));
    }
    out.batches = std::move(ordered);
}

}  // namespace top
