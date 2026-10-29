#include "top/Play.h"
#include "top/PlayCombatRules.h"
#include "top/JobNames.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace top {
using namespace play_combat;
namespace {
const char* jobWeapon(int job) {
    switch (job) {
    case 1: case 3: case 8: case 9: case 10: return "espada";
    case 2: case 12: return "arco";
    case 4: case 16: return "adaga";
    case 5: case 13: case 14: return "cajado";
    case 6: case 7: case 11: case 15: case 17: case 18: return "arma da classe";
    default: return "";
    }
}

// Coeficientes de ExAttrCheck definidos em AttrCalculate.lua (JobType 0..18).
struct JobRad {
    float hp1, hp2, hplv;
    float sp1, sp2, splv;
    float str1, str2, dex1, dex2;
    float def1, def2;
    float hit, flee;
    float hrecHp, hrecCon;
    float srecSp, srecSta;
    float aspd;
};

JobRad jobRad(int job) {
    if (job == 1) {
        return {5.f, 7.f, 25.f, 1.f, 0.f, 3.f, 1.5f, 0.4f, 0.f, 0.f, 0.2f, 0.2f, 0.6f, 0.6f, 1.f / 180.f, 1.f / 8.f,
                1.f / 100.f, 1.f / 12.f, 1.1f};
    }
    if (job == 2) {
        return {3.f, 3.f, 25.f, 1.f, 0.f, 3.f, 0.f, 0.f, 1.7f, 0.4f, 0.14f, 0.1f, 0.7f, 0.7f, 1.f / 180.f, 1.f / 8.f,
                1.f / 100.f, 1.f / 12.f, 1.2f};
    }
    if (job == 4) {
        return {5.f, 5.f, 25.f, 2.f, 1.5f, 5.f, 1.5f, 0.4f, 0.f, 0.f, 0.13f, 0.1f, 0.6f, 0.6f, 1.f / 180.f, 1.f / 8.f,
                1.f / 100.f, 1.f / 12.f, 1.1f};
    }
    if (job == 5) {
        return {5.f, 5.f, 25.f, 2.f, 1.5f, 5.f, 1.5f, 0.4f, 0.f, 0.f, 0.13f, 0.1f, 0.6f, 0.6f,
                1.f / 180.f, 1.f / 8.f, 1.f / 100.f, 1.f / 12.f, 1.1f};
    }
    if (job == 3) { // Sailor
        return {3.f, 2.f, 15.f, .5f, .5f, 1.f, .9f, .9f, 0.f, 0.f, .45f, .45f, .31f, .31f,
                1.f/200.f, 1.f/100.f, 1.f/200.f, 1.f/120.f, 1.1f};
    }
    if (job == 6 || job == 7 || job == 11 || job == 15 || job == 17 || job == 18) {
        // Artisan/Merchant/Animal Tamer/Captain/Upstart/Engineer share these
        // coefficients, except for the listed evasion and attack-speed values.
        const float flee = job == 11 ? .5f : .31f;
        const float aspd = job == 11 ? 1.2f : 1.1f;
        return {2.f, 2.f, 10.f, .5f, .5f, 1.f, .8f, .8f, 0.f, 0.f, .5f, .5f, .31f, flee,
                1.f/200.f, 1.f/100.f, 1.f/200.f, 1.f/120.f, aspd};
    }
    if (job == 8) { // Champion
        return {6.f, 7.f, 40.f, 1.f, 0.f, 3.f, 1.f, .4f, 0.f, 0.f, .2f, .7f, .7f, .4f,
                1.f/180.f, 1.f/8.f, 1.f/100.f, 1.f/12.f, 1.f};
    }
    if (job == 9) { // Crusader
        return {3.5f, 3.5f, 30.f, 1.f, 0.f, 3.f, 1.5f, .45f, 0.f, 0.f, .2f, .2f, .6f, .75f,
                1.f/180.f, 1.f/8.f, 1.f/100.f, 1.f/12.f, 1.25f};
    }
    if (job == 10) { // White Knight
        return {5.f, 5.f, 30.f, 3.f, 1.5f, 5.f, 1.5f, .4f, 0.f, 0.f, .15f, .1f, .6f, .6f,
                1.f/180.f, 1.f/8.f, 1.f/100.f, 1.f/12.f, 1.1f};
    }
    if (job == 12) { // Sharpshooter
        return {5.5f, 5.5f, 30.f, 1.f, 0.f, 3.f, 0.f, 0.f, 2.f, .5f, .2f, .2f, .6f, .85f,
                1.f/180.f, 1.f/8.f, 1.f/100.f, 1.f/12.f, 1.2f};
    }
    if (job == 13) { // Cleric
        return {3.5f, 3.5f, 30.f, 3.f, 1.5f, 5.f, 1.5f, .4f, 0.f, 0.f, .15f, .15f, .6f, .85f,
                1.f/180.f, 1.f/8.f, 1.f/100.f, 1.f/12.f, 1.1f};
    }
    if (job == 14) { // Seal Master
        return {5.5f, 5.5f, 30.f, 3.f, 1.5f, 5.f, 1.5f, .4f, 0.f, 0.f, .2f, .3f, .6f, .75f,
                1.f/180.f, 1.f/8.f, 1.f/100.f, 1.f/12.f, 1.1f};
    }
    if (job == 16) { // Voyager
        return {6.f, 6.f, 35.f, 3.f, 1.5f, 5.f, 1.5f, .4f, 0.f, 0.f, .2f, .4f, .6f, .7f,
                1.f/180.f, 1.f/8.f, 1.f/100.f, 1.f/12.f, 1.1f};
    }
    return {3.f, 2.f, 15.f, 1.f, 0.f, 3.f, 1.5f, 0.4f, 0.f, 0.f, 0.1f, 0.1f, 0.6f, 0.6f, 1.f / 200.f, 1.f / 8.f,
            1.f / 100.f, 1.f / 12.f, 1.1f};
}

// AttrCalculate.lua: Crt_luk_rad is 0.25 for Hunter (job 2), 0.31 for
// every other supported job.
float criticalLuckRad(int job) { return job == 2 ? .25f : .31f; }

int formulaHp(const JobRad& rad, int con, int level) {
    const double quad = static_cast<double>(rad.hp2) * std::pow(std::floor(con * 3.0 / 20.0), 2.0);
    return static_cast<int>(std::floor(con * 3.0 * rad.hp1 + quad + level * static_cast<double>(rad.hplv) + 40.0));
}

int formulaSp(const JobRad& rad, int sta, int level) {
    const double quad = static_cast<double>(rad.sp2) * std::pow(std::floor(sta * 3.0 / 20.0), 2.0);
    return static_cast<int>(std::floor(sta * 3.0 * rad.sp1 + quad + level * static_cast<double>(rad.splv) + 5.0));
}

int formulaAtk(float primary, float quadRad, int stat) {
    const double quad = static_cast<double>(quadRad) * std::pow(std::floor(stat * 4.0 / 20.0), 2.0);
    return static_cast<int>(std::floor(stat * static_cast<double>(primary) + quad));
}

int formulaDef(const JobRad& rad, int con) {
    const double quad = static_cast<double>(rad.def2) * std::floor(std::pow(con * 3.0 / 20.0, 2.0));
    return static_cast<int>(std::floor(con * 5.0 * rad.def1 + quad));
}

int cookingPotionDefense(int level) {
    static constexpr int bonus[] = {0, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8};
    return level >= 0 && level <= 10 ? bonus[level] : 0;
}

int cookingPotionAttack(int level) {
    return level > 0 && level <= 10 ? 50 + (level - 1) * 100 : 0;
}

} // namespace

