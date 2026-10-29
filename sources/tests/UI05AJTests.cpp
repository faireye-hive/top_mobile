// 05-AJ: dados passivos dos NPCs e selecao da primeira missao elegivel.
#include "top/NpcLuaData.h"
#include "top/NpcMissionSelection.h"
#include "top/NpcSystem.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace top;
int main() {
    const std::string lua = R"lua(
        -- function r_shop() InitTrade() Weapon(999) end
        function r_shop_extra()
           Talk(1,"Outra loja")
           InitTrade() Weapon(999)
        end
        function r_shop()
           Talk (1, "Goldie: Olá! \"Forge\" - não é função")
           -- Text(1,"False",OpenForge)
           local text = "AddNpcMission(777) Weapon(444)"
           --[=[ AddNpcMission(888) ]=]
           Obj.AddNpcMission(667)
           Obj.Weapon(666)
           local function hidden()
             Talk(1,"ESCONDIDO")
             AddNpcMission(999)
             InitTrade()
             Weapon(998)
           end
           if permission then
              AddNpcMission ( 701 )
           end
           AddNpcMission(
             631
           )
           AddNpcMission( 42 + 1 )
           InitTrade()
           Weapon(0290)
           Defence(0466)
           Other(1001)
           for i=1,2 do
             if stock then
               Pet(88)
             end
           end
           Talk(2, "Segunda pagina")
        end
        function r_bank()
           Talk(1, "Banker label is not the service")
           InitTrigger()
           TriggerCondition(1, HasMoney, 200)
           TriggerAction(1, TakeMoney, 200)
           TriggerAction(1, OpenBank)
        end
        function r_fake_bank()
           Talk(1, "OpenBank")
           Text(1, "Vault (200G Fee)", BuyPage)
           -- TriggerAction(1, OpenBank)
        end
        function r_tutorial()
           Talk(1, "Main page")
           Text(1, "Go to page 4", JumpPage, 4)
           Text(1, "Close", CloseTalk)
           Talk(4, "Destination page")
           Text(4, "Back", JumpPage, 1)
        end
        function after() AddNpcMission(101) end
    )lua";
    NpcLuaData data;
    assert(readNpcLuaData(lua, "r_shop", data));
    assert(data.firstTalk.find("Goldie:") == 0);
    assert(data.firstTalk.find("ESCONDIDO") == std::string::npos);
    assert((data.missionIds == std::vector<int>{701, 631}));
    assert((data.tradeItems == std::vector<int>{290,466,1001,88}));
    assert(!readNpcLuaData(lua,"r_sho",data));
    assert(!readNpcLuaData("function r_shop() Talk(1,'Fail')", "r_shop", data));
    assert(data.firstTalk.empty() && data.missionIds.empty());
    assert(readNpcLuaData("function x() Talk(1,[=[Hello world]=]) end", "x", data));
    assert(data.firstTalk == "Hello world");
    assert(readNpcLuaData("function x() Talk(1,'Backslash\\nnext') end", "x", data));
    assert(data.firstTalk == "Backslashnext"); // controle removido para UI
    assert(readNpcLuaData(lua, "r_bank", data) && data.bankService);
    assert(readNpcLuaData(lua, "r_fake_bank", data) && !data.bankService);
    assert(readNpcLuaData(lua, "r_tutorial", data));
    assert(data.dialogPages.at(1).text == "Main page");
    assert(data.dialogPages.at(1).options.size() == 2);
    assert(data.dialogPages.at(1).options[0].nextPage == 4);
    assert(data.dialogPages.at(1).options[1].close);

    const std::vector<int> offers{100,204,279,281,404};
    std::vector<int> tried;
    const int chosen=firstAvailableNpcMission(offers,[&](int id){tried.push_back(id);return id==279;});
    assert(chosen==279 && (tried==std::vector<int>{100,204,279}));
    assert(firstAvailableNpcMission(offers,[](int){return false;})==0);
    std::vector<int> many(140,1);many[129]=2;
    int calls=0;
    assert(firstAvailableNpcMission(many,[&](int id){++calls;return id==2;})==0);
    assert(calls==128);

    // Integracao real NpcSystem: tres consultas leem a mesma funcao,
    // cacheadas por instancia, sem depender do Client completo.
    const auto previous = std::filesystem::current_path();
    const auto temp = previous / "ui05aj_fixture";
    std::filesystem::remove_all(temp);
    const auto scriptDir=temp / "Client/scripts/offline/script/MisScript";
    std::filesystem::create_directories(scriptDir);
    {
        std::ofstream f(scriptDir / "NpcScript01.lua",std::ios::binary);
        f << lua;
    }
    std::filesystem::current_path(temp);
    NpcSystem npc;
    std::string text;
    std::vector<int> nums;
    assert(npc.dialog("r_shop",text) && text.find("Goldie:")==0);
    assert(npc.missionIds("r_shop",nums) && (nums==std::vector<int>{701,631}));
    assert(npc.catalog("r_shop",nums) && (nums==std::vector<int>{290,466,1001,88}));
    assert(!npc.dialog("r_shop_extra_missing",text));
    assert(!npc.catalog("r_shop_extra_missing",nums) && nums.empty());
    assert(npc.missionIds("r_shop_extra",nums)==false);
    assert(npc.bankService("r_bank"));
    assert(!npc.bankService("r_fake_bank"));
    NpcLuaData::DialogPage dialogPage;
    assert(npc.dialogPage("r_tutorial", 4, dialogPage));
    assert(dialogPage.text == "Destination page" && dialogPage.options.size() == 1);
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(temp);
    // Senna's actual historical script contains a five-option start page and
    // linked class/attribute/help pages; verify the content copied into Client.
    NpcSystem realScripts;
    std::vector<int> jimberryStock;
    assert(realScripts.catalog("r_talk07", jimberryStock));
    assert(jimberryStock.size() > 32);
    assert(std::find(jimberryStock.begin(), jimberryStock.end(), 3238) != jimberryStock.end());
    assert(realScripts.dialogPage("BT_NewUser001", 1, dialogPage));
    assert(dialogPage.options.size() >= 5);
    assert(realScripts.dialogPage("BT_NewUser001", 12, dialogPage));
    assert(dialogPage.text.find("classes") != std::string::npos || dialogPage.text.find("Classes") != std::string::npos);
    assert(dialogPage.options.size() >= 4);
    assert(realScripts.dialogPage("BT_NewUser001", 31, dialogPage));
    const auto unityOffer = std::find_if(dialogPage.options.begin(), dialogPage.options.end(), [](const auto& option) {
        return option.label.find("Star of Unity") != std::string::npos;
    });
    assert(unityOffer != dialogPage.options.end() && unityOffer->triggerId == 1);
    assert(realScripts.dialogPage("BT_NewUser001", 32, dialogPage));
    assert(std::any_of(dialogPage.options.begin(), dialogPage.options.end(), [](const auto& option) {
        return option.teamStarTransfer;
    }));
    std::cout << "05-AJ: menus e missoes Lua sem falsos positivos; proxima oferta elegivel OK\n";
}
