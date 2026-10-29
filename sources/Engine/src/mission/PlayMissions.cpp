// Regras de missao: requisitos, flags, objetivos, recompensas, aceite,
// abandono e persistencia. A fachada Play permanece por compatibilidade.
#include "top/Play.h"
#include "top/MissionTypes.h"
#include "top/SavePaths.h"
#include "top/ProgressionPoints.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
namespace top {
void Play::completeHistoricalMission() { completeHistoricalMission(historical_); }
void Play::completeHistoricalMission(HistoricalProgress& mission) {
    if (!historicalObjectivesReady(mission) || !missionRewardsFit(mission)) return;
    for (const MissionItem& item : mission.neededItems) removeBag(item.id, item.count);
    for (const MissionItem& item : mission.rewardItems) addBag(item.id, item.count);
    mission.active = false;
    mission.complete = true;
    exp_ = std::min(1000000000, exp_ + std::max(0, mission.rewardExp));
    gold_ += mission.rewardGold;
    say("Missao historica concluida: " + mission.title + "  +" + std::to_string(mission.rewardExp) +
        " exp  +" + std::to_string(mission.rewardGold) + " ouro");
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
    }
    save(savePath());
}

bool Play::missionItemsReady() const { return missionItemsReady(historical_); }
bool Play::missionItemsReady(const HistoricalProgress& mission) const {
    std::unordered_map<int, int> totals;
    for (const MissionItem& item : mission.neededItems) {
        if (item.id > 0 && item.count > 0) totals[item.id] += item.count;
    }
    for (const auto& item : totals) if (bagCount(item.first) < item.second) return false;
    return true;
}

void Play::onHistoricalEvent(MissionEventKind kind, int id, int amount) {
    auto update=[&](HistoricalProgress& m) {
        if(!m.active || m.triggers.empty())return;
        if(!applyMissionEvent(m.triggers,m.triggerCounts,m.flags,kind,id,amount))return;
        // A flag final habilita o botao Completar, sem conceder recompensa
        // automaticamente a uma missao com entrega historica.
        if(!m.turnInRequired && historicalObjectivesReady(m))completeHistoricalMission(m);
    };
    update(historical_);
    for(auto& m:additionalMissions_)update(m);
}

bool Play::historicalObjectivesReady(const HistoricalProgress& mission) const {
    if(!mission.triggers.empty()) {
        if(mission.triggerCounts.size()!=mission.triggers.size())return false;
        // Kill counters are mandatory even when the historical result did
        // not explicitly spell out a final flag. Item deliveries still
        // require the physical items via missionItemsReady below.
        for(std::size_t i=0;i<mission.triggers.size();++i)
            if(mission.triggers[i].kind==MissionEventKind::Kill &&
               mission.triggerCounts[i]<mission.triggers[i].required)return false;
    }
    for(int flag:mission.requiredFlags)
        if(std::find(mission.flags.begin(),mission.flags.end(),flag)==mission.flags.end())return false;
    for(int flag:mission.forbiddenFlags)
        if(std::find(mission.flags.begin(),mission.flags.end(),flag)!=mission.flags.end())return false;
    return mission.active && (mission.targetCount <= 0 || mission.have >= mission.targetCount) &&
           missionItemsReady(mission);
}

bool Play::missionRewardsFit() const { return missionRewardsFit(historical_); }
bool Play::missionRewardsFit(const HistoricalProgress& mission) const {
    std::vector<Stack> simulated = bag_;
    auto remove = [&](int id, int count) {
        for (std::size_t i = 0; i < simulated.size() && count > 0;) {
            Stack& stack = simulated[i];
            if (stack.id != id) {
                ++i;
                continue;
            }
            const int used = std::min(count, stack.count);
            stack.count -= used;
            count -= used;
            if (stack.count == 0) simulated.erase(simulated.begin() + static_cast<std::ptrdiff_t>(i));
            else ++i;
        }
        return count == 0;
    };
    for (const MissionItem& item : mission.neededItems) {
        if (item.id > 0 && item.count > 0 && !remove(item.id, item.count)) return false;
    }
    for (const MissionItem& item : mission.rewardItems) {
        if (item.id <= 0 || item.count <= 0) continue;
        const auto found = items_.find(item.id);
        if (!addStack(simulated, item.id, item.count, found==items_.end()?1:found->second.maxStack, bagCapacity_))
            return false;
    }
    return true;
}

