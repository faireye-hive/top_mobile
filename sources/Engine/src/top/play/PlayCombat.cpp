#include "top/Play.h"
#include "top/PlayCombatRules.h"
#include "top/ActionPose.h"
#include "top/MonsterAI.h"
#include "top/PathFinder.h"
#include "top/HistoricalSkillExecution.h"
#include "top/ProgressionPoints.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace top {
using namespace play_combat;
namespace {
std::string lowerSkillText(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
inline constexpr bool historicalItemEffectKeyframesEnabled() { return true; }
std::vector<int> itemEffectCallbackFrames(int actionKeyFrame,
                                           const std::vector<int>& itemKeyFrames) {
    std::vector<int> frames;
    auto add=[&](int frame) {
        if(frame < -2 || frame > 0) return;
        if(std::find(frames.begin(),frames.end(),frame)==frames.end())frames.push_back(frame);
    };
    add(actionKeyFrame);
    for(int frame:itemKeyFrames)add(frame);
    return frames;
}
EffectEvent::Kind castCallbackKind(int frame) {
    return frame==-1?EffectEvent::CastBegin:frame==-2?EffectEvent::CastEnd:
        EffectEvent::CastAction;
}
} // namespace

void Play::castSkill(int slot) {
    if (slot < 0 || slot >= 8 || deadFor_ > 0.f || playerStunFor_ > 0.f ||
        skillCd_[slot] > 0.f) return;
    const auto rule = skillRules_.find(skills_[slot].id);
    if (rule != skillRules_.end()) {
        const auto& skill = rule->second;
        const historical_skill::Descriptor descriptor{skill.type, skill.helpful,
                                                       skill.target, skill.distance};
        if (historical_skill::inputMode(descriptor) == historical_skill::InputMode::SelectEnemy) {
            if (!learnedSkills_.count(skill.id)) {
                say("Voce ainda nao aprendeu " + skill.name + ".");
                return;
            }
            // Selecting an enemy, following it into range and spending SP are
            // separate states. Button press never spends resources.
            requestedSkill_ = slot;
            armedSkill_ = -1;
            pendingBasicStrike_ = false;
            target_ = -1;
            say(skill.name + ": clique no monstro para escolher o alvo.");
            return;
        }
    }
    requestedSkill_ = -1;
    executeSkill(slot);
}

void Play::executeSkill(int slot) {
    if (slot < 0 || slot >= 8 || deadFor_ > 0.f || playerStunFor_ > 0.f || skillCd_[slot] > 0.f) return;
    const SkillDef& skill = skills_[slot];
    if (skill.id <= 0) return; // an unassigned hotkey is silent
    const auto historical = skillRules_.find(skill.id);
    if (!learnedSkills_.count(skill.id)) {
        say("Voce ainda nao aprendeu " + skill.name + ".");
        return;
    }
    if (skill.id == 0) return;
    if (historical != skillRules_.end() && (historical->second.type == 2 ||
        (!historical->second.passiveFormula.empty() && historical->second.effectFormula=="0"))) {
        say(historical->second.name + " e uma habilidade passiva e permanece ativa.");
        return;
    }
    int skillCost = skill.sp;
    float skillCooldown = skill.cooldown;
    if (historical != skillRules_.end()) {
        const int learnedLevel = std::max(1, learnedSkills_[skill.id]);
        skillCost = std::max(0, static_cast<int>(std::floor(historical->second.spBase +
                                                            historical->second.spPerLevel * learnedLevel)));
        skillCooldown = std::max(.05f, static_cast<float>(historical->second.cooldownBaseMs +
                                  historical->second.cooldownPerLevelMs * learnedLevel) / 1000.f);
    }
    if (player_.homeY < static_cast<float>(skillCost)) {
        say("SP baixo para " + skill.name);
        return;
    }
    player_.homeY -= static_cast<float>(skillCost);
    skillCd_[slot] = skillCooldown;
    pendingBasicStrike_ = false;
    armedSkill_ = -1;
    if (historical != skillRules_.end() &&
        (historical->second.spFormulaMissing || historical->second.cooldownFormulaMissing))
        say(skill.name + ": formula ausente no acervo; usando fallback offline documentado.");
    if (skill.id == 99) {
        armedSkill_ = slot;
    } else if (skill.id == 97) {
        // Offline single-player: the historical heal target resolves to self.
        // Xzy uses attacker STA and skill level, not a generic fixed heal.
        const int spirit = std::max(0, bsta_ + gearBonuses().sta);
        const int gain = healSkillAmount(learnedSkills_[skill.id], spirit);
        const int before = player_.hp;
        player_.hp = std::min(player_.maxHp, player_.hp + gain);
        say(skill.name + ": +" + std::to_string(player_.hp - before) + " HP");
    } else if (historical != skillRules_.end() && historical->second.effectFormula == "Skill_Gtyz_End") {
        willOfSteelFor_ = 15.f;
        willOfSteelDefense_ = 3 * std::max(1, learnedSkills_[skill.id]);
        say(skill.name + ": DEF +" + std::to_string(willOfSteelDefense_) + " por 15 s.");
    } else if (historical != skillRules_.end() && historical->second.effectFormula == "Skill_Kb_End") {
        const int learned = std::max(1, learnedSkills_[skill.id]);
        berserkFor_ = 20.f;
        berserkAttackSpeedBonus_ = .20f + .015f * static_cast<float>(learned);
        say(skill.name + ": velocidade de ataque +" + std::to_string(static_cast<int>(std::lround(
            berserkAttackSpeedBonus_ * 100.f))) + "% por 20 s.");
    } else if (skill.id == 84) {
        // Fallback for old profiles that lack skillinfo callback metadata.
        berserkFor_ = 20.f;
        berserkAttackSpeedBonus_ = .215f;
    } else if ((historical != skillRules_.end() && historical->second.effectFormula == "Skill_Shpf_End") ||
               (historical == skillRules_.end() && skill.id == 225)) {
        // Skill_Shpf_End / State_Shpf_Add: 180 s and DEF +(14 + level*4).
        const int level = std::max(1, learnedSkills_[skill.id]);
        hardenFor_ = 180.f;
        hardenDefense_ = 14 + level * 4;
        say(skill.name + ": DEF +" + std::to_string(hardenDefense_) + " por 180 s.");
    } else if (historical != skillRules_.end() && historical->second.helpful != 0) {
        std::string key = historical->second.name + " " + historical->second.effectFormula;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const int learned = std::max(1, learnedSkills_[skill.id]);
        if (key.find("heal") != std::string::npos || key.find("recover") != std::string::npos ||
            key.find("recuper") != std::string::npos || key.find("xzy") != std::string::npos ||
            key.find("hfs") != std::string::npos || key.find("dzy") != std::string::npos) {
            const int gain = 8 + learned * 5 + bsta_ * 2;
            const int before = player_.hp;
            player_.hp = std::min(player_.maxHp, player_.hp + gain);
            say(skill.name + ": +" + std::to_string(player_.hp - before) + " HP");
        } else {
            genericBuffFor_ = 8.f + static_cast<float>(learned * 2);
            const bool defensive = key.find("shield") != std::string::npos || key.find("protect") != std::string::npos ||
                                   key.find("defen") != std::string::npos || key.find("harden") != std::string::npos;
            genericBuffAttack_ = defensive ? 0 : 3 + learned * 2;
            genericBuffDefense_ = defensive ? 5 + learned * 3 : 0;
            say(skill.name + (defensive ? ": defesa aumentada." : ": ataque aumentado."));
        }
    } else {
        armedSkill_ = slot;
    }
    // A acao visual vem de Play Action na tabela historica. Sem o campo,
    // usa-se a primeira animacao de skill, se houver no .lab.
    castingActionBase_ = historical == skillRules_.end() ? 11 :
        choosePoseAction(historical->second.playActions, 0, 11);
    const float nativeSkillAnimation = historical != skillRules_.end()
        ? historical->second.animationDuration : 0.f;
    const int learnedLevel = std::max(1, learnedSkills_.count(skill.id)
        ? learnedSkills_.at(skill.id) : 1);
    const int historicalTimeMs = historical != skillRules_.end() && historical->second.cyclicAnimation &&
        !historical->second.cooldownFormulaMissing
            ? historical->second.animationTimeBaseMs +
                learnedLevel * historical->second.animationTimePerLevelMs
            : 0;
    const float historicalAnimation = historicalTimeMs > 0
        ? static_cast<float>(historicalTimeMs) / 1000.f : 0.f;
    // Offensive skills do not begin their character action until the target is
    // in range. The old CAttackState first moved, then called _UseSkill().
    // Playing this action at button-down made it finish/freeze while chasing.
    castingFor_ = armedSkill_ >= 0 ? 0.f : std::max(.05f,
        historicalAnimation > 0.f ? historicalAnimation :
        nativeSkillAnimation > 0.f ? nativeSkillAnimation : attackDuration_);
    castingPlaybackRate_ = castingFor_ > 0.f && nativeSkillAnimation > 0.f
        ? std::clamp(nativeSkillAnimation / castingFor_, .05f, 10.f) : 1.f;
    // Offensive skills are still only armed here. Their historical cast
    // effect belongs to the attack pose and must not expire while the player
    // is chasing a target into the authored range. Self/buff skills resolve
    // immediately and keep their event at activation time.
    if (armedSkill_ < 0) {
        ++attackSequence_; // repeated casts of the same action must restart
        float nativeImpact = historical != skillRules_.end() &&
            !historical->second.actionImpacts.empty()
                ? historical->second.actionImpacts.front() : 0.f;
        if (nativeSkillAnimation <= 0.f)
            nativeImpact = std::min(attackWindup_, castingFor_ * .75f);
        float effectDelay = nativeImpact / std::max(.05f, castingPlaybackRate_);
        const int keyFrame = historical != skillRules_.end()
            ? historical->second.actionKeyFrame : -3;
        if (keyFrame == -1) effectDelay = 0.f;
        else if (keyFrame == -2) effectDelay = castingFor_;
        else effectDelay = std::clamp(effectDelay, 0.f, castingFor_);
        EffectEvent event{EffectEvent::Cast, hereX_, hereY_, hereX_, hereY_, skill.id};
        event.sourceIsPlayer = true;
        const bool splitItemCallbacks=historicalItemEffectKeyframesEnabled() &&
            historical!=skillRules_.end() && keyFrame>=-2 && keyFrame<=0;
        if(splitItemCallbacks) {
            const auto frames=itemEffectCallbackFrames(keyFrame,
                historical->second.itemEffectKeyFrames);
            for(int callbackFrame:frames) {
                EffectEvent callback=event;callback.kind=castCallbackKind(callbackFrame);
                const float callbackDelay=callbackFrame==-1?0.f:
                    callbackFrame==-2?castingFor_:effectDelay;
                if(callbackDelay<=0.f)effectEvents_.push_back(callback);
                else scheduledEffects_.push_back({callbackDelay,callback,-1,attackSequence_});
            }
        } else if (effectDelay <= 0.f) effectEvents_.push_back(event);
        else scheduledEffects_.push_back({effectDelay, event, -1, attackSequence_});
        // CHitRepresent::ActionExec also runs for self/buff casts. Cast and
        // target/ground/water effects use independent authored callbacks.
        // Without this path Berserk's end-of-action firegush (236), Heal's
        // result and other self effects were never emitted at all.
        if(historical!=skillRules_.end()) {
            const auto& rule=historical->second;
            if(rule.harmMode==1 && rule.flightEffectId==0 && rule.hasHitVisual &&
               rule.hitKeyFrame>=-2 && rule.hitKeyFrame<=0) {
                const float hitDelay=rule.hitKeyFrame==-1?0.f:
                    rule.hitKeyFrame==-2?castingFor_:
                    std::clamp(nativeImpact/std::max(.05f,castingPlaybackRate_),0.f,castingFor_);
                EffectEvent hit{EffectEvent::SkillHit,hereX_,hereY_,hereX_,hereY_,skill.id};
                hit.targetActorIndex=-1;
                if(hitDelay<=0.f)effectEvents_.push_back(hit);
                else scheduledEffects_.push_back({hitDelay,hit,-1,attackSequence_});
            }
        }
    }
    if (slot != 1) say(skill.name);
}

bool Play::fireBoatCannon(float px, float py) {
    if (boatCannonCd_ > 0.f) return false;
    int target = -1;
    float best = 12.f * 12.f;
    for (int i = 0; i < static_cast<int>(foes_.size()); ++i) {
        const Foe& foe = foes_[static_cast<std::size_t>(i)];
        if (foe.hp <= 0 || foe.followsPlayer) continue;
        const float dx = foe.x - px;
        const float dy = foe.y - py;
        const float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; target = i; }
    }
    if (target < 0) {
        say("Nenhum alvo ao alcance do canhao.");
        boatCannonCd_ = .25f;
        return false;
    }
    Foe& foe = foes_[static_cast<std::size_t>(target)];
    const int damage = std::max(1, 18 + player_.level * 3 - foe.def / 3);
    foe.hp = std::max(0, foe.hp - damage);
    foe.provoked = true;
    foe.lostSightFor = 0.f;
    foe.lastSeenX = px; foe.lastSeenY = py;
    boatCannonCd_ = 1.8f;
    pops_.push_back({foe.x, foe.y, 1.1f, 1.f, .55f, .15f, std::to_string(damage)});
    EffectEvent cannonHit{EffectEvent::SkillHit, px, py, foe.x, foe.y, 900};
    cannonHit.targetActorIndex=target;
    effectEvents_.push_back(cannonHit);
    say("Canhao: " + std::to_string(damage) + " de dano.");
    if (foe.hp == 0) {
        foe.deadFor = .01f;
        onKill(foe);
    }
    return true;
}

