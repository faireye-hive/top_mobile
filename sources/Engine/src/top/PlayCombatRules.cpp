#include "top/PlayCombatRules.h"

#include "top/HistoricalStats.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace top { namespace play_combat {

int sampleAttack(int minAttack, int maxAttack, int level, int rollValue) {
    if (maxAttack < minAttack) std::swap(minAttack, maxAttack);
    if (maxAttack <= 0) {
        minAttack = std::max(1, level);
        maxAttack = minAttack + 1;
    }
    const int span = maxAttack - minAttack;
    return minAttack + (span > 0 ? rollValue % (span + 1) : 0);
}

int attackDamageLevelScale(int damage, int attackerLevel, int levelDifference) {
    const float levelScale = std::clamp(1.f + .025f * static_cast<float>(levelDifference), .8f, 1.2f);
    return std::max(static_cast<int>(std::floor(damage * levelScale)),
                    static_cast<int>(std::floor(attackerLevel * .25f)));
}

int applyAttackCoefficient(int baseAttack, int coefficient) {
    const long long radix = std::clamp(1000LL + static_cast<long long>(coefficient), 0LL, 1000000LL);
    return static_cast<int>(std::clamp(static_cast<long long>(std::floor(
        std::max(0, baseAttack) * static_cast<double>(radix) / 1000.0)), 0LL, 10000000LL));
}

int lightningBoltDamage(int skillLevel, int sta, int attackerLevel) {
    // Skill_Lj_End: floor(80 + sklv*10 + Sta(atker)*6 + Lv(atker)*3).
    return std::max(1, 80 + std::max(1, skillLevel) * 10 + std::max(0, sta) * 6 +
                       std::max(1, attackerLevel) * 3);
}

int healSkillAmount(int skillLevel, int spirit) {
    // Skill_Xzy_End: HP_Endure_Dmg(defer, -floor(50+50*sklv+Sta(atker)*7)).
    return std::max(0, 50 + 50 * std::max(1, skillLevel) + 7 * std::max(0, spirit));
}

float berserkIntervalMultiplier(int skillLevel) {
    // State_Kb_Add raises ASPD; interval scales by reciprocal speed.
    return 1.f / (1.f + .20f + .015f * static_cast<float>(std::max(1, skillLevel)));
}

int playerDefenseWithStates(int baseDefense, int hardenDefense, int genericDefense, int willOfSteelDefense) {
    return std::max(0, baseDefense + std::max(0, hardenDefense) + genericDefense + willOfSteelDefense);
}

int conchRayDamage(int skillLevel, int sta) {
    // Skill_Bkcj_End: floor(200 + sklv*30 + Sta(atker)*6) * 1.1.
    const int base = 200 + std::max(1, skillLevel) * 30 + std::max(0, sta) * 6;
    return std::max(1, static_cast<int>(std::floor(base * 1.1)));
}

bool isGarner2UnnormalMonster(int id) {
    constexpr int ids[] = {1, 2, 3, 4, 728, 729, 730, 731, 739, 740, 742, 743, 744, 745};
    return std::find(std::begin(ids), std::end(ids), id) != std::end(ids);
}

namespace {
double magicRate1(int job) {
    switch (job) {
        case 5: case 6: return 1.5;
        case 13: return 2.0;
        case 14: return 3.0;
        case 16: return 2.0;
        default: return 1.0;
    }
}

double magicRate2(int job) {
    switch (job) {
        case 5: case 6: return .3;
        case 13: case 16: return .35;
        case 14: return .45;
        default: return .4;
    }
}
} // namespace

int magicAtkDamage(int minAttack, int maxAttack, int attackerSta, int attackerJob, int attackerLevel,
                   int defenderSta, int defenderLevel, int defenderResist, unsigned rollValue) {
    if (maxAttack < minAttack) std::swap(minAttack, maxAttack);
    const int sta = std::max(0, attackerSta);
    const double statBonus = sta * magicRate1(attackerJob) +
        magicRate2(attackerJob) * std::pow(std::floor(sta * 4.0 / 20.0), 2.0);
    const int minMagic = static_cast<int>(std::floor(minAttack + statBonus));
    const int maxMagic = static_cast<int>(std::floor(maxAttack + statBonus));
    const unsigned span = static_cast<unsigned>(std::max(0, maxMagic - minMagic));
    const int attack = minMagic + static_cast<int>(span ? rollValue % (span + 1u) : 0u);
    const double resist = std::min(.85, defenderResist / 100.0);
    const int raw = static_cast<int>(std::floor((attack - std::max(0, defenderSta) * 2.0) * (1.0 - resist)));
    const double levelEffect = std::clamp(1.0 + .025 * (attackerLevel - defenderLevel), .5, 1.5);
    const int minimum = static_cast<int>(std::floor(std::max(1, attackerLevel) * .25)) + 1;
    return std::max(minimum, static_cast<int>(std::floor(levelEffect * raw)));
}

