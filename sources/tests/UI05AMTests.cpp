// 05-AM: parser passive de requisitos historicos sem executar scripts Lua.
#include "top/NpcSystem.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace top;
int main() {
    const auto initial=std::filesystem::current_path();
    // Fora da arvore do projeto: ../../Client nao deve misturar scripts reais
    // com a fixture, que tem IDs e regras artificiais.
    const auto fixture=std::filesystem::temp_directory_path()/"top05am_rules_fixture";
    std::filesystem::remove_all(fixture);
    auto dir=fixture/"Client/scripts/offline/script/MisScript";
    std::filesystem::create_directories(dir);
    {
        std::ofstream file(dir/"MissionScript01.lua");
        file << R"lua(
function Tests()
    DefineMission(700, "First quest", 900)
    MisBeginTalk("Complete the first quest")
    MisBeginCondition(NoMission, 900)
    MisBeginCondition(NoRecord, 900)
    MisBeginAction(AddMission, 999)
    MisNeed(MIS_NEED_KILL, 127, 2, 10, 2)
    DefineMission(701, "Next quest", 901)
    MisBeginTalk("Must finish first")
    MisBeginCondition(HasRecord, 999)
    MisBeginCondition(NoMission, 901)
    MisBeginCondition(LvCheck, ">", 10)
    MisBeginCondition(LvCheck, "<", 35)
    MisBeginCondition(PfEqual, 4)
    MisBeginAction(AddMission, 901)
    MisNeed(MIS_NEED_ITEM, 88, 1, 1, 1)
    DefineMission(702, "Unsupported quest", 902)
    MisBeginTalk("Unknown profession logic")
    MisBeginCondition(CheckConvertProfession, MIS_RISKER)
    MisBeginAction(AddMission, 902)
    MisNeed(MIS_NEED_KILL, 127, 1)
    DefineMission(703, "Quest with item", 903)
    MisBeginTalk("Bring item")
    MisBeginCondition(HasItem, 77, 3)
    MisBeginCondition(NoPfEqual, 3)
    MisBeginAction(AddMission, 903)
    MisNeed(MIS_NEED_KILL, 127, 1)
    DefineMission(704, "Hidden", 904, COMPLETE_SHOW)
    MisBeginCondition(AlwaysFailure)
end
)lua";
    }
    std::filesystem::current_path(fixture);
    NpcSystem npc;
    HistoricalMission m;
    assert(npc.mission(900,m) && m.title=="First quest" && m.recordId==999);
    assert(m.gate.needsNotActive==std::vector<int>{900});
    assert(m.gate.needsNotCompleted==std::vector<int>{900});
    assert(npc.mission(901,m) && m.title=="Next quest" && m.minLevel==11 && m.maxLevel==34);
    assert(m.gate.needsCompleted==std::vector<int>{999} && m.gate.jobEqual==4);
    assert(m.gate.needsNotActive==std::vector<int>{901});
    const auto index=npc.missionRecordIndex();
    assert(index.at(999)==std::vector<int>{900} && index.at(901)==std::vector<int>{901});
    assert(npc.mission(902,m) && m.gate.unsupported.find("CheckConvertProfession")!=std::string::npos);
    assert(npc.mission(903,m) && m.gate.jobNotEqual==3 && m.gate.needsItems.size()==1 &&
           m.gate.needsItems[0].id==77 && m.gate.needsItems[0].count==3);
    assert(!npc.mission(904,m));
    std::filesystem::current_path(initial);
    std::filesystem::remove_all(fixture);
    std::cout << "05-AM: dependencias Lua, oficio, faixa de nivel e falha fechada OK\n";
}
