#include "top/MonsterAI.h"

#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
    using namespace top::monster_ai;
    // Valores de ai_define.lua: 0 NONE, 1 N_ATK, 2 FLEE, 5 R_ATK,
    // 10 ATK, 11 ATK_FLEE. So AI 11 foge no raio historico de 400 cm.
    assert(shouldRetreat(11, true, 3.99f));
    assert(!shouldRetreat(11, true, 4.00f));
    assert(shouldRetreat(11, true, 4.5f, true));
    assert(!shouldRetreat(11, true, 5.0f, true));
    assert(!shouldRetreat(11, false, 2.f));
    assert(!shouldRetreat(10, true, 2.f));
    assert(shouldRetreat(2, true, 7.f));
    assert(!shouldRetreat(2, false, 2.f));
    assert(!shouldRetreat(11, true, NAN));
    // Memoria curta de aggro para reativos: nao esquecer enquanto o alvo
    // estiver visivel; zona segura e morte interrompem imediatamente.
    assert(!shouldForgetProvocation(1, true, false, true, 1.59f));
    assert(shouldForgetProvocation(1, true, false, true, 1.6f));
    assert(shouldForgetProvocation(5, true, true, true, 0.f));
    assert(shouldForgetProvocation(5, true, false, false, 0.f));
    assert(!shouldForgetProvocation(5, false, true, false, 3.f));
    assert(!shouldForgetProvocation(10, true, false, true, 3.f));
    // AI_NONE executa nenhum ai_loop, mesmo que o movimento base seja > 0.
    assert(!canRoam(0, 5.f));
    assert(!canRoam(1, 0.f));
    assert(canRoam(1, 4.f));
    assert(!canRoam(10, INFINITY));
    assert(std::abs(roamRadius(10.f) - 6.f) < 0.001f);
    assert(std::abs(roamRadius(3.f) - 2.8f) < 0.001f);
    assert(roamRadius(0.1f) == 0.f);
    assert(roamRadius(0.f) == 6.f);
    float x = 12.f, y = 4.f;
    constrainToLeash(4.f, 4.f, 3.f, x, y);
    assert(std::abs(x - 7.f) < .001f && y == 4.f);
    std::puts("05-P: AI 11 retreat, aggro expiry, AI_NONE idle, bounded patrol OK");
}
