#include "top/Play.h"
#include "top/ProgressionPoints.h"
#include "top/InventoryStackRules.h"
#include "top/InventoryCapacity.h"
#include "top/ItemInstantiationRules.h"
#include "top/TiChunRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace top {
void Play::loadBoatBuildCatalog(const std::string& clientRoot) {
    boatBuildOptions_.clear();
    auto readTable = [](const std::string& path) {
        std::unordered_map<int, std::vector<std::string>> rows;
        std::ifstream input(path);
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '/') continue;
            std::vector<std::string> cells;
            std::size_t begin = 0;
            for (;;) {
                const std::size_t end = line.find('\t', begin);
                cells.push_back(line.substr(begin, end == std::string::npos ? end : end - begin));
                if (end == std::string::npos) break;
                begin = end + 1;
            }
            if (cells.empty()) continue;
            char* tail = nullptr;
            const long id = std::strtol(cells[0].c_str(), &tail, 10);
            if (tail == cells[0].c_str() || *tail || id <= 0 || id > 1000000) continue;
            rows[static_cast<int>(id)] = std::move(cells);
        }
        return rows;
    };
    auto integer = [](const std::vector<std::string>& row, std::size_t column, int fallback = 0) {
        if (column >= row.size()) return fallback;
        char* tail = nullptr;
        const long value = std::strtol(row[column].c_str(), &tail, 10);
        if (tail == row[column].c_str() || *tail || value < -1000000 || value > 1000000000) return fallback;
        return static_cast<int>(value);
    };
    auto ids = [](const std::string& field) {
        std::vector<int> result;
        std::size_t begin = 0;
        while (begin < field.size()) {
            const std::size_t end = field.find(',', begin);
            const std::string token = field.substr(begin, end == std::string::npos ? end : end - begin);
            char* tail = nullptr;
            const long value = std::strtol(token.c_str(), &tail, 10);
            if (tail != token.c_str() && *tail == '\0' && value > 0 && value <= 1000000)
                result.push_back(static_cast<int>(value));
            if (end == std::string::npos) break;
            begin = end + 1;
        }
        return result;
    };
    const auto ships = readTable(clientRoot + "/scripts/table/shipinfo.txt");
    const auto parts = readTable(clientRoot + "/scripts/table/shipiteminfo.txt");
    for (const auto& entry : ships) {
        const auto& ship = entry.second;
        if (ship.size() < 12) continue;
        BoatBuildOption option;
        option.id = entry.first;
        option.name = ship[1];
        option.requiredLevel = std::max(0, integer(ship, 10));
        option.maxHp = std::max(0, integer(ship, 12));
        option.maxSp = std::max(0, integer(ship, 22));
        if (ship[11] != "-1") option.allowedJobs = ids(ship[11]);

        // Sinbad's historical CreateBoat starts each configurable slot at
        // authored option zero. GetData charges those parts and any motor
        // records attached to the selected engine.
        std::vector<int> selectedParts;
        bool complete = true;
        for (const std::size_t column : {std::size_t(5), std::size_t(6), std::size_t(7),
                                         std::size_t(8), std::size_t(9)}) {
            if (column >= ship.size()) { complete = false; break; }
            const auto choices = ids(ship[column]);
            if (choices.empty()) continue; // -1 means this hull has no such part.
            selectedParts.push_back(choices.front());
            if (column == 6) {
                const auto engine = parts.find(choices.front());
                if (engine == parts.end() || engine->second.size() < 8) { complete = false; break; }
                for (std::size_t motorColumn = 3; motorColumn <= 6; ++motorColumn) {
                    const int motorId = integer(engine->second, motorColumn, 0);
                    if (motorId > 0) selectedParts.push_back(motorId);
                }
            }
        }
        if (!complete || selectedParts.empty()) continue;
        long long cost = 0;
        for (const int partId : selectedParts) {
            const auto part = parts.find(partId);
            if (part == parts.end() || part->second.size() <= 7) { complete = false; break; }
            const int partPrice = integer(part->second, 7, -1);
            if (partPrice < 0 || cost > 1000000000LL - partPrice) { complete = false; break; }
            cost += partPrice;
            option.maxHp += std::max(0, integer(part->second, 8));
            option.maxSp += std::max(0, integer(part->second, 18));
        }
        if (!complete || cost <= 0 || cost > 1000000000LL || option.maxHp <= 0 || option.maxSp <= 0) continue;
        option.cost = static_cast<int>(cost);
        boatBuildOptions_.push_back(std::move(option));
    }
    std::sort(boatBuildOptions_.begin(), boatBuildOptions_.end(),
              [](const BoatBuildOption& a, const BoatBuildOption& b) { return a.id < b.id; });
}

bool Play::ownsBoat(int id) const {
    return std::find(ownedBoatIds_.begin(), ownedBoatIds_.end(), id) != ownedBoatIds_.end();
}

int Play::boatHp(int id) const {
    const auto current = ownedBoatHp_.find(id);
    return current == ownedBoatHp_.end() ? boatMaxHp(id) : current->second;
}

int Play::boatMaxHp(int id) const {
    const auto option = std::find_if(boatBuildOptions_.begin(), boatBuildOptions_.end(),
                                     [id](const BoatBuildOption& row) { return row.id == id; });
    return option == boatBuildOptions_.end() ? 0 : option->maxHp;
}

int Play::boatSp(int id) const {
    const auto current = ownedBoatSp_.find(id);
    return current == ownedBoatSp_.end() ? boatMaxSp(id) : current->second;
}

int Play::boatMaxSp(int id) const {
    const auto option = std::find_if(boatBuildOptions_.begin(), boatBuildOptions_.end(),
                                     [id](const BoatBuildOption& row) { return row.id == id; });
    return option == boatBuildOptions_.end() ? 0 : option->maxSp;
}

int Play::boatRepairCost(int id) const {
    if (!ownsBoat(id) || player_.level <= 10) return 0;
    const int deficit = std::max(0, boatMaxHp(id) - boatHp(id));
    // Character.cpp::CCharacter::BoatSelected (BERTH_REPAIR_LIST):
    // DWORD(float(missingHP) * 0.05) + characterLevel * 20.
    const long long cost = deficit / 20 + static_cast<long long>(player_.level) * 20;
    return static_cast<int>(std::min<long long>(cost, 1000000000LL));
}

int Play::boatSupplyCost(int id) const {
    if (!ownsBoat(id) || player_.level <= 10) return 0;
    const int deficit = std::max(0, boatMaxSp(id) - boatSp(id));
    // Character.cpp::CCharacter::BoatSelected (BERTH_SUPPLY_LIST):
    // missingSP + characterLevel * 20.
    const long long cost = static_cast<long long>(deficit) + static_cast<long long>(player_.level) * 20;
    return static_cast<int>(std::min<long long>(cost, 1000000000LL));
}

bool Play::buildHistoricalBoat(int id) {
    const auto option = std::find_if(boatBuildOptions_.begin(), boatBuildOptions_.end(),
                                     [id](const BoatBuildOption& row) { return row.id == id; });
    if (option == boatBuildOptions_.end() || (id != 1 && id != 2 && id != 3 && id != 6)) {
        say("Sinbad: casco sem regra historica de construcao.");
        return false;
    }
    if (static_cast<int>(ownedBoatIds_.size()) >= 3) {
        say("Sinbad: limite historico de tres barcos atingido.");
        return false;
    }
    if (player_.level < option->requiredLevel) {
        say("Sinbad: este casco exige nivel " + std::to_string(option->requiredLevel) + ".");
        return false;
    }
    if (!option->allowedJobs.empty() &&
        std::find(option->allowedJobs.begin(), option->allowedJobs.end(), job_) == option->allowedJobs.end()) {
        say("Sinbad: sua classe nao pode construir este casco.");
        return false;
    }
    if (static_cast<int>(bag_.size()) >= bagCapacity_) {
        say("Sinbad: libere um espaco na bolsa para registrar o barco no perfil.");
        return false;
    }
    if (gold_ < option->cost) {
        say("Sinbad: construcao de " + option->name + " custa " + std::to_string(option->cost) + "G.");
        return false;
    }
    const int oldGold = gold_;
    gold_ -= option->cost;
    ownedBoatIds_.push_back(id);
    ownedBoatHp_[id] = option->maxHp;
    ownedBoatSp_[id] = option->maxSp;
    if (!save(savePath())) {
        gold_ = oldGold;
        ownedBoatIds_.pop_back();
        ownedBoatHp_.erase(id);
        ownedBoatSp_.erase(id);
        return false;
    }
    // PORT_APPROX: first authored components are the historical initial
    // selection; the per-component choice UI and the database-backed deed
    // item are not yet represented by this offline ownership record.
    say("Barco " + option->name + " construido por " + std::to_string(option->cost) + "G e salvo no perfil.");
    return true;
}

bool Play::repairOwnedBoat(int id) {
    if (!ownsBoat(id)) { say("Shirley: barco nao registrado neste atracadouro."); return false; }
    const int maximum = boatMaxHp(id);
    const int current = boatHp(id);
    if (maximum <= 0 || current >= maximum) { say("Este barco nao precisa de reparo."); return false; }
    const int deficit = maximum - std::max(0, current);
    const int fee = boatRepairCost(id);
    if (fee > gold_) { say("Ouro insuficiente para reparar o barco (" + std::to_string(fee) + "G)."); return false; }
    const int previousGold = gold_;
    const int previousHp = current;
    gold_ -= static_cast<int>(fee);
    ownedBoatHp_[id] = maximum;
    if (!save(savePath())) {
        gold_ = previousGold;
        ownedBoatHp_[id] = previousHp;
        return false;
    }
    say("Barco reparado: " + std::to_string(deficit) + " HP por " + std::to_string(fee) + "G.");
    return true;
}

bool Play::refuelOwnedBoat(int id) {
    if (!ownsBoat(id)) { say("Shirley: barco nao registrado neste atracadouro."); return false; }
    const int maximum = boatMaxSp(id);
    const int current = boatSp(id);
    if (maximum <= 0 || current >= maximum) { say("Este barco ja tem combustivel total."); return false; }
    const int deficit = maximum - std::max(0, current);
    const int fee = boatSupplyCost(id);
    if (fee > gold_) { say("Ouro insuficiente para reabastecer o barco (" + std::to_string(fee) + "G)."); return false; }
    const int previousGold = gold_;
    const int previousSp = current;
    gold_ -= static_cast<int>(fee);
    ownedBoatSp_[id] = maximum;
    if (!save(savePath())) {
        gold_ = previousGold;
        ownedBoatSp_[id] = previousSp;
        return false;
    }
    say("Barco reabastecido: " + std::to_string(deficit) + " SP por " + std::to_string(fee) + "G.");
    return true;
}