void Play::recomputeGear() {
    const auto gear = gearBonuses();
    const int baseMinimum = applyAttackCoefficient(baseMin_ + attrMin_, gear.minAttackCoefficient);
    const int baseMaximum = applyAttackCoefficient(baseMax_ + attrMax_, gear.maxAttackCoefficient);
    const int foodAttack = cookingPotionAttack(foodStates_[1].level);
    player_.minAtk = std::max(0, baseMinimum + gear.minAttack + foodAttack + itemBodyAttackBonus_ +
                                itemPotionAttackState_.bonus +
                                (itemMaliciousHornState_.remaining > 0.f ? itemMaliciousHornState_.bonus : 0));
    player_.maxAtk = std::max(player_.minAtk, baseMaximum + gear.maxAttack + foodAttack +
                              itemBodyAttackBonus_ + itemPotionAttackState_.bonus +
                              (itemMaliciousHornState_.remaining > 0.f ? itemMaliciousHornState_.bonus : 0));
    player_.def = std::max(0, baseDef_ + attrDef_ + gear.defense +
        cookingPotionDefense(foodStates_[0].level) + itemBodyDefenseBonus_ +
        itemLotteryDefenseState_.bonus + itemLanternDefenseState_.bonus +
        (itemFairyMarchState_.remaining > 0.f ? itemFairyMarchState_.bonus : 0));
}

