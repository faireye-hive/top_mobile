#pragma once

#include <array>

namespace top::item_amplifier {

enum class Effect { Experience, DropRate, FairyGrowth };

struct Rule {
    int itemId = 0;
    Effect effect = Effect::Experience;
    int multiplierPercent = 100;
    int durationSeconds = 0;
    int minimumLevel = 0;
    int maximumLevel = 0; // zero means no upper bound
    bool hasLuckyStrikeBonus = false;
};

// PORT_APPROX: these values are transcribed from iteminfo.txt descriptions.
// The package declares callback AmplifierItem but omits its Lua implementation,
// so server state stacking and Lucky Strike probability are not available.
inline constexpr std::array<Rule,18> rules{{
    {48,Effect::Experience,500,120,0,40,false},
    {56,Effect::Experience,1000,120,0,40,false},
    {578,Effect::FairyGrowth,200,900,0,0,false},
    {898,Effect::Experience,200,180,30,0,false},
    {993,Effect::Experience,1000,900,0,49,false},
    {998,Effect::Experience,1000,1800,65,75,false},
    {1006,Effect::DropRate,200,900,0,0,false},
    {1128,Effect::Experience,200,180,0,39,false},
    {3039,Effect::Experience,500,900,0,70,false},
    {3094,Effect::Experience,200,1800,0,0,false},
    {3095,Effect::Experience,250,1800,60,0,true},
    {3096,Effect::DropRate,200,1800,0,0,false},
    {3097,Effect::DropRate,300,1800,60,0,false},
    {3844,Effect::Experience,200,900,0,0,false},
    {3845,Effect::DropRate,200,900,0,0,false},
    {3880,Effect::Experience,250,900,60,0,true},
    {3881,Effect::DropRate,250,900,40,0,false},
    {3882,Effect::DropRate,300,900,60,0,false}
}};

inline const Rule* find(int itemId) {
    for (const Rule& rule : rules) if (rule.itemId == itemId) return &rule;
    return nullptr;
}

} // namespace top::item_amplifier
