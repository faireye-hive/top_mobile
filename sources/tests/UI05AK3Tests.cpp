// 05-AK.3: o indice tokeniza cada NpcScript uma vez e preserva o comportamento
// do leitor Lua passivo (incluindo erros de sintaxe e funcoes aninhadas).
#include "top/NpcSystem.h"
#include "top/NpcLuaData.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

static bool same(const top::NpcLuaData& a, const top::NpcLuaData& b) {
    return a.firstTalk == b.firstTalk && a.tradeItems == b.tradeItems && a.missionIds == b.missionIds;
}
int main() {
    namespace fs=std::filesystem;
    const std::string code = R"LUA(
-- function fake() Talk(1, "nao contar") end
function r_first()
    Talk(1, "Ola ao viajante")
    if true then AddNpcMission(700) end
    function nested()
        AddNpcMission(9999)
        InitTrade()
        Weapon(9999)
    end
    InitTrade()
    Weapon(0290)
    Defence(0466)
end
function r_second()
    Talk(1, "Outra fala")
    AddNpcMission(
        702
    )
    Other(888) -- nao existe InitTrade nesta funcao
end
function r_first() Talk(1, "Nao sobrescrever") end
function incomplete()
    AddNpcMission(777)
)LUA";
    std::unordered_map<std::string, top::NpcLuaData> index;
    top::readNpcLuaDataIndex(code,index);
    for(const std::string name : {"r_first", "r_second"}) {
        top::NpcLuaData old;
        assert(top::readNpcLuaData(code,name,old));
        assert(index.count(name)==1 && same(old,index.at(name)));
    }
    assert(index.at("r_first").missionIds == std::vector<int>{700});
    assert(index.at("r_first").tradeItems == (std::vector<int>{290,466}));
    assert(index.at("r_second").tradeItems.empty());
    assert(index.count("fake") == 0 && index.count("nested") == 0 && index.count("incomplete") == 0);
    const std::string prefix = "top_ui05ak3_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path base=fs::temp_directory_path()/prefix;
    const fs::path initial=fs::current_path();
    fs::create_directories(base / "Client/scripts/offline/maps/garner");
    fs::create_directories(base / "Client/scripts/offline/script/MisScript");
    {
        std::ofstream npc(base/"Client/scripts/offline/maps/garner/garnernpc.txt");
        npc << "2\tBlacksmith - Goldie\t1\t20\t1\t219312,270612\t219312,270612\t114\tArgent City\t1\t0\tr_first\t0\n";
    }
    const fs::path script=base/"Client/scripts/offline/script/MisScript/NpcScript01.lua";
    {std::ofstream f(script);f<<code;}
    fs::current_path(base);
    top::NpcSystem npcs;
    assert(npcs.load("garner"));
    std::vector<int> ids, items;
    std::string talk;
    assert(npcs.missionIds("r_first", ids) && ids == std::vector<int>{700});
    assert(npcs.catalog("r_first",items) && items == (std::vector<int>{290,466}));
    assert(npcs.dialog("r_second",talk) && talk == "Outra fala");
    assert(!npcs.missionIds("nested",ids));
    assert(!npcs.dialog("incomplete",talk));
    // Dados cacheados devem ser invalidados ao recarregar.
    {std::ofstream f(script,std::ios::trunc);f<<"function r_first()\n Talk(1, \"Nova frase\")\n AddNpcMission(900)\n end\n";}
    assert(npcs.load("garner"));
    assert(npcs.missionIds("r_first",ids) && ids == std::vector<int>{900});
    assert(npcs.dialog("r_first",talk) && talk == "Nova frase");
    assert(!npcs.missionIds("r_second",ids));
    fs::current_path(initial);
    fs::remove_all(base);
    return 0;
}
