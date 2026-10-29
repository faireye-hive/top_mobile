#pragma once

// 05-M: regras locais de aggro/retorno inspiradas no ai.lua historico.
// Estas funcoes sao puras, sem dependencias de renderizacao ou de rede.

#include <algorithm>
#include <cmath>

namespace top { namespace monster_ai {

inline float distance(float ax, float ay, float bx, float by) {
    return std::hypot(ax - bx, ay - by);
}

inline bool outsideLeash(float distanceFromHome, float leash) {
    return leash > 0.f && distanceFromHome > leash;
}

inline bool hostile(int ai, bool provoked) {
    // AI 10+ procura alvo; 1 e 5 revidam; 0 parado e 2 foge.
    return ai >= 10 || ((ai == 1 || ai == 5) && provoked);
}

inline bool canPursue(int ai, bool provoked, bool playerSafe, bool playerAlive,
                      bool returning, bool lineClear, float playerDistance,
                      float vision, float homeDistance, float leash) {
    return hostile(ai, provoked) && playerAlive && !playerSafe && !returning && lineClear &&
           !outsideLeash(homeDistance, leash) && playerDistance <= vision;
}

inline bool canHit(int ai, bool provoked, bool playerSafe, bool playerAlive,
                   bool returning, float playerDistance, float hitReach,
                   float homeDistance, float leash) {
    return hostile(ai, provoked) && playerAlive && !playerSafe && !returning &&
           !outsideLeash(homeDistance, leash) && playerDistance <= hitReach;
}

inline bool shouldGoHome(float homeDistance, float leash, bool returning) {
    // O estado de retorno continua ate perto do ponto de nascimento (histerese).
    return homeDistance > 1.f && (returning || outsideLeash(homeDistance, leash) || homeDistance > 6.f);
}

// 05-P: AI_NONE=0, AI_N_ATK=1, AI_FLEE=2, AI_MOVETOHOST=4,
// AI_R_ATK=5, AI_ATK=10 e AI_ATK_FLEE=11 no ai_define.lua historico.
// AI 11 so tenta fugir quando o alvo esta visivel e a menos de 400 cm.
inline bool shouldRetreat(int ai, bool visibleTarget, float distanceToTarget,
                          bool retreating = false) {
    return visibleTarget && std::isfinite(distanceToTarget) &&
           ((ai == 2 && distanceToTarget > .001f) ||
            (ai == 11 && distanceToTarget < (retreating ? 5.f : 4.f)));
}

// A memoria do ultimo alvo nao e permanente: reativos esquecem a provocacao
// apos perder contato; zona segura/morte limpam aggro imediatamente.
inline bool shouldForgetProvocation(int ai, bool provoked, bool targetSafe,
                                   bool targetAlive, float unseenSeconds) {
    return (ai == 1 || ai == 5) && provoked &&
           (targetSafe || !targetAlive || unseenSeconds >= 1.6f);
}

// AI_NONE e o unico que nao executa ai_loop, como no script do GameServer.
inline bool canRoam(int ai, float speed) {
    return ai != 0 && speed > 0.f && std::isfinite(speed);
}

// birth_rand_move(c,600) move em um raio de 600 cm a partir do nascimento.
// Um monstro com coleira menor nao deve escolher um destino fora dela.
inline float roamRadius(float leash) {
    return leash > 0.f ? std::max(0.f, std::min(6.f, leash - .2f)) : 6.f;
}

// Segmento amostrado a cada 50 cm (a grade historica de colisao). Evita
// agressividade atraves de paredes, mas nao altera a navegacao do jogador.
template <typename Blocked>
bool clearSight(float x0, float y0, float x1, float y1, Blocked&& blocked) {
    const float d = distance(x0, y0, x1, y1);
    if (!std::isfinite(d) || d > 80.f) return false;
    if (d < .5f) return true;
    const int steps = std::max(1, static_cast<int>(std::ceil(d / .5f)));
    for (int i = 1; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (blocked(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t)) return false;
    }
    return true;
}

// Impede deslocamentos de fuga/perseguicao para fora do raio lCDis.
inline void constrainToLeash(float homeX, float homeY, float leash, float& x, float& y) {
    if (leash <= 0.f) return;
    const float dx = x - homeX;
    const float dy = y - homeY;
    const float d = std::hypot(dx, dy);
    if (d > leash && d > .00001f) {
        x = homeX + dx * leash / d;
        y = homeY + dy * leash / d;
    }
}

} } // namespace top::monster_ai