void Play::applyAttrs(bool fillGain) {
    if (!sheetReady_) return;
    const auto gear = gearBonuses();
    const JobRad rad = jobRad(job_);
    const int level = std::max(1, player_.level);
    // Bônus primários de equipamento entram na fórmula ANTES do cálculo dos
    // atributos derivados, mas não alteram os pontos permanentes do save.
    const auto primary=effectivePrimaryAttributes();
    const int str=primary[0],con=primary[1],agi=primary[2],dex=primary[3],sta=primary[4];
    const auto passive=passiveBonuses();
    // characterinfo.txt supplies actor/NPC template stats (e.g. 40 HP and
    // 4-5 attack for the race models), not a playable character's base stats.
    // Recompute both the Newbie baseline and the selected class from the
    // historical AttrCalculate formulas instead of adding a class delta to
    // those unrelated template values. Keeping the baseline fields also lets
    // the existing Q-save format migrate transparently when it is loaded.
    const JobRad newbie = jobRad(0);
    sheetHp_ = formulaHp(newbie, con, level);
    sheetSp_ = formulaSp(newbie, sta, level);
    baseMin_ = formulaAtk(newbie.str1, newbie.str2, str) + formulaAtk(newbie.dex1, newbie.dex2, dex);
    baseMax_ = baseMin_;
    baseDef_ = formulaDef(newbie, con);
    const int dHp = formulaHp(rad, con, level) - sheetHp_;
    const int dSp = formulaSp(rad, sta, level) - sheetSp_;
    const int classAttack = formulaAtk(rad.str1, rad.str2, str) + formulaAtk(rad.dex1, rad.dex2, dex);
    const int dMin = classAttack - baseMin_;
    const int dMax = dMin;
    const int dDef = formulaDef(rad, con) - baseDef_;
    const int oldHp = player_.maxHp;
    const float oldSp = player_.homeX;
    const int foodHp = foodStates_[0].level * foodStates_[0].level * 100;
    player_.maxHp = std::max(1, sheetHp_ + dHp + gear.hp + foodHp + itemPotionMaxHpState_.bonus+passive.hp);
    player_.homeX = static_cast<float>(std::max(1, sheetSp_ + dSp + gear.sp+passive.sp));
    if (fillGain && player_.maxHp > oldHp) player_.hp += player_.maxHp - oldHp;
    player_.hp = std::clamp(player_.hp, 0, player_.maxHp);
    if (fillGain && player_.homeX > oldSp) player_.homeY += player_.homeX - oldSp;
    player_.homeY = std::clamp(player_.homeY, 0.f, player_.homeX);
    attrMin_ = dMin;
    attrMax_ = dMax;
    attrDef_ = dDef;
    recomputeGear();
    player_.resist = std::clamp(baseResist_ + gear.resistance+passive.resistance, 0, 1000);
    const int luck = std::max(0, baseLuck_ + gear.luk);
    player_.crit = std::clamp(11 + static_cast<int>(std::floor(luck * 3.f * criticalLuckRad(job_))) +
                              gear.critical, 0, 100);
    const int formHp = std::max(1, formulaHp(rad, con, level));
    const int formSp = std::max(1, formulaSp(rad, sta, level));
    const double hrec = std::max(2.0 * formHp * rad.hrecHp + con * 3.0 * rad.hrecCon, 1.0);
    const double srec = std::max((formSp * rad.srecSp + sta * 3.0 * rad.srecSta) / 2.0, 1.0);
    hrecGain_ = std::max(1, static_cast<int>(std::lround(hrec)) + gear.hpRegen+passive.hpRegen);
    srecGain_ = std::max(1, static_cast<int>(std::lround(srec)) + gear.spRegen +
        (itemFairyMarchState_.remaining > 0.f ? 20 : 0)+passive.spRegen);
    hit_ = std::max(0, static_cast<int>(std::floor(dex * static_cast<double>(rad.hit))) + level * 2 + 5 +
                       gear.hit + itemPotionHitState_.bonus+passive.hit);
    flee_ = std::max(0, static_cast<int>(std::floor(agi * static_cast<double>(rad.flee))) + level * 2 + 5 + gear.dodge+passive.dodge);
    player_.hit = hit_;
    player_.dodge = flee_ + itemEtherCloverDodge_;
    // MSPD em centimetros por segundo, como em characterinfo/ItemInfo.
    player_.speed = std::clamp(baseMoveSpeed_ + gear.moveSpeed / 100.f, 0.25f, 16.f);
    const float baseAspd = 65.f + 5.f * rad.aspd;
    const float nowAspd = 65.f + static_cast<float>(agi) * rad.aspd;
    // ASPD do iteminfo e coeficiente aditivo por equipamento (radix=1000),
    // seguindo AspdIa/Aspd_final do functions.lua. Duas espadas acumulam.
    const float gearAspd = std::clamp(1.f + static_cast<float>(gear.aspdCoefficient) / 1000.f, .25f, 4.f);
    // State_PKKBYS adds 140 to historical AspdSb, after the item/state
    // multipliers. Keep it in that additive stage and let the normal clamp
    // bound the OpenGL attack interval.
    const float aspdStateBonus = itemPotionAspdState_.remaining > 0.f
        ? static_cast<float>(itemPotionAspdState_.bonus) : 0.f;
    player_.cd = std::clamp(0.85f * baseAspd /
                            std::max(1.f, nowAspd * gearAspd + aspdStateBonus), 0.23f, 1.7f);
    appliedSkillAttributes_=learnedSkills_;
}