Play::ProductionPreview Play::previewProduction(int fairyIndex,int blueprintIndex,int toolIndex) const {
    ProductionPreview p;
    auto fail=[&](const char* message){p.message=message;return p;};
    if(fairyIndex<0 || blueprintIndex<0 || fairyIndex>=static_cast<int>(bag_.size()) ||
       blueprintIndex>=static_cast<int>(bag_.size()) || fairyIndex==blueprintIndex ||
       (toolIndex>=0 && (toolIndex>=static_cast<int>(bag_.size()) ||
                         toolIndex==fairyIndex || toolIndex==blueprintIndex)))
        return fail("Selecione fada, blueprint e ferramenta em slots distintos.");
    const Stack& fairy=bag_[static_cast<std::size_t>(fairyIndex)];
    const Stack& paper=bag_[static_cast<std::size_t>(blueprintIndex)];
    const auto fairyInfo=items_.find(fairy.id);
    if(fairyInfo==items_.end() || fairyInfo->second.type!=59 || fairy.durability<=0)
        return fail("A fada precisa estar viva e ter energia.");
    const auto paperInfo=items_.find(paper.id);
    if(paper.id<2300 || paper.id>2302 || paperInfo==items_.end() || paperInfo->second.type!=69 ||
       paper.count!=1 || paper.extra.agi<=0 ||
       paper.itemValStaTenths<10 || paper.maxDurability<=0 || paper.durability<=0)
        return fail("Blueprint invalido ou sem usos/energia.");
    if(toolIndex<0) return fail("A receita historica exige a ferramenta correspondente ao blueprint.");
    const Stack& tool=bag_[static_cast<std::size_t>(toolIndex)];
    const int expectedTool=paper.id==2300?1068:paper.id==2301?1069:1067;
    if(tool.id!=expectedTool)
        return fail("Ferramenta e blueprint nao correspondem segundo a tabela historica.");
    if(tool.durability<=0) return fail("A ferramenta esta sem durabilidade.");
    const int requiredFairySkill=paper.id==2300?13:paper.id==2301?14:16;
    int fairySkillLevel=0;
    for(std::size_t slot=0;slot<3;++slot) {
        if(fairy.fairySkills[slot*2]==requiredFairySkill)
            fairySkillLevel=std::max(fairySkillLevel,fairy.fairySkills[slot*2+1]);
    }
    const int toolLevel=std::max(0,tool.extra.str);
    if(3*fairySkillLevel+1<toolLevel)
        return fail("O nivel da habilidade da fada nao corresponde a ferramenta.");
    const auto product=items_.find(paper.extra.agi);
    if(product==items_.end() || product->second.maxStack<=0)
        return fail("Produto historico ausente do catalogo.");
    p.productId=paper.extra.agi;
    p.fee=paper.maxDurability;
    p.materialIds={paper.extra.str,paper.extra.con,paper.extra.dex};
    p.materialCounts=paper.blueprintParts;
    for(std::size_t i=0;i<p.materialIds.size();++i) {
        if(p.materialIds[i]<=0 || p.materialCounts[i]<=0)
            return fail("Faltam materiais definidos no blueprint.");
        int total=p.materialIds[i]==855?p.fee:0;
        for(std::size_t j=0;j<p.materialIds.size();++j)
            if(p.materialIds[j]==p.materialIds[i]) total+=p.materialCounts[j];
        if(bagCount(p.materialIds[i])<total)
            return fail("Faltam materiais definidos no blueprint.");
    }
    int coinMaterials=0;
    for(std::size_t i=0;i<p.materialIds.size();++i)
        if(p.materialIds[i]==855) coinMaterials+=p.materialCounts[i];
    if(bagCount(855)<p.fee+coinMaterials) return fail("Faltam moedas de fada (item 855).");
    // Historical can_manufacture_item_main requires at least one free grid.
    if(static_cast<int>(bag_.size())>=bagCapacity_)
        return fail("Libere um espaco na bolsa para receber o produto.");

    const int skillId=paper.id==2300?338:paper.id==2301?340:339;
    const auto skill=lifeSkills_.find(skillId);
    const int lifeLevel=skill==lifeSkills_.end()?0:skill->second;
    const int paperLevel=std::max(0,paper.durability);
    if(lifeLevel<paperLevel)
        return fail("O nivel da habilidade profissional nao atende ao blueprint.");
    const int paperEnergy=paper.maxEnergy-100;
    const double raw=(std::min(lifeLevel,paperLevel)*0.03 + toolLevel*0.05 +
                      (100-paperEnergy*10)*0.01)*100.0;
    p.successChance=std::clamp(static_cast<int>(raw),0,100);
    p.valid=true;
    const auto name=product->second.name.empty()?std::to_string(p.productId):product->second.name;
    p.message="Produto: "+name+"\nMateriais: "+std::to_string(p.materialIds[0])+"x"+
              std::to_string(p.materialCounts[0])+" "+std::to_string(p.materialIds[1])+"x"+
              std::to_string(p.materialCounts[1])+" "+std::to_string(p.materialIds[2])+"x"+
              std::to_string(p.materialCounts[2])+"\nMoedas fada: "+std::to_string(p.fee)+
              " | base "+std::to_string(p.successChance)+"% (dados aprox.)";
    return p;
}

bool Play::processProduction(int fairyIndex,int blueprintIndex,int toolIndex) {
    const ProductionPreview p=previewProduction(fairyIndex,blueprintIndex,toolIndex);
    if(!p.valid) {say(p.message);return false;}
    const Stack paper=bag_[static_cast<std::size_t>(blueprintIndex)];
    const int skillId=paper.id==2300?338:paper.id==2301?340:339;
    const auto skill=lifeSkills_.find(skillId);
    const int lifeLevel=skill==lifeSkills_.end()?0:skill->second;
    const int paperLevel=std::max(0,paper.durability);
    const int toolLevel=std::max(0,bag_[static_cast<std::size_t>(toolIndex)].extra.str);
    const int paperEnergy=paper.maxEnergy-100;
    const double raw=(std::min(lifeLevel,paperLevel)*0.03 + toolLevel*0.05 +
                      (100-paperEnergy*10)*0.01)*100.0;
    int chance=std::clamp(static_cast<int>(raw),0,100);

    // PORT_APPROX: the historical UI's dice/prediction callback is unavailable
    // in the OpenGL client. Recreate its possible result range, then apply the
    // Lua end_manufacture_item thresholds. Its MakeItem(..., 4) argument is
    // the item-creation event, not an item quality: Creat_Item has no random
    // affix branch for event 4, so products keep their static base attributes.
    const int starCheck=paper.id==2302?roll(100)+1:roll(11)+1;
    int quantity=1;
    if(paper.id==2300) {
        if(starCheck==1) quantity=0;
        else if(starCheck<=4) quantity=1;
        else if(starCheck<=7) quantity=2;
        else if(starCheck<=10) quantity=3;
        else quantity=4;
        if(p.productId==1067 || p.productId==1068 || p.productId==1069 ||
           p.productId==1070 || p.productId==2236) quantity=1;
        chance=starCheck>=2?100:1;
    } else if(paper.id==2302) {
        const int difference=std::abs(starCheck-75);
        if(difference==0) quantity=5;
        else if(difference==1) quantity=4;
        else if(difference==2) quantity=3;
        else if(difference<=6 && starCheck<=77) quantity=2;
        else if(difference<=25 && starCheck<=77) quantity=1;
        else quantity=0;
    }
    const bool success=paper.id==2300 ? (starCheck>=2 && quantity>0) :
                       (starCheck!=0 && quantity>0 && raw>roll(100)+1);

    std::vector<Stack> next=bag_;
    auto consume=[&](int id,int count) {
        int remaining=count;
        for(auto it=next.begin();it!=next.end() && remaining>0;) {
            if(it->id!=id) {++it;continue;}
            const int amount=std::min(remaining,it->count);
            it->count-=amount;remaining-=amount;
            if(it->count<=0) it=next.erase(it); else ++it;
        }
        return remaining==0;
    };
    for(std::size_t i=0;i<p.materialIds.size();++i) {
        if(!consume(p.materialIds[i],p.materialCounts[i])) {
            say("Materiais mudaram durante a producao; nenhuma alteracao aplicada.");return false;
        }
    }
    if(!consume(855,p.fee)) {
        say("Moedas de fada insuficientes; nenhuma alteracao aplicada.");return false;
    }
    auto findUid=[&](int uid)->Stack* {
        const auto it=std::find_if(next.begin(),next.end(),[&](const Stack& s){return s.uid==uid;});
        return it==next.end()?nullptr:&*it;
    };
    Stack* persistedPaper=findUid(paper.uid);
    Stack* persistedTool=toolIndex>=0?findUid(bag_[static_cast<std::size_t>(toolIndex)].uid):nullptr;
    if(!persistedPaper) {say("Blueprint saiu da bolsa; producao cancelada.");return false;}
    persistedPaper->itemValStaTenths=std::max(0,persistedPaper->itemValStaTenths-10);
    if(toolIndex>=0 && persistedTool) {
        const int skillToolLevel=std::max(0,persistedTool->extra.str);
        persistedTool->durability=std::max(0,persistedTool->durability-50*skillToolLevel);
        int toolXp=persistedTool->energy+(success?paperLevel:1);
        toolXp=std::min(10000,toolXp);
        if(skillToolLevel>0 && toolXp>=skillToolLevel*skillToolLevel*100) {
            persistedTool->extra.str=skillToolLevel+1;
            toolXp=0;
        }
        persistedTool->energy=toolXp;
    }
    if(success) {
        const auto output=items_.find(p.productId);
        const std::size_t oldSize=next.size();
        const bool specialInstance=(p.productId>=1067 && p.productId<=1070) || p.productId==2236;
        bool inserted=false;
        if(output!=items_.end()) {
            if(specialInstance) {
                if(quantity==1 && next.size()<static_cast<std::size_t>(bagCapacity_)) {
                    next.push_back(Stack{p.productId,1});
                    inserted=true;
                }
            } else inserted=inventory_rules::insert(next,p.productId,quantity,
                                                     output->second.maxStack,bagCapacity_);
        }
        if(!inserted) {
            say("Produto nao cabe na bolsa; producao preservada.");return false;
        }
        // MakeItem(..., 4) passes creation event 4, whose Creat_Item branch
        // leaves quality at zero and keeps the ItemInfo base attributes.
        // Initialize the default instance fields only on newly inserted
        // non-stackable products; existing stacks remain untouched.
        if(output->second.maxStack==1)
            for(std::size_t i=oldSize;i<next.size();++i) {
                if(next[i].id!=p.productId) continue;
                next[i].quality=0;
                next[i].maxDurability=output->second.maxDurability;
                next[i].durability=next[i].maxDurability;
                next[i].maxEnergy=output->second.maxEnergy;
                next[i].energy=next[i].maxEnergy;
            }
        if(specialInstance && next.size()>oldSize) {
            Stack& made=next.back();
            if(p.productId>=1067 && p.productId<=1070) {
                made.extra.str=1;
                made.maxEnergy=10000;
                made.energy=1;
            } else made.extra.str=paperLevel; // ITEMATTR_VAL_STR for blueprint 2236.
        }
    }
    bag_.swap(next);
    assignItemUids();
    if(success) onHistoricalEvent(MissionEventKind::GetItem,p.productId,quantity);
    say(success?"Producao concluida: item "+std::to_string(p.productId)+" x"+std::to_string(quantity)+
                ". Atributos-base historicos aplicados; previsao da roleta continua aproximada.":
                "Producao falhou; materiais, moedas e um uso do blueprint foram consumidos historicamente.");
    save(savePath());
    return success;
}