std::string Play::missionLockReason(int id,int minLevel,int maxLevel,const MissionGate& gate) const {
    if(id<=0)return "ID de missao invalido";
    if(const auto* quest=findHistoricalMission(id))
        return quest->complete?"Missao concluida":"Missao em andamento";
    if(historicalActiveCount()>=kMaxActiveMissions)return "Diario cheio (8 missoes)";
    if(player_.level<std::max(1,minLevel))return "Exige nivel "+std::to_string(std::max(1,minLevel));
    if(!kAllowPastLevelQuests && maxLevel>0&&player_.level>maxLevel)return "Nivel maximo permitido: "+std::to_string(maxLevel);
    if(!gate.unsupported.empty())return gate.unsupported+" (indisponivel offline)";
    if(gate.jobEqual>=0&&job_!=gate.jobEqual)return "Exige profissao "+std::to_string(gate.jobEqual);
    if(gate.jobNotEqual>=0&&job_==gate.jobNotEqual)return "Profissao atual nao permitida";
    // Registros historicos correspondem aos IDs de AddMission nos scripts,
    // nem sempre ao primeiro argumento de DefineMission. Nao assumir que
    // existe registro se ele nao aparece no indice de missoes conhecido.
    auto recordStatus=[&](int record,bool completed) {
        const auto found=missionsByRecord_.find(record);
        if(found!=missionsByRecord_.end()) {
            for(int missionId:found->second) {
                const auto* progress=findHistoricalMission(missionId);
                if(progress&&(completed?progress->complete:progress->active))return true;
            }
        }
        // Compatibilidade com missões feitas por IDs diretos em saves antigos.
        const auto* progress=findHistoricalMission(record);
        return progress&&(completed?progress->complete:progress->active);
    };
    for(int record:gate.needsCompleted)
        if(!recordStatus(record,true))return "Conclua primeiro a missao "+std::to_string(record);
    for(int record:gate.needsNotCompleted)
        if(recordStatus(record,true))return "Missao anterior ja concluida: "+std::to_string(record);
    for(int record:gate.needsActive)
        if(!recordStatus(record,false))return "Requer missao ativa "+std::to_string(record);
    for(int record:gate.needsNotActive)
        if(recordStatus(record,false))return "Missao "+std::to_string(record)+" ja esta ativa";
    for(const auto& item:gate.needsItems)
        if(bagCount(item.id)<item.count)
            return "Exige item "+std::to_string(item.id)+" x"+std::to_string(item.count);
    return {};
}

bool Play::hasActiveHistoricalMission(int missionId) const {
    if (missionId <= 0) return false;
    const auto* progress = findHistoricalMission(missionId);
    return progress && progress->active;
}

bool Play::hasHistoricalRecord(int recordId) const {
    if (recordId <= 0) return false;
    if (historicalRecords_.count(recordId)) return true;
    const auto found = missionsByRecord_.find(recordId);
    if (found != missionsByRecord_.end()) {
        for (int missionId : found->second) {
            const auto* progress = findHistoricalMission(missionId);
            if (progress && progress->complete) return true;
        }
    }
    // Compatibility with old saves whose mission ID was stored directly.
    const auto* progress = findHistoricalMission(recordId);
    return progress && progress->complete;
}

bool Play::hasNewcomerHealingEntitlement() const {
    return player_.level < 6 && hasHistoricalRecord(500);
}