int Play::skillAtSlot(int slot) const {
    return slot >= 0 && slot < 8 ? skills_[slot].id : 0;
}
std::string Play::skillIcon(int skillId) const {
    const auto rule=skillRules_.find(skillId);
    return rule == skillRules_.end() ? std::string{} : rule->second.icon;
}
bool Play::assignSkillToSlot(int skillId,int slot) {
    if (slot < 0 || slot >= 8) return false;
    const auto rule=skillRules_.find(skillId);
    if (!learnedSkills_.count(skillId) || rule==skillRules_.end()) {
        say("Aprenda a skill antes de coloca-la na barra.");
        return false;
    }
    // Moving the same skill to another slot does not create duplicates or
    // reset an active cooldown (which could otherwise be exploited).
    float previousCd=0.f;
    for (int i=0;i<8;++i) if (skills_[i].id==skillId) {
        previousCd=std::max(previousCd,skillCd_[i]);
        if(i!=slot) { skills_[i]={}; skillCd_[i]=0.f; }
    }
    skills_[slot]={skillId,rule->second.name,rule->second.sp,rule->second.cooldown};
    skillCd_[slot]=previousCd;
    say(rule->second.name+" colocada em F"+std::to_string(slot+1)+".");
    save(savePath());
    return true;
}
void Play::moveSkillSlot(int from,int to) {
    if (from < 0 || from >= 8 || to < 0 || to >= 8 || from == to) return;
    std::swap(skills_[from],skills_[to]);
    std::swap(skillCd_[from],skillCd_[to]);
    save(savePath());
}
void Play::assignSkill(int skillId) {
    // Legacy API used in regression fixtures and programmatic callers only.
    for (int i=0;i<8;++i) if (skills_[i].id==0) {
        assignSkillToSlot(skillId,i);
        return;
    }
    say("Barra cheia; arraste a skill para substituir um slot.");
}

void Play::onKill(const Foe& foe) {
    const int baseGain = foe.expReward >= 0 ? foe.expReward : 6 + foe.level * 4;
    const bool monsterExperienceDisabled = std::any_of(bag_.begin(), bag_.end(), [](const Stack& stack) {
        return stack.id == 9621 && stack.count > 0 && stack.expDisableFlag == 1;
    });
    const int multiplier = experienceBoost_.remaining > 0.f ?
        std::max(100, experienceBoost_.multiplierPercent) : 100;
    const int gain = monsterExperienceDisabled ? 0 : static_cast<int>(std::min<long long>(1000000000LL,
        std::max(0LL,static_cast<long long>(baseGain)) * multiplier / 100LL));
    exp_ = std::min(1000000000, exp_ + gain);
    gold_ += foe.level * 3;
    char line[96];
    std::snprintf(line, sizeof line, "%s caiu. +%d exp", foe.name.c_str(), gain);
    say(line);
    for (Quest& quest : quests_) {
        if (quest.claimed) continue;
        if (foe.id != quest.idA && foe.id != quest.idB) continue;
        if (quest.have < quest.need) ++quest.have;
        if (quest.have >= quest.need) {
            quest.claimed = true;
            exp_ = std::min(1000000000, exp_ + std::max(0, quest.rewardExp));
            gold_ += quest.rewardGold;
            std::snprintf(line, sizeof line, "Missao cumprida: %s  +%d exp  +%d ouro", quest.title, quest.rewardExp,
                          quest.rewardGold);
            say(line);
        }
    }
    // 05-AU: dispatch a real kill event to the mission module first.
    onHistoricalEvent(MissionEventKind::Kill,foe.id);
    // Each active historical hunt receives a kill independently, including
    // missions with the same monster ID. Completion may change the inventory.
    auto recordKill = [&](HistoricalProgress& mission) {
        if (!mission.active || !mission.triggers.empty() || mission.targetId != foe.id || mission.targetCount <= 0) return;
        mission.have = std::min(mission.targetCount, mission.have + 1);
        if (!mission.turnInRequired && mission.have >= mission.targetCount &&
            (mission.targetX < 0.f || mission.targetY < 0.f)) completeHistoricalMission(mission);
    };
    recordKill(historical_);
    for (auto& mission : additionalMissions_) recordKill(mission);
    while (expNext_ > 0 && exp_ >= expNext_ && player_.level < 100) {
        exp_ -= expNext_;
        ++player_.level;
        const int gained = progression::attrPointAt(player_.level);
        ap_ += gained;
        tp_ += progression::skillPointAt(player_.level);
        if (sheetReady_) applyAttrs(false);
        player_.hp = player_.maxHp;
        player_.homeY = player_.homeX;
        expNext_ = experience_.needed(player_.level);
        std::snprintf(line, sizeof line, "Nivel %d. +%d pontos.", player_.level, gained);
        say(line);
    }
    dropLoot(foe);
    save(savePath());
}

