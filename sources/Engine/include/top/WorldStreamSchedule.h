#pragma once

#include <cmath>

namespace top {

// Independiza a origem da malha estaticamente cacheada das atualizacoes
// frequentes de animacao dos objetos de cena. O terreno e construido apenas
// quando a janela de tiles precisa seguir o jogador ou quando muda o mapa.
class WorldStreamSchedule {
public:
    struct Update { bool terrain = false; bool scene = false; };

    void invalidate() {
        terrainReady_ = false;
        sceneTick_ = -1;
    }

    Update advance(int originX, int originY, int animationTick, bool sceneHasAnimation) {
        constexpr int kMargin = 8;
        const bool terrain = !terrainReady_ ||
            std::abs(originX - terrainX_) > kMargin ||
            std::abs(originY - terrainY_) > kMargin;
        // Os objetos animados sao atualizados a 15 Hz; isso NAO move
        // a ancora do terreno. A cena acompanha o setor quando necessario.
        const bool scene = terrain || (sceneHasAnimation &&
                                      animationTick / 2 != sceneTick_ / 2);
        if (terrain) {
            terrainX_ = originX;
            terrainY_ = originY;
            terrainReady_ = true;
        }
        if (scene) sceneTick_ = animationTick;
        return {terrain, scene};
    }

    bool containsAnchor(int x, int y) const {
        return terrainReady_ && terrainX_ == x && terrainY_ == y;
    }

private:
    int terrainX_ = 0;
    int terrainY_ = 0;
    int sceneTick_ = -1;
    bool terrainReady_ = false;
};

}  // namespace top