void Play::dropLoot(const Foe& foe) {
    for (const Foe::Drop& drop : foe.drops) {
        if (drop.id <= 0 || drop.rate <= 0 || !items_.count(drop.id)) continue;
        int chance = drop.rate;
        if (dropRateBoost_.remaining > 0.f) {
            chance = static_cast<int>(std::min<long long>(10000LL,
                static_cast<long long>(chance) * std::max(100,dropRateBoost_.multiplierPercent) / 100LL));
        }
        if (chance > 10000) chance = 10000;
        if (chance < 10000 && roll(10000) >= chance) continue;
        if (loots_.size() >= 24) {
            say("O chao nao cabe mais loot.");
            return;
        }
        const float ang = static_cast<float>(roll(628)) * 0.01f;
        const float rad = 0.4f + static_cast<float>(roll(5)) * 0.08f;
        Loot loot;
        loot.id = drop.id;
        loot.x = foe.x + std::sin(ang) * rad;
        loot.y = foe.y + std::cos(ang) * rad;
        loots_.push_back(loot);
    }
}

void Play::collectRequested(float px,float py) {
    if(requestedLoot_<0 || requestedLoot_>=static_cast<int>(loots_.size())) { requestedLoot_=-1; return; }
    const Loot& loot=loots_[static_cast<std::size_t>(requestedLoot_)];
    if(std::hypot(loot.x-px,loot.y-py)>1.6f) return;
    if(!collectInstance(loot.instance.uid > 0 ? loot.instance : Stack{loot.id,1})) {
        say("Bolsa cheia. Venda ou descarte itens."); requestedLoot_=-1; return;
    }
    const auto found=items_.find(loot.id);
    if(found!=items_.end()) say("Pegou "+found->second.name);
    onHistoricalEvent(MissionEventKind::GetItem,loot.id,
                      std::max(1,loot.instance.count));
    loots_.erase(loots_.begin()+requestedLoot_);
    requestedLoot_=-1;
    auto checkItemMission = [&](HistoricalProgress& m) {
        if(m.active && !m.turnInRequired && m.targetId<=0 && m.targetX<0.f && m.targetY<0.f && missionItemsReady(m))
            completeHistoricalMission(m);
    };
    checkItemMission(historical_);
    for(auto& m:additionalMissions_)checkItemMission(m);
}

void Play::gather(float px, float py) {
    for (std::size_t i = 0; i < loots_.size();) {
        const float dx = loots_[i].x - px;
        const float dy = loots_[i].y - py;
        if (dx * dx + dy * dy > 1.15f * 1.15f) {
            ++i;
            continue;
        }
        if (!collectInstance(loots_[i].instance.uid > 0 ? loots_[i].instance : Stack{loots_[i].id,1})) {
            // O loot fica no chao. O aviso nao se repete a cada quadro, e os
            // outros itens ao alcance continuam sendo testados.
            if (log_.empty() || log_.back() != "Bolsa cheia.") say("Bolsa cheia.");
            ++i;
            continue;
        }
        const auto it = items_.find(loots_[i].id);
        if (it != items_.end()) say("Pegou " + it->second.name);
        const int collectedId=loots_[i].id;
        const int collectedCount=std::max(1,loots_[i].instance.count);
        loots_.erase(loots_.begin() + static_cast<std::ptrdiff_t>(i));
        onHistoricalEvent(MissionEventKind::GetItem,collectedId,collectedCount);
        // Item-only historical missions finish as soon as the last required
        // stack is collected, instead of waiting for the next movement tick.
        auto checkItemMission = [&](HistoricalProgress& m) {
            if(m.active && !m.turnInRequired && m.targetId<=0 && m.targetX<0.f && m.targetY<0.f && missionItemsReady(m))
                completeHistoricalMission(m);
        };
        checkItemMission(historical_);
        for(auto& m:additionalMissions_)checkItemMission(m);
    }
}

void Play::setVendor(float x, float y, const std::string& name, const std::vector<int>& stock) {
    vendorX_ = x;
    vendorY_ = y;
    vendorOn_ = true;
    vendorName_ = name.empty() ? "Vendedor" : name;
    vendorStock_ = stock;
    if (vendorStock_.empty()) vendorStock_.push_back(8);
    // A tabela inteira ja foi lida no bind. So volta ao arquivo se o estoque
    // tiver algum item que ainda nao esta carregado.
    bool missing = false;
    for (int id : vendorStock_) {
        if (!items_.count(id)) {
            missing = true;
            break;
        }
    }
    if (missing) loadItems(root_ + "/scripts/table/iteminfo.txt", vendorStock_);
    shopOffset_ = 0;
    shopOpen_ = false;
}

void Play::clearVendor() {
    vendorOn_ = false;
    vendorName_ = "Vendedor";
    vendorStock_.clear();
    shopOpen_ = false;
    shopOffset_ = 0;
}

void Play::interact(float px, float py) {
    if (deadFor_ > 0.f) return;
    if (!vendorOn_) return;
    const float dx = vendorX_ - px;
    const float dy = vendorY_ - py;
    if (dx * dx + dy * dy > 2.4f * 2.4f) {
        say("Chegue perto de " + vendorName_ + ".");
        return;
    }
    shopOpen_ = !shopOpen_;
    if (shopOpen_) npcTalkOpen_ = false;
    say(shopOpen_ ? "Loja de " + vendorName_ + " aberta." : "Loja fechada.");
}

int Play::shopHit(int x, int y, int screenW, int) const {
    if (!shopOpen_) return -1;
    const int left = screenW - 354;
    const int top = 64;
    if (x < left || x >= left + 198 || y < top || y >= top + 394) return -1;
    const int lx = x - left;
    const int ly = y - top;
    if (lx >= 172 && ly >= 2 && lx < 196 && ly < 18) return -2;
    static const int cols[] = {23, 59, 96, 133};
    static const int rows[] = {62, 98, 134, 170, 206, 242, 278, 314};
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 4; ++col) {
            if (lx < cols[col] || ly < rows[row] || lx >= cols[col] + 32 || ly >= rows[row] + 32) continue;
            const int index = shopOffset_ + row * 4 + col;
            return index < static_cast<int>(vendorStock_.size()) ? index : -1;
        }
    }
    return -1;
}

void Play::closeShop() { shopOpen_ = false; }

void Play::clickShop(int index) {
    if (!shopOpen_ || index < 0 || index >= static_cast<int>(vendorStock_.size())) return;
    const int id = vendorStock_[static_cast<std::size_t>(index)];
    const auto it = items_.find(id);
    if (it == items_.end()) {
        say("Item sem ficha no estoque.");
        return;
    }
    if (gold_ < it->second.price) {
        say("Ouro insuficiente para " + it->second.name + ".");
        return;
    }
    if (!addBag(id, 1)) {
        say("Bolsa cheia.");
        return;
    }
    gold_ -= it->second.price;
    say("Comprou " + it->second.name + " de " + vendorName_ + ".");
    save(savePath());
}

void Play::scrollShop(int rows) {
    constexpr int visible = 32;
    constexpr int step = 4;
    if (!shopOpen_ || static_cast<int>(vendorStock_.size()) <= visible) return;
    shopOffset_ += rows * step;
    int maximum = std::max(0, static_cast<int>(vendorStock_.size()) - visible);
    if (maximum % step) maximum += step - (maximum % step);
    if (shopOffset_ < 0) shopOffset_ = 0;
    if (shopOffset_ > maximum) shopOffset_ = maximum;
}

bool Play::increaseSkill(int id) {
    const auto known=learnedSkills_.find(id);
    const auto rule=skillRules_.find(id);
    if(known==learnedSkills_.end() || rule==skillRules_.end()) { say("Skill nao aprendida."); return false; }
    if(known->second>=10) { say("Skill no nivel maximo offline (10)."); return false; }
    if(tp_<=0) { say("Sem pontos de habilidade."); return false; }
    if(rule->second.level>player_.level) { say("Nivel do personagem insuficiente."); return false; }
    --tp_;
    ++known->second;
    say(rule->second.name+" nivel "+std::to_string(known->second)+". TP restante "+std::to_string(tp_));
    save(savePath());
    return true;
}

bool Play::testIncreaseSkill(int id) {
    if(!skillTestMode_)return false;
    const auto rule=skillRules_.find(id);
    if(rule==skillRules_.end()) {say("Skill nao encontrada no catalogo.");return false;}
    const bool professional = lifeSkillIds_.count(id) != 0;
    if (professional) {
        const auto current = lifeSkills_.find(id);
        const int level = current == lifeSkills_.end() ? 0 : current->second;
        if (level >= 10) { say("Habilidade profissional no nivel maximo de teste (10)."); return false; }

        // The historical first-level life-skill books grant the profession's
        // starter tool (Server.Life.Skill[].Give). Free test upgrades must
        // carry that same data-driven reward so the production workbench can
        // be tested without a separate purchase.
        std::vector<Stack> preview = bag_;
        int rewardItem = 0;
        if (level == 0) {
            for (const auto& entry : lifeSkillBooks_) {
                const LifeSkillBookRule& book = entry.second;
                if (book.skillId == id && book.expectedLevel == 0 && book.addLevel == 1 && book.rewardItem > 0) {
                    rewardItem = book.rewardItem;
                    break;
                }
            }
        }
        if (rewardItem > 0) {
            const auto item = items_.find(rewardItem);
            if (item == items_.end() || item->second.maxStack != 1 ||
                !inventory_rules::insert(preview, rewardItem, 1, item->second.maxStack, bagCapacity_)) {
                say("Teste profissional: sem dados ou espaco para a ferramenta inicial; nivel preservado.");
                return false;
            }
            auto tool = std::find_if(preview.rbegin(), preview.rend(), [rewardItem](const Stack& stack) {
                return stack.id == rewardItem;
            });
            if (tool == preview.rend()) return false;
            tool->extra.str = 1; // ITEMATTR_VAL_STR
            tool->maxEnergy = 10000;
            tool->energy = 1;
        }

        const std::vector<Stack> oldBag = bag_;
        bag_.swap(preview);
        lifeSkills_[id] = level + 1;
        assignItemUids();
        const std::string path = savePath();
        if (!path.empty() && !save(path)) {
            bag_ = oldBag;
            if (level == 0) lifeSkills_.erase(id);
            else lifeSkills_[id] = level;
            assignItemUids();
            say("Teste profissional: falha ao salvar; nivel e bolsa preservados.");
            return false;
        }
        say("Teste: habilidade profissional nivel " + std::to_string(level + 1) + ".");
        return true;
    }
    const auto previous = learnedSkills_.find(id);
    const int oldLevel = previous == learnedSkills_.end() ? 0 : previous->second;
    int& level=learnedSkills_[id];
    if(level>=10) {say("Skill no nivel maximo de teste (10).");return false;}
    ++level;
    const std::string path = savePath();
    if (!path.empty() && !save(path)) {
        if (oldLevel == 0) learnedSkills_.erase(id);
        else learnedSkills_[id] = oldLevel;
        say("Teste: falha ao salvar; nivel da skill preservado.");
        return false;
    }
    say("Teste: "+rule->second.name+" nivel "+std::to_string(level)+".");
    return true;
}

