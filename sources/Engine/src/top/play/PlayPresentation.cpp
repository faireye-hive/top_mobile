#include "top/Play.h"
#include "top/JobNames.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace top {
namespace {
void project(const float* m, float x, float height, float y, int screenW, int screenH, float& sx, float& sy) {
    const float cx = m[0] * x + m[4] * height + m[8] * y + m[12];
    const float cy = m[1] * x + m[5] * height + m[9] * y + m[13];
    float cw = m[3] * x + m[7] * height + m[11] * y + m[15];
    if (std::fabs(cw) < 1e-4f) cw = 1.f;
    const float nx = cx / cw, ny = cy / cw;
    sx = (nx * .5f + .5f) * static_cast<float>(screenW);
    sy = (.5f - ny * .5f) * static_cast<float>(screenH);
}
} // namespace

void Play::syncActionTiming(const ActorSkin& actors) {
    const int mode = actors.report().playerPoseMode;
    if (actionTimingMode_ == mode) return;
    actionTimingMode_ = mode;
    attackWindup_ = actors.playerAttackWindup();
    attackDuration_ = actors.playerAttackDuration();
    for (int action = 1; action <= 54; ++action) {
        actionDurations_[action] = actors.playerActionDuration(action);
        actionImpacts_[action] = actors.playerActionWindup(action);
    }
    for (auto& entry : skillRules_) {
        SkillRule& rule = entry.second;
        const int action = rule.playActions.empty() ? 11 : rule.playActions.front();
        rule.animationDuration = actionDurations_[action];
        rule.impactOffset = actionImpacts_[action];
        rule.actionDurations.clear();
        rule.actionImpacts.clear();
        for (int poseAction : rule.playActions) {
            rule.actionDurations.push_back(actionDurations_[poseAction]);
            rule.actionImpacts.push_back(actionImpacts_[poseAction]);
        }
    }
}