int garner2SkillMagicDamage(int skillId, int skillLevel, int minAttack, int maxAttack, int attackerSta,
                            int attackerJob, int attackerLevel, int defenderSta, int defenderLevel,
                            int defenderResist, unsigned rollValue) {
    const int base = magicAtkDamage(minAttack, maxAttack, attackerSta, attackerJob, attackerLevel,
                                    defenderSta, defenderLevel, defenderResist, rollValue);
    if (skillId == 214) return static_cast<int>(std::floor(base * std::sqrt(std::max(1, skillLevel))));
    if (skillId == 216) return static_cast<int>(std::floor(base + std::max(1, skillLevel) * 200.0));
    return base;
}

namespace {
int rolledAttack(int minAttack, int maxAttack, int level, int roll) {
    if (maxAttack < minAttack) std::swap(minAttack, maxAttack);
    if (maxAttack <= 0) {
        minAttack = std::max(1, level);
        maxAttack = minAttack + 1;
    }
    const int span = maxAttack - minAttack;
    return minAttack + (span > 0 ? (roll % (span + 1)) : 0);
}
} // namespace

int rawDamage(int minAttack, int maxAttack, int level, int defense, int roll, int resistance) {
    // Phy_Dmg abate a defesa integralmente.
    return historical_stats::physicalDamage(rolledAttack(minAttack, maxAttack, level, roll), defense, resistance);
}

int illusionSlashDamage(int baseDamage, int skillLevel, float attackSpeed) {
    const int speedMultiplier = std::min(3, std::max(1, static_cast<int>(std::floor(attackSpeed / 70.f))));
    const float skillMultiplier = 1.5f + 0.1f * static_cast<float>(std::max(1, skillLevel));
    return std::max(1, static_cast<int>(std::floor(baseDamage * skillMultiplier * speedMultiplier)));
}

int physicalSkillDamage(int baseDamage, int skillLevel, double scaleBase, double scalePerLevel) {
    const double scale = scaleBase + scalePerLevel * std::max(1, skillLevel);
    return std::max(1, static_cast<int>(std::floor(static_cast<double>(baseDamage) * scale)));
}

int scaledAttackDamage(int attack, double multiplier, int defense, int resistance) {
    resistance = std::clamp(resistance, 0, 1000);
    const double resistScale = resistance < 70
        ? 1.0 - std::min(.85, resistance / 100.0)
        : 1.0 - std::min(.85, resistance / 150.0);
    return std::max(1, static_cast<int>(std::floor(static_cast<double>(attack) * multiplier * resistScale -
                                                   std::max(0, defense))));
}

int monsterPhysicalCallbackDamage(int attack, double multiplier, int defense, int resistance) {
    return std::max(20, scaledAttackDamage(attack, multiplier, defense, resistance));
}

int monsterFireDamageWithDistance(int attack, int defense, int resistance, int attackerLevel,
                                  float distanceMeters) {
    // Skill_Paodan_End -> Fire_Dmg/Pow_Dmg, then floor(dmg *
    // (1 - min(Dis/100 * 0.1, 1))). World coordinates are meters in the
    // offline client, so the historical distance falloff is 10% per meter.
    const double resist = std::min(.85, std::max(0, resistance) / 100.0);
    const int mitigated = static_cast<int>(std::floor(std::max(0, attack) * (1.0 - resist))) -
                          std::max(0, defense);
    const int minimum = static_cast<int>(std::floor(std::max(1, attackerLevel) * .25)) + 1;
    const int fireDamage = std::max(minimum, mitigated);
    const float falloff = std::min(1.f, std::max(0.f, distanceMeters) * .1f);
    return static_cast<int>(std::floor(fireDamage * (1.f - falloff)));
}

int monsterAttackSpeedCallbackDamage(int attackDamage, int attackSpeed) {
    const int speedTier = std::clamp(attackSpeed / 70, 1, 3);
    return physicalSkillDamage(attackDamage, 1, 2.5 * speedTier, 0.0);
}

int physicalPlusDamage(int attackDamage, int skillLevel, double additive, double multiplierPerLevel) {
    return std::max(1, static_cast<int>(std::floor(additive +
        static_cast<double>(std::max(1, skillLevel)) * multiplierPerLevel * attackDamage)));
}

int mightyStrikeDamage(int attack, int skillLevel, int defense, int resistance) {
    return scaledAttackDamage(attack, 1.2f + .05f * static_cast<float>(std::max(1, skillLevel)),
                              defense, resistance);
}

int primalRageDamage(int attackDamage, int skillLevel) {
    return std::max(1, static_cast<int>(std::floor(attackDamage * 3.5f)) + std::max(1, skillLevel));
}

int backstabDamage(int attack, int skillLevel, int gearMaxAttack, int defense, int resistance,
                   float facingDifferenceDegrees) {
    const float difference = std::fabs(facingDifferenceDegrees);
    if (difference < 90.f || difference > 180.f)
        return static_cast<int>(std::floor(static_cast<double>(std::max(0, gearMaxAttack)) *
                                           std::max(1, skillLevel) * 0.3));
    return scaledAttackDamage(attack, 1.0 + std::max(1, skillLevel) * 0.05, defense, resistance);
}

} } // namespace top::play_combat