bool Play::redeemNpcFameReward(int fameCost, int rewardItemId, int rewardCount, const std::string& label) {
    auto fail = [&](const std::string& reason) {
        say("Donna: " + reason);
        return false;
    };
    if (fameCost <= 0 || rewardItemId <= 0 || rewardCount <= 0 || fameCost > fame_)
        return fail("reputacao insuficiente para esta recompensa.");
    // r_talk05 requires HasLeaveBagGrid,1 even when an existing stack could
    // accept the reward. Keep the historical free-slot condition.
    if (static_cast<int>(bag_.size()) >= bagCapacity_)
        return fail("libere um espaco na bolsa antes do resgate.");
    const auto item = items_.find(rewardItemId);
    if (item == items_.end() || item->second.maxStack <= 0)
        return fail("recompensa historica ausente no catalogo local.");
    std::vector<Stack> preview = bag_;
    if (!inventory_rules::insert(preview, rewardItemId, rewardCount, item->second.maxStack, bagCapacity_))
        return fail("nao foi possivel acomodar a recompensa; reputacao preservada.");

    const int previousFame = fame_;
    const std::vector<Stack> previousBag = bag_;
    fame_ -= fameCost;
    bag_.swap(preview);
    assignItemUids();
    if (!save(savePath())) {
        fame_ = previousFame;
        bag_ = previousBag;
        assignItemUids();
        return fail("falha ao salvar; reputacao e bolsa preservadas.");
    }
    say("Resgate " + label + " concluido por " + std::to_string(fameCost) + " de reputacao.");
    return true;
}

bool Play::processNpcRecipe(const std::vector<std::pair<int,int>>& inputs, int goldCost,
                            const std::vector<std::pair<int,int>>& outputs, const std::string& label,
                            bool requireEmptySlotBefore, int historicalItemEvent,
                            int historicalRecordToSet) {
    auto fail = [&](const char* reason) {
        say("Receita NPC " + label + ": " + reason);
        return false;
    };
    // Gold-only NPC purchases (for example Donna's one-off wine sale) are
    // valid transactions. Keep rejecting a zero-cost no-op with no inputs.
    if ((inputs.empty() && goldCost == 0) || outputs.empty() || goldCost < 0 || historicalRecordToSet < 0)
        return fail("dados incompletos.");
    if (historicalRecordToSet > 0 && hasHistoricalRecord(historicalRecordToSet))
        return fail("registro historico ja concluido.");
    if (gold_ < goldCost) return fail("ouro insuficiente.");
    if (requireEmptySlotBefore && static_cast<int>(bag_.size()) >= bagCapacity_)
        return fail("libere um espaco na bolsa antes da troca.");

    std::unordered_map<int,int> required;
    for (const auto& input : inputs) {
        const auto item = items_.find(input.first);
        if (input.first <= 0 || input.second <= 0 || item == items_.end() || item->second.maxStack <= 0)
            return fail("material sem ficha ou quantidade invalida.");
        if (required[input.first] > 1000000 - input.second)
            return fail("quantidade de materiais invalida.");
        required[input.first] += input.second;
    }
    for (const auto& input : required)
        if (bagCount(input.first) < input.second) return fail("faltam materiais.");

    // Stage the complete transaction so failed output placement never consumes
    // ingredients or gold. The historical GiveItem creates ordinary catalog
    // instances, matching addBag's defaults.
    std::vector<Stack> next = bag_;
    for (const auto& input : required) {
        int remaining = input.second;
        for (std::size_t i = 0; i < next.size() && remaining > 0;) {
            Stack& stack = next[i];
            if (stack.id != input.first) { ++i; continue; }
            const int consumed = std::min(remaining, stack.count);
            stack.count -= consumed;
            remaining -= consumed;
            if (stack.count <= 0) next.erase(next.begin() + static_cast<std::ptrdiff_t>(i));
            else ++i;
        }
        if (remaining != 0) return fail("materiais mudaram durante a operacao.");
    }
    std::vector<std::size_t> generatedSlots;
    for (const auto& output : outputs) {
        const auto item = items_.find(output.first);
        if (output.first <= 0 || output.second <= 0 || item == items_.end() || item->second.maxStack <= 0)
            return fail("produto sem ficha ou quantidade invalida.");
        if (historicalItemEvent > 0 && item->second.chInstance && output.second != 1)
            return fail("a instancia historica exige uma unidade por copia.");
        const std::size_t before = next.size();
        if (!inventory_rules::insert(next, output.first, output.second, item->second.maxStack, bagCapacity_))
            return fail("libere espaco na bolsa para receber o produto.");
        if (historicalItemEvent > 0 && item->second.chInstance) {
            if (next.size() != before + 1)
                return fail("nao foi possivel isolar a instancia historica.");
            generatedSlots.push_back(next.size() - 1);
        }
    }
    if (!generatedSlots.empty()) {
        std::unordered_set<int> usedUids;
        for (const Stack& stack : bag_) if (stack.uid > 0) usedUids.insert(stack.uid);
        for (const Stack& stack : temp_) if (stack.uid > 0) usedUids.insert(stack.uid);
        int candidateUid = std::max(1, nextItemUid_);
        for (std::size_t slot : generatedSlots) {
            while (candidateUid <= 1000000000 && usedUids.count(candidateUid)) ++candidateUid;
            if (candidateUid <= 0 || candidateUid > 1000000000)
                return fail("sem identificador livre para a instancia.");
            Stack& stack = next[slot];
            stack.uid = candidateUid++;
            usedUids.insert(stack.uid);
            const ItemInfo& info = items_.at(stack.id);
            const auto event = static_cast<item_instantiation::HistoricalEvent>(historicalItemEvent);
            const auto instance = item_instantiation::materialize(event, info.type, info.level, stack.uid,
                info.stats, info.statsMaximum, info.maxDurability, info.maxDurabilityRange,
                info.maxEnergy, info.maxEnergyRange);
            if (!instance.valid) return fail("gerador historico ainda nao cobre o produto.");
            stack.quality = instance.quality;
            stack.maxDurability = instance.maxDurability;
            stack.durability = instance.maxDurability;
            stack.maxEnergy = instance.maxEnergy;
            stack.energy = instance.maxEnergy;
            stack.extra += instance.bonus;
        }
    }

    const int oldGold = gold_;
    const int oldNextItemUid = nextItemUid_;
    bag_.swap(next);
    gold_ -= goldCost;
    if (historicalRecordToSet > 0) historicalRecords_.insert(historicalRecordToSet);
    assignItemUids();
    if (!save(savePath())) {
        bag_.swap(next);
        gold_ = oldGold;
        nextItemUid_ = oldNextItemUid;
        if (historicalRecordToSet > 0) historicalRecords_.erase(historicalRecordToSet);
        return fail("nao foi possivel salvar; materiais e ouro foram restaurados.");
    }
    say("Receita NPC " + label + " concluida.");
    return true;
}

bool Play::applyHistoricalHairStyle(int modelItemId,
                                    const std::vector<std::pair<int,int>>& inputs,
                                    int goldCost) {
    auto fail = [&](const char* reason) {
        say(std::string("Cartel - penteado: ") + reason);
        return false;
    };
    const auto style = items_.find(modelItemId);
    if (style == items_.end() || style->second.type != 28 || modelItemId <= 0 ||
        goldCost < 0 || inputs.empty()) return fail("dados historicos de estilo invalidos.");
    if (gold_ < goldCost) return fail("ouro insuficiente.");

    std::unordered_map<int,int> required;
    for (const auto& input : inputs) {
        const auto item = items_.find(input.first);
        if (input.first <= 0 || input.second <= 0 || item == items_.end() || item->second.maxStack <= 0 ||
            required[input.first] > 1000000-input.second)
            return fail("material historico invalido.");
        required[input.first] += input.second;
    }
    for (const auto& input : required)
        if (bagCount(input.first) < input.second) return fail("faltam materiais.");

    std::vector<Stack> next=bag_;
    for (const auto& input : required) {
        int remaining=input.second;
        for (std::size_t i=0;i<next.size() && remaining>0;) {
            Stack& stack=next[i];
            if (stack.id!=input.first) { ++i; continue; }
            const int used=std::min(remaining,stack.count);
            stack.count-=used;remaining-=used;
            if (stack.count<=0) next.erase(next.begin()+static_cast<std::ptrdiff_t>(i));
            else ++i;
        }
        if (remaining!=0) return fail("materiais mudaram durante a operacao.");
    }

    const int oldGold=gold_;
    const int oldHairStyle=hairStyleItemId_;
    bag_.swap(next);
    gold_-=goldCost;
    hairStyleItemId_=modelItemId;
    assignItemUids();
    if (!save(savePath())) {
        bag_.swap(next);
        gold_=oldGold;
        hairStyleItemId_=oldHairStyle;
        assignItemUids();
        return fail("save falhou; materiais, ouro e aparencia foram restaurados.");
    }
    say("Penteado aplicado: modelo historico "+std::to_string(modelItemId)+".");
    return true;
}

