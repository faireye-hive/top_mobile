#pragma once
// Formula historica de pontos de atributo e skill, usada pelo gameplay.
namespace top::progression {
// BsAttrUpgrade: 1 ponto, 5 quando o nível fecha dezena, +1 a partir do 60.
inline int attrPointAt(int level) {
    if (level < 2) return 0;
    int points = (level % 10 == 0) ? 5 : 1;
    if (level >= 60) ++points;
    return points;
}

inline int skillPointAt(int level) {
    if (level <= 9) return 0;
    if (level >= 65 && level % 5 == 0) return 2;
    return 1;
}

}