void Play::landPlayerAttack(float px, float py) {
    const int index = pendingTarget_;
    const int skillSlot = pendingSkill_;
    pendingTarget_ = -1;
    pendingSkill_ = -1;
    pendingAttack_ = 0.f;
    if (index < 0 || index >= static_cast<int>(foes_.size())) return;
    Foe& foe = foes_[static_cast<std::size_t>(index)];
    if (foe.hp <= 0) return;

    // skilleffect.lua Check_MisorCrt: esquiva durante o impacto.
    // Nao tornar hit/miss permanentes no inicio da animacao.
    if (historical_stats::misses(player_.hit, foe.dodge, roll(1000))) {
        // The airborne slash still reaches its target when damage misses.
        // Never duplicate the impact on a successful strike below.
        if (skillSlot >= 0 && skillSlot < 8 && skills_[skillSlot].id == 81) {
            EffectEvent hit{EffectEvent::SkillHit, px, py, foe.x, foe.y, 81};
            hit.targetActorIndex=index;
            effectEvents_.push_back(hit);
        }
        pops_.push_back({foe.x, foe.y, .75f, .85f, .85f, 1.f, "MISS"});
        return;
    }

    // O dano chega no quadro-chave do pose de ataque, e nao antes da arma
    // comecar a se mover. O pose e o quadro vêm de CharacterAction.tx.
    const int attackRoll = roll(97);
    const int attack = sampleAttack(player_.minAtk + genericBuffAttack_, player_.maxAtk + genericBuffAttack_,
                                    player_.level, attackRoll);
    const int targetDefense = std::max(0, foe.def - foe.defenseDownLevel * 4);
    int dmg = historical_stats::physicalDamage(attack, targetDefense, foe.resist);
    const bool skillHit = skillSlot >= 0 && skillSlot < 8;
    const int committedSkillId = skillHit ? skills_[skillSlot].id : 0;
    float skillRadius = 0.f;
    if (skillHit) {
        const auto rule = skillRules_.find(committedSkillId);
        if (rule != skillRules_.end() && rule->second.radius > 0)
            skillRadius = std::max(.5f, static_cast<float>(rule->second.radius) / 100.f);
        const int levelDiff = player_.level - foe.level;
        const float levelScale = 1.f + static_cast<float>(levelDiff) * 0.025f;
        const int skillLevel = std::max(1, learnedSkills_.count(committedSkillId) ? learnedSkills_.at(committedSkillId) : 1);
        // The original Skill_* formulas distinguish physical and magic skills.
        // Keep that distinction offline instead of applying one fixed number to
        // every skill: Atk_Dmg skills scale with their learned level, while the
        // classic 10 + Sta*2 formula is used by magic skills.
        std::string formula;
        if (rule != skillRules_.end()) {
            formula = lowerSkillText(rule->second.effectFormula + " " + rule->second.positiveFormula);
        }
        if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::Enfeeble) {
            // Skill_Sj_End calls HP_Red_SkillS with a literal 100 + level*10;
            // this callback does not use Atk_Dmg, magic stats, defense, or crit.
            dmg = static_cast<int>(std::floor(rule->second.damageScaleBase +
                                               skillLevel * rule->second.damageScalePerLevel));
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::MightyStrike) {
            // Skill_Zj_End delegates to Atk_Raise: it scales the sampled attack before Phy_Dmg.
            dmg = mightyStrikeDamage(attack, skillLevel, targetDefense, foe.resist);
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::IllusionSlash) {
            // Skill_Hyz_End: (1.5 + 0.1 * level) * clamp(floor(Aspd / 70), 1, 3) * Atk_Dmg.
            // Aspd() is approximately the inverse of the local attack interval.
            const float attackSpeed = 100.f / std::max(.01f, player_.cd);
            const int leveledDamage = attackDamageLevelScale(dmg, player_.level, levelDiff);
            dmg = illusionSlashDamage(leveledDamage, skillLevel, attackSpeed);
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::PrimalRage) {
            // Skill_Swzq_End: floor(Atk_Dmg * 3.5) + learned level.
            const int leveledDamage = attackDamageLevelScale(dmg, player_.level, levelDiff);
            dmg = primalRageDamage(leveledDamage, skillLevel);
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::AttackRaise) {
            // Atk_Raise multiplies the sampled attack before the historical
            // Phy_Dmg resistance/defense step (unlike Atk_Dmg, no level-diff
            // factor is applied by that Lua helper).
            const double multiplier = rule->second.damageScaleBase +
                rule->second.damageScalePerLevel * skillLevel;
            dmg = scaledAttackDamage(attack, multiplier, targetDefense, foe.resist);
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::Backstab) {
            // Skill_Bc_End compares GetObjDire(attacker) and GetObjDire(target).
            // Its enhanced branch reads the gear-only ATTR_ITEMV_MXATK.
            const float difference = (playerFacing_ - foe.yaw) * (180.f / 3.14159265358979323846f);
            dmg = backstabDamage(attack, skillLevel, gearBonuses().maxAttack,
                                 targetDefense, foe.resist, difference);
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::PhysicalPlus) {
            const int leveledDamage = attackDamageLevelScale(dmg, player_.level, levelDiff);
            dmg = physicalPlusDamage(leveledDamage, skillLevel,
                                     rule->second.damageScaleBase, rule->second.damageScalePerLevel);
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::PhysicalLinear) {
            // Exact linear multipliers transcribed from the original Lua
            // callbacks; do not infer coefficients from names or substrings.
            const int leveledDamage = attackDamageLevelScale(dmg, player_.level, levelDiff);
            dmg = physicalSkillDamage(leveledDamage, skillLevel,
                                      rule->second.damageScaleBase, rule->second.damageScalePerLevel);
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::LightningBolt) {
            const auto gear = gearBonuses();
            if (worldMap_ == "garner2" && isGarner2UnnormalMonster(foe.id)) {
                dmg = garner2SkillMagicDamage(committedSkillId, skillLevel, gear.minAttack, gear.maxAttack,
                    bsta_ + gear.sta, job_, player_.level, foe.sta, foe.level, foe.resist,
                    static_cast<unsigned>(roll(100000)));
            } else {
                dmg = lightningBoltDamage(skillLevel, std::max(0, bsta_ + gear.sta), player_.level);
            }
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::ConchRay) {
            const auto gear = gearBonuses();
            if (worldMap_ == "garner2" && isGarner2UnnormalMonster(foe.id)) {
                dmg = garner2SkillMagicDamage(committedSkillId, skillLevel, gear.minAttack, gear.maxAttack,
                    bsta_ + gear.sta, job_, player_.level, foe.sta, foe.level, foe.resist,
                    static_cast<unsigned>(roll(100000)));
            } else {
                dmg = conchRayDamage(skillLevel, std::max(0, bsta_ + gear.sta));
            }
        } else if (rule != skillRules_.end() && rule->second.damageFormula == SkillRule::DamageFormula::StateOnly) {
            dmg = 0;
        } else if (formula.find("atk_dmg") != std::string::npos || formula.find("phy_dmg") != std::string::npos) {
            // Fallback only for literal formulas that actually contain these
            // terms. Most historical skills name a callback instead and are
            // classified by that exact callback in loadSkillRules().
            dmg = std::max(1, static_cast<int>(std::floor(static_cast<float>(dmg) * levelScale)));
        } else {
            const float magicScale = 1.25f * (1.f + 0.25f * static_cast<float>(skillLevel - 1));
            dmg = std::max(1, static_cast<int>(std::floor((10.f + static_cast<float>(bsta_ * 2)) *
                                                          magicScale * levelScale * 1.1f)));
        }
    }
    const auto committedRule = skillHit ? skillRules_.find(committedSkillId) : skillRules_.end();
    const bool fixedCallbackDamage = committedRule != skillRules_.end() &&
        (committedRule->second.damageFormula == SkillRule::DamageFormula::Enfeeble ||
         committedRule->second.damageFormula == SkillRule::DamageFormula::StateOnly);
    const bool crit = !fixedCallbackDamage && historical_stats::critical(player_.crit, player_.level,
                                                     foe.level, roll(1000));
    if (crit) dmg = static_cast<int>(std::min(100000000LL, 2LL * dmg));
    foe.hp -= dmg;
    if (committedRule != skillRules_.end() && committedRule->second.effectFormula == "Skill_Pj_End" &&
        foe.hp > 0) { // STATE_PJ: DEF -4/level for 15s.
        const int learned = std::max(1, learnedSkills_.count(committedSkillId)
                                         ? learnedSkills_.at(committedSkillId) : 1);
        float duration = 15.f;
        if (worldMap_ == "garner2" && isGarner2UnnormalMonster(foe.id))
            duration = std::max(1.f, static_cast<float>(bagi_ + gearBonuses().agi) / 10.f);
        if (learned >= foe.defenseDownLevel) {
            foe.defenseDownLevel = learned;
            foe.defenseDownFor = duration;
        }
        pops_.push_back({foe.x, foe.y, 1.1f, .9f, .5f, .2f,
                         "DEF -" + std::to_string(foe.defenseDownLevel * 4)});
    }
    if (committedSkillId == 86) { // Skill_Guz_End applies STATE_XY after damage.
        const int learned = std::max(1, learnedSkills_.count(committedSkillId)
                                         ? learnedSkills_.at(committedSkillId) : 1);
        float stunSeconds = static_cast<float>(3 + learned / 2);
        const float directionDifference = std::fabs(playerFacing_ - foe.yaw) *
                                          (180.f / 3.14159265358979323846f);
        if (directionDifference < 90.f || directionDifference > 180.f) stunSeconds *= 2.f;
        if (worldMap_ == "garner2" && isGarner2UnnormalMonster(foe.id))
            stunSeconds *= std::max(0, bagi_ + gearBonuses().agi) / 200.f;
        if (foe.maxHp >= 100000) stunSeconds = std::floor(stunSeconds / 2.f) + 1.f;
        foe.stunFor = std::max(foe.stunFor, stunSeconds);
        foe.attackPending = 0.f;
        foe.pendingSkillId = 0;
        foe.pendingReach = 0.f;
    }
    if (committedSkillId == 214) { // Skill_Lj_End applies STATE_MB level 2 for 3 seconds.
        foe.numbFor = 3.f;
        foe.numbLevel = 2;
    }
    if (committedSkillId == 87 && foe.hp > 0) { // Skill_Db_End applies STATE_ZD.
        const int learned = std::max(1, learnedSkills_.count(committedSkillId)
                                         ? learnedSkills_.at(committedSkillId) : 1);
        // STATE_ZD uses AddLarger: equal/lower levels do not replace or refresh it.
        if (learned > foe.poisonLevel) {
            // State_Zd_Add runs immediately on AddState, then skilleff.txt repeats
            // it once per second. Hp_Endure_Dmg bypasses defense and resistance.
            int poisonDamage = 10 + learned * 2;
            if (worldMap_ == "garner2" && isGarner2UnnormalMonster(foe.id))
                poisonDamage = std::max(5, 320);
            foe.hp = std::max(0, foe.hp - poisonDamage);
            foe.poisonFor = static_cast<float>(5 + learned * 4);
            foe.poisonTick = 1.f;
            foe.poisonLevel = learned;
            pops_.push_back({foe.x, foe.y, 1.1f, .5f, .85f, .55f,
                             "POISON " + std::to_string(poisonDamage)});
        }
    }
    if (committedSkillId == 95 && foe.hp > 0) { // Skill_Sj_End: STATE_SJ + STATE_JNJZ.
        const int learned = std::max(1, learnedSkills_.count(committedSkillId)
                                         ? learnedSkills_.at(committedSkillId) : 1);
        float duration = static_cast<float>(5 + learned / 2);
        if (foe.maxHp >= 100000)
            duration = static_cast<float>(std::max(0, static_cast<int>(std::floor(3.f + learned * .3f))));
        // Higher-level applications replace weaker ones; equal levels refresh.
        if (learned >= foe.enfeebleLevel) {
            foe.enfeebleLevel = learned;
            foe.enfeebleFor = std::max(foe.enfeebleFor, duration);
        }
        // A silence interrupts a committed monster skill, but does not cancel
        // an ordinary weapon swing already in progress.
        if (foe.pendingSkillId > 0) {
            foe.pendingSkillId = 0;
            foe.pendingReach = foe.radius + foe.range + player_.radius;
        }
    }
    if (committedSkillId == 216 && worldMap_ == "garner2" && isGarner2UnnormalMonster(foe.id) &&
        roll(200) + 1 >= std::max(0, foe.con)) {
        foe.stunFor = 1.f;
        foe.attackPending = 0.f;
        foe.pendingSkillId = 0;
        foe.pendingReach = 0.f;
    }
    foe.provoked = true;
    foe.lostSightFor = 0.f;
    foe.lastSeenX = px; foe.lastSeenY = py;
    pops_.push_back({foe.x, foe.y, 1.1f, 1.f, .92f, .35f,
                     (crit ? "CRIT " : "") + std::to_string(dmg)});
    EffectEvent impact{skillHit ? EffectEvent::SkillHit : EffectEvent::Hit, px, py, foe.x, foe.y,
                       skillHit ? skills_[skillSlot].id : 0};
    if (skillHit) impact.targetActorIndex=index;
    effectEvents_.push_back(impact);
    if (skillRadius > 0.f) {
        for (std::size_t areaIndex = 0; areaIndex < foes_.size(); ++areaIndex) {
            Foe& area = foes_[areaIndex];
            if (&area == &foe || area.hp <= 0) continue;
            const float areaDx = area.x - foe.x;
            const float areaDy = area.y - foe.y;
            if (areaDx * areaDx + areaDy * areaDy > skillRadius * skillRadius) continue;
            const int areaDamage = std::max(1, dmg * 3 / 4);
            area.hp = std::max(0, area.hp - areaDamage);
            area.provoked = true;
            area.lostSightFor = 0.f;
            area.lastSeenX = px; area.lastSeenY = py;
            pops_.push_back({area.x, area.y, 1.1f, .7f, .85f, 1.f, std::to_string(areaDamage)});
            EffectEvent areaHit{EffectEvent::SkillHit, px, py, area.x, area.y,
                                skillHit ? skills_[skillSlot].id : 0};
            if (skillHit) areaHit.targetActorIndex=static_cast<int>(areaIndex);
            effectEvents_.push_back(areaHit);
            if (area.hp == 0) {
                area.deadFor = .01f;
                onKill(area);
            }
        }
    }
    if (foe.hp <= 0) {
        foe.hp = 0;
        foe.deadFor = .01f;
        onKill(foe);
    }
}

