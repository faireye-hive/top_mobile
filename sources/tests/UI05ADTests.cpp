// 05-AD: servicos realmente declarados pelo NPC + tab restrita + hitboxes.
#include "top/NpcForgeServices.h"
#include "top/NpcSystem.h"
#include "../Engine/ui/screens/ForgeWorkbenchUI.h"
#include "../Engine/ui/screens/NpcDialogUI.h"
#include <cassert>
#include <iostream>
int main(){
    const std::string lua=R"(
        function r_trade01 ()
          Talk(1,"Goldie")
          Text(1,"Trade",BuyPage)
          Text(1,"Combine",OpenUnite)
          Text(1,"Forge",OpenForge)
          Text(1,"Socket",OpenMilling)
          -- Text(1,"Double",OpenForge)
        end
        function r_talk30 ()
          Talk(1,"Other Blacksmith")
          Text(1,"Repair",OpenRepair)
          Text(1,"Forge",OpenForge)
        end
        function r_fake ()
          -- Text(1, "Forge", OpenForge)
          Talk(1,"OpenForge is only a word")
          Text(1,"Other",OpenUniteExtended)
        end
    )";
    using namespace top;
    assert(readNpcForgeServices(lua,"r_trade01")==7u);
    assert(readNpcForgeServices(lua,"r_talk30")==kNpcForge);
    assert(readNpcForgeServices(lua,"r_fake")==0u);
    assert(readNpcForgeServices(lua,"does_not_exist")==0u);
    top::NpcSystem sys;
    // Test the actual game scripts when present. This test is independent
    // of external Client content so that CI can run without the large assets.
    const unsigned goldie=sys.forgeServices("r_trade01");
    if(goldie!=0)assert(goldie==7u);
    // Argent's Monica and Icicle's Belinda use the original OpenBank action.
    // Their NPC names are presentation only; script actions control service.
    assert(sys.bankService("r_talk13"));
    assert(sys.bankService("r_talk81"));
    top::ui::ForgeWorkbenchUI bench;
    bench.open(kNpcForge|kNpcSocket);
    assert(bench.visible());
    assert(bench.allowedModes()==5u);
    assert(bench.mode()==top::ui::ForgeWorkbenchUI::Mode::Forge);
    const auto b=top::ui::ForgeWorkbenchUI::bounds(1280,720);
    assert(bench.hit(int(b.x+122),int(b.y+48),1280,720)==0);
    bench.setMode(top::ui::ForgeWorkbenchUI::Mode::Combine);
    assert(bench.mode()==top::ui::ForgeWorkbenchUI::Mode::Forge);
    bench.setMode(top::ui::ForgeWorkbenchUI::Mode::Socket);
    assert(bench.mode()==top::ui::ForgeWorkbenchUI::Mode::Socket);
    bench.close();bench.toggle();
    assert(bench.allowedModes()==7u);
    top::ui::NpcDialogUI npc;
    npc.setWindow(true,"Goldie","Hi!",false,true,true);
    assert(npc.hasTrade()&&npc.hasForge());
    npc.setWindow(false,"Goldie","Hi!",false,true,true);
    assert(!npc.hasForge());
    npc.setWindow(true,"Newbie Guide - Senna","Choose a topic.");
    npc.setMissions({top::NpcQuestView{}},-1,0);
    npc.setDialogOptions({"Star of Unity","Regarding Argent City","Regarding Classes and Attributes",
                          "Regarding Monster Coordinates","I have understood all"},false,true);
    assert(npc.tutorialGuideHit(720,444,1280,720)==1);
    npc.setMissions({top::NpcQuestView{}},0,0);
    assert(npc.tutorialGuideHit(720,444,1280,720)==-1);
    npc.setMissions({top::NpcQuestView{}},-1,0);
    npc.setDialogOptions({"Star of Unity","Regarding Argent City","Regarding Classes and Attributes",
                          "Regarding Monster Coordinates","I have understood all"},true,true);
    assert(npc.dialogOptionHit(500,512,1280,720)==0);
    assert(npc.dialogOptionHit(500,612,1280,720)==4);
    assert(npc.tutorialFooterHit(520,674,1280,720)==1);
    assert(npc.tutorialFooterHit(650,674,1280,720)==0);
    npc.setDialogOptions({},false,false);
    std::cout<<"05-AD: scripted forge services, NPC dialog and restricted workbench OK\n";
}