void Play::spendAttr(int which) {
    if (deadFor_ > 0.f) return;
    int* slot = nullptr;
    const char* name = "";
    if (which == 0) {
        slot = &bstr_;
        name = "FOR";
    } else if (which == 1) {
        slot = &bagi_;
        name = "AGI";
    } else if (which == 2) {
        slot = &bcon_;
        name = "CON";
    } else if (which == 3) {
        slot = &bsta_;
        name = "STA";
    } else if (which == 4) {
        slot = &bdex_;
        name = "DEX";
    }
    if (!slot) return;
    if (ap_ < 1) {
        say("Sem pontos de atributo.");
        return;
    }
    if (*slot >= 200) return;
    --ap_;
    ++*slot;
    applyAttrs(true);
    char line[96];
    std::snprintf(line, sizeof line, "%s %d. Pontos %d.", name, *slot, ap_);
    say(line);
    save(savePath());
}

void Play::setJob(int job) {
    if (deadFor_ > 0.f) return;
    if (job != 1 && job != 2 && job != 4 && job != 5) return;
    if (job_ != 0) {
        say("Voce ja concluiu a primeira promocao de classe.");
        return;
    }
    // NpcSdk.lua ClassChange(Type=1): primeira promocao a partir do nivel 9,
    // com custo de 2.500 ouro e apenas uma troca definitiva.
    constexpr int firstClassLevel = 9;
    constexpr int firstClassCost = 2500;
    if (player_.level < firstClassLevel) {
        say("Precisa do nivel " + std::to_string(firstClassLevel) + " para a promocao.");
        return;
    }
    if (gold_ < firstClassCost) {
        say("Precisa de " + std::to_string(firstClassCost) + " ouro para a promocao.");
        return;
    }
    const int race = player_.id;
    const bool compatible = (job == 1 && race != 4) ||
                            (job == 2 && race != 2 && race != 4) ||
                            (job == 4 && race != 2) || (job == 5 && (race == 3 || race == 4));
    if (!compatible) {
        say(player_.name + " nao pode seguir essa classe.");
        return;
    }
    gold_ -= firstClassCost;
    job_ = job;
    applyAttrs(true);
    if (job_ == 2) say("Hunter. A arma e o arco.");
    else say(std::string(jobName(job_)) + ". A arma e a " + jobWeapon(job_) + ".");
    save(savePath());
}

