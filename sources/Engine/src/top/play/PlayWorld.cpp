#include "top/Play.h"
#include "top/MonsterAI.h"
#include "top/HistoricalSkillExecution.h"
#include "top/PopulationIdentity.h"

#include <algorithm>
#include <cmath>

namespace top {
bool Play::historicalNpcTeleportFareReady() const {
    return player_.level <= 40 || gold_ >= 500;
}

bool Play::payHistoricalNpcTeleportFare() {
    if (player_.level <= 40) return true;
    if (gold_ < 500) {
        say("You need at least 500G to teleport.");
        return false;
    }
    gold_ -= 500;
    say("500G teleport fee paid.");
    save(savePath());
    return true;
}

void Play::snapshotWorldMonsters() {
    for (const Foe& foe : foes_)
        if (foe.spawnKey != 0)
            worldMonsters_.save(worldMap_, foe.spawnKey, worldClock_, foe.hp,
                                foe.x, foe.y, foe.yaw, foe.deadFor, foe.respawnSeconds);
}

void Play::setWorldMap(const std::string& mapName) {
    if (worldMap_ == mapName) return;
    snapshotWorldMonsters();
    worldMap_ = mapName;
}

bool Play::hasTransientSummons() const {
    return std::any_of(foes_.begin(), foes_.end(), [](const Foe& foe) {
        return foe.summoned || foe.followsPlayer;
    });
}

bool Play::refreshPopulation(const ActorSkin& actors) {
    if (root_.empty() || actors.playerId() != player_.id || hasTransientSummons()) return false;
    snapshotWorldMonsters();
    std::vector<Foe> updated;
    const int count = actors.monsterCount();
    updated.reserve(static_cast<std::size_t>(count));
    std::vector<int> mapping(foes_.size(), -1);
    std::vector<bool> used(foes_.size(), false);
    for (int i = 0; i < count; ++i) {
        const MonsterSlot slot = actors.monsterSlot(i);
        int match = -1;
        for (std::size_t j = 0; j < foes_.size(); ++j) {
            if (used[j]) continue;
            if (population_identity::same(
                    {foes_[j].id, foes_[j].homeX, foes_[j].homeY, false, foes_[j].spawnKey},
                    {slot.id, slot.x, slot.y, false, slot.spawnKey})) {
                match = static_cast<int>(j); break;
            }
        }
        if (match >= 0) {
            used[static_cast<std::size_t>(match)] = true;
            mapping[static_cast<std::size_t>(match)] = i;
            updated.push_back(std::move(foes_[static_cast<std::size_t>(match)]));
        } else {
            // Rows are cached at bind time; request an unfamiliar historical
            // character row only when a new ID first enters the local sector.
            ensureWorldFoeRow(slot.id);
            updated.push_back(createWorldFoe(slot, actors, i));
        }
        updated.back().actorSlot = -1; // standard index follows visual monster ordinal
    }
    auto remap = [&](int& index) {
        index = index >= 0 && static_cast<std::size_t>(index) < mapping.size() ?
            mapping[static_cast<std::size_t>(index)] : -1;
    };
    remap(target_);
    const int oldPendingTarget = pendingTarget_;
    remap(pendingTarget_);
    if (oldPendingTarget >= 0 && pendingTarget_ < 0) {
        pendingAttack_ = 0.f;
        pendingSkill_ = -1;
    }
    // Scheduled effects own a foe index. A disappeared owner must not turn
    // into the player (-1) or affect an unrelated foe after reordering.
    scheduledEffects_.erase(std::remove_if(scheduledEffects_.begin(), scheduledEffects_.end(),
        [&](ScheduledEffect& effect) {
            if (effect.owner < 0) return false;
            if (static_cast<std::size_t>(effect.owner) >= mapping.size() ||
                mapping[static_cast<std::size_t>(effect.owner)] < 0) return true;
            effect.owner = mapping[static_cast<std::size_t>(effect.owner)];
            return false;
        }), scheduledEffects_.end());
    foes_ = std::move(updated);
    // No bind(), no save(), no skill/mission/item catalog reload on walking.
    return true;
}

bool Play::rebindWorld(const ActorSkin& actors) {
    if (root_.empty()) return false;
    snapshotWorldMonsters();
    // Troca efetiva de personagem usa bind limpo, nao herda efeitos de outro.
    if (actors.playerId() != player_.id) return bind(root_, actors);
    const std::string keepRoot = root_;
    const int keepProfileId = profileId_;
    const std::string keepProfileName = profileName_;
    // O bind é útil para substituir monstros, mas nao deve zerar estados
    // temporarios do jogador nem as recargas de habilidades.
    const float keepSp = player_.homeY;
    const int keepHp = player_.hp;
    const float keepDeath = deadFor_;
    const int keepArmedSkill = armedSkill_;
    const int keepRequestedSkill = requestedSkill_;
    const bool keepBasicStrike = pendingBasicStrike_;
    const float keepCasting = castingFor_;
    const int keepCastingAction = castingActionBase_;
    const unsigned keepAttackSequence = attackSequence_;
    const float keepBerserk = berserkFor_;
    const float keepBerserkAttackSpeed = berserkAttackSpeedBonus_;
    const float keepHarden = hardenFor_;
    const int keepHardenDefense = hardenDefense_;
    const float keepEtherClover = itemEtherCloverFor_;
    const int keepEtherCloverDodge = itemEtherCloverDodge_;
    const int keepBodyStateId = itemBodyStateId_;
    const float keepBodyStateFor = itemBodyStateFor_;
    const float keepBlessedPotionFor = itemBlessedPotionFor_;
    const float keepBodyMoveSpeedBonus = itemBodyMoveSpeedBonus_;
    const int keepBodyAttackBonus = itemBodyAttackBonus_;
    const int keepBodyDefenseBonus = itemBodyDefenseBonus_;
    const ItemAttributeState keepStrengthState = itemStrengthState_;
    const ItemAttributeState keepConstitutionState = itemConstitutionState_;
    const ItemAttributeState keepAgilityState = itemAgilityState_;
    const ItemAttributeState keepAccuracyState = itemAccuracyState_;
    const ItemAttributeState keepSpiritState = itemSpiritState_;
    const ItemAttributeState keepPotionStrengthState = itemPotionStrengthState_;
    const ItemAttributeState keepPotionAgilityState = itemPotionAgilityState_;
    const ItemAttributeState keepPotionConstitutionState = itemPotionConstitutionState_;
    const ItemAttributeState keepPotionAccuracyState = itemPotionAccuracyState_;
    const ItemAttributeState keepPotionSpiritState = itemPotionSpiritState_;
    const ItemAttributeState keepPotionMspdState = itemPotionMspdState_;
    const ItemAttributeState keepPotionAttackState = itemPotionAttackState_;
    const ItemAttributeState keepPotionAspdState = itemPotionAspdState_;
    const ItemAttributeState keepPotionHitState = itemPotionHitState_;
    const ItemAttributeState keepPotionMaxHpState = itemPotionMaxHpState_;
    const ItemAttributeState keepKaraVictoryState = itemKaraVictoryState_;
    const ItemAttributeState keepLotteryDefenseState = itemLotteryDefenseState_;
    const ItemAttributeState keepLanternDefenseState = itemLanternDefenseState_;
    const ItemAttributeState keepFairyMarchState = itemFairyMarchState_;
    const ItemAttributeState keepMaliciousHornState = itemMaliciousHornState_;
    const ItemAllAttributeState keepInnocenceState = itemInnocenceState_;
    const float keepForgeBoostFor = itemForgeBoostFor_;
    const int keepForgeBoostLevel = itemForgeBoostLevel_;
    const float keepCombineBoostFor = itemCombineBoostFor_;
    const int keepCombineBoostLevel = itemCombineBoostLevel_;
    const float keepWillOfSteel = willOfSteelFor_;
    const int keepWillOfSteelDefense = willOfSteelDefense_;
    const float keepGeneric = genericBuffFor_;
    const int keepBuffAttack = genericBuffAttack_;
    const int keepBuffDefense = genericBuffDefense_;
    const float keepRegen = regen_;
    const float keepPlayerCooldown = player_.cdLeft;
    const float keepBoatCooldown = boatCannonCd_;
    const bool keepSitting = sitting_;
    std::array<float, 8> keepSkills{};
    std::copy(std::begin(skillCd_),std::end(skillCd_),keepSkills.begin());
    save(savePath(keepRoot));
    if (!bind(keepRoot, actors, keepProfileId, keepProfileName)) return false;
    player_.homeY = std::clamp(keepSp, 0.f, player_.homeX);
    player_.hp = std::clamp(keepHp, 0, player_.maxHp);
    deadFor_ = keepDeath;
    armedSkill_ = keepArmedSkill;
    requestedSkill_ = keepRequestedSkill;
    pendingBasicStrike_ = keepBasicStrike;
    castingFor_ = keepCasting;
    castingActionBase_ = keepCastingAction;
    attackSequence_ = keepAttackSequence;
    berserkFor_ = keepBerserk;
    berserkAttackSpeedBonus_ = keepBerserkAttackSpeed;
    hardenFor_ = keepHarden;
    hardenDefense_ = keepHardenDefense;
    itemEtherCloverFor_ = keepEtherClover;
    itemEtherCloverDodge_ = keepEtherCloverDodge;
    itemBodyStateId_ = keepBodyStateId;
    itemBodyStateFor_ = keepBodyStateFor;
    itemBlessedPotionFor_ = keepBlessedPotionFor;
    itemBodyMoveSpeedBonus_ = keepBodyMoveSpeedBonus;
    itemBodyAttackBonus_ = keepBodyAttackBonus;
    itemBodyDefenseBonus_ = keepBodyDefenseBonus;
    itemStrengthState_ = keepStrengthState;
    itemConstitutionState_ = keepConstitutionState;
    itemAgilityState_ = keepAgilityState;
    itemAccuracyState_ = keepAccuracyState;
    itemSpiritState_ = keepSpiritState;
    itemPotionStrengthState_ = keepPotionStrengthState;
    itemPotionAgilityState_ = keepPotionAgilityState;
    itemPotionConstitutionState_ = keepPotionConstitutionState;
    itemPotionAccuracyState_ = keepPotionAccuracyState;
    itemPotionSpiritState_ = keepPotionSpiritState;
    itemPotionMspdState_ = keepPotionMspdState;
    itemPotionAttackState_ = keepPotionAttackState;
    itemPotionAspdState_ = keepPotionAspdState;
    itemPotionHitState_ = keepPotionHitState;
    itemPotionMaxHpState_ = keepPotionMaxHpState;
    itemKaraVictoryState_ = keepKaraVictoryState;
    itemLotteryDefenseState_ = keepLotteryDefenseState;
    itemLanternDefenseState_ = keepLanternDefenseState;
    itemFairyMarchState_ = keepFairyMarchState;
    itemMaliciousHornState_ = keepMaliciousHornState;
    itemInnocenceState_ = keepInnocenceState;
    itemForgeBoostFor_ = keepForgeBoostFor;
    itemForgeBoostLevel_ = keepForgeBoostLevel;
    itemCombineBoostFor_ = keepCombineBoostFor;
    itemCombineBoostLevel_ = keepCombineBoostLevel;
    willOfSteelFor_ = keepWillOfSteel;
    willOfSteelDefense_ = keepWillOfSteelDefense;
    genericBuffFor_ = keepGeneric;
    genericBuffAttack_ = keepBuffAttack;
    genericBuffDefense_ = keepBuffDefense;
    regen_ = keepRegen;
    player_.cdLeft = keepPlayerCooldown;
    boatCannonCd_ = keepBoatCooldown;
    sitting_ = keepSitting;
    if (itemStrengthState_.remaining > 0.f || itemConstitutionState_.remaining > 0.f ||
        itemAgilityState_.remaining > 0.f || itemAccuracyState_.remaining > 0.f ||
        itemSpiritState_.remaining > 0.f || itemPotionStrengthState_.remaining > 0.f ||
        itemPotionAgilityState_.remaining > 0.f || itemPotionConstitutionState_.remaining > 0.f ||
        itemPotionAccuracyState_.remaining > 0.f || itemPotionSpiritState_.remaining > 0.f ||
        itemPotionMspdState_.remaining > 0.f ||
        itemPotionAttackState_.remaining > 0.f || itemPotionAspdState_.remaining > 0.f ||
        itemPotionHitState_.remaining > 0.f ||
        itemPotionMaxHpState_.remaining > 0.f ||
        itemKaraVictoryState_.remaining > 0.f ||
        itemLanternDefenseState_.remaining > 0.f ||
        itemFairyMarchState_.remaining > 0.f ||
        itemMaliciousHornState_.remaining > 0.f ||
        itemEtherCloverDodge_ > 0 || itemBodyStateFor_ > 0.f || itemInnocenceState_.remaining > 0.f) {
        applyAttrs(false);
    } else {
        player_.dodge = flee_;
    }
    for (int i = 0; i < 8; ++i) skillCd_[i] = keepSkills[static_cast<std::size_t>(i)];
    return true;
}

void Play::pickFoe(int index) {
    if(index<0 || index>=static_cast<int>(foes_.size()) || foes_[static_cast<std::size_t>(index)].hp<=0 ||
       foes_[static_cast<std::size_t>(index)].followsPlayer) return;
    target_=index;
    requestedLoot_=-1;
    // Skill hotkey + enemy click only sets an explicit request. The movement
    // and attack loop will commit once the actor is within historical range.
    if (requestedSkill_ >= 0) {
        pendingBasicStrike_ = false;
    } else if (armedSkill_ < 0) {
        pendingBasicStrike_ = true; // explicit, one-shot basic attack
    }
    say("Alvo: "+foes_[static_cast<std::size_t>(index)].name);
}

bool Play::selectedMonster(float& x, float& y, float& radius) const {
    if (target_ < 0 || target_ >= static_cast<int>(foes_.size())) return false;
    const Foe& foe = foes_[static_cast<std::size_t>(target_)];
    if (foe.hp <= 0 || foe.followsPlayer) return false;
    x = foe.x;
    y = foe.y;
    radius = std::clamp(foe.radius + .54f, .85f, 3.f);
    return true;
}

screen_pick::Hit Play::rayHit(const screen_pick::Ray& ray, float terrainT, const MapFile* map) const {
    screen_pick::Hit hit;
    for(int i=0;i<static_cast<int>(foes_.size());++i) {
        const Foe& foe=foes_[static_cast<std::size_t>(i)];
        if(foe.hp<=0 || foe.followsPlayer) continue;
        // O modelo pode ser maior que o circulo no terreno: selecionar o corpo inteiro.
        const float t=screen_pick::actor(ray,foe.x,map?map->heightAt(foe.x,foe.y):0.f,foe.y,std::max(.58f,foe.radius+.32f),2.1f);
        if(t<hit.t && screen_pick::visible(t,terrainT)) hit={1,i,t};
    }
    for(int i=0;i<static_cast<int>(loots_.size());++i) {
        const Loot& loot=loots_[static_cast<std::size_t>(i)];
        const float t=screen_pick::sphere(ray,loot.x,(map?map->heightAt(loot.x,loot.y):0.f)+.60f,loot.y,.85f);
        if(t<hit.t && screen_pick::visible(t,terrainT)) hit={2,i,t};
    }
    return hit;
}

bool Play::requestLoot(int index,float px,float py,float& tx,float& ty) {
    if(index<0 || index>=static_cast<int>(loots_.size())) return false;
    target_=-1;
    pendingBasicStrike_ = false;
    armedSkill_ = -1;
    requestedSkill_ = -1;
    requestedLoot_=index;
    const Loot& loot=loots_[static_cast<std::size_t>(index)];
    tx=loot.x; ty=loot.y;
    collectRequested(px,py);
    return true;
}

void Play::pick(float x, float y) {
    int best=-1;
    float bestD=2.2f;
    for(int i=0;i<static_cast<int>(foes_.size());++i) {
        const Foe& foe=foes_[static_cast<std::size_t>(i)];
        if(foe.hp<=0 || foe.followsPlayer) continue;
        const float d=std::hypot(foe.x-x,foe.y-y);
        if(d<bestD) { bestD=d; best=i; }
    }
    target_=best;
    pendingBasicStrike_ = false;
    if (best < 0) { armedSkill_ = -1; requestedSkill_ = -1; }
    requestedLoot_=-1;
    if(best>=0) say("Alvo: "+foes_[static_cast<std::size_t>(best)].name);
}

int Play::cursorOver(float x, float y) const {
    for (const Loot& loot : loots_) {
        const float dx = loot.x - x;
        const float dy = loot.y - y;
        if (dx * dx + dy * dy < 1.15f * 1.15f) return 2;
    }
    float best = 2.2f;
    bool hit = false;
    for (const Foe& mob : foes_) {
        if (mob.hp <= 0 || mob.followsPlayer) continue;
        const float dx = mob.x - x;
        const float dy = mob.y - y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < best) {
            best = dist;
            hit = true;
        }
    }
    return hit ? 1 : 0;
}

