// Adaptador NPC/UI: conversas e cliques direcionados as regras de jogo.
// Nao calcula recompensas nem le arquivos Lua.
#include "top/Play.h"
#include "top/MissionTypes.h"
#include "../../ui/QuestDialogInput.h"
#include <algorithm>
namespace top {
void Play::talkNpc(const std::string& name, const std::string& text, bool trade, bool forge) {
    const std::string npcName=name.empty()?"NPC":name;
    auto appendRunning=[&](const HistoricalProgress& mission) {
        if(!mission.active || !mission.turnInRequired || mission.turnInNpc!=npcName)return;
        if(std::any_of(npcMissionChoices_.begin(),npcMissionChoices_.end(),
            [&](const MissionOffer& m){return m.id==mission.id;}))return;
        MissionOffer display;
        display.id=mission.id;display.title=mission.title;display.objective=mission.objective;
        display.rewardExp=mission.rewardExp;display.rewardGold=mission.rewardGold;
        for(const auto& item:mission.neededItems)display.neededItems.push_back(item);
        npcMissionChoices_.push_back(std::move(display));
        npcMissionDescriptions_.push_back(mission.objective);
    };
    appendRunning(historical_);
    for(const auto& mission:additionalMissions_)appendRunning(mission);
    if(hasWelcomeMission() && worldMap_=="garner" && npcName.find("Senna")!=std::string::npos) {
        if(std::none_of(npcMissionChoices_.begin(),npcMissionChoices_.end(),
            [](const MissionOffer& m){return m.id==1;})) {
            MissionOffer welcome;
            welcome.id=1;welcome.title="Welcome";welcome.objective=historical_.objective;
            welcome.rewardExp=6;
            npcMissionChoices_.insert(npcMissionChoices_.begin(),std::move(welcome));
            npcMissionDescriptions_.insert(npcMissionDescriptions_.begin(),historical_.objective);
        }
    }
    if (text.empty() && !trade && !forge && npcMissionChoices_.empty()) return;
    npcOffersTrade_ = trade && vendorOn_;
    npcOffersForge_ = forge;
    shopOpen_ = false;
    npcTalkName_ = name.empty() ? "NPC" : name;
    npcTalkText_ = text.empty() ? "Bem-vindo! Como posso ajudar?" : text;
    npcTalkFor_ = 8.f;
    npcTalkOpen_ = true;
    say(npcTalkName_ + ": " + npcTalkText_.substr(0, std::min<std::size_t>(npcTalkText_.size(), 72)));
}

int Play::dialogActionAt(int x, int y, int screenW, int screenH) const {
    // 05-AT: geometria de UI fica em Engine/ui. Play apenas informa o
    // estado autorizado (a entrega continua validada em clickDialog).
    ui::QuestDialogHitState state;
    state.open = npcTalkOpen_;
    state.trade = npcOffersTrade_;
    state.forge = npcOffersForge_;
    state.offer = offer_.active;
    state.missionCount = static_cast<int>(npcMissionChoices_.size());
    state.missionSelected = npcMissionSelected_;
    state.missionPage = npcMissionPage_;
    if (npcMissionSelected_ >= 0 && npcMissionSelected_ < state.missionCount) {
        state.ready = missionReadyAtNpc(
            npcMissionChoices_[static_cast<std::size_t>(npcMissionSelected_)].id,
            npcTalkName_);
    }
    return ui::QuestDialogInput::hit(state, x, y, screenW, screenH);
}

void Play::clickDialog(int action) {
    if (!npcTalkOpen_) return;
    if(!npcMissionChoices_.empty()) {
        if(action>=10&&action<10+npc_quest_layout::kRows) {
            const int index=npcMissionPage_*npc_quest_layout::kRows+action-10;
            if(index>=0&&index<int(npcMissionChoices_.size())) {
                npcMissionSelected_=index;
                offer_=MissionOffer{};
                const auto& m=npcMissionChoices_[static_cast<std::size_t>(index)];
                if(missionAvailable(m)) {
                    offerHistoricalMission(m.id,m.title,m.objective,m.targetX,m.targetY,
                        m.targetId,m.targetCount,m.rewardExp,m.rewardGold,m.minLevel,m.maxLevel,
                        m.neededItems,m.rewardItems);
                }
            }
            return;
        }
        if(action==30){npcMissionPage_=std::max(0,npcMissionPage_-1);return;}
        if(action==31){npcMissionPage_=std::min((int(npcMissionChoices_.size())-1)/npc_quest_layout::kRows,npcMissionPage_+1);return;}
        if(action==40)return;
        if(action==4&&npcOffersForge_){closeDialog();return;}
        if(action==3&&npcOffersTrade_){interact(hereX_,hereY_);if(shopOpen_)closeDialog();return;}
        if(action==2){npcMissionSelected_=-1;offer_=MissionOffer{};return;}
        if(action==5 && npcMissionSelected_>=0 && npcMissionSelected_<int(npcMissionChoices_.size())) {
            const int selectedId=npcMissionChoices_[std::size_t(npcMissionSelected_)].id;
            if(!missionReadyAtNpc(selectedId,npcTalkName_))return;
            const bool done=selectedId==1?deliverWelcomeToSenna(npcTalkName_):
                deliverHistoricalToNpc(npcTalkName_,selectedId);
            if(done)closeDialog();
            return;
        }
        if(action==1&&offer_.active) {
            if(npcMissionSelected_<0||npcMissionSelected_>=int(npcMissionChoices_.size())||
               !missionAvailable(npcMissionChoices_[static_cast<std::size_t>(npcMissionSelected_)]))return;
            const bool accepted=acceptHistoricalMission(offer_.id,offer_.title,offer_.objective,
                  offer_.targetX,offer_.targetY,offer_.targetId,offer_.targetCount,
                  offer_.rewardExp,offer_.rewardGold,offer_.minLevel,offer_.maxLevel,
                  offer_.neededItems,offer_.rewardItems);
            if(accepted)closeDialog();
            return;
        }
        if(action==0){closeDialog();return;}
    }
    if (action == 4 && npcOffersForge_) { closeDialog(); return; }
    if (action==3 && npcOffersTrade_) {
        interact(hereX_,hereY_);
        if (shopOpen_) closeDialog();
        return;
    }
    if (offer_.active) {
        if (action == 1) {
            acceptHistoricalMission(offer_.id, offer_.title, offer_.objective, offer_.targetX, offer_.targetY,
                                    offer_.targetId, offer_.targetCount, offer_.rewardExp, offer_.rewardGold,
                                    offer_.minLevel, offer_.maxLevel, offer_.neededItems, offer_.rewardItems);
            offer_ = MissionOffer{};
            closeDialog();
        } else if (action == 2) {
            say("Missao recusada: " + offer_.title);
            offer_ = MissionOffer{};
            closeDialog();
        }
        return;
    }
    closeDialog();
}

void Play::closeDialog() {
    npcTalkOpen_ = false;
    npcTalkFor_ = 0.f;
    npcOffersTrade_ = false;
    npcOffersForge_ = false;
    offer_ = MissionOffer{};
    npcMissionChoices_.clear();npcMissionDescriptions_.clear();
    npcMissionSelected_=-1;npcMissionPage_=0;
}


} // namespace top
