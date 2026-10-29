#include "top/MonsterAI.h"

#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
    using namespace top::monster_ai;
    assert(hostile(10, false));
    assert(!hostile(1, false) && hostile(1, true));
    assert(!hostile(2, true) && !hostile(0, true));
    assert(canPursue(10, false, false, true, false, true, 4.f, 9.f, 2.f, 6.f));
    assert(!canPursue(10, false, true, true, false, true, 4.f, 9.f, 2.f, 6.f));
    assert(!canPursue(10, false, false, true, true, true, 4.f, 9.f, 2.f, 6.f));
    assert(!canPursue(10, false, false, true, false, false, 4.f, 9.f, 2.f, 6.f));
    assert(!canPursue(10, false, false, true, false, true, 4.f, 9.f, 7.f, 6.f));
    assert(!canPursue(1, false, false, true, false, true, 4.f, 9.f, 2.f, 6.f));
    assert(canPursue(1, true, false, true, false, true, 4.f, 9.f, 2.f, 6.f));
    assert(canHit(10, false, false, true, false, 1.5f, 1.8f, 3.f, 6.f));
    assert(!canHit(10, false, false, true, false, 4.f, 1.8f, 3.f, 6.f));
    assert(!canHit(10, false, true, true, false, 1.f, 1.8f, 3.f, 6.f));
    assert(!canHit(10, false, false, true, false, 1.f, 1.8f, 7.f, 6.f));
    assert(shouldGoHome(8.f, 6.f, false));
    assert(shouldGoHome(2.f, 6.f, true));
    assert(!shouldGoHome(.5f, 6.f, true));
    float x = 8.f, y = 0.f;
    constrainToLeash(0.f, 0.f, 6.f, x, y);
    assert(std::abs(x - 6.f) < .001f && y == 0.f);
    x = 3.f; y = 4.f;
    constrainToLeash(0.f, 0.f, 6.f, x, y);
    assert(x == 3.f && y == 4.f);
    // Uma parede cruza o segmento entre o inimigo e o jogador.
    const auto wall = [](float a, float) { return a >= 2.f && a <= 2.7f; };
    assert(!clearSight(0.f, 0.f, 5.f, 0.f, wall));
    assert(clearSight(0.f, 0.f, 1.5f, 0.f, wall));
    assert(clearSight(0.f, 0.f, 5.f, 0.f, [](float, float) { return false; }));
    assert(!clearSight(0.f, 0.f, 90.f, 0.f, [](float, float) { return false; }));
    std::puts("05-M: aggro, range, line of sight, flee leash and return passed");
}