bool Play::cleanWithNpcWasher() {
    static constexpr std::array<std::pair<int,int>,74> rewards{{
        {817,1},{818,1},{819,1},{867,1},{868,1},{869,1},{872,1},{873,1},{874,1},
        {1630,2},{1631,1},{1632,1},{1633,2},{1634,1},{1635,1},{1636,2},{1637,1},
        {1638,1},{1639,2},{1640,1},{1641,1},{1642,2},{1643,1},{1644,1},
        {3360,1},{3361,1},{3362,1},{3363,1},{3364,1},{3365,1},{3366,1},{3367,1},
        {3425,1},{3426,1},{3427,1},{3428,1},{3429,1},{3430,1},{3431,1},{3432,1},{3433,1},
        {1804,1},{1805,1},{1806,1},{1807,1},{1808,1},{1809,1},{1810,1},{1811,1},
        {1787,1},{1788,1},{1789,1},{1790,1},{1791,1},{1792,1},{1793,1},
        {3930,1},{3931,1},{3932,1},{3930,1},{3931,1},{3932,1},{3930,1},{3931,1},{3932,1},
        {3930,1},{3931,1},{3932,1},{3930,1},{3931,1},{3932,1},{3930,1},{3930,1},{3931,1}
    }};
    if (gold_ < 200) { say("Cleaner: sao necessarios 200G."); return false; }
    if (bagCount(3927) < 1) { say("Cleaner: falta Muddy Item (3927)."); return false; }
    if (static_cast<int>(bagCapacity_) - static_cast<int>(bag_.size()) < 2) {
        say("Cleaner: libere 2 espacos na bolsa."); return false;
    }
    const auto& reward = rewards[static_cast<std::size_t>(roll(static_cast<int>(rewards.size())))];
    return processNpcRecipe({{3927,1}},200,{{reward.first,reward.second}},
                            "Cleaner - Clean Mud",true,
                            static_cast<int>(item_instantiation::HistoricalEvent::QuestAward2));
}

bool Play::processNpcRecipeAnyInput(const std::vector<int>& inputItemIds, int goldCost,
                                    const std::vector<std::pair<int,int>>& outputs, const std::string& label,
                                    bool requireEmptySlotBefore) {
    for (int itemId : inputItemIds) {
        if (itemId > 0 && bagCount(itemId) > 0)
            return processNpcRecipe({{itemId, 1}}, goldCost, outputs, label, requireEmptySlotBefore);
    }
    say("Receita NPC " + label + ": faltam materiais.");
    return false;
}

bool Play::redeemNpcDiamondVoucher(int level) {
    static constexpr int gemIds[] = {878,879,880,881,882,883,884,887};
    int voucherId = 0;
    int emptySlots = 0;
    bool randomGem = false;
    std::vector<std::pair<int,int>> outputs;
    const char* label = "";
    switch (level) {
        case 1:
            voucherId=3877; emptySlots=2; label="Bingo - Lv 1 Refining Gem Voucher";
            randomGem=true; outputs={{885,1}};
            break;
        case 2:
            voucherId=3878; emptySlots=2; label="Bingo - Lv 2 Refining Gem Voucher";
            randomGem=true; outputs={{885,1}};
            break;
        case 3:
            voucherId=3885; emptySlots=1; label="Bingo - Refining Gem Voucher";
            outputs={{885,1}};
            break;
        case 4:
            voucherId=3886; emptySlots=1; label="Bingo - Gem Voucher";
            randomGem=true;
            break;
        default:
            say("Bingo: nivel de voucher desconhecido; nenhum item foi consumido.");
            return false;
    }
    if (bagCount(voucherId) < 1) {
        say("Bingo: voce nao possui o voucher exigido.");
        return false;
    }
    if (static_cast<int>(bagCapacity_)-static_cast<int>(bag_.size()) < emptySlots) {
        say("Bingo: libere " + std::to_string(emptySlots) + " espaco(s) na bolsa.");
        return false;
    }
    if (randomGem) outputs.insert(outputs.begin(),{gemIds[roll(8)],1});
    return processNpcRecipe({{voucherId,1}},0,outputs,label,true);
}

bool Play::receiveNpcHealing(int goldCost, int hpPercent, bool restoreAll) {
    if (goldCost < 0 || hpPercent < 0 || hpPercent > 100 || (restoreAll && hpPercent != 0)) {
        say("Cura NPC: dados invalidos.");
        return false;
    }
    if (gold_ < goldCost) {
        say("Cura NPC: ouro insuficiente (" + std::to_string(goldCost) + "G).");
        return false;
    }
    const int previousGold = gold_;
    const int previousHp = player_.hp;
    const float previousSp = player_.homeY;
    const float previousDeadFor = deadFor_;
    gold_ -= goldCost;
    if (restoreAll) {
        player_.hp = player_.maxHp;
        player_.homeY = player_.homeX; // current SP = max SP
    } else {
        // Historical ReHp passes a percent of maximum HP, and the DX9
        // character method truncates the percentage calculation to an integer.
        const int hpAmount = hpPercent * player_.maxHp / 100;
        player_.hp = std::min(player_.maxHp, player_.hp + hpAmount);
    }
    if (player_.hp > 0) deadFor_ = 0.f;
    const int healed = player_.hp - previousHp;
    if (!save(savePath())) {
        gold_ = previousGold;
        player_.hp = previousHp;
        player_.homeY = previousSp;
        deadFor_ = previousDeadFor;
        return false;
    }
    say(restoreAll ? "Cura NPC: HP e SP restaurados." :
         "Cura NPC: +" + std::to_string(healed) + " HP.");
    return true;
}

bool Play::claimArgentWelcomeGift() {
    if (hasHistoricalRecord(500)) return false;
    const Foe previousPlayer = player_;
    const int previousExp = exp_;
    const int previousExpNext = expNext_;
    const int previousGold = gold_;
    const int previousAp = ap_;
    const int previousTp = tp_;
    const int previousBaseMin = baseMin_, previousBaseMax = baseMax_, previousBaseDef = baseDef_;
    const int previousAttrMin = attrMin_, previousAttrMax = attrMax_, previousAttrDef = attrDef_;
    const int previousHit = hit_, previousFlee = flee_, previousHrecGain = hrecGain_, previousSrecGain = srecGain_;
    const int previousSheetHp = sheetHp_, previousSheetSp = sheetSp_;

    gold_ = static_cast<int>(std::min(1000000000LL, static_cast<long long>(gold_) + 100));
    // Historical r_talk01 rolls 100..199 XP; this local roll preserves the
    // authored range, while its exact server RNG sequence is PORT_APPROX.
    const int experience = 100 + roll(100);
    exp_ = std::min(1000000000, exp_ + experience);
    while (expNext_ > 0 && exp_ >= expNext_ && player_.level < 100) {
        exp_ -= expNext_;
        ++player_.level;
        ap_ += progression::attrPointAt(player_.level);
        tp_ += progression::skillPointAt(player_.level);
        if (sheetReady_) applyAttrs(false);
        player_.hp = player_.maxHp;
        player_.homeY = player_.homeX;
        expNext_ = experience_.needed(player_.level);
    }
    historicalRecords_.insert(500);
    if (!save(savePath())) {
        player_ = previousPlayer;
        exp_ = previousExp;
        expNext_ = previousExpNext;
        gold_ = previousGold;
        ap_ = previousAp;
        tp_ = previousTp;
        baseMin_ = previousBaseMin; baseMax_ = previousBaseMax; baseDef_ = previousBaseDef;
        attrMin_ = previousAttrMin; attrMax_ = previousAttrMax; attrDef_ = previousAttrDef;
        hit_ = previousHit; flee_ = previousFlee; hrecGain_ = previousHrecGain; srecGain_ = previousSrecGain;
        sheetHp_ = previousSheetHp; sheetSp_ = previousSheetSp;
        historicalRecords_.erase(500);
        return false;
    }
    say("Salvier: presente de boas-vindas recebido: +" + std::to_string(experience) + " EXP e +100G.");
    return true;
}

Play::WorkbenchPreview Play::previewWorkbench(int mode,int a,int b,int c) const {
    WorkbenchPreview p;
    if(mode==4) {
        p.cost=repairItemCost(a);
        if(p.cost<0){p.message="Selecione um equipamento reparavel da bolsa";return p;}
        if(p.cost==0){p.message="Durabilidade completa; nao precisa de reparo";return p;}
        if(gold_<p.cost){p.message="Ouro insuficiente: reparo custa "+std::to_string(p.cost)+"G";return p;}
        p.valid=true;
        p.message="Reparo restaura a durabilidade maxima";
        return p;
    }
    if(mode==5) {
        p.cost=gemExtractionCost(a,b);
        if(p.cost<0){p.message="Selecione equipamento com gema e o alicate da posicao";return p;}
        if(bag_.size()>=static_cast<std::size_t>(bagCapacity_)){
            p.message="A extracao historica exige um espaco livre na bolsa";return p;
        }
        if(gold_<p.cost){p.message="Ouro insuficiente: extracao custa "+std::to_string(p.cost)+"G";return p;}
        p.valid=true;
        p.message="Extrai uma gema e reduz um nivel do encaixe";
        return p;
    }
    if(mode==6) {
        p.cost=tiChunCost(a,b);
        if(p.cost<0){p.message="Selecione equipamento e catalisador historico compativeis";return p;}
        if(gold_<p.cost){p.message="Ouro insuficiente: aprimoramento custa "+std::to_string(p.cost)+"G";return p;}
        p.valid=true;
        p.message="Aprimoramento historico: equipamento e catalisador serao consumidos";
        return p;
    }
    if(mode==7) {
        p.cost=equipmentUpgradeCost(b);
        if(p.cost<0){p.message="Selecione equipamento, scroll tipo 62 e crystal tipo 63";return p;}
        if(a<0||c<0||a>=static_cast<int>(bag_.size())||c>=static_cast<int>(bag_.size())||a==b||c==b||a==c){
            p.message="Selecione os tres itens em slots diferentes";return p;
        }
        const auto scroll=items_.find(bag_[static_cast<std::size_t>(a)].id);
        const auto crystal=items_.find(bag_[static_cast<std::size_t>(c)].id);
        if(scroll==items_.end()||crystal==items_.end()||scroll->second.type!=62||crystal->second.type!=63||
           bag_[static_cast<std::size_t>(a)].count!=1||bag_[static_cast<std::size_t>(c)].count!=1){
            p.message="Aprimoramento requer exatamente um scroll tipo 62 e um crystal tipo 63";return p;
        }
        const int level=bag_[static_cast<std::size_t>(b)].equipmentUpgradeLevel;
        p.chance=10000-level*1000;
        if(gold_<p.cost){p.message="Ouro insuficiente: aprimoramento custa "+std::to_string(p.cost)+"G";return p;}
        p.valid=true;
        p.message="Nivel "+std::to_string(level)+" -> "+std::to_string(level+1)+"; chance "+
            std::to_string(p.chance/100)+"%; custo "+std::to_string(p.cost)+"G";
        return p;
    }
    if(a<0||b<0||c<0||a==b||a==c||b==c||
       a>=int(bag_.size())||b>=int(bag_.size())||c>=int(bag_.size())){
        p.message="Selecione tres itens diferentes";return p;
    }
    const Stack& x=bag_[std::size_t(a)], &y=bag_[std::size_t(b)], &z=bag_[std::size_t(c)];
    const auto xi=items_.find(x.id),yi=items_.find(y.id),zi=items_.find(z.id);
    if(xi==items_.end()||yi==items_.end()||zi==items_.end()){
        p.message="Item nao encontrado no catalogo";return p;
    }
    if(mode==2) {
        p.cost=socketCost(a);
        if(p.cost<0){p.message="Equipamento nao aceita encaixe";return p;}
        if(!socket_rules::materials(y.id,z.id)||y.count<1||z.count<1){
            p.message="Precisa estabilizador 890 e catalisador 891";return p;
        }
        p.chance=socket_rules::successChancePer10000(x.socketCount);
        if(p.chance<=0){p.message="Nivel de encaixe indisponivel";return p;}
    }else if(mode==0){
        if(x.count!=1||y.count!=1||z.count!=1||yi->second.type!=49||zi->second.type!=50){
            p.message="Equipamento, gema comum e Refining Gem";return p;
        }
        const auto rule=gemCatalog_.find(y.id);
        if(rule==gemCatalog_.end()||!rule->second.compatible(xi->second.type)){
            p.message="Gema incompativel com equipamento";return p;
        }
        if(y.stoneLevel<1||y.stoneLevel>rule->second.maxLevel||z.stoneLevel<1||z.stoneLevel>9){
            p.message="Nivel de gema ou refiner invalido";return p;
        }
        p.cost=forgeCost(a);
        if(p.cost<0){p.message="Abra um encaixe antes de forjar";return p;}
        int slot=-1;
        for(int i=0;i<x.socketCount;++i)if(x.gemIds[i]==y.id){slot=i;break;}
        if(slot<0)for(int i=0;i<x.socketCount;++i)if(x.gemIds[i]==0){slot=i;break;}
        if(slot<0){p.message="Sem encaixe livre";return p;}
        const int next=x.gemLevels[slot]+1;
        if(next>rule->second.maxLevel||y.stoneLevel<next||z.stoneLevel<next){
            p.message="Gema ou refiner abaixo do nivel necessario";return p;
        }
        p.chance=forge_rules::forgingChanceWithFruitPer10000(next,
            itemForgeBoostFor_>0.f?itemForgeBoostLevel_:0);
        if(p.chance<=0){p.message="Nivel de forja indisponivel";return p;}
    }else if(mode==1){
        if(xi->second.type!=47 || (yi->second.type!=49&&yi->second.type!=50)||
           yi->second.type!=zi->second.type||y.id!=z.id || x.count<1||y.count!=1||z.count!=1||
           y.stoneLevel!=z.stoneLevel||y.stoneLevel<1||y.stoneLevel>9){
            p.message="Pergaminho e duas gemas iguais";return p;
        }
        int cap=9;
        if(yi->second.type==49){
            const auto rule=gemCatalog_.find(y.id);
            if(rule==gemCatalog_.end()){p.message="Gema desconhecida";return p;}
            cap=rule->second.maxLevel;
        }
        if(y.stoneLevel>=cap){p.message="Gema no nivel maximo";return p;}
        p.cost=5000;
        p.chance=forge_rules::combiningChanceWithFruitPer10000(y.stoneLevel,yi->second.type,
            itemCombineBoostFor_>0.f?itemCombineBoostLevel_:0);
        if(p.chance<=0){p.message="Combinacao indisponivel";return p;}
    }else {p.message="Modo desconhecido";return p;}
    if(gold_<p.cost){p.message="Ouro insuficiente";return p;}
    p.valid=true;
    p.message="Pronto para revisar operacao";
    return p;
}

