#include "top/QuestPresentation.h"
#include "../Engine/ui/HudDrawList.h"
#include "../Engine/ui/screens/NpcDialogUI.h"
#include "../Engine/ui/UIRenderer.h"
#include <cassert>
#include <string>
int main() {
    using namespace top;
    ui::NpcDialogUI npc;
    npc.setWindow(true,"Newbie Guide - Senna","Bem vindo!",true,false,false);
    NpcQuestView ready;
    ready.id=701;ready.title="Blacksmith's Greetings";
    ready.objective="Retorne com a carta";
    ready.description="Goldie recebeu a carta.";
    ready.rewardExp=9;ready.status=NpcQuestView::Ready;
    npc.setMissions({ready},0,0);
    ui::UIRenderer renderer(1280,720);
    ui::HudDrawList list;
    npc.draw(renderer,1280,720);
    npc.appendText(list,1280,720);
    bool complete=false,status=false;
    for(const auto& label:list.labels){
        if(label.value=="Completar")complete=true;
        if(label.value=="Pronta para entregar")status=true;
    }
    assert(complete && status && !renderer.empty());
    ready.status=NpcQuestView::Available;
    npc.setMissions({ready},0,0);
    ui::HudDrawList accept;
    npc.appendText(accept,1280,720);
    bool accepts=false;
    for(const auto& label:accept.labels)if(label.value=="Aceitar")accepts=true;
    assert(accepts);
    return 0;
}