bool Play::hasWelcomeMission() const {
    const auto* m=findHistoricalMission(1);
    return m && m->active;
}
bool Play::deliverWelcomeToSenna(const std::string& npcName) {
    const auto* m=findHistoricalMission(1);
    if(!m || !m->active || npcName.find("Senna")==std::string::npos ||
       worldMap_!="garner") return false;
    // A missao mundial nao é concluida por proximidade: entrega real ao NPC.
    HistoricalProgress* progress=findHistoricalMission(1);
    completeHistoricalMission(*progress); // 6 EXP historicos; sobe para Lv 2 (DEXP[2]=5).
    return progress->complete;
}
bool Play::applyNpcMissionStep(const HistoricalMissionStep& step) {
    if(step.displayId<=0 || step.recordId<=0 || step.absentFlag<=0 ||
       step.resultingFlag!=step.absentFlag || step.takeItems.empty())return false;
    // The record mapping is sourced from AddMission(), not DefineMission's display ID.
    std::vector<int> ids;
    const auto record=missionsByRecord_.find(step.recordId);
    if(record!=missionsByRecord_.end()) ids=record->second;
    else ids.push_back(step.recordId); // historical direct-ID save compatibility
    for(int id:ids) {
        auto* progress=findHistoricalMission(id);
        if(!progress || !progress->active || !progress->turnInRequired ||
           std::find(progress->flags.begin(),progress->flags.end(),step.absentFlag)!=progress->flags.end())continue;
        // Only a step producing a flag required by this mission is relevant.
        if(std::find(progress->requiredFlags.begin(),progress->requiredFlags.end(),step.resultingFlag)==progress->requiredFlags.end())continue;
        std::vector<Stack> trial=bag_;
        bool ok=true;
        for(const auto& item:step.takeItems) {
            if(item.id<=0 || item.count<=0) {ok=false;break;}
            int remain=item.count;
            for(std::size_t i=0;i<trial.size() && remain>0;) {
                if(trial[i].id!=item.id){++i;continue;}
                const int take=std::min(remain,trial[i].count);
                trial[i].count-=take;remain-=take;
                if(trial[i].count==0)trial.erase(trial.begin()+static_cast<std::ptrdiff_t>(i));
                else ++i;
            }
            if(remain>0){ok=false;break;}
        }
        if(!ok)continue;
        bag_=std::move(trial);
        progress->flags.push_back(step.resultingFlag);
        say("Etapa da missao concluida: "+progress->title);
        save(savePath());
        return true;
    }
    return false;
}