int Play::tiChunCost(int equipmentIndex,int catalystIndex) const {
    if(equipmentIndex<0||catalystIndex<0||equipmentIndex==catalystIndex||
       equipmentIndex>=static_cast<int>(bag_.size())||catalystIndex>=static_cast<int>(bag_.size())||
       equippedSlotAtBagIndex(equipmentIndex)>=0||equippedSlotAtBagIndex(catalystIndex)>=0)return -1;
    const Stack& equipment=bag_[static_cast<std::size_t>(equipmentIndex)];
    const Stack& catalyst=bag_[static_cast<std::size_t>(catalystIndex)];
    if(equipment.count!=1||catalyst.count!=1)return -1;
    const int mainId=equipment.maxDurability==25000&&equipment.fuseId>0?equipment.fuseId:equipment.id;
    const auto rule=std::find_if(ti_chun::kRules.begin(),ti_chun::kRules.end(),[&](const ti_chun::Rule& row){
        return row.mainId==mainId&&row.catalystId==catalyst.id;
    });
    if(rule==ti_chun::kRules.end())return -1;
    const auto result=items_.find(rule->resultId);
    if(result==items_.end()||result->second.maxStack!=1)return -1;
    return ti_chun::kGoldCost;
}

int Play::equipmentUpgradeCost(int equipmentIndex) const {
    static constexpr std::array<int,18> upgradeableTypes{
        1,2,3,4,7,11,20,22,23,24,27,9,25,26,81,82,83,88
    };
    if(equipmentIndex<0||equipmentIndex>=static_cast<int>(bag_.size())||
       equippedSlotAtBagIndex(equipmentIndex)>=0)return -1;
    const Stack& equipment=bag_[static_cast<std::size_t>(equipmentIndex)];
    const auto definition=items_.find(equipment.id);
    if(definition==items_.end()||equipment.count!=1||equipment.equipmentUpgradeLevel<0||
       equipment.equipmentUpgradeLevel>=10||definition->second.maxDurability==25000||
       std::find(upgradeableTypes.begin(),upgradeableTypes.end(),definition->second.type)==upgradeableTypes.end())return -1;
    const int nextLevel=equipment.equipmentUpgradeLevel+1;
    return nextLevel*nextLevel*10000;
}

bool Play::upgradeEquipmentEffectiveness(int scrollIndex,int equipmentIndex,int crystalIndex) {
    const WorkbenchPreview preview=previewWorkbench(7,scrollIndex,equipmentIndex,crystalIndex);
    if(!preview.valid){say(preview.message);return false;}
    const int equipmentUid=bag_[static_cast<std::size_t>(equipmentIndex)].uid;
    const int equipmentId=bag_[static_cast<std::size_t>(equipmentIndex)].id;
    const int oldLevel=bag_[static_cast<std::size_t>(equipmentIndex)].equipmentUpgradeLevel;
    const auto oldBag=bag_;
    const int oldGold=gold_;
    const int oldNextUid=nextItemUid_;
    const int nextLevel=oldLevel+1;
    const std::int64_t threshold=static_cast<std::int64_t>(preview.chance)*100000;
    const bool success=static_cast<std::int64_t>(roll(1000000001))<=threshold;
    const int high=std::max(scrollIndex,crystalIndex),low=std::min(scrollIndex,crystalIndex);
    bag_.erase(bag_.begin()+high);
    bag_.erase(bag_.begin()+low);
    if(success){
        const auto equipment=std::find_if(bag_.begin(),bag_.end(),[equipmentUid](const Stack& stack){return stack.uid==equipmentUid;});
        if(equipment==bag_.end()){bag_=oldBag;return false;}
        equipment->equipmentUpgradeLevel=nextLevel;
    }
    gold_-=preview.cost;
    assignItemUids();
    if(sheetReady_)applyAttrs(false);else recomputeGear();
    if(!save(savePath())) {
        bag_=oldBag;gold_=oldGold;nextItemUid_=oldNextUid;
        if(sheetReady_)applyAttrs(false);else recomputeGear();
        say("Efetividade: falha ao salvar; equipamento, itens e ouro preservados.");
        return false;
    }
    say(success?"Equipamento "+items_.at(equipmentId).name+" aprimorado para nivel "+std::to_string(nextLevel)+" por "+std::to_string(preview.cost)+"G."
              :"Aprimoramento falhou; scroll, crystal e "+std::to_string(preview.cost)+"G foram consumidos; equipamento preservado.");
    return true;
}

bool Play::upgradeEquipmentTiChun(int equipmentIndex,int catalystIndex) {
    const WorkbenchPreview preview=previewWorkbench(6,equipmentIndex,catalystIndex,-1);
    if(!preview.valid){say(preview.message);return false;}
    const Stack source=bag_[static_cast<std::size_t>(equipmentIndex)];
    const Stack catalyst=bag_[static_cast<std::size_t>(catalystIndex)];
    const int mainId=source.maxDurability==25000&&source.fuseId>0?source.fuseId:source.id;
    const auto rule=std::find_if(ti_chun::kRules.begin(),ti_chun::kRules.end(),[&](const ti_chun::Rule& row){
        return row.mainId==mainId&&row.catalystId==catalyst.id;
    });
    if(rule==ti_chun::kRules.end())return false;
    const auto resultInfo=items_.find(rule->resultId);
    if(resultInfo==items_.end()||bag_.size()-2>=static_cast<std::size_t>(bagCapacity_)){
        say("Aprimoramento: resultado indisponivel ou sem espaco; itens e ouro preservados.");
        return false;
    }

    int itemEvent=2;
    if(source.energy>=1000) {
        itemEvent=12;
        if(source.energy<10000)itemEvent=12+(source.energy/1000)-1;
    }
    Stack upgraded{rule->resultId,1};
    upgraded.extra=resultInfo->second.stats;
    upgraded.forgeLevel=source.forgeLevel;
    upgraded.gemIds=source.gemIds;
    upgraded.gemLevels=source.gemLevels;
    upgraded.socketCount=source.socketCount;
    upgraded.maxDurability=resultInfo->second.maxDurability;
    upgraded.durability=upgraded.maxDurability;

    const auto oldBag=bag_;
    const int oldGold=gold_;
    const int oldNextUid=nextItemUid_;
    const int first=std::max(equipmentIndex,catalystIndex);
    const int second=std::min(equipmentIndex,catalystIndex);
    bag_.erase(bag_.begin()+first);
    bag_.erase(bag_.begin()+second);
    bag_.push_back(std::move(upgraded));
    gold_-=preview.cost;
    assignItemUids();
    Stack& result=bag_.back();
    const auto instance=resultInfo->second.chInstance ? item_instantiation::materialize(
        static_cast<item_instantiation::HistoricalEvent>(itemEvent),resultInfo->second.type,
        resultInfo->second.level,result.uid,resultInfo->second.stats,resultInfo->second.statsMaximum,
        resultInfo->second.maxDurability,resultInfo->second.maxDurabilityRange,
        resultInfo->second.maxEnergy,resultInfo->second.maxEnergyRange) : item_instantiation::GeneratedInstance{};
    if(resultInfo->second.chInstance&&!instance.valid) {
        bag_=oldBag;gold_=oldGold;nextItemUid_=oldNextUid;
        say("Aprimoramento: gerador historico ainda nao cobre o resultado; itens e ouro preservados.");
        return false;
    }
    if(instance.valid) {
        result.quality=instance.quality;
        result.maxDurability=instance.maxDurability;
        result.durability=instance.maxDurability;
        result.maxEnergy=instance.maxEnergy;
        result.energy=instance.maxEnergy;
        result.extra+=instance.bonus;
    }
    if(sheetReady_)applyAttrs(false);else recomputeGear();
    if(!save(savePath())) {
        bag_=oldBag;gold_=oldGold;nextItemUid_=oldNextUid;
        if(sheetReady_)applyAttrs(false);else recomputeGear();
        say("Aprimoramento: falha ao salvar; equipamento, catalisador e ouro preservados.");
        return false;
    }
    say("Equipamento aprimorado: "+items_.at(rule->resultId).name+" por "+std::to_string(preview.cost)+"G; atributos gerados pelo evento historico.");
    return true;
}

