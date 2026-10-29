#include "top/PathFinder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace top {
namespace {

constexpr float kCell = .5f;

bool passable(MapFile& map, int x, int y, bool sailing,
              const PathFinder::BlockedAt& additionalBlocked = {}, bool allowBlockedStart = false) {
    const float px = (static_cast<float>(x) + .5f) * kCell;
    const float py = (static_cast<float>(y) + .5f) * kCell;
    if (sailing ? !map.sailableAt(px, py) : map.blockedAt(px, py)) return false;
    return allowBlockedStart || !additionalBlocked || !additionalBlocked(px, py);
}

int octile(int dx, int dy) {
    dx = std::abs(dx);
    dy = std::abs(dy);
    return 10 * std::max(dx, dy) + 4 * std::min(dx, dy);
}

}  // namespace

bool PathFinder::find(MapFile& map, float startX, float startY, float goalX, float goalY,
                      bool sailing, std::vector<PathPoint>& out, const BlockedAt& additionalBlocked) {
    out.clear();
    if (!map.ok()) return false;
    const int sx = static_cast<int>(std::floor(startX / kCell));
    const int sy = static_cast<int>(std::floor(startY / kCell));
    int gx = static_cast<int>(std::floor(goalX / kCell));
    int gy = static_cast<int>(std::floor(goalY / kCell));
    bool adjustedGoal = false;
    // Permit the initial cell even if an object overlaps the character: this
    // allows a player who is nudged into a collider to walk back out of it.
    if (!passable(map, sx, sy, sailing, {}, true)) return false;

    // Um clique pode cair exatamente na metade bloqueada de um tile. Como no
    // FindPath antigo, usa a celula livre mais proxima do destino.
    if (!passable(map, gx, gy, sailing, additionalBlocked)) {
        adjustedGoal = true;
        bool found = false;
        for (int ring = 1; ring <= 5 && !found; ++ring) {
            for (int y = gy - ring; y <= gy + ring && !found; ++y) {
                for (int x = gx - ring; x <= gx + ring; ++x) {
                    if (std::abs(x - gx) != ring && std::abs(y - gy) != ring) continue;
                    if (!passable(map, x, y, sailing, additionalBlocked)) continue;
                    gx = x;
                    gy = y;
                    found = true;
                    break;
                }
            }
        }
        if (!found) return false;
    }
    if (sx == gx && sy == gy) {
        if (adjustedGoal)
            out.push_back({(static_cast<float>(gx) + .5f) * kCell, (static_cast<float>(gy) + .5f) * kCell});
        else
            out.push_back({goalX, goalY});
        return true;
    }

    // Cliques normais ficam dentro da camera. A margem deixa a busca contornar
    // casas e muros sem alocar uma matriz do mapa inteiro (garner tem 4096²).
    constexpr int kMargin = 40;  // 20 metros para cada lado.
    const int minX = std::max(0, std::min(sx, gx) - kMargin);
    const int minY = std::max(0, std::min(sy, gy) - kMargin);
    const int maxX = std::min(map.width() * 2 - 1, std::max(sx, gx) + kMargin);
    const int maxY = std::min(map.height() * 2 - 1, std::max(sy, gy) + kMargin);
    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    if (width <= 0 || height <= 0 || static_cast<std::int64_t>(width) * height > 500000) return false;
    const int count = width * height;
    auto index = [&](int x, int y) { return (y - minY) * width + (x - minX); };
    auto gridX = [&](int i) { return i % width + minX; };
    auto gridY = [&](int i) { return i / width + minY; };

    struct Open {
        int score;
        int node;
        bool operator<(const Open& other) const { return score > other.score; }
    };
    std::priority_queue<Open> open;
    std::vector<int> cost(static_cast<std::size_t>(count), std::numeric_limits<int>::max());
    std::vector<int> parent(static_cast<std::size_t>(count), -1);
    std::vector<std::uint8_t> closed(static_cast<std::size_t>(count), 0);
    const int start = index(sx, sy);
    const int goal = index(gx, gy);
    cost[static_cast<std::size_t>(start)] = 0;
    open.push({octile(gx - sx, gy - sy), start});
    static constexpr int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                       {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    int visits = 0;
    while (!open.empty() && visits < 250000) {
        const int current = open.top().node;
        open.pop();
        if (closed[static_cast<std::size_t>(current)]) continue;
        closed[static_cast<std::size_t>(current)] = 1;
        ++visits;
        if (current == goal) break;
        const int cx = gridX(current);
        const int cy = gridY(current);
        for (const auto& dir : dirs) {
            const int nx = cx + dir[0];
            const int ny = cy + dir[1];
            if (nx < minX || ny < minY || nx > maxX || ny > maxY ||
                !passable(map, nx, ny, sailing, additionalBlocked)) continue;
            // Nao corta diagonalmente o canto de duas celulas bloqueadas.
            if (dir[0] != 0 && dir[1] != 0 &&
                (!passable(map, cx + dir[0], cy, sailing, additionalBlocked) ||
                 !passable(map, cx, cy + dir[1], sailing, additionalBlocked))) continue;
            const int next = index(nx, ny);
            const int nextCost = cost[static_cast<std::size_t>(current)] + (dir[0] && dir[1] ? 14 : 10);
            if (nextCost >= cost[static_cast<std::size_t>(next)]) continue;
            cost[static_cast<std::size_t>(next)] = nextCost;
            parent[static_cast<std::size_t>(next)] = current;
            open.push({nextCost + octile(gx - nx, gy - ny), next});
        }
    }
    if (parent[static_cast<std::size_t>(goal)] < 0) return false;

    std::vector<int> reverse;
    for (int node = goal; node != start && node >= 0; node = parent[static_cast<std::size_t>(node)])
        reverse.push_back(node);
    if (reverse.empty()) return false;
    std::reverse(reverse.begin(), reverse.end());
    int oldDx = 99;
    int oldDy = 99;
    for (std::size_t i = 0; i < reverse.size(); ++i) {
        const int node = reverse[i];
        const int previous = i == 0 ? start : reverse[i - 1];
        const int dx = gridX(node) - gridX(previous);
        const int dy = gridY(node) - gridY(previous);
        const bool turn = i > 0 && (dx != oldDx || dy != oldDy);
        if (turn) {
            const int corner = reverse[i - 1];
            out.push_back({(static_cast<float>(gridX(corner)) + .5f) * kCell,
                           (static_cast<float>(gridY(corner)) + .5f) * kCell});
        }
        oldDx = dx;
        oldDy = dy;
    }
    out.push_back({(static_cast<float>(gx) + .5f) * kCell, (static_cast<float>(gy) + .5f) * kCell});
    return true;
}

// A busca para monstros usa a mesma grade e o mesmo custo octile do jogador,
// mas proibe exploracao distante do spawn e restringe visitas/alocacoes.
// Isso evita alocar a grade 4096x4096 em cada monstro visivel.
bool PathFinder::findLocalGrid(int gridWidth, int gridHeight, float startX, float startY,
                               float goalX, float goalY, float homeX, float homeY,
                               float leash, const BlockedAt& blocked,
                               std::vector<PathPoint>& out) {
    out.clear();
    if (!blocked || gridWidth <= 0 || gridHeight <= 0 ||
        !std::isfinite(startX) || !std::isfinite(startY) ||
        !std::isfinite(goalX) || !std::isfinite(goalY) ||
        !std::isfinite(homeX) || !std::isfinite(homeY) ||
        !std::isfinite(leash) || leash < 0.f) return false;
    const auto cell = [](float v) { return static_cast<int>(std::floor(v / kCell)); };
    const int sx = cell(startX), sy = cell(startY);
    const int gx = cell(goalX), gy = cell(goalY);
    if (sx < 0 || sy < 0 || gx < 0 || gy < 0 ||
        sx >= gridWidth || gx >= gridWidth || sy >= gridHeight || gy >= gridHeight) return false;
    // NPCs recebem apenas rotas locais. Metas muito distantes sao tratadas no
    // proximo tique de IA, nunca com uma busca de centenas de milhares de nos.
    if (std::hypot(startX - goalX, startY - goalY) > 32.f) return false;
    const float maxRadius = leash > 0.f ? leash + .35f : 32.f;
    const auto valid = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) return false;
        const float px = (static_cast<float>(x) + .5f) * kCell;
        const float py = (static_cast<float>(y) + .5f) * kCell;
        // Se o NPC ja nasceu ligeiramente fora do raio, permite apenas a celula
        // de partida: o movimento subsequente o conduz para dentro.
        if (!(x == sx && y == sy) && leash > 0.f &&
            std::hypot(px - homeX, py - homeY) > maxRadius) return false;
        return !blocked(px, py);
    };
    if (!valid(sx, sy) || !valid(gx, gy)) return false;
    if (sx == gx && sy == gy) {
        out.push_back({goalX, goalY});
        return true;
    }
    constexpr int kMargin = 16; // 8 m para cada lado
    const int minX = std::max(0, std::min(sx, gx) - kMargin);
    const int minY = std::max(0, std::min(sy, gy) - kMargin);
    const int maxX = std::min(gridWidth - 1, std::max(sx, gx) + kMargin);
    const int maxY = std::min(gridHeight - 1, std::max(sy, gy) + kMargin);
    const int w = maxX - minX + 1, h = maxY - minY + 1;
    if (w <= 0 || h <= 0 || static_cast<std::int64_t>(w) * h > 12500) return false;
    const int n = w * h;
    const auto idx = [&](int x, int y) { return (y - minY) * w + x - minX; };
    const auto coordX = [&](int i) { return i % w + minX; };
    const auto coordY = [&](int i) { return i / w + minY; };
    struct Open {
        int score;
        int node;
        bool operator<(const Open& o) const { return score > o.score; }
    };
    const int start = idx(sx, sy), goal = idx(gx, gy);
    std::vector<int> cost(static_cast<std::size_t>(n), std::numeric_limits<int>::max());
    std::vector<int> parent(static_cast<std::size_t>(n), -1);
    std::vector<std::uint8_t> done(static_cast<std::size_t>(n), 0);
    std::priority_queue<Open> open;
    cost[static_cast<std::size_t>(start)] = 0;
    open.push({octile(gx - sx, gy - sy), start});
    static constexpr int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},
                                       {1,1},{1,-1},{-1,1},{-1,-1}};
    int visits = 0;
    while (!open.empty() && visits < 5000) {
        const int cur = open.top().node;
        open.pop();
        if (done[static_cast<std::size_t>(cur)]) continue;
        done[static_cast<std::size_t>(cur)] = 1;
        ++visits;
        if (cur == goal) break;
        const int cx = coordX(cur), cy = coordY(cur);
        for (const auto& dir : dirs) {
            const int x = cx + dir[0], y = cy + dir[1];
            if (x < minX || y < minY || x > maxX || y > maxY || !valid(x,y)) continue;
            // Nao atravessa uma quina mesmo se a diagonal estiver livre.
            if (dir[0] && dir[1] &&
                (!valid(cx + dir[0], cy) || !valid(cx, cy + dir[1]))) continue;
            const int next = idx(x,y);
            if (done[static_cast<std::size_t>(next)]) continue;
            const int nc = cost[static_cast<std::size_t>(cur)] + (dir[0] && dir[1] ? 14 : 10);
            if (nc >= cost[static_cast<std::size_t>(next)]) continue;
            cost[static_cast<std::size_t>(next)] = nc;
            parent[static_cast<std::size_t>(next)] = cur;
            open.push({nc + octile(gx - x, gy - y), next});
        }
    }
    if (parent[static_cast<std::size_t>(goal)] < 0) return false;
    std::vector<int> reverse;
    for (int node = goal; node != start && node >= 0; node = parent[static_cast<std::size_t>(node)]) {
        reverse.push_back(node);
        if (reverse.size() > 5500) return false;
    }
    std::reverse(reverse.begin(), reverse.end());
    int oldDx = 99, oldDy = 99;
    for (std::size_t i = 0; i < reverse.size(); ++i) {
        const int node = reverse[i];
        const int prev = i ? reverse[i-1] : start;
        const int dx = coordX(node) - coordX(prev), dy = coordY(node) - coordY(prev);
        if (i && (dx != oldDx || dy != oldDy)) {
            const int turn = reverse[i-1];
            out.push_back({(coordX(turn) + .5f) * kCell, (coordY(turn) + .5f) * kCell});
        }
        oldDx = dx; oldDy = dy;
    }
    // Para o monstro nao encostar numa parede por erro de arredondamento,
    // usa o centro da ultima celula em vez de um destino arbitrario.
    out.push_back({(gx + .5f) * kCell, (gy + .5f) * kCell});
    return true;
}

bool PathFinder::findLocal(MapFile& map, float sx, float sy, float gx, float gy,
                           float hx, float hy, float leash, std::vector<PathPoint>& out) {
    out.clear();
    if (!map.ok()) return false;
    return findLocalGrid(map.width() * 2, map.height() * 2, sx, sy, gx, gy,
                         hx, hy, leash,
                         [&](float x, float y) { return map.blockedAt(x,y); }, out);
}

}  // namespace top