bool Play::missionReadyAtNpc(int id, const std::string& npcName) const {
    const auto* mission=findHistoricalMission(id);
    if(!mission || !mission->active || npcName.empty())return false;
    if(id==1)return worldMap_=="garner" && npcName.find("Senna")!=std::string::npos;
    return mission->turnInRequired && mission->turnInNpc==npcName &&
           historicalObjectivesReady(*mission) && missionRewardsFit(*mission);
}
bool Play::deliverHistoricalToNpc(const std::string& npcName, int missionId) {
    if(npcName.empty())return false;
    auto deliver=[&](HistoricalProgress& mission) {
        if((missionId>0 && mission.id!=missionId) || !mission.active || !mission.turnInRequired ||
           mission.turnInNpc!=npcName || !historicalObjectivesReady(mission) ||
           !missionRewardsFit(mission))return false;
        completeHistoricalMission(mission);
        return mission.complete;
    };
    bool delivered=deliver(historical_);
    for(auto& m:additionalMissions_)if(deliver(m)) delivered=true;
    return delivered;
}
std::vector<MapQuestView> Play::availableMapQuests() const {
    std::vector<MapQuestView> out;
    for(const auto& npc:questNpcMarkers_) {
        for(const auto& m:npc.missions) {
            if(!m.valid || !missionLockReason(m.id,m.minLevel,m.maxLevel,m.gate).empty())continue;
            auto dup=std::find_if(out.begin(),out.end(),[&](const MapQuestView& q){return q.id==m.id && q.npcName==npc.npcName;});
            if(dup!=out.end())continue;
            MapQuestView q;
            q.id=m.id; q.title=m.title; q.npcName=npc.npcName;
            q.x=npc.x;q.y=npc.y;
            q.distance=std::hypot(npc.x-hereX_,npc.y-hereY_);
            out.push_back(std::move(q));
        }
    }
    std::sort(out.begin(),out.end(),[](const MapQuestView& a,const MapQuestView& b){
        if(a.distance!=b.distance)return a.distance<b.distance;
        return a.id<b.id;
    });
    return out;
}
bool Play::missionAvailable(const MissionOffer& m) const {
    if(!missionLockReason(m.id,m.minLevel,m.maxLevel,m.gate).empty())return false;
    return (m.targetX>=0.f&&m.targetY>=0.f)||(m.targetId>0&&m.targetCount>0)||
           !m.neededItems.empty() || (npcMissionFlagRules_.count(m.id) &&
           !npcMissionFlagRules_.at(m.id).eventTriggers.empty());
}
void Play::setNpcMissionList(const std::vector<HistoricalMission>& missions) {
    npcMissionChoices_.clear();npcMissionDescriptions_.clear();
    npcMissionSelected_=-1;npcMissionPage_=0;offer_=MissionOffer{};
    npcMissionGates_.clear();npcMissionTurnIns_.clear();npcMissionFlagRules_.clear();
    for(const auto& m:missions) {
        if(npcMissionChoices_.size()>=128)break;
        // Guardar requisitos mesmo quando a oferta fica oculta. Um fluxo de
        // aceite chamado por API nao pode ignorar os bloqueios de Lua.
        npcMissionGates_[m.id]=m.gate;
        npcMissionTurnIns_[m.id]=m.turnInRequired;
        npcMissionFlagRules_[m.id]=m;
        if(!missionLockReason(m.id,m.minLevel,m.maxLevel,m.gate).empty()) continue;
        const bool hasObjective=(m.targetX>=0.f&&m.targetY>=0.f)||
                    (m.targetId>0&&m.targetCount>0)||!m.neededItems.empty() ||
                    !m.eventTriggers.empty();
        if(!hasObjective) continue;
        MissionOffer offer;
        offer.id=m.id;offer.title=m.title;offer.objective=m.objective.empty()?m.beginText:m.objective;
        offer.targetX=m.targetX;offer.targetY=m.targetY;
        offer.targetId=m.targetId;offer.targetCount=m.targetCount;
        offer.rewardExp=m.rewardExp;offer.rewardGold=m.rewardGold;
        offer.minLevel=m.minLevel;offer.maxLevel=m.maxLevel;
        offer.gate=m.gate;offer.turnInRequired=m.turnInRequired;
        npcMissionGates_[m.id]=m.gate;
        for(const auto& item:m.neededItems)offer.neededItems.push_back({item.id,item.count});
        for(const auto& item:m.rewardItems)offer.rewardItems.push_back({item.id,item.count});
        npcMissionChoices_.push_back(std::move(offer));
        npcMissionDescriptions_.push_back(m.beginText);
    }
}
bool Play::abandonHistoricalMission() {
    if(journalSelectedId_!=0) return journalSelectedId_>0 && abandonHistoricalMission(journalSelectedId_);
    if(historical_.active) return abandonHistoricalMission(historical_.id);
    for(const auto& m:additionalMissions_)if(m.active)return abandonHistoricalMission(m.id);
    return false;
}
bool Play::abandonHistoricalMission(int missionId) {
    HistoricalProgress* m=findHistoricalMission(missionId);
    if(!m || !m->active) return false;
    const std::string title=m->title;
    if(m==&historical_)historical_=HistoricalProgress{};
    else additionalMissions_.erase(std::remove_if(additionalMissions_.begin(),additionalMissions_.end(),
         [&](const HistoricalProgress& p){return p.id==missionId;}),additionalMissions_.end());
    journalSelectedId_=0;
    say("Missao abandonada: "+title);
    save(savePath());
    return true;
}
std::size_t Play::historicalActiveCount() const {
    std::size_t count=historical_.active?1:0;
    for(const auto& m:additionalMissions_)if(m.active)++count;
    return count;
}
bool Play::historicalMissionActive() const {return historicalActiveCount()>0;}
const Play::HistoricalProgress* Play::findHistoricalMission(int id) const {
    if(id<=0)return nullptr;
    if(historical_.id==id)return &historical_;
    for(const auto& m:additionalMissions_)if(m.id==id)return &m;
    return nullptr;
}
Play::HistoricalProgress* Play::findHistoricalMission(int id) {
    return const_cast<HistoricalProgress*>(static_cast<const Play*>(this)->findHistoricalMission(id));
}
bool Play::selectJournalAction(int action) {
    std::vector<int> ids;
    if(historical_.id>0)ids.push_back(historical_.id);
    for(const auto& m:additionalMissions_)if(m.id>0)ids.push_back(m.id);
    for(int i=0;i<3;++i)ids.push_back(-i-1);
    const int pages=std::max(1,(int(ids.size())+4)/5);
    if(action==20){journalPage_=std::max(0,journalPage_-1);return true;}
    if(action==21){journalPage_=std::min(pages-1,journalPage_+1);return true;}
    if(action>=1&&action<=5){
        const int index=journalPage_*5+action-1;
        if(index<0||index>=int(ids.size()))return false;
        journalSelectedId_=ids[static_cast<std::size_t>(index)];
        return true;
    }
    return false;
}
bool Play::offerHistoricalMission(int id, const std::string& title, const std::string& objective, float targetX,
                                  float targetY, int targetId, int targetCount, int rewardExp, int rewardGold,
                                  int minLevel, int maxLevel, const std::vector<MissionItem>& neededItems,
                                  const std::vector<MissionItem>& rewardItems) {
    const bool destination = targetX >= 0.f && targetY >= 0.f;
    const bool hunting = targetId > 0 && targetCount > 0;
    if (id <= 0 || (!destination && !hunting && neededItems.empty() &&
        (npcMissionFlagRules_.count(id)==0 || npcMissionFlagRules_.at(id).eventTriggers.empty())) ||
        historicalActiveCount()>=kMaxActiveMissions || findHistoricalMission(id))
        return false;
    if (player_.level < std::max(1, minLevel) || (!kAllowPastLevelQuests && maxLevel > 0 && player_.level > maxLevel)) return false;
    offer_ = MissionOffer{};
    offer_.id = id;
    offer_.title = title;
    offer_.objective = objective;
    offer_.targetX = targetX;
    offer_.targetY = targetY;
    offer_.targetId = targetId;
    offer_.targetCount = targetCount;
    offer_.rewardExp = rewardExp;
    offer_.rewardGold = rewardGold;
    offer_.minLevel = minLevel;
    offer_.maxLevel = maxLevel;
    offer_.neededItems = neededItems;
    offer_.rewardItems = rewardItems;
    offer_.active = true;
    return true;
}