int Play::repairItemCost(int index) const {
    static constexpr std::array<int,18> repairableTypes{
        1,2,3,4,7,11,20,22,23,24,27,9,25,26,81,82,83,88
    };
    if(index<0||index>=static_cast<int>(bag_.size()))return -1;
    const Stack& stack=bag_[static_cast<std::size_t>(index)];
    const auto definition=items_.find(stack.id);
    if(definition==items_.end()||stack.count!=1||
       std::find(repairableTypes.begin(),repairableTypes.end(),definition->second.type)==repairableTypes.end()||
       stack.maxDurability<=2500||stack.durability<0||stack.durability>stack.maxDurability)
        return -1;
    if(stack.durability==stack.maxDurability)return 0;
    const int repairPoint=static_cast<int>(std::floor(std::pow(definition->second.level/10.0,1.7)))-1;
    const int wearPoints=std::max(0,stack.maxDurability/50)-std::max(0,stack.durability/50);
    return std::max(1,wearPoints*repairPoint);
}

bool Play::repairBagItem(int index) {
    const WorkbenchPreview preview=previewWorkbench(4,index,-1,-1);
    if(!preview.valid){say(preview.message);return false;}
    Stack& stack=bag_[static_cast<std::size_t>(index)];
    const int oldGold=gold_;
    const Stack oldStack=stack;
    gold_-=preview.cost;
    stack.durability=stack.maxDurability;
    if(sheetReady_)applyAttrs(false);else recomputeGear();
    if(!save(savePath())) {
        gold_=oldGold;
        stack=oldStack;
        if(sheetReady_)applyAttrs(false);else recomputeGear();
        say("Reparo: falha ao salvar; ouro e durabilidade preservados.");
        return false;
    }
    say("Reparo concluido: "+items_.at(stack.id).name+" por "+std::to_string(preview.cost)+"G.");
    return true;
}

int Play::gemExtractionCost(int equipmentIndex,int plierIndex) const {
    static constexpr std::array<int,18> extractableTypes{
        1,2,3,4,7,11,20,22,23,24,27,9,25,26,81,82,83,88
    };
    if(equipmentIndex<0||plierIndex<0||equipmentIndex==plierIndex||
       equipmentIndex>=static_cast<int>(bag_.size())||plierIndex>=static_cast<int>(bag_.size())||
       equippedSlotAtBagIndex(equipmentIndex)>=0)return -1;
    const Stack& equipment=bag_[static_cast<std::size_t>(equipmentIndex)];
    const Stack& plier=bag_[static_cast<std::size_t>(plierIndex)];
    const auto equipmentInfo=items_.find(equipment.id);
    const auto plierInfo=items_.find(plier.id);
    if(equipmentInfo==items_.end()||plierInfo==items_.end()||equipment.count!=1||plier.count!=1||
       std::find(extractableTypes.begin(),extractableTypes.end(),equipmentInfo->second.type)==extractableTypes.end())
        return -1;
    int socket=-1;
    if(plier.id==1020)socket=0;
    else if(plier.id==6870)socket=1;
    else if(plier.id==6871)socket=2;
    else return -1;
    if(equipment.socketCount<1||equipment.socketCount>3||socket>=equipment.socketCount||
       !forge_rules::validSockets(equipment.socketCount,equipment.gemIds,equipment.gemLevels))return -1;
    const int gemId=equipment.gemIds[static_cast<std::size_t>(socket)];
    const int gemLevel=equipment.gemLevels[static_cast<std::size_t>(socket)];
    const auto gem=gemCatalog_.find(gemId);
    if(gemId<=0||gemLevel<=0||gem==gemCatalog_.end()||items_.find(gemId)==items_.end())return -1;
    int totalLevels=0;
    for(int i=0;i<3;++i)totalLevels+=std::max(0,equipment.gemLevels[static_cast<std::size_t>(i)]);
    return totalLevels*10000;
}

bool Play::extractSocketGem(int equipmentIndex,int plierIndex) {
    const WorkbenchPreview preview=previewWorkbench(5,equipmentIndex,plierIndex,-1);
    if(!preview.valid){say(preview.message);return false;}
    const int equipmentUid=bag_[static_cast<std::size_t>(equipmentIndex)].uid;
    const int plierUid=bag_[static_cast<std::size_t>(plierIndex)].uid;
    const int oldGold=gold_;
    const int oldNextUid=nextItemUid_;
    const auto oldBag=bag_;
    const Stack& equipment=bag_[static_cast<std::size_t>(equipmentIndex)];
    const int socket=bag_[static_cast<std::size_t>(plierIndex)].id==1020?0:
                     bag_[static_cast<std::size_t>(plierIndex)].id==6870?1:2;
    const int gemId=equipment.gemIds[static_cast<std::size_t>(socket)];
    const int gemLevel=equipment.gemLevels[static_cast<std::size_t>(socket)];
    auto target=std::find_if(bag_.begin(),bag_.end(),[equipmentUid](const Stack& stack){return stack.uid==equipmentUid;});
    if(target==bag_.end())return false;
    --target->gemLevels[static_cast<std::size_t>(socket)];
    if(target->gemLevels[static_cast<std::size_t>(socket)]==0)
        target->gemIds[static_cast<std::size_t>(socket)]=0;
    target->forgeLevel=std::accumulate(target->gemLevels.begin(),target->gemLevels.end(),0,
        [](int sum,int level){return sum+std::max(0,level);});
    target->forgeLevel=std::min(15,target->forgeLevel);
    const auto tool=std::find_if(bag_.begin(),bag_.end(),[plierUid](const Stack& stack){return stack.uid==plierUid;});
    if(tool==bag_.end()){bag_=oldBag;return false;}
    if(--tool->count<=0)bag_.erase(tool);
    Stack extracted{gemId,1};
    extracted.stoneLevel=gemLevel;
    const auto item=items_.find(gemId);
    if(item==items_.end()||bag_.size()>=static_cast<std::size_t>(bagCapacity_)){
        bag_=oldBag;
        say("Extracao: sem item de gema valido ou espaco livre; equipamento e alicate preservados.");
        return false;
    }
    bag_.push_back(std::move(extracted));
    gold_-=preview.cost;
    assignItemUids();
    if(sheetReady_)applyAttrs(false);else recomputeGear();
    if(!save(savePath())) {
        bag_=oldBag;
        gold_=oldGold;
        nextItemUid_=oldNextUid;
        if(sheetReady_)applyAttrs(false);else recomputeGear();
        say("Extracao: falha ao salvar; equipamento, alicate e ouro preservados.");
        return false;
    }
    say("Gema extraida: "+item->second.name+" nivel "+std::to_string(gemLevel)+
        " por "+std::to_string(preview.cost)+"G.");
    return true;
}

int Play::socketCost(int equipmentIndex) const {
    if (equipmentIndex<0 || equipmentIndex>=static_cast<int>(bag_.size())) return -1;
    const Stack& equipment=bag_[static_cast<std::size_t>(equipmentIndex)];
    const auto rule=items_.find(equipment.id);
    if (rule==items_.end() || equipment.count!=1 ||
        !forge_rules::validSockets(equipment.socketCount,equipment.gemIds,equipment.gemLevels)) return -1;
    return socket_rules::cost(socketConfig_,equipment.socketCount,rule->second.maxSockets,
                              rule->second.maxDurability);
}

bool Play::socketEquipment(int equipmentIndex,int stabilizerIndex,int catalystIndex){
    if(equipmentIndex<0||stabilizerIndex<0||catalystIndex<0 ||
       equipmentIndex==stabilizerIndex||equipmentIndex==catalystIndex||stabilizerIndex==catalystIndex||
       equipmentIndex>=static_cast<int>(bag_.size())||
       stabilizerIndex>=static_cast<int>(bag_.size())||
       catalystIndex>=static_cast<int>(bag_.size())){
        say("Perfuração: indices da bolsa invalidos.");return false;
    }
    const Stack& original=bag_[static_cast<std::size_t>(equipmentIndex)];
    const Stack& stabilizer=bag_[static_cast<std::size_t>(stabilizerIndex)];
    const Stack& catalyst=bag_[static_cast<std::size_t>(catalystIndex)];
    const int price=socketCost(equipmentIndex);
    if(price<0){say("Perfuração: equipamento sem capacidade de encaixes ou apparel.");return false;}
    if(!socket_rules::materials(stabilizer.id,catalyst.id) ||
       stabilizer.count<1 || catalyst.count<1){
        say("Perfuração: precisa de Equipment Stabilizer (890) e Equipment Catalyst (891).");return false;
    }
    if(gold_<price){say("Perfuração: ouro insuficiente ("+std::to_string(price)+").");return false;}
    if(socket_rules::successChancePer10000(original.socketCount)!=10000)return false;
    // Transacao totalmente validada antes de cobrar/remover materiais.
    // UID e gemas permanecem associados ao mesmo equipamento.
    const int equipUid=original.uid;
    gold_-=price;
    ++bag_[static_cast<std::size_t>(equipmentIndex)].socketCount;
    for(int index : {std::max(stabilizerIndex,catalystIndex),std::min(stabilizerIndex,catalystIndex)}){
        Stack& consumed=bag_[static_cast<std::size_t>(index)];
        if(--consumed.count<=0)bag_.erase(bag_.begin()+index);
    }
    const auto equipped=std::find_if(bag_.begin(),bag_.end(),[&](const Stack& s){return s.uid==equipUid;});
    if(equipped==bag_.end()){say("Perfuração: erro de identidade.");return false;}
    say("Perfuração: " + std::to_string(equipped->socketCount) + " encaixe(s) aberto(s).");
    save(savePath());
    return true;
}

int Play::forgeCost(int index) const {
    if(index<0||index>=static_cast<int>(bag_.size()))return -1;
    const Stack& item=bag_[static_cast<std::size_t>(index)];
    const auto found=items_.find(item.id);
    if(found==items_.end()||item.count!=1||item.socketCount<1||item.socketCount>3)return -1;
    if(!forge_rules::validSockets(item.socketCount,item.gemIds,item.gemLevels))return -1;
    return forge_rules::forgingCost(item.gemLevels);
}

