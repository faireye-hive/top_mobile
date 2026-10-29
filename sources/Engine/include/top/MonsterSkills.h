#pragma once

// 05-Q: selecao de skills do characterinfo.txt (Skill ID/Skill Rate),
// combinada com alcance e projeteis do skillinfo.txt. O dano usa os callbacks
// Lua que Play classifica explicitamente; callbacks desconhecidos nao sao
// tratados como se tivessem uma formula historica equivalente.
#include <algorithm>
#include <cmath>
#include <vector>

namespace top { namespace monster_skills {

struct Attack {
    int id = 0;
    int weight = 0;           // characterinfo coluna 45, peso relativo
    float distance = 1.f;     // skillinfo coluna 16, em metros
    float projectileSpeed = 0.f; // skillinfo coluna 60, metros/s
};

inline bool valid(const Attack& a) {
    return a.id > 0 && a.weight > 0 && a.weight <= 10000 &&
           std::isfinite(a.distance) && a.distance >= .1f && a.distance <= 30.f &&
           std::isfinite(a.projectileSpeed) && a.projectileSpeed >= 0.f && a.projectileSpeed <= 300.f;
}

inline float reach(const Attack& a, float attackerRadius, float targetRadius) {
    return std::max(.1f, attackerRadius) + a.distance + std::max(.1f, targetRadius);
}

inline float maxReach(const std::vector<Attack>& attacks, float fallback,
                      float attackerRadius, float targetRadius) {
    float result = fallback;
    for (const auto& a : attacks) if (valid(a)) result = std::max(result, reach(a, attackerRadius, targetRadius));
    return result;
}

// A sorte e sorteada apenas ENTRE skills efetivamente ao alcance. Se nao
// houver nenhuma, o chamador pode tentar o ataque comum da ficha.
inline int choose(const std::vector<Attack>& attacks, float targetDistance,
                  float attackerRadius, float targetRadius, unsigned randomTicket) {
    if (!std::isfinite(targetDistance) || targetDistance < 0.f) return -1;
    unsigned total = 0;
    for (const auto& a : attacks) if (valid(a) &&
          targetDistance <= reach(a, attackerRadius, targetRadius))
        total += static_cast<unsigned>(a.weight);
    if (total == 0) return -1;
    const unsigned ticket = randomTicket % total;
    unsigned sum = 0;
    for (std::size_t i = 0; i < attacks.size(); ++i) {
        const auto& a = attacks[i];
        if (!valid(a) || targetDistance > reach(a, attackerRadius, targetRadius)) continue;
        sum += static_cast<unsigned>(a.weight);
        if (ticket < sum) return static_cast<int>(i);
    }
    return -1;
}

inline float windup(float targetDistance, float projectileSpeed) {
    // 150 ms do golpe base; projetil percorre distancia em m/s, com limite
    // para nao prolongar a animacao indefinidamente.
    if (projectileSpeed <= 0.f || !std::isfinite(projectileSpeed)) return .15f;
    return std::max(.15f, std::min(.65f, targetDistance / projectileSpeed));
}

} } // namespace top::monster_skills