bool Play::testSetJob(int job) {
    if (deadFor_ > 0.f || job < 1 || job > 18) return false;
    const int previousJob=job_;
    job_ = job;
    applyAttrs(true);
    const std::string path=savePath();
    if(!path.empty() && !save(path)) {
        job_=previousJob;
        applyAttrs(true);
        say("Modo de teste: falha ao salvar; classe anterior preservada.");
        return false;
    }
    say("Modo de teste: classe alterada para " + std::string(jobName(job_)) +
        (path.empty() ? "." : " e salva."));
    return true;
}

// Cada pilha guarda no maximo 99. Quando o item nao cabe, devolve false e nao
// mexe em nada: antes a pilha era cortada em 99 e o excesso sumia, mas a
// funcao dizia que tinha dado certo (a compra gastava o ouro sem entregar).
// Atomic: simulate the complete insertion before mutating the inventory.
} // namespace top

namespace top {
Play::PassiveBonuses Play::passiveBonuses() const {
    PassiveBonuses result;
    for(const auto& learned:learnedSkills_) {
        const auto rule=skillRules_.find(learned.first);
        if(rule==skillRules_.end() || learned.second<=0)continue;
        const int level=learned.second;const auto& callback=rule->second.passiveFormula;
        // The supplied skilleffect.lua callbacks. Derive from callback names,
        // not IDs: other tables can assign the same mechanics to other skills.
        if(callback=="Skill_Qhtz_Use") {
            result.hp+=20*level+effectivePrimaryAttributes()[1]*3;
            if(worldMap_=="garner2")for(const auto& other:learnedSkills_) {
                const auto steel=skillRules_.find(other.first);
                if(steel!=skillRules_.end() && steel->second.effectFormula=="Skill_Gtyz_End")
                    result.resistance+=std::max(0,other.second);
            }
        }
        else if(callback=="Skill_Lqhb_Use")result.dodge+=3*level;
        else if(callback=="Skill_Jdzz_Use")result.hit+=level;
        else if(callback=="Skill_Jsjc_Use")result.sp+=40*level;
        else if(callback=="Skill_Tzhf_Use")result.hpRegen+=level;
        else if(callback=="Skill_Sy_Use")result.spRegen+=1+level;
        else if(callback=="Skill_Jfb_Use")result.moveCoefficient+=.02f*level;
    }
    return result;
}
std::array<int,5> Play::effectivePrimaryAttributes() const {
    const auto b=gearBonuses();
    return {{std::max(0,bstr_+b.str+itemStrengthState_.bonus+itemPotionStrengthState_.bonus+itemInnocenceState_.bonuses[0]),
        std::max(0,bcon_+b.con+itemConstitutionState_.bonus+itemPotionConstitutionState_.bonus+itemInnocenceState_.bonuses[2]),
        std::max(0,bagi_+b.agi+itemAgilityState_.bonus+itemPotionAgilityState_.bonus+itemInnocenceState_.bonuses[1]),
        std::max(0,bdex_+b.dex+itemAccuracyState_.bonus+itemPotionAccuracyState_.bonus+itemInnocenceState_.bonuses[3]),
        std::max(0,bsta_+b.sta+foodStates_[2].level*5+itemSpiritState_.bonus+itemPotionSpiritState_.bonus+
            itemInnocenceState_.bonuses[4]+itemKaraVictoryState_.bonus)}};
}
float Play::effectiveAttackInterval() const {
    const float berserk=berserkFor_>0.f?1.f+std::max(0.f,berserkAttackSpeedBonus_):1.f;
    const float numb=playerNumbFor_>0.f?1.f+.1f+.03f*playerNumbLevel_:1.f;
    return player_.cd/berserk*numb;
}
int Play::effectiveDefense() const {
    return play_combat::playerDefenseWithStates(player_.def,hardenDefense_+itemHardenDefense_,
        genericBuffDefense_,willOfSteelDefense_);
}
int Play::equippedFairyItemId() const {
    const auto worn=worn_.find(16),uid=wornUid_.find(16);
    if(worn==worn_.end() || uid==wornUid_.end())return 0;
    const auto info=items_.find(worn->second);
    if(info==items_.end() || info->second.type!=59)return 0;
    for(const auto& stack:bag_)if(stack.id==worn->second && stack.uid==uid->second)
        return stack.durability>=50?stack.id:0; // CCharacter::RefreshLink
    return 0;
}
std::string Play::statDiagnostic() const {
    const auto p=effectivePrimaryAttributes();
    std::ostringstream out;
    out<<"{\"str\":"<<p[0]<<",\"con\":"<<p[1]<<",\"agi\":"<<p[2]
       <<",\"dex\":"<<p[3]<<",\"spr\":"<<p[4]<<",\"min_attack\":"<<player_.minAtk+genericBuffAttack_
       <<",\"max_attack\":"<<player_.maxAtk+genericBuffAttack_<<",\"defense\":"<<effectiveDefense()
       <<",\"move_cm_s\":"<<std::lround(moveSpeed()*100.f)
       <<",\"attacks_s\":"<<1.f/std::max(.01f,effectiveAttackInterval())
       <<",\"fairy_item\":"<<equippedFairyItemId()<<"}";
    return out.str();
}
bool Play::configureStatPreview(const std::string& scenario) {
    if(!readOnlyPreview_ || (scenario!="base" && scenario!="necklace" && scenario!="berserk" &&
        scenario!="harden" && scenario!="fairy" && scenario!="fairy-exhausted" && scenario!="passives"))return false;
    // A controlled RAM-only Swordsman. No inventory grants, saves, permanent
    // points or job changes escape this screenshot process.
    job_=1;player_.level=20;sheetReady_=true;
    bstr_=bcon_=bagi_=bdex_=bsta_=5;ap_=tp_=0;
    baseMoveSpeed_=4.8f;worn_.clear();wornUid_.clear();
    learnedSkills_.clear();
    berserkFor_=hardenFor_=willOfSteelFor_=genericBuffFor_=playerNumbFor_=0.f;
    berserkAttackSpeedBonus_=0.f;hardenDefense_=willOfSteelDefense_=genericBuffDefense_=genericBuffAttack_=0;
    if(scenario=="necklace") {
        const auto item=items_.find(418);if(item==items_.end())return false;
        worn_[5]=418;
    }
    if(scenario=="fairy" || scenario=="fairy-exhausted") {
        if(!items_.count(183))return false;
        Stack fairy;fairy.id=183;fairy.count=1;fairy.uid=nextItemUid_++;
        fairy.durability=fairy.maxDurability=5000;fairy.fairyStats={{5,3,2,1,4}};
        if(scenario=="fairy-exhausted")fairy.durability=49;
        bag_.push_back(fairy);worn_[16]=fairy.id;wornUid_[16]=fairy.uid;
    }
    if(scenario=="passives")for(int id:{64,65,66,72,75,79,80})learnedSkills_[id]=1;
    applyAttrs(false);player_.hp=player_.maxHp;player_.homeY=player_.homeX;
    if(scenario=="berserk" || scenario=="harden") {
        const int id=scenario=="berserk"?84:225;
        if(!skillRules_.count(id))return false;
        learnedSkills_[id]=1;
        if(!assignSkillToSlot(id,0))return false;
        skillCd_[0]=0.f;castSkill(0);
    }
    return true;
}
}