bool Play::skillArmed() const { return requestedSkill_ >= 0 || armedSkill_ >= 0; }

void Play::clearTarget() {
    target_ = -1;
    pendingBasicStrike_ = false;
    armedSkill_ = -1;
    requestedSkill_ = -1;
}

bool Play::aim(float px, float py, float& yaw) const {
    if (target_ < 0 || target_ >= static_cast<int>(foes_.size())) return false;
    const Foe& foe = foes_[static_cast<std::size_t>(target_)];
    if (foe.hp <= 0 || foe.followsPlayer) return false;
    const float dx = foe.x - px;
    const float dy = foe.y - py;
    if (dx * dx + dy * dy < 1e-6f) return false;
    yaw = std::atan2(dx, dy);
    return true;
}

bool Play::chasePoint(float px, float py, float& outX, float& outY) const {
    if (target_ < 0 || target_ >= static_cast<int>(foes_.size())) return false;
    const Foe& foe = foes_[static_cast<std::size_t>(target_)];
    if (foe.hp <= 0 || foe.followsPlayer) return false;
    const float dx = foe.x - px;
    const float dy = foe.y - py;
    const float dist = std::sqrt(dx * dx + dy * dy);
    // Selecting a foe never begins automatic combat. A manual attack or an
    // explicitly armed skill may approach its own historical range. Attack
    // logic consumes the command once, without repeating after the cast.
    if (!pendingBasicStrike_ && armedSkill_ < 0 && requestedSkill_ < 0) return false;
    const float reach = attackReach(foe);
    if (dist <= reach || dist < 1e-3f) {
        outX = px;
        outY = py;
        return true;
    }
    // Para dentro do alcance. Parar na borda deixa o corpo a um passo
    // curto e o golpe, que exige dist <= alcance, nunca sai.
    const float stop = historical_skill::approachStopMeters(reach);
    outX = foe.x - dx / dist * stop;
    outY = foe.y - dy / dist * stop;
    return true;
}

bool Play::lockMove() const { return deadFor_ > 0.f || playerStunFor_ > 0.f; }

int Play::playerPose() const {
    if (deadFor_ > 0.f) return 3;
    if (swing_ > 0.f) return 2;
    if (sitting_) return 4;
    if (castingFor_ > 0.f && castingActionBase_ > 0) return 5;
    return 0;
}

void Play::toggleSit() {
    if (deadFor_ > 0.f) return;
    sitting_ = !sitting_;
    say(sitting_ ? "Sentou. HP e SP sobem mais rapido." : "Levantou.");
}

} // namespace top
