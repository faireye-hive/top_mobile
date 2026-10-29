#pragma once

// Regras puras de combate do cliente offline. Sem acesso a Play, UI ou GL,
// para que a logica historica seja testavel e reutilizavel por outros modos.
namespace top { namespace play_combat {

int applyAttackCoefficient(int baseAttack, int coefficient);
int sampleAttack(int minAttack, int maxAttack, int level, int rollValue);
int attackDamageLevelScale(int damage, int attackerLevel, int levelDifference);
int lightningBoltDamage(int skillLevel, int sta, int attackerLevel);
int healSkillAmount(int skillLevel, int spirit);
float berserkIntervalMultiplier(int skillLevel);
int playerDefenseWithStates(int baseDefense, int hardenDefense, int genericDefense, int willOfSteelDefense);
int conchRayDamage(int skillLevel, int sta);
bool isGarner2UnnormalMonster(int id);
int magicAtkDamage(int minAttack, int maxAttack, int attackerSta, int attackerJob, int attackerLevel,
                   int defenderSta, int defenderLevel, int defenderResist, unsigned rollValue);
int garner2SkillMagicDamage(int skillId, int skillLevel, int minAttack, int maxAttack, int attackerSta,
                            int attackerJob, int attackerLevel, int defenderSta, int defenderLevel,
                            int defenderResist, unsigned rollValue);
int rawDamage(int minAttack, int maxAttack, int level, int defense, int roll, int resistance = 0);
int illusionSlashDamage(int baseDamage, int skillLevel, float attackSpeed);
int physicalSkillDamage(int baseDamage, int skillLevel, double scaleBase, double scalePerLevel);
int scaledAttackDamage(int attack, double multiplier, int defense, int resistance);
int monsterPhysicalCallbackDamage(int attack, double multiplier, int defense, int resistance);
int monsterFireDamageWithDistance(int attack, int defense, int resistance, int attackerLevel,
                                  float distanceMeters);
int monsterAttackSpeedCallbackDamage(int attackDamage, int attackSpeed);
int physicalPlusDamage(int attackDamage, int skillLevel, double additive, double multiplierPerLevel);
int mightyStrikeDamage(int attack, int skillLevel, int defense, int resistance);
int primalRageDamage(int attackDamage, int skillLevel);
int backstabDamage(int attack, int skillLevel, int gearMaxAttack, int defense, int resistance,
                   float facingDifferenceDegrees);

} } // namespace top::play_combat
