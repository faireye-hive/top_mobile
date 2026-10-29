#pragma once
// 05-U: dados de experiencia e atributos do cliente historico.
// Extrai somente atribuicoes literais DEXP[n] = integer de variable.lua; nao executa Lua.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <string>

namespace top { namespace historical_stats {

class Experience {
public:
    bool load(const std::string& path) {
        values_.fill(-1);
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;
        char line[1024];
        while (std::fgets(line, sizeof line, f)) {
            int level = 0;
            long long val = -1;
            int used = 0;
            if (std::sscanf(line, " DEXP [ %d ] = %lld %n", &level, &val, &used) == 2 &&
                used > 0 && level >= 1 && level <= 100 && val >= 0 && val <= 5000000000LL) {
                const char* extra = line + used;
                while (*extra == ' ' || *extra == '\t' || *extra == '\r') ++extra;
                if (*extra == '\n' || *extra == '\0' || (*extra == '-' && extra[1] == '-'))
                    values_[static_cast<std::size_t>(level)] = static_cast<std::int64_t>(val);
            }
        }
        std::fclose(f);
        if (values_[1] != 0 || values_[2] <= 0) { values_.fill(-1); return false; }
        for (int i = 2; i <= 100; ++i) {
            if (values_[i] <= values_[i - 1]) { values_.fill(-1); return false; }
            const auto delta = values_[i] - values_[i - 1];
            if (delta > std::numeric_limits<int>::max()) { values_.fill(-1); return false; }
        }
        return true;
    }
    bool valid() const { return values_[1] == 0 && values_[2] > 0; }
    // EXP local ao nivel, calculado como diferenca da tabela cumulativa original.
    int needed(int currentLevel) const {
        if (currentLevel < 1) currentLevel = 1;
        if (valid()) {
            if (currentLevel >= 100) return 0;
            return static_cast<int>(values_[currentLevel + 1] - values_[currentLevel]);
        }
        return currentLevel >= 100 ? 0 : 25 * currentLevel; // compatibilidade sem scripts
    }
    std::int64_t cumulative(int level) const {
        if (!valid() || level < 1 || level > 100) return -1;
        return values_[level];
    }
private:
    std::array<std::int64_t, 101> values_{};
};

struct Monster {
    int level = 1;
    int hp = 1;
    int minAttack = 0, maxAttack = 0;
    int defense = 0, hit = 0, dodge = 0, experience = 0;
    int physicalResistance = 0, criticalPercent = 0;
};

// Os indices sao do arquivo characterinfo.txt tabulado do cliente.
inline Monster monsterFromColumns(int level, int hp, int exp, int minAtk, int maxAtk,
                                  int def, int hit, int dodge, int physicalResistance = 0, int criticalPercent = 0) {
    Monster m;
    m.level = std::clamp(level, 1, 1000);
    m.hp = std::clamp(hp, 1, 100000000); // preserva boss HP; nao usar 40 + nivel*8
    m.minAttack = std::clamp(minAtk, 0, 10000000);
    m.maxAttack = std::clamp(maxAtk, m.minAttack, 10000000);
    m.defense = std::clamp(def, 0, 10000000);
    m.hit = std::clamp(hit, 0, 10000000);
    m.dodge = std::clamp(dodge, 0, 10000000);
    m.experience = std::clamp(exp, 0, 100000000);
    m.physicalResistance = std::clamp(physicalResistance, 0, 1000);
    m.criticalPercent = std::clamp(criticalPercent, 0, 100);
    return m;
}

// skilleffect.lua Check_MisorCrt: miss = min(.9, max((DODGE - HIT + 10)/100, 0)).
inline int missPermille(int attackerHit, int defenderDodge) {
    if (attackerHit <= 0 && defenderDodge <= 0) return 0; // fixtures sem ficha
    return std::clamp((defenderDodge - attackerHit + 10) * 10, 0, 900);
}
inline bool misses(int attackerHit, int defenderDodge, int roll0To999) {
    return roll0To999 < missPermille(attackerHit, defenderDodge);
}
// functions.lua: Crt(a) = ATTR_CRT / CRTRADIX, CRTRADIX = 100.
// skilleffect.lua: Check_MisorCrt soma ate +15 pontos percentuais
// quando o nivel de quem ataca supera o nivel de quem defende.
inline int criticalPermille(int criticalPercent, int attackerLevel, int defenderLevel) {
    const int bonus = std::clamp((attackerLevel - defenderLevel) * 30, 0, 150);
    return std::clamp(criticalPercent * 10 + bonus, 0, 1000);
}
inline bool critical(int criticalPercent, int attackerLevel, int defenderLevel, int roll0To999) {
    return roll0To999 < criticalPermille(criticalPercent, attackerLevel, defenderLevel);
}
// skilleffect.lua Phy_Dmg: resiste antes de subtrair defesa (teto 85%).
inline int physicalDamage(int attack, int defense, int physicalResistance = 0) {
    physicalResistance = std::clamp(physicalResistance, 0, 1000);
    const double resist = physicalResistance < 70
        ? std::min(.85, physicalResistance / 100.0)
        : std::min(.85, physicalResistance / 150.0);
    const double scaled = static_cast<double>(std::max(0, attack)) * (1.0 - resist);
    return std::max(1, static_cast<int>(std::floor(scaled - std::max(0, defense))));
}
} } // namespace top::historical_stats
