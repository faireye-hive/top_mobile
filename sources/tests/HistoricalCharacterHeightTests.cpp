#include "top/HistoricalCharacterHeight.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    using top::historical_character_height::baseHeight;
    assert(std::fabs(baseHeight(1, 75, -8.f) - .75f) < 1e-6f);
    assert(std::fabs(baseHeight(1, -25, -8.f) + .25f) < 1e-6f);
    assert(std::fabs(baseHeight(0, 75, -8.f) + 8.f) < 1e-6f);
    assert(std::fabs(baseHeight(2, 75, 1.5f) - 1.5f) < 1e-6f);
    assert(std::fabs(baseHeight(1, 0, -7.f)) < 1e-6f);
    std::puts("sea character height DX9 territory rules OK");
}
