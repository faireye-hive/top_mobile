#include "top/WorldStreamSchedule.h"
#include <cassert>
#include <cstdio>

int main() {
    top::WorldStreamSchedule stream;
    auto first = stream.advance(200, 300, 0, false);
    assert(first.terrain && first.scene);
    assert(stream.containsAnchor(200, 300));
    // Atualizacoes de animacao nao podem "mover" o setor de terreno.
    int animatedSceneBuilds = 0;
    for (int tick = 1; tick <= 100; ++tick) {
        const auto u = stream.advance(200, 300, tick, true);
        assert(!u.terrain);
        if (u.scene) ++animatedSceneBuilds;
        assert(stream.containsAnchor(200, 300));
    }
    assert(animatedSceneBuilds >= 45 && animatedSceneBuilds <= 51);

    // Caminhada: a cena anima a 15 Hz, mas o terreno precisa seguir
    // deslocamentos superiores a oito tiles desde a ULTIMA construcao.
    int terrainRebuilds = 0;
    for (int dx = 1; dx <= 37; ++dx) {
        const auto u = stream.advance(200 + dx, 300, 100 + dx * 2, true);
        if (u.terrain) {
            ++terrainRebuilds;
            assert(u.scene);
            assert(stream.containsAnchor(200 + dx, 300));
        }
    }
    assert(terrainRebuilds == 4); // 209,218,227,236
    assert(stream.containsAnchor(236, 300));
    assert(!stream.advance(237, 300, 176, false).terrain);
    assert(stream.advance(245, 300, 177, false).terrain);
    assert(stream.containsAnchor(245, 300));

    // Trocar de mapa invalida o setor mesmo com as mesmas coordenadas.
    stream.invalidate();
    const auto teleported = stream.advance(245, 300, 0, false);
    assert(teleported.terrain && teleported.scene);
    assert(stream.containsAnchor(245, 300));
    assert(!stream.advance(245, 300, 0, false).scene);
    // Setor em Y, nao somente X.
    assert(stream.advance(245, 309, 0, false).terrain);
    std::puts("05-X1: terreno acompanha caminhada independentemente da animacao de cena");
}
