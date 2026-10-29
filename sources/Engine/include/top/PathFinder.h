#pragma once

#include "top/MapFile.h"

#include <vector>
#include <functional>

namespace top {

struct PathPoint {
    float x = 0.f;
    float y = 0.f;
};

// Busca local na mesma grade de 50 cm usada pelos quatro bits de colisao do
// .map. O resultado ja vem reduzido a pontos de curva.
class PathFinder {
public:
    using BlockedAt = std::function<bool(float, float)>;
    static bool find(MapFile& map, float startX, float startY, float goalX, float goalY,
                     bool sailing, std::vector<PathPoint>& out,
                     const BlockedAt& additionalBlocked = {});
    // A* local para NPCs (grade de 50 cm, limites de busca e raio do spawn).
    // O callback e publico para poder testar com mapas artificiais em memoria.
    static bool findLocalGrid(int gridWidth, int gridHeight,
                              float startX, float startY, float goalX, float goalY,
                              float homeX, float homeY, float leash,
                              const BlockedAt& blocked, std::vector<PathPoint>& out);
    static bool findLocal(MapFile& map, float startX, float startY,
                          float goalX, float goalY, float homeX, float homeY,
                          float leash, std::vector<PathPoint>& out);
};

}  // namespace top
