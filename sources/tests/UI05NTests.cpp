#include "top/PathFinder.h"
#include "top/MonsterAI.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    using top::PathFinder;
    using top::PathPoint;
    constexpr int width = 64, height = 64; // 32 x 32 m
    // Parede na coordenada x=8, com passagem na parte de cima.
    const auto wall = [&](float x, float y) {
        const int cx = static_cast<int>(std::floor(2*x));
        const int cy = static_cast<int>(std::floor(2*y));
        return cx == 16 && cy >= 7 && cy <= 22;
    };
    std::vector<PathPoint> path;
    // O raio 9 m permite desviar para o alto e chegar atras da parede.
    assert(PathFinder::findLocalGrid(width,height,6.25f,7.25f,10.25f,7.25f,
                                     8.25f,7.25f,9.f,wall,path));
    assert(!path.empty());
    assert(path.size() >= 2); // existe curva em torno da parede
    float px = 6.25f, py = 7.25f;
    for (const auto& pt : path) {
        assert(!wall(pt.x,pt.y));
        assert(std::hypot(pt.x-8.25f,pt.y-7.25f) < 9.4f);
        // As arestas comprimidas nao atravessam a parede.
        assert(top::monster_ai::clearSight(px,py,pt.x,pt.y,wall));
        px=pt.x;py=pt.y;
    }
    assert(std::hypot(px-10.25f,py-7.25f)<.5f);
    // O monstro nao pode sair de seu spawn para passar por uma abertura distante.
    assert(!PathFinder::findLocalGrid(width,height,6.25f,7.25f,10.25f,7.25f,
                                      8.25f,7.25f,2.8f,wall,path));
    assert(path.empty());
    // Um bloqueio completo deve falhar, sem fornecer caminho parcial inventado.
    const auto solid = [](float x, float) { return x >= 8.f && x < 8.5f; };
    assert(!PathFinder::findLocalGrid(width,height,6.25f,7.25f,10.25f,7.25f,
                                      8.25f,7.25f,9.f,solid,path));
    assert(path.empty());
    // Sem obstaculo, o retorno ao spawn deve chegar ao destino rapidamente.
    const auto clear = [](float,float) { return false; };
    assert(PathFinder::findLocalGrid(width,height,6.25f,7.25f,10.25f,7.25f,
                                     8.25f,7.25f,9.f,clear,path));
    assert(path.size() == 1);
    // Canto diagonal fechado: nao atravessa os dois vizinhos ortogonais.
    const auto corner = [](float x,float y) {
        const int cx = static_cast<int>(std::floor(x*2));
        const int cy = static_cast<int>(std::floor(y*2));
        return (cx==9 && cy==8) || (cx==8 && cy==9);
    };
    assert(PathFinder::findLocalGrid(width,height,4.25f,4.25f,4.75f,4.75f,
                                     4.5f,4.5f,5.f,corner,path));
    assert(path.size() >= 2);
    // Metas invalidas e distancias exageradas nao criam alocacoes gigantes.
    assert(!PathFinder::findLocalGrid(width,height,-1.f,1.f,2.f,2.f,
                                      0.f,0.f,4.f,clear,path));
    assert(!PathFinder::findLocalGrid(1000,1000,1.f,1.f,35.f,1.f,
                                      1.f,1.f,0.f,clear,path));
    assert(!PathFinder::findLocalGrid(width,height,1.f,1.f,2.f,2.f,
                                      1.f,1.f,-3.f,clear,path));
    std::puts("05-N: local bounded A*, detours, leash, solid walls, diagonal corners OK");
}