void Play::pushActors(ActorSkin& actors) {
    std::unordered_map<int, int> visualWorn = worn_;
    for (const auto& equipped : wornUid_) {
        const auto wornItem = worn_.find(equipped.first);
        if (wornItem == worn_.end()) continue;
        const auto item = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& stack) {
            return stack.uid == equipped.second && stack.id == wornItem->second;
        });
        if (item != bag_.end() && item->fuseId > 0)
            visualWorn[equipped.first] = item->fuseId;
    }
    // Hairstyle is a persisted appearance choice from hairs.txt, not an
    // equipment item: a worn head item still takes visual precedence.
    if (hairStyleItemId_>0 && !visualWorn.count(0))
        visualWorn[0]=hairStyleItemId_;
    actors.setEquipment(visualWorn);
    syncActionTiming(actors);
    const float hp01 = player_.maxHp > 0 ? static_cast<float>(player_.hp) / static_cast<float>(player_.maxHp) : 0.f;
    const float spMax = std::max(1.f, player_.homeX);
    const float sp01 = std::max(0.f, player_.homeY) / spMax;
    const float exp01 = expNext_ > 0 ? static_cast<float>(exp_) / static_cast<float>(expNext_) : 0.f;
    actors.setVitals(hp01, sp01, exp01);
    actors.setPlayerPose(playerPose());
    actors.setPlayerActionBase(swing_ > 0.f ? attackActionBase_ : castingActionBase_);
    actors.setPlayerActionOnce(swing_ > 0.f || castingFor_ > 0.f);
    actors.setPlayerActionPlaybackRate(swing_ > 0.f ? swingPlaybackRate_ :
        castingFor_ > 0.f ? castingPlaybackRate_ : 1.f);
    actors.setPlayerActionCycle(attackSequence_);
    actors.setPlayerWeaponsDrawn(playerFightArea_);
    actors.setJobMark(job_);
    actors.setLiveLine(1, std::to_string(player_.level));
    actors.setLiveLine(2, std::to_string(player_.hp) + "/" + std::to_string(player_.maxHp));
    actors.setLiveLine(3, std::to_string(static_cast<int>(player_.homeY)) + "/" + std::to_string(static_cast<int>(player_.homeX)));
    const auto equipmentBonus = gearBonuses();
    const auto primary=effectivePrimaryAttributes();
    actors.setLiveLine(4, std::to_string(primary[0]));
    actors.setLiveLine(5, std::to_string(primary[2]));
    actors.setLiveLine(6, std::to_string(primary[3]));
    actors.setLiveLine(7, std::to_string(primary[1]));
    actors.setLiveLine(8, std::to_string(primary[4]));
    actors.setLiveLine(10, jobName(job_));
    char expLine[32];
    std::snprintf(expLine, sizeof expLine, "%d/%d", exp_, expNext_);
    actors.setLiveLine(11, expLine);
    actors.setLiveLine(12, std::to_string(ap_));
    actors.setLiveLine(13, std::to_string(tp_));
    actors.setLiveLine(14, std::to_string(player_.minAtk+genericBuffAttack_));
    actors.setLiveLine(15, std::to_string(player_.maxAtk+genericBuffAttack_));
    actors.setLiveLine(16, std::to_string(hit_));
    actors.setLiveLine(17, std::to_string(player_.dodge));
    actors.setLiveLine(18, std::to_string(effectiveDefense()));
    char aspdLine[24];
    std::snprintf(aspdLine,sizeof(aspdLine),"%.2f/s",1.f/std::max(.01f,effectiveAttackInterval()));
    actors.setLiveLine(19, aspdLine);
    for (int i = 0; i < static_cast<int>(foes_.size()); ++i) {
        const Foe& foe = foes_[static_cast<std::size_t>(i)];
        int pose = 0;
        bool hide = false;
        if (foe.hp <= 0) {
            pose = 3;
            hide = foe.deadFor > 1.1f;
        } else if (foe.swing > 0.f && foe.stunFor <= 0.f) {
            pose = 2;
        } else if (foe.moving) {
            pose = 1;
        }
        // O cliente antigo nao mantinha a barra de todos os monstros da regiao
        // permanentemente aberta. Alem de poluir a cidade, isso fazia dezenas
        // de quads aparecerem atraves dos objetos. Mostra o alvo atual e quem
        // ja sofreu dano; -1 desliga a barra no ActorSkin.
        const bool showBar = i == target_ || foe.hp < foe.maxHp;
        const float bar = showBar && foe.maxHp > 0
                              ? static_cast<float>(foe.hp) / static_cast<float>(foe.maxHp)
                              : -1.f;
        actors.setMonster(foe.actorSlot >= 0 ? foe.actorSlot : i, foe.x, foe.y, foe.yaw, foe.moving, pose, hide, bar,
                          foe.action, foe.playbackRate, foe.actionCycle);
    }
    // 05-AB: historical item inspection. The values originate from iteminfo,
    // the UID-specific stack and the same forge catalog used by gearBonuses().
    // Never print fabricated current durability, stat rolls or forging rates.
    using TL = ui::TooltipLine;
    auto makeTooltip = [&](const Stack& stack, bool equipped) {
        std::vector<TL> tip;
        const auto found = items_.find(stack.id);
        const ItemInfo* info = found == items_.end() ? nullptr : &found->second;
        auto add = [&](std::string text, TL::Kind kind = TL::Kind::Normal) {
            if (!text.empty()) tip.push_back({std::move(text),kind});
        };
        add(info ? info->name : "Item " + std::to_string(stack.id), TL::Kind::Title);
        add("ID " + std::to_string(stack.id) + (stack.uid > 0 ? "  |  UID " + std::to_string(stack.uid) : ""), TL::Kind::Muted);
        if (equipped) add("[Equipped]", TL::Kind::Gem);
        if (stack.count > 1) add("Quantity: " + std::to_string(stack.count));
        if (!info) return tip;
        if (info->minAtk > 0 || info->maxAtk > 0)
            add("Attack: " + std::to_string(info->minAtk) + " - " + std::to_string(info->maxAtk));
        if (info->def > 0) add("Defense: " + std::to_string(info->def));
        if (stack.quality > 0)
            add("Quality: " + std::to_string(stack.quality),TL::Kind::Gem);
        if (stack.id == 9604 && stack.cloakLevel > 0)
            add("Admiral Cloak level: " + std::to_string(stack.cloakLevel) + "/5", TL::Kind::Gem);
        if (stack.equipmentUpgradeLevel > 0)
            add("Effectiveness level: " + std::to_string(stack.equipmentUpgradeLevel) + "/10", TL::Kind::Gem);
        if (stack.maxDurability > 0 && stack.maxDurability < 25000)
            add("Durability: " + std::to_string(stack.durability / 50) + "/" +
                std::to_string(stack.maxDurability / 50),TL::Kind::Muted);
        else if (info->maxDurability > 0 && info->maxDurability < 25000)
            // Original UIItemCommand.cpp presents sEndure / 50. We have only
            // the table maximum, not the instance's current wear value.
            add("Maximum durability: " + std::to_string(info->maxDurability / 50),TL::Kind::Muted);
        if (stack.maxEnergy > 0)
            add("Energy: " + std::to_string(stack.energy) + "/" + std::to_string(stack.maxEnergy),TL::Kind::Muted);
        if (info->level > 0)
            add("Level requirement: " + std::to_string(info->level),
                info->level > player_.level ? TL::Kind::Warning : TL::Kind::Normal);
        // JobType.lua: numeric IDs are not the same as player level.
        static const char* jobNames[] = {
            "Newbie","Swordsman","Hunter","Sailor","Explorer","Herbalist",
            "Artisan","Merchant","Champion","Crusader","White Knight",
            "Animal Tamer","Sharpshooter","Cleric","Seal Master","Captain",
            "Voyager","Upstart","Engineer"};
        std::string classes, characters;
        for(int id: info->jobs) {
            if(id==-1) {classes="All"; break;}
            if(id<0) continue; // -2 pads the fixed-width iteminfo arrays
            if(!classes.empty()) classes+=", ";
            classes+= id < int(sizeof(jobNames)/sizeof(jobNames[0])) ? jobNames[id] : "#"+std::to_string(id);
        }
        if(!classes.empty()) add("Classes: "+classes,TL::Kind::Muted);
        static const char* avatarNames[]={"", "Lance", "Carsise", "Phyllis", "Ami"};
        for(int id: info->races) {
            if(id==-1) {characters="All";break;}
            if(id<0) continue;
            if(!characters.empty()) characters+=", ";
            characters+= id>=1 && id<=4 ? avatarNames[id] : "#"+std::to_string(id);
        }
        if(!characters.empty()) add("Characters: "+characters,TL::Kind::Muted);
        const auto bonusLines = [&](const equipment_stats::Bonus& b, const std::string& prefix, TL::Kind color, bool base) {
            auto stat = [&](const char* name,int amount) {
                if(amount) add(prefix + name + ": " + (amount>0?"+":"") + std::to_string(amount),color);
            };
            stat("Strength",b.str); stat("Agility",b.agi); stat("Dexterity",b.dex);
            stat("Constitution",b.con); stat("Spirit",b.sta); stat("Luck",b.luk);
            if (!base) {stat("Min Attack",b.minAttack);stat("Max Attack",b.maxAttack);stat("Defense",b.defense);}
            stat("Max HP",b.hp); stat("Max SP",b.sp); stat("Hit Rate",b.hit);
            stat("Dodge",b.dodge); stat("Critical",b.critical);
            stat("HP Recovery",b.hpRegen); stat("SP Recovery",b.spRegen);
            stat("Movement Speed",b.moveSpeed); stat("Resistance",b.resistance);
            if (b.aspdCoefficient) {
                char buf[40]; std::snprintf(buf,sizeof(buf),"%.1f%%",static_cast<double>(b.aspdCoefficient)/10.);
                add(prefix+"Attack Speed: " + (b.aspdCoefficient>0?"+":"") + buf,color);
            }
        };
        bonusLines(info->stats,"",TL::Kind::Bonus,true);
        bonusLines(stack.extra,"Individual ",TL::Kind::Bonus,false);
        if (stack.forgeLevel > 0) add("Forge level: +" + std::to_string(stack.forgeLevel), TL::Kind::Gem);
        if ((info->maxSockets > 0 || stack.socketCount > 0) &&
            (info->slot >= 0 || info->minAtk>0 || info->def>0)) {
            add("Sockets: " + std::to_string(stack.socketCount) + "/" + std::to_string(info->maxSockets), TL::Kind::Muted);
            for (int i=0;i<stack.socketCount && i<3;++i) {
                const int gemId=stack.gemIds[i],level=stack.gemLevels[i];
                if (!gemId || !level) {add("  Socket " + std::to_string(i+1) + ": Empty",TL::Kind::Muted); continue;}
                const auto gemInfo=items_.find(gemId);
                const auto gem=gemCatalog_.find(gemId);
                add("  Socket " + std::to_string(i+1) + ": " +
                    (gemInfo!=items_.end()?gemInfo->second.name:"Gem #"+std::to_string(gemId)) +
                    " Lv"+std::to_string(level),TL::Kind::Gem);
                if (gem != gemCatalog_.end() && gem->second.compatible(info->type))
                    bonusLines(forge_rules::effect(gem->second,level),"    ",TL::Kind::Gem,false);
            }
        }
        if ((info->type == 49 || info->type == 50) && stack.stoneLevel > 0)
            add("Gem level: " + std::to_string(stack.stoneLevel),TL::Kind::Gem);
        if (info->healHp > 0) add("Restores HP: " + std::to_string(info->healHp),TL::Kind::Gem);
        if (info->healSp > 0) add("Restores SP: " + std::to_string(info->healSp),TL::Kind::Gem);
        if (info->price > 0) add("Catalog price: " + std::to_string(info->price) + " gold",TL::Kind::Muted);
        return tip;
    };
    std::vector<ActorSkin::BagView> rows;
    for (int i = 0; i < static_cast<int>(bag_.size()); ++i) {
        const Stack& stack = bag_[static_cast<std::size_t>(i)];
        ActorSkin::BagView view;
        view.index = i;
        view.uid = stack.uid;
        view.tooltip = makeTooltip(stack, equippedSlotAtBagIndex(i) >= 0);
        view.id = stack.id;
        view.count = stack.count;
        view.worn = equippedSlotAtBagIndex(i) >= 0;
        const auto it = items_.find(stack.id);
        if (it != items_.end()) {
            view.equip = it->second.slot;
            view.icon = it->second.icon;
            view.name = it->second.name;
        } else {
            view.name = std::to_string(stack.id);
        }
        rows.push_back(std::move(view));
    }
    std::vector<ActorSkin::BagView> tempRows;
    const std::vector<Stack>& secondaryStorage = bankOpen_ ? bank_ : temp_;
    for (int i = 0; i < static_cast<int>(secondaryStorage.size()); ++i) {
        const Stack& stack = secondaryStorage[static_cast<std::size_t>(i)];
        ActorSkin::BagView view;
        view.index = i;
        view.uid = stack.uid;
        view.tooltip = makeTooltip(stack,false);
        view.id = stack.id;
        view.count = stack.count;
        const auto it = items_.find(stack.id);
        if (it != items_.end()) {
            view.equip = it->second.slot;
            view.icon = it->second.icon;
            view.name = it->second.name;
        } else {
            view.name = std::to_string(stack.id);
        }
        tempRows.push_back(std::move(view));
    }
    actors.setBagView(std::move(rows), worn_, std::move(tempRows), gold_, bagCapacity_, wornUid_, bankOpen_);
    std::vector<std::string> skillLines;
    std::vector<ActorSkin::SkillView> skillViews;
    std::vector<int> catalog;
    for (const auto& entry : skillRules_) {
        const SkillRule& rule = entry.second;
        const bool professional = lifeSkillIds_.count(rule.id) != 0;
        if (skillTestMode_ || learnedSkills_.count(rule.id) || (professional && lifeSkills_.count(rule.id)))
            catalog.push_back(rule.id);
    }
    for (const SkillDef& skill : skills_) if (skill.id > 0 && learnedSkills_.count(skill.id) &&
        std::find(catalog.begin(), catalog.end(), skill.id) == catalog.end()) catalog.push_back(skill.id);
    std::sort(catalog.begin(), catalog.end());
    for (int id : catalog) {
        const auto found = skillRules_.find(id);
        if (found == skillRules_.end()) continue;
        const SkillRule& rule = found->second;
        ActorSkin::SkillView view;
        view.id = id;
        view.professional = lifeSkillIds_.count(id) != 0;
        const bool isLearned = view.professional ? lifeSkills_.count(id) != 0 : learnedSkills_.count(id) != 0;
        const int learnedLevel = isLearned ?
            (view.professional ? lifeSkills_.at(id) : learnedSkills_.at(id)) : 0;
        view.name = rule.name + " Lv" + std::to_string(learnedLevel) +
            ((!view.professional && (rule.spFormulaMissing || rule.cooldownFormulaMissing)) ? " [fallback]" : "");
        view.level = learnedLevel;
        view.sp = view.professional || rule.type == 2 ? 0 : std::max(0, static_cast<int>(std::floor(
            rule.spBase + rule.spPerLevel * learnedLevel)));
        view.learned = isLearned || skillTestMode_;
        view.upgradeable = skillTestMode_ ? learnedLevel < 10 :
            (!view.professional && isLearned && learnedLevel < 10);
        view.icon = rule.icon;
        for (int slot = 0; slot < 8; ++slot) if (skills_[slot].id == id) {
            view.hotkey = slot;
            view.cooldown = skillCd_[slot] > 0.f;
        }
        skillViews.push_back(std::move(view));
    }
    for (int slot = 0; slot < 8; ++slot) {
        int shownSp = skills_[slot].sp;
        const auto shownRule = skillRules_.find(skills_[slot].id);
        if (shownRule != skillRules_.end()) {
            const int learnedLevel = std::max(1, learnedSkills_.count(skills_[slot].id) ?
                                               learnedSkills_.at(skills_[slot].id) : 1);
            shownSp = shownRule->second.type == 2 ? 0 : std::max(0, static_cast<int>(std::floor(
                shownRule->second.spBase + shownRule->second.spPerLevel * learnedLevel)));
        }
        std::string line = "F" + std::to_string(slot + 1) + "  " + skills_[slot].name + "  SP " +
                           std::to_string(shownSp);
        if (!learnedSkills_.count(skills_[slot].id)) line += "  nao aprendida";
        if (skillCd_[slot] > 0.f) line += "  espera";
        skillLines.push_back(std::move(line));
    }
    if (berserkFor_ > 0.f) skillLines.push_back("Berserk ativo");
    if (hardenFor_ > 0.f) skillLines.push_back("Harden +" + std::to_string(hardenDefense_) + " DEF");
    if (willOfSteelFor_ > 0.f) skillLines.push_back("Will of Steel +" + std::to_string(willOfSteelDefense_) + " DEF");
    std::array<std::string, 8> barIcons{};
    for (int i=0;i<8;++i) {
        if (skills_[i].id <= 0 || !learnedSkills_.count(skills_[i].id)) continue;
        const auto rule=skillRules_.find(skills_[i].id);
        if (rule!=skillRules_.end()) barIcons[static_cast<std::size_t>(i)]=rule->second.icon;
    }
    actors.setShortcutIcons(std::move(barIcons));
    actors.setSkills(std::move(skillLines));
    actors.setSkillView(std::move(skillViews), tp_, skillTestMode_);
    // 05-AV: montagem do diario no dominio mission; sem regras na UI/GL.
    MissionJournalView journal=missionJournalView();
    actors.setQuests(std::move(journal.lines));
    actors.setQuestCanAbandon(journal.canAbandon);
    std::vector<GroundMark> marks;
    for (const Loot& loot : loots_) marks.push_back({loot.x, loot.y, 0});
    // O poste provisório do vendedor era necessário antes dos NPCs históricos
    // entrarem no ActorSkin. Hoje ele só duplica o comerciante real como uma
    // caixa alta colorida; nome, modelo e marcador dourado já vêm do NpcSystem.
    actors.setMarks(std::move(marks));
}