bool Play::forgeBag(int equipmentIndex,int gemIndex,int refinerIndex){
    // Identidade de instancia e IDs, nao so IDs de tipos: consumiveis nunca
    // sao confundidos com uma segunda copia da mesma espada.
    if(equipmentIndex<0||gemIndex<0||refinerIndex<0 ||
       equipmentIndex==gemIndex||equipmentIndex==refinerIndex||gemIndex==refinerIndex||
       equipmentIndex>=static_cast<int>(bag_.size())||
       gemIndex>=static_cast<int>(bag_.size())||refinerIndex>=static_cast<int>(bag_.size())){
        say("Forja: indices da bolsa invalidos.");return false;
    }
    Stack& equipment=bag_[static_cast<std::size_t>(equipmentIndex)];
    const Stack gem=bag_[static_cast<std::size_t>(gemIndex)];
    const Stack refiner=bag_[static_cast<std::size_t>(refinerIndex)];
    const auto ei=items_.find(equipment.id), gi=items_.find(gem.id), ri=items_.find(refiner.id);
    if(ei==items_.end()||gi==items_.end()||ri==items_.end()||
       gi->second.type!=49||ri->second.type!=50||gem.count!=1||refiner.count!=1||
       equipment.count!=1){say("Forja: precisa de equipamento, gema tipo 49 e refiner tipo 50.");return false;}
    const auto rule=gemCatalog_.find(gem.id);
    if(rule==gemCatalog_.end()||!rule->second.compatible(ei->second.type)){
        say("Forja: gema nao compativel com este equipamento.");return false;
    }
    if(gem.stoneLevel<1||gem.stoneLevel>rule->second.maxLevel||
       refiner.stoneLevel<1||refiner.stoneLevel>9){say("Forja: nivel de pedra invalido.");return false;}
    const int price=forgeCost(equipmentIndex);
    if(price<0){say("Forja: equipamento sem encaixes abertos.");return false;}
    int slot=-1;
    for(int i=0;i<equipment.socketCount;++i){
        if(equipment.gemIds[i]==gem.id){slot=i;break;}
    }
    if(slot<0){
        for(int i=0;i<equipment.socketCount;++i)
            if(equipment.gemIds[i]==0){slot=i;break;}
    }
    if(slot<0){say("Forja: nenhum encaixe livre.");return false;}
    const int nextLevel=equipment.gemLevels[slot]+1;
    if(nextLevel>rule->second.maxLevel || gem.stoneLevel<nextLevel || refiner.stoneLevel<nextLevel){
        say("Forja: nivel da gema ou refiner insuficiente.");return false;
    }
    if(price>gold_){say("Forja: ouro insuficiente ("+std::to_string(price)+").");return false;}
    const int chance=forge_rules::forgingChanceWithFruitPer10000(nextLevel,
        itemForgeBoostFor_>0.f?itemForgeBoostLevel_:0);
    if(chance<=0){say("Forja: nivel nao suportado.");return false;}
    // Transacao: cobra o ouro, consome UMA gema e UM refiner tanto em
    // sucesso quanto falha. Equipamento nunca e destruido pelo sorteio.
    gold_-=price;
    const bool success=roll(10000)<chance;
    if(success){
        equipment.gemIds[slot]=gem.id;
        equipment.gemLevels[slot]=nextLevel;
        int total=0;
        for(int lv:equipment.gemLevels)total+=lv;
        equipment.forgeLevel=std::min(15,total); // teto herdado do campo Y1
    }
    for(int index : {std::max(gemIndex,refinerIndex),std::min(gemIndex,refinerIndex)}){
        Stack& consumed=bag_[static_cast<std::size_t>(index)];
        if(--consumed.count<=0)bag_.erase(bag_.begin()+index);
    }
    if(sheetReady_)applyAttrs(false);else recomputeGear();
    say(success?"Forja: gema aplicada (nivel "+std::to_string(nextLevel)+").":"Forja falhou: equipamento preservado; pedras consumidas.");
    save(savePath());
    return success;
}

bool Play::combineGems(int scrollIndex,int leftIndex,int rightIndex){
    if(scrollIndex<0||leftIndex<0||rightIndex<0||
       scrollIndex==leftIndex||scrollIndex==rightIndex||leftIndex==rightIndex||
       scrollIndex>=static_cast<int>(bag_.size())||
       leftIndex>=static_cast<int>(bag_.size())||rightIndex>=static_cast<int>(bag_.size())){
        say("Combinar: indices da bolsa invalidos.");return false;
    }
    const Stack scroll=bag_[static_cast<std::size_t>(scrollIndex)];
    const Stack left=bag_[static_cast<std::size_t>(leftIndex)];
    const Stack right=bag_[static_cast<std::size_t>(rightIndex)];
    const auto si=items_.find(scroll.id),li=items_.find(left.id),ri=items_.find(right.id);
    if(si==items_.end()||li==items_.end()||ri==items_.end()||
       si->second.type!=47 ||
       (li->second.type!=49 && li->second.type!=50) ||
       li->second.type!=ri->second.type ||left.id!=right.id ||
       scroll.count<1||left.count!=1||right.count!=1 ||
       left.stoneLevel!=right.stoneLevel ||left.stoneLevel<1 || left.stoneLevel>9){
        say("Combinar: precisa de pergaminho e duas gemas iguais no mesmo nivel.");return false;
    }
    int cap=9;
    if(li->second.type==49){
        const auto rule=gemCatalog_.find(left.id);
        if(rule==gemCatalog_.end()){
            say("Combinar: tipo de gema desconhecido.");return false;
        }
        cap=rule->second.maxLevel;
    }
    if(left.stoneLevel>=cap){say("Combinar: gema ja no nivel maximo.");return false;}
    if(gold_<5000){say("Combinar: faltam 5000 de ouro.");return false;}
    const int chance=forge_rules::combiningChanceWithFruitPer10000(left.stoneLevel,li->second.type,
        itemCombineBoostFor_>0.f?itemCombineBoostLevel_:0);
    if(chance<=0)return false;
    gold_-=5000;
    const bool success=roll(10000)<chance;
    const int leftUid=left.uid;
    const int newLevel=left.stoneLevel+1;
    // Em falha forge.lua remove os dois cristais e o pergaminho.
    // Em sucesso a copia da esquerda conserva o UID e recebe +1 nivel.
    std::array<int,3> removals{scrollIndex,rightIndex,success?-1:leftIndex};
    std::sort(removals.begin(),removals.end(),std::greater<int>());
    for(int index:removals){
        if(index<0)continue;
        auto& item=bag_[static_cast<std::size_t>(index)];
        if(--item.count<=0)bag_.erase(bag_.begin()+index);
    }
    if(success){
        const auto found=std::find_if(bag_.begin(),bag_.end(),[&](const Stack& x){return x.uid==leftUid;});
        if(found!=bag_.end())found->stoneLevel=newLevel;
    }
    say(success?"Combinacao bem-sucedida: gema nivel "+std::to_string(newLevel)+".":
        "Combinacao falhou: gemas e pergaminho consumidos.");
    save(savePath());
    return success;
}

bool Play::gmOpenSockets(int index,int sockets){
    if(index<0||index>=static_cast<int>(bag_.size())||sockets<1||sockets>3)return false;
    Stack& item=bag_[static_cast<std::size_t>(index)];
    const auto it=items_.find(item.id);
    if(it==items_.end()||item.count!=1)return false;
    // Classes de equipamento permitidas em Item_CanJinglian_ID, variable.lua.
    static const int types[]={1,2,3,4,7,11,20,22,23,24,27,9,25,26,81,82,83,88};
    if(std::find(std::begin(types),std::end(types),it->second.type)==std::end(types))return false;
    if(sockets<item.socketCount)return false;
    item.socketCount=sockets;
    say("GM offline: "+std::to_string(sockets)+" encaixes de teste abertos em "+it->second.name);
    save(savePath());return true;
}
void Play::gmAddGold(int amount){
    if(amount<=0)return;
    gold_=static_cast<int>(std::min(1000000000LL,static_cast<long long>(gold_)+amount));
    say("GM offline: +"+std::to_string(amount)+" ouro.");
    save(savePath());
}

void Play::gmAddExperience(int amount) {
    if(amount<=0) return;
    exp_=std::min(1000000000,exp_+amount);
    while(expNext_>0 && exp_>=expNext_ && player_.level<100) {
        exp_-=expNext_;
        ++player_.level;
        ap_+=progression::attrPointAt(player_.level);
        tp_+=progression::skillPointAt(player_.level);
        if(sheetReady_) applyAttrs(false);
        player_.hp=player_.maxHp;
        player_.homeY=player_.homeX;
        expNext_=experience_.needed(player_.level);
    }
    say("GM offline: +"+std::to_string(amount)+" XP; nivel "+std::to_string(player_.level));
    save(savePath());
}

void Play::grantItemExperience(int amount) {
    if(amount<=0) return;
    exp_=std::min(1000000000,exp_+amount);
    while(expNext_>0 && exp_>=expNext_ && player_.level<100) {
        exp_-=expNext_;
        ++player_.level;
        ap_+=progression::attrPointAt(player_.level);
        tp_+=progression::skillPointAt(player_.level);
        if(sheetReady_) applyAttrs(false);
        player_.hp=player_.maxHp;
        player_.homeY=player_.homeX;
        expNext_=experience_.needed(player_.level);
    }
}

bool Play::grantMarriamPkTest() {
    if (player_.level < 1 || player_.level >= 2) {
        say("Marriam: o teste de PK historico so pode ser usado no nivel 1.");
        return false;
    }
    constexpr int historicalExperience=116688305;
    constexpr int historicalGold=1000000;
    constexpr int maxGold=1000000000;
    if (gold_ > maxGold-historicalGold) {
        say("Marriam: limite de ouro atingido; teste preservado.");
        return false;
    }

    const Foe previousPlayer=player_;
    const int previousExp=exp_;
    const int previousExpNext=expNext_;
    const int previousAp=ap_;
    const int previousTp=tp_;
    const int previousGold=gold_;
    const std::string path=savePath();
    grantItemExperience(historicalExperience);
    gold_+=historicalGold;
    if(!path.empty() && !save(path)) {
        player_=previousPlayer;
        exp_=previousExp;
        expNext_=previousExpNext;
        ap_=previousAp;
        tp_=previousTp;
        gold_=previousGold;
        say("Marriam: falha ao salvar; nivel, experiencia e ouro foram preservados.");
        return false;
    }
    say("Marriam: teste de PK concluido; nivel "+std::to_string(player_.level)+
        ", +1000000G. PORT_APPROX: AddExpAndType tipo 2 (370500) sem trilha offline correspondente.");
    return true;
}

bool Play::gmGiveItem(int id,int count) {
    if(id<=0 || count<1 || count>99 || !items_.count(id)) { say("GM offline: ID de item invalido."); return false; }
    if(!addBag(id,count)) { say("GM offline: bolsa cheia."); return false; }
    say("GM offline: +"+std::to_string(count)+" "+items_.at(id).name);
    save(savePath());
    return true;
}

void Play::gmHeal() {
    player_.hp=player_.maxHp;
    player_.homeY=player_.homeX;
    deadFor_=0.f;
    say("GM offline: HP/SP restaurados.");
    save(savePath());
}

} // namespace top
