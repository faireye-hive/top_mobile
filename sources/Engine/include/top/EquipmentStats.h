#pragma once

// 05-W: atributos estaticos (primeiro valor) de iteminfo.txt.
// Cada celula "min,max" descreve o intervalo de atributos possiveis do item.
// O save offline nao guarda rolagens/forja individuais; usar APENAS o primeiro
// valor evita inventar bonus variaveis e torna o calculo reproduzivel.
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <vector>

namespace top::equipment_stats {

struct Bonus {
    int str = 0, agi = 0, dex = 0, con = 0, sta = 0, luk = 0;
    int minAttack = 0, maxAttack = 0, defense = 0;
    int minAttackCoefficient = 0, maxAttackCoefficient = 0; // iteminfo 39/40, additive radix 1000
    int hp = 0, sp = 0, dodge = 0, hit = 0, critical = 0;
    int hpRegen = 0, spRegen = 0, moveSpeed = 0, resistance = 0;
    int aspdCoefficient = 0; // iteminfo coluna 37: ASPD Bonus (radix 1000)

    Bonus& operator+=(const Bonus& b) {
        auto add = [](int a, int c) {
            return static_cast<int>(std::clamp(static_cast<long long>(a) + c, -1000000LL, 1000000LL));
        };
        str=add(str,b.str); agi=add(agi,b.agi); dex=add(dex,b.dex);
        con=add(con,b.con); sta=add(sta,b.sta); luk=add(luk,b.luk);
        minAttack=add(minAttack,b.minAttack); maxAttack=add(maxAttack,b.maxAttack);
        minAttackCoefficient=add(minAttackCoefficient,b.minAttackCoefficient);
        maxAttackCoefficient=add(maxAttackCoefficient,b.maxAttackCoefficient);
        defense=add(defense,b.defense); hp=add(hp,b.hp); sp=add(sp,b.sp);
        dodge=add(dodge,b.dodge); hit=add(hit,b.hit); critical=add(critical,b.critical);
        hpRegen=add(hpRegen,b.hpRegen); spRegen=add(spRegen,b.spRegen);
        moveSpeed=add(moveSpeed,b.moveSpeed); resistance=add(resistance,b.resistance);
        aspdCoefficient=add(aspdCoefficient,b.aspdCoefficient);
        return *this;
    }
};

inline int firstValue(const std::string& field) {
    if (field.empty()) return 0;
    const char* ptr = field.c_str();
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(ptr, &end, 10);
    if (ptr == end || errno == ERANGE || (*end != ',' && *end != '\0' && *end != '\r')) return 0;
    if (v < -1000000L || v > 1000000L) return 0;
    return static_cast<int>(v);
}

inline int secondValue(const std::string& field) {
    const auto comma = field.find(',');
    return comma == std::string::npos ? firstValue(field) : firstValue(field.substr(comma + 1));
}

inline Bonus fromColumns(const std::vector<std::string>& cols) {
    auto n = [&](std::size_t index) { return index < cols.size() ? firstValue(cols[index]) : 0; };
    Bonus b;
    // iteminfo.txt: 52..56 atributos primarios e 57 Lukconstant bonus;
    // 60..73 atributos diretos. Colunas 29..51 sao *coeficientes*, NAO somas.
    b.str=n(52); b.agi=n(53); b.dex=n(54); b.con=n(55); b.sta=n(56); b.luk=n(57);
    b.minAttack=n(60); b.maxAttack=n(61); b.defense=n(62);
    b.minAttackCoefficient=n(39); b.maxAttackCoefficient=n(40);
    b.hp=n(63); b.sp=n(64); b.dodge=n(65); b.hit=n(66);
    b.critical=n(67); b.hpRegen=n(69); b.spRegen=n(70);
    b.moveSpeed=n(71); b.resistance=n(73);
    b.aspdCoefficient=n(37);
    return b;
}

inline Bonus maximumFromColumns(const std::vector<std::string>& cols) {
    auto n = [&](std::size_t index) { return index < cols.size() ? secondValue(cols[index]) : 0; };
    Bonus b;
    b.str=n(52); b.agi=n(53); b.dex=n(54); b.con=n(55); b.sta=n(56); b.luk=n(57);
    b.minAttack=n(60); b.maxAttack=n(61); b.defense=n(62);
    b.minAttackCoefficient=n(39); b.maxAttackCoefficient=n(40);
    b.hp=n(63); b.sp=n(64); b.dodge=n(65); b.hit=n(66);
    b.critical=n(67); b.hpRegen=n(69); b.spRegen=n(70);
    b.moveSpeed=n(71); b.resistance=n(73);
    b.aspdCoefficient=n(37);
    return b;
}

}  // namespace top::equipment_stats
