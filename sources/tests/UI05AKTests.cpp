// 05-AK: geometria de clique e UI procedural no fluxo de missoes.
#include "top/QuestPresentation.h"
#include "../Engine/ui/HudDrawList.h"
#include "../Engine/ui/screens/NpcDialogUI.h"
#include "../Engine/ui/screens/QuestUI.h"
#include "../Engine/ui/UIRenderer.h"
#include <cassert>
#include <string>
#include <vector>
int main() {
    using namespace top;
    const int x=(1280-320)/2,y=720-316;
    assert(npc_quest_layout::rowAt(x+22,y+106,x,y)==0);
    assert(npc_quest_layout::rowAt(x+22,y+135,x,y)==1);
    assert(npc_quest_layout::rowAt(x+22,y+193,x,y)==3);
    assert(npc_quest_layout::rowAt(x+22,y+223,x,y)==-1);
    assert(npc_quest_layout::rowAt(x+12,y+106,x,y)==-1);
    ui::NpcDialogUI dialog;
    dialog.setWindow(true,"TIntin","",false,true,true);
    std::vector<NpcQuestView> quests(6);
    for(int i=0;i<6;++i) {
        quests[i].id=i+1; quests[i].title="Quest "+std::to_string(i);
        quests[i].description="Descricao historica";
        quests[i].objective="Conseguir itens";
        quests[i].rewardGold=5;quests[i].rewardExp=10;
        quests[i].status=i==0?NpcQuestView::Unavailable:NpcQuestView::Available;
    }
    dialog.setMissions(quests,-1,0);
    ui::UIRenderer r(1280,720);
    ui::HudDrawList text;
    dialog.draw(r,1280,720);
    dialog.appendText(text,1280,720);
    assert(!r.empty()&&!text.labels.empty());
    bool seen=false;
    for(const auto& t:text.labels)if(t.value.find("Quest 1")!=std::string::npos)seen=true;
    assert(seen);
    dialog.setMissions(quests,4,1);
    ui::UIRenderer details(1280,720);
    ui::HudDrawList detailText;
    dialog.draw(details,1280,720);
    dialog.appendText(detailText,1280,720);
    seen=false;
    for(const auto& t:detailText.labels)if(t.value.find("Quest 4")!=std::string::npos)seen=true;
    assert(seen&&!details.empty());
    ui::UIRenderer log(1280,720);
    ui::QuestUI::draw(log,10,64,{},true);
    ui::HudDrawList journal;
    ui::QuestUI::appendText(journal,{"[Ativa] Quest 4","Itens 1/3"},10,64);
    assert(!log.empty()&&!journal.labels.empty());
    return 0;
}