bool Play::acceptHistoricalMission(int id, const std::string& title, const std::string& objective, float targetX,
                                   float targetY, int targetId, int targetCount, int rewardExp, int rewardGold,
                                   int minLevel, int maxLevel, const std::vector<MissionItem>& neededItems,
                                   const std::vector<MissionItem>& rewardItems) {
    const bool destination = targetX >= 0.f && targetY >= 0.f;
    const bool hunting = targetId > 0 && targetCount > 0;
    if (id <= 0 || (!destination && !hunting && neededItems.empty() &&
        (npcMissionFlagRules_.count(id)==0 || npcMissionFlagRules_.at(id).eventTriggers.empty())) ||
        player_.level < std::max(1, minLevel) ||
        (!kAllowPastLevelQuests && maxLevel > 0 && player_.level > maxLevel))
        return false;
    if(findHistoricalMission(id) || historicalActiveCount()>=kMaxActiveMissions) return false;
    const auto gateIt=npcMissionGates_.find(id);
    if(gateIt!=npcMissionGates_.end() && !missionLockReason(id,minLevel,maxLevel,gateIt->second).empty())return false;
    HistoricalProgress* dest=nullptr;
    if(historical_.id==0) dest=&historical_;
    else {
        // Never overwrite a finished legacy mission or a running one.
        if(additionalMissions_.size()>=kMaxSavedExtraMissions) return false;
        additionalMissions_.emplace_back();
        dest=&additionalMissions_.back();
    }
    // 05-AR: introductory mission items are granted only if the entire
    // acceptance fits in the bag; never accept a mission without its letter.
    const auto introductory=npcMissionFlagRules_.find(id);
    if(introductory!=npcMissionFlagRules_.end()) {
        std::vector<Stack> trial=bag_;
        for(const auto& item:introductory->second.beginItems) {
            const auto info=items_.find(item.id);
            if(item.id<=0 || item.count<=0 ||
               !addStack(trial,item.id,item.count,info==items_.end()?1:info->second.maxStack,bagCapacity_)) {
                if(dest!=&historical_)additionalMissions_.pop_back();
                return false;
            }
        }
        bag_=std::move(trial);
        assignItemUids();
    }
    dest->id=id; dest->title=title; dest->objective=objective;
    dest->targetX=targetX; dest->targetY=targetY;
    dest->rewardExp=std::max(0,rewardExp); dest->rewardGold=std::max(0,rewardGold);
    dest->targetId=targetId; dest->targetCount=std::max(0,targetCount);
    dest->have=0; dest->neededItems=neededItems; dest->rewardItems=rewardItems;
    dest->active=true; dest->complete=false;
    const auto turnIn=npcMissionTurnIns_.find(id);
    dest->turnInRequired=turnIn!=npcMissionTurnIns_.end() && turnIn->second && npcTalkOpen_;
    dest->turnInNpc=dest->turnInRequired?npcTalkName_:std::string{};
    const auto flagRules=npcMissionFlagRules_.find(id);
    if(flagRules!=npcMissionFlagRules_.end()) {
        dest->flags=flagRules->second.beginFlags;
        dest->requiredFlags=flagRules->second.resultHasFlags;
        dest->forbiddenFlags=flagRules->second.resultNoFlags;
        dest->triggers=flagRules->second.eventTriggers;
        dest->triggerCounts.assign(dest->triggers.size(),0);
    }
    journalSelectedId_=id;
    say("Missao historica aceita: " + dest->title);
    save(savePath());
    return true;
}


} // namespace top