void Play::pushHud(ActorSkin& actors, const float* mvp, int screenW, int screenH) const {
    const char* raceNames[] = {"", "Lance", "Carsise", "Phyllis", "Ami"};
    std::vector<std::string> status(25);
    status[0] = player_.id > 0 && player_.id < 5 ? raceNames[player_.id] : player_.name;
    status[1] = std::to_string(player_.level);
    status[2] = std::to_string(player_.hp) + "/" + std::to_string(player_.maxHp);
    status[3] = std::to_string(static_cast<int>(player_.homeY)) + "/" + std::to_string(static_cast<int>(player_.homeX));
    const auto equipmentBonus = gearBonuses();
    const auto primary=effectivePrimaryAttributes();
    status[4] = std::to_string(primary[0]);
    status[5] = std::to_string(primary[1]);
    // CharacterStatusUI's historical field order is STR, CON, DEX (ACC),
    // AGI, SPR. Keep indices aligned with those fixed label positions.
    status[6] = std::to_string(primary[3]);
    status[7] = std::to_string(primary[2]);
    status[8] = std::to_string(primary[4]);
    status[9] = std::to_string(std::max(0, baseLuck_ + equipmentBonus.luk));
    status[10] = jobName(job_);
    status[11] = std::to_string(exp_) + "/" + std::to_string(expNext_);
    status[12] = std::to_string(ap_);
    status[13] = std::to_string(tp_);
    // Status precisa mostrar os valores finais usados pelo combate, não a
    // diferença entre classes que serve apenas para montar o baseline do save.
    status[14] = std::to_string(player_.minAtk+genericBuffAttack_);
    status[15] = std::to_string(player_.maxAtk+genericBuffAttack_);
    status[16] = std::to_string(effectiveDefense());
    status[17] = std::to_string(hit_);
    status[18] = std::to_string(player_.dodge);
    char attacksPerSecond[24];
    std::snprintf(attacksPerSecond, sizeof attacksPerSecond, "%.2f/s",
                  1.f / std::max(.01f, effectiveAttackInterval()));
    status[19] = attacksPerSecond;
    status[20] = std::to_string(player_.crit) + "%";
    status[21] = std::to_string(player_.resist);
    status[22] = std::to_string(hrecGain_);
    status[23] = std::to_string(srecGain_);
    status[24] = std::to_string(static_cast<int>(std::lround(moveSpeed()*100.f)))+" cm/s";
    actors.setStatusLines(std::move(status), job_);
    std::vector<HudNote> notes;
    char head[160];
    std::snprintf(head, sizeof head, "Lv %d  %s   HP %d/%d   SP %d/%d   Exp %d/%d   Ouro %d   Pontos %d", player_.level,
                  jobName(job_), player_.hp, player_.maxHp, static_cast<int>(player_.homeY), static_cast<int>(player_.homeX),
                  exp_, expNext_, gold_, ap_);
    notes.push_back({head, 220.f, 28.f, 1.f, 0.95f, 0.75f});
    char expBar[32];
    std::snprintf(expBar, sizeof expBar, "%d/%d", exp_, expNext_);
    notes.push_back({expBar, 90.f, 42.f, 1.f, 1.f, 1.f});
    float y = 70.f;
    if (target_ >= 0 && target_ < static_cast<int>(foes_.size()) && foes_[static_cast<std::size_t>(target_)].hp > 0) {
        const Foe& foe = foes_[static_cast<std::size_t>(target_)];
        char line[128];
        std::snprintf(line, sizeof line, "Lv%d %s HP %d/%d ATK %d-%d DEF %d HIT %d EVA %d",
                      foe.level, foe.name.c_str(), foe.hp, foe.maxHp,
                      foe.minAtk, foe.maxAtk, foe.def, foe.hit, foe.dodge);
        notes.push_back({line, 12.f, y + 4.f, 1.f, 0.75f, 0.45f});
        y += 20.f;
    }
    // 04-H: a lista de skills (1-4) era um overlay temporario de debug,
    // desenhado SEMPRE, mesmo com as janelas fechadas. A interface K e a
    // barra de atalhos continuam disponiveis; nao cobrir mais o mapa.
    // O log fica no canto esquerdo, acima da barra de atalhos F1-F8 (que ocupa
    // os 50 px de baixo), para nao passar por cima dela.
    y = static_cast<float>(screenH) - 62.f - static_cast<float>(log_.size()) * 16.f;
    for (const std::string& line : log_) {
        notes.push_back({line, 10.f, y, 1.f, 1.f, 1.f});
        y += 16.f;
    }
    if (mvp) {
        for (const Pop& pop : pops_) {
            float sx = 0.f;
            float sy = 0.f;
            project(mvp, pop.x, 3.1f, pop.y, screenW, screenH, sx, sy);
            sy -= (1.1f - pop.life) * 36.f;
            notes.push_back({pop.text, sx, sy, pop.r, pop.g, pop.b});
        }
        for (const Loot& loot : loots_) {
            const auto it = items_.find(loot.id);
            if (it == items_.end()) continue;
            float sx = 0.f;
            float sy = 0.f;
            project(mvp, loot.x, 1.1f, loot.y, screenW, screenH, sx, sy);
            notes.push_back({it->second.name, sx, sy, 1.f, 0.9f, 0.45f});
        }
        if (vendorOn_) {
            float sx = 0.f;
            float sy = 0.f;
            project(mvp, vendorX_, 2.1f, vendorY_, screenW, screenH, sx, sy);
            notes.push_back({vendorName_, sx, sy, 1.f, 0.85f, 0.4f});
            const float dx = vendorX_ - hereX_;
            const float dy = vendorY_ - hereY_;
            if (dx * dx + dy * dy < 9.f) {
                notes.push_back({shopOpen_ ? "E fecha a loja" : "E conversa (Trade)", 220.f, 48.f, 1.f, 0.85f, 0.45f});
            }
        }
    }
    std::vector<std::string> shopLines;
    std::vector<std::string> shopIcons;
    std::vector<std::string> shopNames;
    if (shopOpen_) {
        for (int shown = 0; shown < 32; ++shown) {
            const int index = shopOffset_ + shown;
            if (index >= static_cast<int>(vendorStock_.size())) break;
            const int id = vendorStock_[static_cast<std::size_t>(index)];
            const auto it = items_.find(id);
            if (it == items_.end()) {
                shopLines.emplace_back();
                shopIcons.emplace_back();
                shopNames.emplace_back();
                continue;
            }
            shopLines.push_back(std::to_string(it->second.price));
            shopIcons.push_back(it->second.icon);
            shopNames.push_back(it->second.name);
        }
    }
    actors.setShopWindow(shopOpen_, vendorName_, std::move(shopLines), shopOffset_, static_cast<int>(vendorStock_.size()),
                         gold_, std::move(shopIcons), std::move(shopNames));
    actors.setNpcWindow(npcTalkOpen_ && (!npcTalkText_.empty() || !npcMissionChoices_.empty()), npcTalkName_, npcTalkText_, offer_.active, npcOffersTrade_, npcOffersForge_);
    std::vector<NpcQuestView> questViews;
    if(npcTalkOpen_) {
        questViews.reserve(npcMissionChoices_.size());
        for(std::size_t i=0;i<npcMissionChoices_.size();++i) {
            const auto& m=npcMissionChoices_[i];
            NpcQuestView q;
            q.id=m.id;q.title=m.title;q.objective=m.objective;
            q.description=i<npcMissionDescriptions_.size()?npcMissionDescriptions_[i]:std::string{};
            q.rewardGold=m.rewardGold;q.rewardExp=m.rewardExp;
            for(const auto& item:m.neededItems)
                q.requirements.push_back("Item "+std::to_string(item.id)+" x"+std::to_string(item.count));
            const HistoricalProgress* tracked=findHistoricalMission(m.id);
            q.status=tracked&&tracked->complete?NpcQuestView::Complete:
                     tracked&&tracked->active?(missionReadyAtNpc(m.id,npcTalkName_)?NpcQuestView::Ready:NpcQuestView::Active):
                     missionAvailable(m)?NpcQuestView::Available:NpcQuestView::Unavailable;
            if(q.status==NpcQuestView::Unavailable) {
                const std::string reason=missionLockReason(m.id,m.minLevel,m.maxLevel,m.gate);
                q.requirements.insert(q.requirements.begin(),reason.empty()?"Objetivo ainda nao suportado offline":reason);
            }
            questViews.push_back(std::move(q));
        }
    }
    actors.setNpcQuestViews(std::move(questViews),npcMissionSelected_,npcMissionPage_);
    if(actors.mapQuestPanelOpen()) actors.setMapQuestList(availableMapQuests());
    // 05-AS: green = available, yellow = objectives ready for explicit NPC hand-in.
    // Active-but-unready remains '?'. Never mark completion based only on distance.
    for(const auto& npc:questNpcMarkers_) {
        const float dx=npc.x-hereX_,dy=npc.y-hereY_;
        if(dx*dx+dy*dy>22.f*22.f)continue;
        bool ready=false,available=false,active=false;
        auto examine=[&](const HistoricalProgress& m){
            if(!m.active || !m.turnInRequired || m.turnInNpc!=npc.npcName)return;
            if(historicalObjectivesReady(m) && missionRewardsFit(m))ready=true;
            else active=true;
        };
        examine(historical_);
        for(const auto& m:additionalMissions_)examine(m);
        if(worldMap_=="garner" && npc.npcName.find("Senna")!=std::string::npos && hasWelcomeMission())
            ready=true;
        for(const auto& mission:npc.missions) {
            const HistoricalProgress* tracked=findHistoricalMission(mission.id);
            if(tracked && tracked->active)active=true;
            if(historicalActiveCount()>=kMaxActiveMissions || tracked)continue;
            if(mission.valid && missionLockReason(mission.id,mission.minLevel,mission.maxLevel,mission.gate).empty())
                available=true;
        }
        if(!ready&&!available&&!active)continue;
        float sx=0.f,sy=0.f;
        project(mvp,npc.x,3.05f,npc.y,screenW,screenH,sx,sy);
        if(sx<0.f||sx>screenW-12.f||sy<0.f||sy>screenH-12.f)continue;
        const char* symbol=ready?"?":available?"!":"?";
        const float red=ready?1.f:available?.12f:1.f;
        const float green=ready?.85f:available?.95f:.82f;
        const float blue=ready?.08f:available?.17f:.14f;
        notes.push_back({symbol,sx,sy,red,green,blue});
    }
    actors.setNotes(std::move(notes));
}


} // namespace top