void Play::resolve(float dt, float px, float py, MapFile* map) {
    // General historical execution: move toward the target first, then
    // commit once. Resources, cooldown, animation and projectile timing are
    // still owned by executeSkill()/the existing attack timeline.
    hereX_ = px;
    hereY_ = py;
    hereSafeZone_ = map && (map->regionAt(px, py) & 0x0002) != 0;
    for (Foe& foe : foes_) {
        if (!foe.summoned || foe.summonRemaining <= 0.f) continue;
        foe.summonRemaining = std::max(0.f, foe.summonRemaining - dt);
        if (foe.summonRemaining <= 0.f) {
            foe.hp = 0;
            foe.deadFor = std::max(foe.deadFor, 2.f);
            foe.moving = false;
        }
    }
    // Summons are appended after the region's historical spawn list. Removing
    // only expired tail entries keeps every remaining ActorSkin slot stable.
    while (!foes_.empty() && foes_.back().summoned && foes_.back().summonRemaining <= 0.f) {
        const int last = static_cast<int>(foes_.size()) - 1;
        if (target_ == last) target_ = -1;
        if (pendingTarget_ == last) { pendingTarget_ = -1; pendingAttack_ = 0.f; pendingSkill_ = -1; }
        scheduledEffects_.erase(std::remove_if(scheduledEffects_.begin(), scheduledEffects_.end(),
            [last](const ScheduledEffect& effect) { return effect.owner == last; }), scheduledEffects_.end());
        foes_.pop_back();
    }
    if (requestedSkill_ >= 0 && requestedSkill_ < 8 &&
        target_ >= 0 && target_ < static_cast<int>(foes_.size()) &&
        skillCd_[requestedSkill_] <= 0.f && player_.cdLeft <= 0.f &&
        deadFor_ <= 0.f && playerStunFor_ <= 0.f &&
        (!map || (map->regionAt(px, py) & 0x0002) == 0) &&
        pendingTarget_ < 0 && swing_ <= 0.f && castingFor_ <= 0.f) {
        const auto& foe = foes_[static_cast<std::size_t>(target_)];
        const int slot = requestedSkill_;
        const auto rule = skillRules_.find(skills_[slot].id);
        if (rule != skillRules_.end()) {
            const auto& skill = rule->second;
            const historical_skill::Descriptor descriptor{skill.type, skill.helpful,
                                                           skill.target, skill.distance};
            if (historical_skill::targetStep(descriptor, foe.hp > 0,
                    std::hypot(foe.x - px, foe.y - py), player_.radius, foe.radius) ==
                    historical_skill::TargetStep::Ready) {
                requestedSkill_ = -1; // consume request before execution
                executeSkill(slot); // old combat handles one single cast
            }
        } else {
            requestedSkill_ = -1; // no rule: do not cast an arbitrary melee hit
        }
    }
    if (swing_ > 0.f) {
        swing_ = std::max(0.f, swing_ - dt);
        if (swing_ <= 0.f) {
            swingIsSkill_ = false;
            swingPlaybackRate_ = 1.f;
        }
    }
    if (castingFor_ > 0.f) castingFor_ = std::max(0.f, castingFor_ - dt);
    for (std::size_t i = 0; i < scheduledEffects_.size();) {
        ScheduledEffect& scheduled = scheduledEffects_[i];
        const bool playerEffect = scheduled.owner < 0;
        const Foe* owner = !playerEffect && scheduled.owner < static_cast<int>(foes_.size())
            ? &foes_[static_cast<std::size_t>(scheduled.owner)] : nullptr;
        const bool cancelled = playerEffect
            ? deadFor_ > 0.f || playerStunFor_ > 0.f || scheduled.actionCycle != attackSequence_
            : !owner || owner->hp <= 0 || owner->stunFor > 0.f || owner->retreating || owner->goingHome ||
                scheduled.actionCycle != owner->actionCycle;
        if (cancelled) {
            scheduledEffects_.erase(scheduledEffects_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        scheduled.delay -= dt;
        if (scheduled.delay <= 0.f) {
            effectEvents_.push_back(scheduled.event);
            scheduledEffects_.erase(scheduledEffects_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
    if (boatCannonCd_ > 0.f) boatCannonCd_ = std::max(0.f, boatCannonCd_ - dt);
    for (float& cooldown : skillCd_) if (cooldown > 0.f) cooldown = std::max(0.f, cooldown - dt);
    if (berserkFor_ > 0.f) {
        berserkFor_ = std::max(0.f, berserkFor_ - dt);
        if (berserkFor_ <= 0.f) berserkAttackSpeedBonus_ = 0.f;
    }
    if (hardenFor_ > 0.f) {
        hardenFor_ = std::max(0.f, hardenFor_ - dt);
        if (hardenFor_ <= 0.f) hardenDefense_ = 0;
    }
    if (itemHardenFor_ > 0.f) {
        itemHardenFor_ = std::max(0.f, itemHardenFor_ - dt);
        if (itemHardenFor_ <= 0.f) itemHardenDefense_ = 0;
    }
    if (itemEtherCloverFor_ > 0.f) {
        itemEtherCloverFor_ = std::max(0.f, itemEtherCloverFor_ - dt);
        if (itemEtherCloverFor_ <= 0.f && itemEtherCloverDodge_ > 0) {
            player_.dodge = std::max(0, player_.dodge - itemEtherCloverDodge_);
            itemEtherCloverDodge_ = 0;
        }
    }
    if (willOfSteelFor_ > 0.f) {
        willOfSteelFor_ = std::max(0.f, willOfSteelFor_ - dt);
        if (willOfSteelFor_ <= 0.f) willOfSteelDefense_ = 0;
    }
    if (genericBuffFor_ > 0.f) {
        genericBuffFor_ = std::max(0.f, genericBuffFor_ - dt);
        if (genericBuffFor_ <= 0.f) genericBuffAttack_ = genericBuffDefense_ = 0;
    }
    if (deadFor_ > 0.f) {
        deadFor_ += dt;
        if (deadFor_ > 1.6f) {
            player_.hp = player_.maxHp;
            player_.homeY = player_.homeX;
            deadFor_ = 0.f;
            ++respawnSerial_;
            sitting_ = false;
            say("Voce caiu. O HP voltou.");
            save(savePath());
        }
        return;
    }
    if (playerPoisonFor_ > 0.f) {
        const float active = std::min(dt, playerPoisonFor_);
        playerPoisonFor_ = std::max(0.f, playerPoisonFor_ - dt);
        playerPoisonTick_ -= active;
        while (player_.hp > 0 && playerPoisonTick_ <= 0.f) {
            if (itemBlessedPotionFor_ <= 0.f) {
                player_.hp = std::max(0, player_.hp - 160);
                pops_.push_back({hereX_, hereY_, 1.1f, .5f, .3f, .9f, "POISON 160"});
            }
            playerPoisonTick_ += 1.f;
            if (player_.hp == 0) {
                deadFor_ = .01f;
                playerPoisonFor_ = playerPoisonTick_ = 0.f;
                playerPoisonLevel_ = 0;
                sitting_ = false;
                target_ = -1;
                pendingTarget_ = -1;
                pendingAttack_ = 0.f;
                for (Foe& other : foes_) {
                    other.attackPending = 0.f;
                    other.pendingSkillId = 0;
                    other.pendingReach = 0.f;
                }
                break;
            }
        }
        if (playerPoisonFor_ <= 0.f) {
            playerPoisonLevel_ = 0;
            playerPoisonTick_ = 0.f;
        }
    }
    // O relogio da regeneracao zera a cada tique. Antes ele acumulava com o HP
    // cheio e, no primeiro dano, a vida voltava inteira em poucos quadros.
    regen_ += dt;
    const float regenPeriod = sitting_ ? 0.45f : 2.f;
    if (regen_ >= regenPeriod) {
        regen_ = 0.f;
        if (sitting_) {
            if (player_.hp < player_.maxHp) player_.hp = std::min(player_.maxHp, player_.hp + std::max(1, hrecGain_));
            if (player_.homeY < player_.homeX) {
                player_.homeY = std::min(player_.homeX, player_.homeY + static_cast<float>(std::max(1, srecGain_)));
            }
        } else if (player_.hp < player_.maxHp) {
            ++player_.hp;
        }
    }
    if (player_.cdLeft > 0.f) player_.cdLeft -= dt;
    if (playerStunFor_ > 0.f) playerStunFor_ = std::max(0.f, playerStunFor_ - dt);
    if (playerNumbFor_ > 0.f) {
        playerNumbFor_ = std::max(0.f, playerNumbFor_ - dt);
        if (playerNumbFor_ <= 0.f) playerNumbLevel_ = 0;
    }
    if (pendingTarget_ >= 0) {
        pendingAttack_ -= dt;
        if (pendingAttack_ <= 0.f) landPlayerAttack(px, py);
    }
    constexpr std::uint16_t kNoFight = 0x0002;
    const bool fightArea = !map || (map->regionAt(px, py) & kNoFight) == 0;
    playerFightArea_ = fightArea;
    const bool aimed = target_ >= 0 && target_ < static_cast<int>(foes_.size());
    // Do not start or repeat a melee strike because a foe is highlighted.
    // Only an explicitly clicked basic strike or an armed skill can fire.
    if (fightArea && !sitting_ && playerStunFor_ <= 0.f && aimed &&
        (pendingBasicStrike_ || armedSkill_ >= 0) && player_.cdLeft <= 0.f &&
        swing_ <= 0.f && castingFor_ <= 0.f && pendingTarget_ < 0) {
        Foe& foe = foes_[static_cast<std::size_t>(target_)];
        const float dx = foe.x - px;
        const float dy = foe.y - py;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (foe.hp > 0 && dist <= attackReach(foe)) {
            player_.cdLeft = effectiveAttackInterval();
            // Sincronizar golpe/impacto ao intervalo de ASPD, sem permitir
            // que a animacao longa de 30 FPS limite a frequencia das espadas.
            pendingTarget_ = target_;
            pendingSkill_ = armedSkill_;
            pendingBasicStrike_ = false; // consume the single manual click
            const auto skillRule = pendingSkill_ >= 0 && pendingSkill_ < 8
                ? skillRules_.find(skills_[pendingSkill_].id) : skillRules_.end();
            attackActionBase_ = cycleNormalAttack(attackSequence_);
            if (skillRule != skillRules_.end())
                attackActionBase_ = choosePoseAction(skillRule->second.playActions, attackSequence_, attackActionBase_);
            const std::size_t skillActionIndex = skillRule != skillRules_.end() &&
                !skillRule->second.playActions.empty()
                ? attackSequence_ % skillRule->second.playActions.size() : 0;
            const float authoredDuration = skillRule != skillRules_.end() &&
                skillActionIndex < skillRule->second.actionDurations.size()
                    ? skillRule->second.actionDurations[skillActionIndex]
                    : skillRule != skillRules_.end() ? skillRule->second.animationDuration : 0.f;
            const int skillLevel = skillRule != skillRules_.end()
                ? std::max(1, learnedSkills_[skillRule->first]) : 1;
            const int historicalTimeMs = skillRule != skillRules_.end() && skillRule->second.cyclicAnimation &&
                !skillRule->second.cooldownFormulaMissing
                    ? skillRule->second.animationTimeBaseMs +
                        skillLevel * skillRule->second.animationTimePerLevelMs
                    : 0;
            const float historicalDuration = historicalTimeMs > 0
                ? std::clamp(static_cast<float>(historicalTimeMs) / 1000.f, .05f, 30.f) : 0.f;
            const bool hasSkillPose = pendingSkill_ >= 0 && pendingSkill_ < 8 &&
                skillRule != skillRules_.end() && !skillRule->second.playActions.empty();
            // Keep a skill one-shot even when its authored action is absent
            // from this character's pose bank. In that case ActorSkin uses
            // the equipped attack pose, and we still let that fallback finish.
            const bool hasSkillAnimation = hasSkillPose;
            if (hasSkillAnimation) {
                // STAttack::_UseSkill overrides non-cyclic skill velocity
                // to 30 FPS. Cooldown (e.g. Illusion Slash's 5s) only stretches
                // cyclic actions; applying it here slowed every active skill.
                const float nativeDuration = authoredDuration > 0.f
                    ? authoredDuration : std::max(.08f, attackDuration_);
                swing_ = skillRule->second.cyclicAnimation
                    ? (historicalDuration > 0.f ? historicalDuration : std::max(.08f, player_.cdLeft))
                    : nativeDuration;
                swingPlaybackRate_ = std::clamp(nativeDuration / swing_, .05f, 10.f);
                const float authoredImpact = skillRule != skillRules_.end() &&
                    skillActionIndex < skillRule->second.actionImpacts.size()
                        ? skillRule->second.actionImpacts[skillActionIndex]
                        : skillRule != skillRules_.end() ? skillRule->second.impactOffset : 0.f;
                const float nativeImpact = authoredDuration > 0.f
                    ? authoredImpact : std::min(attackWindup_, nativeDuration * .75f);
                // DirectX reports skill callback IDs from the animation track.
                // Positive IDs are normalized to 0 by CWaitAttackState, but
                // the callback still occurs at the pose's authored keyframe,
                // not at t=0.01s. Convert that pose-local time to wall time
                // after the skill's fire-speed has scaled the clip.
                auto keyFrameDelay = [&](int keyFrame) {
                    if (keyFrame == -1) return 0.f; // ACTION_BEGIN_HIT
                    if (keyFrame == -2) return swing_; // ACTION_END_HIT
                    return std::clamp(nativeImpact / std::max(.05f, swingPlaybackRate_),
                                      0.f, swing_);
                };
                const int actionKeyFrame = skillRule != skillRules_.end()
                    ? skillRule->second.actionKeyFrame : -3;
                const float actionDelay = keyFrameDelay(actionKeyFrame);
                pendingAttack_ = actionDelay;

                // A skill with a travelling effect does not apply its harm at
                // the damage keyframe. The DX9 client starts the flight at its
                // own authored keyframe and attaches harm to the flight delay.
                if (skillRule != skillRules_.end() && skillRule->second.flightEffectId > 0 &&
                    skillRule->second.flightSpeed > 0.f) {
                    const float flightDistance = std::hypot(foe.x - px, foe.y - py);
                    const float flightDuration = flightDistance / skillRule->second.flightSpeed;
                    const int flightKeyFrame = skillRule->second.flightActionKeyFrame != -3
                        ? skillRule->second.flightActionKeyFrame : actionKeyFrame;
                    const float flightDelay = keyFrameDelay(flightKeyFrame);
                    pendingAttack_ = flightDelay + flightDuration;

                    EffectEvent flight{EffectEvent::Flight, px, py, foe.x, foe.y,
                                       skills_[pendingSkill_].id, flightDuration};
                    flight.targetActorIndex=target_;
                    flight.sourceIsPlayer=true;
                    if (flightDelay <= 0.f) effectEvents_.push_back(flight);
                    else scheduledEffects_.push_back({flightDelay, flight, -1, attackSequence_ + 1u});
                }

                // Character/item effects use sActionKeyFrme; projectile
                // effects use sSkyEffectActionKeyFrame (parsed separately).
                EffectEvent cast{EffectEvent::Cast, px, py, px, py,
                                 skills_[pendingSkill_].id};
                cast.sourceIsPlayer = true;
                const bool splitItemCallbacks=historicalItemEffectKeyframesEnabled() &&
                    skillRule!=skillRules_.end() && actionKeyFrame>=-2 && actionKeyFrame<=0;
                if(splitItemCallbacks) {
                    const auto frames=itemEffectCallbackFrames(actionKeyFrame,
                        skillRule->second.itemEffectKeyFrames);
                    for(int callbackFrame:frames) {
                        EffectEvent callback=cast;callback.kind=castCallbackKind(callbackFrame);
                        const float callbackDelay=keyFrameDelay(callbackFrame);
                        if(callbackDelay<=0.f)effectEvents_.push_back(callback);
                        else scheduledEffects_.push_back({callbackDelay,callback,-1,attackSequence_+1u});
                    }
                } else if (actionDelay <= 0.f) effectEvents_.push_back(cast);
                else scheduledEffects_.push_back({actionDelay, cast, -1, attackSequence_ + 1u});
            } else {
                const float duration = actionDurations_[attackActionBase_] > 0.f
                    ? actionDurations_[attackActionBase_] : attackDuration_;
                const float impact = actionDurations_[attackActionBase_] > 0.f
                    ? actionImpacts_[attackActionBase_] : attackWindup_;
                // Fit the selected weapon action to ASPD and advance its
                // frames at that same speed; a shorter timer alone cuts it off.
                swing_ = std::max(.08f, player_.cdLeft);
                swingPlaybackRate_ = std::clamp(duration / swing_, .05f, 10.f);
                pendingAttack_ = std::clamp(impact / swingPlaybackRate_, 0.f, swing_);
            }
            swingIsSkill_ = hasSkillAnimation;
            ++attackSequence_;
            castingFor_ = 0.f;
            armedSkill_ = -1;
        }
    }
    for (Foe& foe : foes_) {
        if (foe.swing > 0.f) foe.swing -= dt;
        if (foe.numbFor > 0.f) {
            foe.numbFor = std::max(0.f, foe.numbFor - dt);
            if (foe.numbFor <= 0.f) foe.numbLevel = 0;
        }
        if (foe.enfeebleFor > 0.f) {
            foe.enfeebleFor = std::max(0.f, foe.enfeebleFor - dt);
            if (foe.enfeebleFor <= 0.f) foe.enfeebleLevel = 0;
        }
        if (foe.defenseDownFor > 0.f) {
            foe.defenseDownFor = std::max(0.f, foe.defenseDownFor - dt);
            if (foe.defenseDownFor <= 0.f) foe.defenseDownLevel = 0;
        }
        if (foe.stunFor > 0.f) foe.stunFor = std::max(0.f, foe.stunFor - dt);
        if (foe.poisonFor > 0.f) {
            const float active = std::min(dt, foe.poisonFor);
            foe.poisonFor = std::max(0.f, foe.poisonFor - dt);
            foe.poisonTick -= active;
            while (foe.hp > 0 && foe.poisonTick <= 0.f) {
                int poisonDamage = 10 + std::max(1, foe.poisonLevel) * 2;
                if (worldMap_ == "garner2" && isGarner2UnnormalMonster(foe.id))
                    poisonDamage = std::max(5, 320);
                foe.hp = std::max(0, foe.hp - poisonDamage);
                pops_.push_back({foe.x, foe.y, 1.1f, .5f, .85f, .55f,
                                 "POISON " + std::to_string(poisonDamage)});
                foe.poisonTick += 1.f;
                if (foe.hp == 0) {
                    foe.deadFor = .01f;
                    onKill(foe);
                }
            }
            if (foe.poisonFor <= 0.f) {
                foe.poisonLevel = 0;
                foe.poisonTick = 0.f;
            }
        }
        if (foe.hp <= 0) continue;
        if (foe.cdLeft > 0.f) foe.cdLeft -= dt;
        if (foe.retreating) {
            foe.attackPending = 0.f; foe.pendingSkillId = 0; foe.pendingReach = 0.f;
            continue;
        }
        if (foe.attackPending > 0.f) {
            foe.attackPending -= dt;
            if (foe.attackPending <= 0.f) {
                const float hitDistance = monster_ai::distance(px, py, foe.x, foe.y);
                const float fromHome = monster_ai::distance(foe.x, foe.y, foe.homeX, foe.homeY);
                const float reach = foe.pendingReach > 0.f ? foe.pendingReach :
                    foe.radius + foe.range + player_.radius;
                const int committedSkill = foe.pendingSkillId;
                foe.pendingSkillId = 0;
                foe.pendingReach = 0.f;
                // No instante do impacto, o alvo ainda precisa estar ao alcance.
                // Entrar em zona segura ou sair do raio cancela o golpe preparado.
                if (!monster_ai::canHit(foe.ai, foe.provoked, map && (map->regionAt(px, py) & 0x0002),
                                        player_.hp > 0 && deadFor_ <= 0.f, foe.goingHome,
                                        hitDistance, reach, fromHome, foe.leash) ||
                    (map && !monster_ai::clearSight(foe.x, foe.y, px, py,
                        [&](float x, float y) { return map->blockedAt(x, y); }))) continue;
                const auto skillRule = skillRules_.find(committedSkill);
                const bool scriptedHitRoll = committedSkill == 0 ||
                    (skillRule != skillRules_.end() &&
                     (skillRule->second.effectFormula == "Skill_Melee_End" ||
                      skillRule->second.effectFormula == "Skill_Range_End"));
                if (scriptedHitRoll && historical_stats::misses(foe.hit, player_.dodge, roll(1000))) {
                    pops_.push_back({px, py, .75f, .8f, .9f, 1.f, "MISS"});
                    continue;
                }
                const float enfeebleScale = foe.enfeebleFor > 0.f ? .8f : 1.f;
                const int effectiveMinAtk = std::max(1, static_cast<int>(std::floor(foe.minAtk * enfeebleScale)));
                const int effectiveMaxAtk = std::max(effectiveMinAtk, static_cast<int>(std::floor(foe.maxAtk * enfeebleScale)));
                const int attackRoll = roll(97);
                const int sampledAttack = sampleAttack(effectiveMinAtk, effectiveMaxAtk, foe.level, attackRoll);
                int dmg = damageOf(effectiveMinAtk, effectiveMaxAtk, foe.level,
                                         effectiveDefense(),
                                         attackRoll, player_.resist);
                bool applyGarner2Stun = false;
                if (skillRule != skillRules_.end()) {
                    const SkillRule& rule = skillRule->second;
                    const int skillLevel = 1; // characterinfo gives monsters a skill ID/rate, not a learned level
                    const int leveledDamage = attackDamageLevelScale(dmg, foe.level, foe.level - player_.level);
                    const int defenderDefense = effectiveDefense();
                    if (rule.damageFormula == SkillRule::DamageFormula::PhysicalLinear) {
                        dmg = physicalSkillDamage(leveledDamage, skillLevel,
                                                  rule.damageScaleBase, rule.damageScalePerLevel);
                    } else if (rule.damageFormula == SkillRule::DamageFormula::PhysicalPlus) {
                        dmg = physicalPlusDamage(leveledDamage, skillLevel,
                                                 rule.damageScaleBase, rule.damageScalePerLevel);
                    } else if (rule.damageFormula == SkillRule::DamageFormula::AttackRaise ||
                               rule.damageFormula == SkillRule::DamageFormula::MightyStrike) {
                        const double scale = rule.damageScaleBase +
                            rule.damageScalePerLevel * skillLevel;
                        dmg = scaledAttackDamage(sampledAttack, scale, defenderDefense, player_.resist);
                    } else if (rule.damageFormula == SkillRule::DamageFormula::IllusionSlash) {
                        const int speed = static_cast<int>(100.f / std::max(.01f, foe.cd));
                        dmg = illusionSlashDamage(leveledDamage, skillLevel, static_cast<float>(speed));
                    } else if (rule.damageFormula == SkillRule::DamageFormula::MonsterPhysicalFloor20) {
                        dmg = monsterPhysicalCallbackDamage(sampledAttack, rule.damageScaleBase,
                                                            defenderDefense, player_.resist);
                    } else if (rule.damageFormula == SkillRule::DamageFormula::MonsterPhysicalDouble) {
                        dmg = monsterPhysicalCallbackDamage(sampledAttack, 2.0, defenderDefense, player_.resist);
                    } else if (rule.damageFormula == SkillRule::DamageFormula::MonsterPhysicalPlus800) {
                        dmg = monsterPhysicalCallbackDamage(sampledAttack + 800, 1.0, defenderDefense, player_.resist);
                    } else if (rule.damageFormula == SkillRule::DamageFormula::MonsterFireDistance) {
                        dmg = monsterFireDamageWithDistance(sampledAttack, defenderDefense, player_.resist,
                                                            foe.level, hitDistance);
                    } else if (rule.damageFormula == SkillRule::DamageFormula::MonsterAttackSpeed) {
                        const int attackSpeed = static_cast<int>(100.f / std::max(.01f, foe.cd));
                        dmg = monsterAttackSpeedCallbackDamage(leveledDamage, attackSpeed);
                    } else if (worldMap_ == "garner2" && isGarner2UnnormalMonster(player_.id) &&
                               (rule.damageFormula == SkillRule::DamageFormula::LightningBolt ||
                                rule.damageFormula == SkillRule::DamageFormula::ConchRay)) {
                        dmg = garner2SkillMagicDamage(committedSkill, skillLevel, 0, 0, foe.sta, foe.job,
                            foe.level, bsta_ + gearBonuses().sta, player_.level, player_.resist,
                            static_cast<unsigned>(roll(100000)));
                        if (rule.damageFormula == SkillRule::DamageFormula::ConchRay)
                            dmg = std::max(1, dmg); // callback's special branch includes its skill-level add
                        if (rule.damageFormula == SkillRule::DamageFormula::ConchRay &&
                            roll(200) + 1 >= std::max(0, bcon_ + gearBonuses().con))
                            applyGarner2Stun = true;
                    }
                    if (rule.effectFormula == "Skill_wldb_End")
                        dmg = 0; // Lua's direct hpdmg is unset; STATE_WLDB supplies the 160 HP hit.
                }
                const bool critical = scriptedHitRoll &&
                    historical_stats::critical(foe.crit, foe.level, player_.level, roll(1000));
                const int finalDamage = critical ? static_cast<int>(std::min(100000000LL, 2LL * dmg)) : dmg;
                int poisonApplicationDamage = 0;
                if (committedSkill == 291 && playerPoisonLevel_ < 1 && itemBlessedPotionFor_ <= 0.f) {
                    // State_wldb_Add runs on AddState and repeats every second for 10 s.
                    poisonApplicationDamage = 160;
                    playerPoisonFor_ = 10.f;
                    playerPoisonTick_ = 1.f;
                    playerPoisonLevel_ = 1;
                }
                if (itemBlessedPotionFor_ <= 0.f)
                    player_.hp -= finalDamage + poisonApplicationDamage;
                if (applyGarner2Stun) {
                    playerStunFor_ = 1.f;
                    playerNumbFor_ = 0.f;
                    playerNumbLevel_ = 0;
                    pendingTarget_ = -1;
                    pendingAttack_ = 0.f;
                    swing_ = 0.f;
                }
                if (committedSkill > 0) {
                    const auto callback = skillRules_.find(committedSkill);
                    if (callback != skillRules_.end() && callback->second.effectFormula == "Skill_Lj_End") {
                        playerNumbFor_ = 3.f;
                        playerNumbLevel_ = 2;
                    }
                }
                pops_.push_back({px, py, 1.1f, 1.f, .35f, .3f,
                                 (critical ? "CRIT " : "") + std::to_string(finalDamage + poisonApplicationDamage)});
                effectEvents_.push_back({EffectEvent::Hurt, foe.x, foe.y, px, py});
                // 05-Q: o visual do impacto usa o skill ID, o catalogo de
                // EffectSystem resolve os IDs .eff/.par sem adivinhar nomes.
                if (committedSkill > 0 && committedSkill != 25) {
                    EffectEvent impact{EffectEvent::SkillHit, foe.x, foe.y, px, py,
                                       committedSkill};
                    impact.targetActorIndex=-1;
                    effectEvents_.push_back(impact);
                }
                if (player_.hp <= 0) {
                    player_.hp = 0;
                    deadFor_ = .01f;
                    sitting_ = false;
                    target_ = -1;
                    pendingTarget_ = -1;
                    pendingAttack_ = 0.f;
                    for (Foe& other : foes_) {
                        other.attackPending = 0.f;
                        other.pendingSkillId = 0;
                        other.pendingReach = 0.f;
                    }
                    break;
                }
            }
        }
        const float dx = px - foe.x;
        const float dy = py - foe.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        // AI 10 (e especiais) ataca ao ver. AI 1 e 5 so depois de tomar dano.
        // O alcance e o do FightAble: raio + distancia da ficha + raio do alvo.
        const float basicHit = foe.radius + foe.range + player_.radius;
        const float hit = monster_skills::maxReach(foe.attacks, basicHit,
                                                  foe.radius, player_.radius);
        const float fromHome = monster_ai::distance(foe.x, foe.y, foe.homeX, foe.homeY);
        if (!monster_ai::canHit(foe.ai, foe.provoked, !fightArea, player_.hp > 0,
                                foe.goingHome, dist, hit, fromHome, foe.leash) ||
            foe.stunFor > 0.f || foe.cdLeft > 0.f || foe.attackPending > 0.f ||
            (foe.swing > 0.f && foe.actionDurations[foe.action] > 0.f) ||
            (map && !monster_ai::clearSight(foe.x, foe.y, px, py,
                [&](float x, float y) { return map->blockedAt(x, y); }))) continue;
        // Escolhe apenas habilidades validas ao alcance. Se nao ha habilidade
        // configurada, preserva o autoataque melee do jogo offline anterior.
        const int choice = foe.enfeebleFor > 0.f ? -1 : monster_skills::choose(
            foe.attacks, dist, foe.radius, player_.radius, static_cast<unsigned>(roll(100000)));
        if (choice < 0 && dist > basicHit) continue;
        const monster_skills::Attack* chosen = choice >= 0 ?
            &foe.attacks[static_cast<std::size_t>(choice)] : nullptr;
        foe.pendingSkillId = chosen ? chosen->id : 0;
        foe.pendingReach = chosen ? monster_skills::reach(*chosen, foe.radius, player_.radius)
                                  : basicHit;
        const float numbAttackPenalty = foe.numbFor > 0.f ? 1.f + .1f + .03f * foe.numbLevel : 1.f;
        foe.cdLeft = foe.cd * numbAttackPenalty;
        const auto skillIt = chosen ? skillRules_.find(chosen->id) : skillRules_.end();
        foe.action = skillIt != skillRules_.end()
            ? choosePoseAction(skillIt->second.playActions, static_cast<unsigned>(foe.actionCycle), 7)
            : cycleNormalAttack(static_cast<unsigned>(foe.actionCycle));
        ++foe.actionCycle;
        const float nativeDuration = foe.actionDurations[foe.action];
        if (nativeDuration > 0.f) {
            const bool cyclic = skillIt == skillRules_.end() || skillIt->second.cyclicAnimation;
            const int fireTimeMs = skillIt != skillRules_.end() && !skillIt->second.cooldownFormulaMissing
                ? skillIt->second.animationTimeBaseMs + skillIt->second.animationTimePerLevelMs : 0;
            foe.swing = cyclic ? (fireTimeMs > 0 ? fireTimeMs / 1000.f : foe.cdLeft) : nativeDuration;
            foe.swing = std::max(.05f, foe.swing);
            foe.playbackRate = std::clamp(nativeDuration / foe.swing, .05f, 10.f);
            auto keyDelay = [&](int keyFrame) {
                if (keyFrame == -1) return 0.f;
                if (keyFrame == -2) return foe.swing;
                return std::clamp(foe.actionImpacts[foe.action] / foe.playbackRate, 0.f, foe.swing);
            };
            const float castDelay = keyDelay(skillIt != skillRules_.end() ? skillIt->second.actionKeyFrame : 0);
            foe.attackPending = std::max(.001f, castDelay);
            if (chosen && chosen->id != 25 && skillIt != skillRules_.end()) {
                EffectEvent cast{EffectEvent::Cast, foe.x, foe.y, px, py, chosen->id};
                cast.sourceActorIndex=static_cast<int>(&foe-foes_.data());
                const bool splitItemCallbacks=historicalItemEffectKeyframesEnabled() &&
                    skillIt->second.actionKeyFrame>=-2 && skillIt->second.actionKeyFrame<=0;
                if(splitItemCallbacks) {
                    const auto frames=itemEffectCallbackFrames(skillIt->second.actionKeyFrame,
                        skillIt->second.itemEffectKeyFrames);
                    for(int callbackFrame:frames) {
                        EffectEvent callback=cast;callback.kind=castCallbackKind(callbackFrame);
                        const float callbackDelay=keyDelay(callbackFrame);
                        const int owner=static_cast<int>(&foe-foes_.data());
                        if(callbackDelay<=0.f)effectEvents_.push_back(callback);
                        else scheduledEffects_.push_back({callbackDelay,callback,owner,foe.actionCycle});
                    }
                } else if (castDelay <= 0.f) effectEvents_.push_back(cast);
                else scheduledEffects_.push_back({castDelay, cast, static_cast<int>(&foe - foes_.data()), foe.actionCycle});
                if (skillIt->second.flightEffectId > 0 && chosen->projectileSpeed > 0.f) {
                    const float flightDelay = keyDelay(skillIt->second.flightActionKeyFrame);
                    const float flightDuration = dist / chosen->projectileSpeed;
                    foe.attackPending = std::max(.001f, flightDelay + flightDuration);
                    EffectEvent flight{EffectEvent::Flight, foe.x, foe.y, px, py, chosen->id, flightDuration};
                    flight.targetActorIndex=-1;
                    flight.sourceActorIndex=static_cast<int>(&foe-foes_.data());
                    if (flightDelay <= 0.f) effectEvents_.push_back(flight);
                    else scheduledEffects_.push_back({flightDelay, flight, static_cast<int>(&foe - foes_.data()), foe.actionCycle});
                }
            }
            continue;
        }
        // Synthetic/asset-less actors retain the documented offline fallback.
        foe.attackPending = chosen ? monster_skills::windup(dist, chosen->projectileSpeed) : .15f;
        foe.swing = std::max(.4f, foe.attackPending);
        // Efeitos existentes do skillinfo: inicio, voo e impacto.
        if (chosen && chosen->id != 25) {
            if (skillIt != skillRules_.end()) {
                effectEvents_.push_back({EffectEvent::Cast, foe.x, foe.y, px, py,
                                         chosen->id});
                if (skillIt->second.flightEffectId > 0)
                    effectEvents_.push_back({EffectEvent::Flight, foe.x, foe.y, px, py,
                                             chosen->id, foe.attackPending});
            }
        }
    }
}

std::vector<EffectEvent> Play::takeEffects() {
    std::vector<EffectEvent> out;
    out.swap(effectEvents_);
    return out;
}

void Play::moveFoes(float dt, MapFile* map, float px, float py) {
    // ai.lua: AI 10 procura dentro da visao. AI 1 e 5 so guardam alvo depois
    // do dano (SetMonsterFightObj). AI 2 foge. Passou de lCDis do spawn,
    // larga o alvo e volta. Sem alvo, birth_rand_move anda ate 6 m do spawn;
    // velocidade 0 fica parado.
    constexpr std::uint16_t kSafe = 0x0002;
    const bool playerSafe = map && (map->regionAt(px, py) & kSafe) != 0;
    auto stepToward = [&](Foe& foe, float tx, float ty) {
        const float dx = tx - foe.x;
        const float dy = ty - foe.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 0.08f || foe.speed <= 0.f || foe.stunFor > 0.f) {
            foe.moving = false;
            return false;
        }
        const float movementFactor = foe.numbFor > 0.f
            ? std::max(.1f, 1.f - .2f - .03f * foe.numbLevel) : 1.f;
        const float step = std::min(foe.speed * movementFactor * dt, dist);
        const float sx = dx / dist * step;
        const float sy = dy / dist * step;
        float nx = foe.x + sx;
        float ny = foe.y + sy;
        // Verifica todo o segmento, incluindo os dois lados de cruzamentos
        // diagonais de celulas. Nao atravesse parede com um passo longo.
        if (map) {
            const int n = std::max(1, static_cast<int>(std::ceil(step / .12f)));
            float lastX = foe.x, lastY = foe.y;
            for (int i=1; i<=n; ++i) {
                const float t = static_cast<float>(i) / n;
                const float cx = foe.x + sx*t, cy = foe.y + sy*t;
                if (map->blockedAt(cx,cy) ||
                    (static_cast<int>(std::floor(cx*2.f)) != static_cast<int>(std::floor(lastX*2.f)) &&
                     static_cast<int>(std::floor(cy*2.f)) != static_cast<int>(std::floor(lastY*2.f)) &&
                     (map->blockedAt(cx,lastY) || map->blockedAt(lastX,cy)))) {
                    nx = lastX; ny = lastY;
                    break;
                }
                lastX = cx; lastY = cy;
            }
        }
        if (nx == foe.x && ny == foe.y) {
            foe.moving = false;
            return false;
        }
        foe.yaw = std::atan2(nx - foe.x, ny - foe.y);
        foe.x = nx;
        foe.y = ny;
        foe.moving = true;
        return true;
    };
    // A* e acionado apenas se o caminho direto estiver bloqueado; recalculo
    // limitado por NPC para evitar 30 buscas caras a cada quadro.
    auto travel = [&](Foe& foe, float tx, float ty, int mode) {
        if (foe.speed <= 0.f) { foe.moving = false; return false; }
        if (!map) {
            foe.route.clear(); foe.routeMode = 0;
            return stepToward(foe, tx, ty);
        }
        const auto blocked = [&](float x, float y) { return map->blockedAt(x, y); };
        // Testa o segmento completo, evitando passos que atravessam paredes.
        if (!map->blockedAt(tx, ty) &&
            monster_ai::clearSight(foe.x, foe.y, tx, ty, blocked)) {
            foe.route.clear(); foe.routeMode = 0;
            return stepToward(foe, tx, ty);
        }
        const float targetMotion = monster_ai::distance(tx,ty,foe.routeGoalX,foe.routeGoalY);
        if (foe.routeMode != mode) {
            foe.route.clear(); foe.routeIndex = 0; foe.routeCooldown = 0.f;
        } else if (targetMotion > 1.4f && foe.routeCooldown <= 0.f) {
            // Alvo movel: replanejar no maximo uma vez a cada 0,7 s.
            foe.route.clear(); foe.routeIndex = 0;
        }
        if (foe.route.empty() && foe.routeCooldown <= 0.f) {
            foe.routeCooldown = .7f; // inclusive quando a rota nao existe
            foe.routeMode = mode;
            foe.routeGoalX = tx; foe.routeGoalY = ty;
            PathFinder::findLocal(*map, foe.x, foe.y, tx, ty,
                                  foe.homeX, foe.homeY, foe.leash, foe.route);
            foe.routeIndex = 0;
            // O A* inicia no centro da celula. Se o monstro estiver perto da
            // borda, passa primeiro pelo centro para evitar cortar uma quina.
            if (!foe.route.empty() &&
                !monster_ai::clearSight(foe.x, foe.y,
                                         foe.route.front().x, foe.route.front().y, blocked)) {
                const float cx = (std::floor(foe.x * 2.f) + .5f) * .5f;
                const float cy = (std::floor(foe.y * 2.f) + .5f) * .5f;
                if (!map->blockedAt(cx,cy) &&
                    monster_ai::distance(foe.x,foe.y,cx,cy) > .1f)
                    foe.route.insert(foe.route.begin(), {cx,cy});
            }
        }
        while (foe.routeIndex < foe.route.size() &&
               monster_ai::distance(foe.x,foe.y,foe.route[foe.routeIndex].x,
                                     foe.route[foe.routeIndex].y) < .16f)
            ++foe.routeIndex;
        if (foe.routeIndex >= foe.route.size()) {
            foe.route.clear(); foe.moving = false;
            return false;
        }
        const auto& point = foe.route[foe.routeIndex];
        // A colisao fina continua sendo verificada no passo de movimento.
        if (!stepToward(foe, point.x, point.y)) {
            foe.route.clear(); foe.routeIndex = 0;
            return false;
        }
        return true;
    };
    for (Foe& foe : foes_) {
        foe.routeCooldown = std::max(0.f, foe.routeCooldown - std::max(0.f, dt));
        if (foe.hp <= 0) {
            foe.route.clear(); foe.routeIndex = 0; foe.routeMode = 0;
            foe.lostSightFor = 3.f;
            foe.retreating = false;
            foe.deadFor += dt;
            foe.moving = false;
            foe.wandering = false;
            if (foe.deadFor >= monster_lifecycle::respawnDelay(foe.respawnSeconds)) {
                foe.hp = foe.maxHp;
                foe.x = foe.homeX;
                foe.y = foe.homeY;
                foe.deadFor = 0.f;
                foe.cdLeft = 0.4f;
                foe.provoked = false;
                foe.goingHome = false;
                foe.pendingSkillId = 0; foe.pendingReach = 0.f; foe.attackPending = 0.f;
                foe.enfeebleFor = 0.f; foe.enfeebleLevel = 0;
                foe.wanderWait = 1.f;
                foe.wandering = false;
            }
            continue;
        }
        if (foe.followsPlayer) {
            // Historical SetChaTarget(new, role): keep the summoned pet near
            // its owner without entering the hostile monster combat loop.
            foe.provoked = false;
            foe.goingHome = false;
            foe.attackPending = 0.f;
            foe.pendingSkillId = 0;
            foe.pendingReach = 0.f;
            foe.homeX = px;
            foe.homeY = py;
            const float followX = px + .05f;
            const float followY = py + .05f;
            if (monster_ai::distance(foe.x, foe.y, followX, followY) > 1.15f)
                travel(foe, followX, followY, 5);
            else {
                foe.moving = false;
                foe.route.clear();
                foe.routeIndex = 0;
                foe.routeMode = 0;
            }
            continue;
        }
        const float dx = px - foe.x;
        const float dy = py - foe.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        const float hx = foe.x - foe.homeX;
        const float hy = foe.y - foe.homeY;
        const float home = std::sqrt(hx * hx + hy * hy);
        const bool leashed = monster_ai::outsideLeash(home, foe.leash);
        const bool sees = !playerSafe && deadFor_ <= 0.f && player_.hp > 0 && dist <= foe.vision;
        // Nao testar cada tile de todos os monstros quando o jogador esta longe.
        const bool lineClear = sees && (!map || monster_ai::clearSight(
            foe.x, foe.y, px, py, [&](float x, float y) { return map->blockedAt(x, y); }));
        if (leashed) {
            foe.goingHome = true;
            foe.provoked = false; // solta aggro ao ultrapassar lCDis do spawn
            foe.attackPending = 0.f;
            foe.pendingSkillId = 0; foe.pendingReach = 0.f;
            foe.lostSightFor = 3.f;
        }
        if (lineClear && !leashed) {
            foe.lastSeenX = px; foe.lastSeenY = py;
            foe.lostSightFor = 0.f;
        } else {
            foe.lostSightFor = std::min(3.f, foe.lostSightFor + std::max(0.f, dt));
        }
        if (monster_ai::shouldForgetProvocation(foe.ai, foe.provoked, playerSafe,
                                                 player_.hp > 0 && deadFor_ <= 0.f,
                                                 foe.lostSightFor)) {
            foe.provoked = false;
            foe.attackPending = 0.f;
            foe.pendingSkillId = 0; foe.pendingReach = 0.f;
            foe.route.clear(); foe.routeIndex = 0; foe.routeMode = 0;
            // Nao manter um antigo alvo de perseguicao na patrulha.
            foe.wandering = false;
        }
        const bool remembered = foe.lostSightFor < 1.6f &&
            monster_ai::distance(foe.x, foe.y, foe.lastSeenX, foe.lastSeenY) > .5f;
        const bool chase = monster_ai::canPursue(foe.ai, foe.provoked, playerSafe,
                                                 player_.hp > 0 && deadFor_ <= 0.f,
                                                 foe.goingHome, lineClear || remembered,
                                                 lineClear ? dist : monster_ai::distance(
                                                     foe.x,foe.y,foe.lastSeenX,foe.lastSeenY),
                                                 foe.vision, home, foe.leash);
        foe.moving = false;
        const bool wasRetreating = foe.retreating;
        foe.retreating = false;
        if (monster_ai::shouldRetreat(foe.ai, lineClear && !foe.goingHome && !leashed,
                                      dist, wasRetreating)) {
            foe.retreating = true;
            foe.attackPending = 0.f;
            foe.pendingSkillId = 0; foe.pendingReach = 0.f;
            foe.wandering = false;
            if (dist > .001f) {
                // O destino inteiro permanece dentro do leash: o A* nao tenta
                // alcancar uma meta impossivel fora da regiao de nascimento.
                const float retreat = foe.ai == 11 ? 4.5f : std::max(2.f, dist);
                float tx = foe.x - dx / dist * retreat;
                float ty = foe.y - dy / dist * retreat;
                monster_ai::constrainToLeash(foe.homeX, foe.homeY,
                     foe.leash > .05f ? foe.leash - .05f : foe.leash, tx, ty);
                travel(foe, tx, ty, 3);
            }
            if (monster_ai::outsideLeash(monster_ai::distance(foe.x, foe.y, foe.homeX, foe.homeY),
                                         std::max(.01f, foe.leash - .1f))) foe.goingHome = true;
            continue;
        }
        if (chase) {
            foe.goingHome = false;
            foe.wandering = false;
            const float hit = monster_skills::maxReach(foe.attacks,
                foe.radius + foe.range + player_.radius, foe.radius, player_.radius);
            if (dist > hit) {
                const float beforeX = foe.x;
                const float beforeY = foe.y;
                travel(foe, lineClear ? px : foe.lastSeenX,
                       lineClear ? py : foe.lastSeenY, 1);
                const float nx = foe.x - foe.homeX;
                const float ny = foe.y - foe.homeY;
                if (foe.leash > 0.f && nx * nx + ny * ny > foe.leash * foe.leash) {
                    foe.x = beforeX;
                    foe.y = beforeY;
                    foe.moving = false;
                    foe.goingHome = true;
                }
            } else if (dist > 0.05f) {
                foe.yaw = std::atan2(dx, dy);
            }
            continue;
        }
        if (monster_ai::shouldGoHome(home, foe.leash, foe.goingHome)) {
            foe.goingHome = true;
            foe.wandering = false;
            travel(foe, foe.homeX, foe.homeY, 2);
            const float leftX = foe.x - foe.homeX;
            const float leftY = foe.y - foe.homeY;
            if (leftX * leftX + leftY * leftY < 1.f) foe.goingHome = false;
            continue;
        }
        foe.goingHome = false;
        if (!monster_ai::canRoam(foe.ai, foe.speed)) {
            foe.wandering = false;
            foe.route.clear(); foe.routeIndex = 0; foe.routeMode = 0;
            continue;
        }
        foe.wanderWait -= dt;
        if (!foe.wandering && foe.wanderWait <= 0.f) {
            foe.wanderWait = 1.f;
            if (roll(20) <= 1) {
                // O script original usa birth_rand_move(cha,600). A rota
                // continua aleatoria, mas nao escolhe obstaculos ou metas
                // impossiveis fora do leash.
                const float roam = monster_ai::roamRadius(foe.leash);
                for (int attempt = 0; attempt < 4 && !foe.wandering && roam > .2f; ++attempt) {
                    const float ang = static_cast<float>(roll(628)) / 100.f;
                    const float rad = (.05f + static_cast<float>(roll(950)) / 1000.f) * roam;
                    const float tx = foe.homeX + std::cos(ang) * rad;
                    const float ty = foe.homeY + std::sin(ang) * rad;
                    if (map && map->blockedAt(tx, ty)) continue;
                    foe.wanderX = tx; foe.wanderY = ty;
                    foe.wandering = true;
                }
            }
        }
        if (foe.wandering) {
            const bool moved = travel(foe, foe.wanderX, foe.wanderY, 4);
            const float wx = foe.x - foe.wanderX;
            const float wy = foe.y - foe.wanderY;
            if (wx * wx + wy * wy < 0.16f || (!moved && foe.route.empty())) {
                foe.wandering = false;
                // Evita solicitar o mesmo percurso impossivel a cada quadro.
                foe.wanderWait = std::max(foe.wanderWait, .8f);
            }
        }
    }
}

void Play::step(float dt, MapFile* map, float px, float py) {
    if (!std::isfinite(dt) || dt < 0.f) return;
    if (dt > 0.05f) dt = 0.05f;
    if(sheetReady_ && learnedSkills_!=appliedSkillAttributes_)applyAttrs(false);
    worldClock_ += dt;
    updateFairyAutoFeed();
    updateFairySystem(dt);
    for (auto cooldown = itemCooldownRemaining_.begin(); cooldown != itemCooldownRemaining_.end();) {
        cooldown->second = std::max(0.f, cooldown->second - dt);
        if (cooldown->second <= 0.f) cooldown = itemCooldownRemaining_.erase(cooldown);
        else ++cooldown;
    }
    if (itemBoatSpeedFor_ > 0.f)
        itemBoatSpeedFor_ = std::max(0.f, itemBoatSpeedFor_ - dt);
    if (itemBlessedPotionFor_ > 0.f)
        itemBlessedPotionFor_ = std::max(0.f, itemBlessedPotionFor_ - dt);
    if (itemForgeBoostFor_ > 0.f) {
        itemForgeBoostFor_ = std::max(0.f, itemForgeBoostFor_ - dt);
        if (itemForgeBoostFor_ <= 0.f) itemForgeBoostLevel_ = 0;
    }
    if (itemCombineBoostFor_ > 0.f) {
        itemCombineBoostFor_ = std::max(0.f, itemCombineBoostFor_ - dt);
        if (itemCombineBoostFor_ <= 0.f) itemCombineBoostLevel_ = 0;
    }
    bool foodStateExpired = false;
    if (itemInnocenceState_.remaining > 0.f) {
        itemInnocenceState_.remaining = std::max(0.f, itemInnocenceState_.remaining - dt);
        if (itemInnocenceState_.remaining <= 0.f) {
            itemInnocenceState_ = ItemAllAttributeState{};
            foodStateExpired = true;
        }
    }
    for (FoodState& state : foodStates_) {
        if (state.level <= 0 || state.remaining <= 0.f) continue;
        state.remaining = std::max(0.f, state.remaining - dt);
        if (state.remaining <= 0.f) {
            state = FoodState{};
            foodStateExpired = true;
        }
    }
    for (ItemAttributeState* state : {&itemStrengthState_, &itemConstitutionState_,
                                      &itemAgilityState_, &itemAccuracyState_, &itemSpiritState_,
                                      &itemPotionStrengthState_, &itemPotionAgilityState_,
                                      &itemPotionConstitutionState_, &itemPotionAccuracyState_,
                                      &itemPotionSpiritState_, &itemPotionMspdState_,
                                      &itemPotionAttackState_,
                                      &itemPotionAspdState_, &itemPotionHitState_,
                                      &itemPotionMaxHpState_, &itemKaraVictoryState_, &itemLotteryDefenseState_,
                                      &itemLanternDefenseState_, &itemFairyMarchState_,
                                      &itemMaliciousHornState_}) {
        if (state->remaining <= 0.f) continue;
        state->remaining = std::max(0.f, state->remaining - dt);
        if (state->remaining <= 0.f) {
            *state = ItemAttributeState{};
            foodStateExpired = true;
        }
    }
    if (itemBodyStateFor_ > 0.f) {
        itemBodyStateFor_ = std::max(0.f, itemBodyStateFor_ - dt);
        if (itemBodyStateFor_ <= 0.f) {
            itemBodyStateId_ = 0;
            itemBodyMoveSpeedBonus_ = 0.f;
            itemBodyAttackBonus_ = 0;
            itemBodyDefenseBonus_ = 0;
            foodStateExpired = true;
        }
    }
    if (foodStateExpired) {
        if (sheetReady_) applyAttrs(false);
        else recomputeGear();
    }
    for (ItemBoostState* boost : {&experienceBoost_, &dropRateBoost_, &fairyGrowthBoost_}) {
        if (boost->remaining <= 0.f) continue;
        boost->remaining = std::max(0.f, boost->remaining - dt);
        if (boost->remaining <= 0.f) *boost = ItemBoostState{};
    }
    hereX_ = px;
    hereY_ = py;
    if (shopOpen_) {
        const float dx = vendorX_ - px;
        const float dy = vendorY_ - py;
        if (!vendorOn_ || dx * dx + dy * dy > 3.2f * 3.2f) shopOpen_ = false;
    }
    moveFoes(dt, map, px, py);
    resolve(dt, px, py, map);
    collectRequested(px, py);
    auto checkMission = [&](HistoricalProgress& mission) {
        // WorldMission Welcome e entregue falando com Senna, nao por distancia.
        if (!mission.active || mission.id==1 || mission.turnInRequired) return;
        const bool huntReady = mission.targetCount <= 0 || mission.have >= mission.targetCount;
        bool destinationReady = mission.targetX < 0.f || mission.targetY < 0.f;
        if (!destinationReady) {
            const float dx = mission.targetX - px;
            const float dy = mission.targetY - py;
            destinationReady = dx * dx + dy * dy <= 2.4f * 2.4f;
        }
        if (huntReady && destinationReady) completeHistoricalMission(mission);
    };
    checkMission(historical_);
    for (auto& mission : additionalMissions_) checkMission(mission);
    for (std::size_t i = 0; i < pops_.size();) {
        pops_[i].life -= dt;
        if (pops_[i].life <= 0.f) pops_.erase(pops_.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }
}


} // namespace top
