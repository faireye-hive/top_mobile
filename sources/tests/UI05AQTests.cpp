#include "top/NpcSystem.h"
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <chrono>

int main() {
    top::NpcSystem npcs;
    top::HistoricalMission a;
    // Original archived scripts: real record IDs, begin SetFlag(721,1),
    // item triggers. The flag logic must not mark those triggers as completed.
    if(!npcs.mission(721,a) || !a.turnInRequired || a.beginFlags.size()!=1 ||
       a.beginFlags[0]!=1 || !a.resultHasFlags.empty() ||
       !a.gate.unsupported.empty()) {
        std::fprintf(stderr,"05-AQ: begin-flag or Leaves Collection regression\n");return 1;
    }
    // 05-AR: 701 now has a proven intermediate NPC step at Goldie (703).
    top::HistoricalMission b;
    if(!npcs.mission(701,b) || !b.gate.unsupported.empty() ||
       !b.turnInRequired || b.beginFlags.empty() || b.beginFlags[0]!=1 ||
       b.beginItems.size()!=1 || b.beginItems[0].id!=3950 ||
       b.resultHasFlags!=std::vector<int>{10}) {
        std::fprintf(stderr,"05-AR: 701 original side-event recovery regressed\n");return 1;
    }
    top::HistoricalMission c;
    if(!npcs.mission(726,c) || !c.turnInRequired ||
       c.gate.unsupported.size() || c.beginFlags.size()!=1 || c.beginFlags[0]!=1) {
        std::fprintf(stderr,"05-AQ: Decoction Recipe or begin flag regression\n");return 1;
    }
    top::NpcSystem argent;
    if(!argent.load("garner"))return 2;
    int testNpcCount=0;
    for(const auto& npc:argent.npcs())if(npc.script.rfind("TOP_TEST_",0)==0 || npc.script=="TOP_CUSTOM_MISSIONS")++testNpcCount;
    if(testNpcCount!=6)return 3;
    for(const char* script:{"TOP_TEST_APPAREL","TOP_TEST_WEAPONS","TOP_TEST_CHESTS","TOP_TEST_GENERAL"}) {
        std::vector<int> stock;
        if(!argent.catalog(script,stock) || stock.empty())return 4;
    }
    namespace fs=std::filesystem;
    const auto previous=fs::current_path();
    const fs::path fixture=fs::temp_directory_path() /
        ("top_05aq_flag_fixture_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(fixture / "Client/scripts/offline/script/MisScript");
    fs::create_directories(fixture / "Client/scripts/offline");
    {
        std::ofstream lua(fixture / "Client/scripts/offline/script/MisScript/MissionScript01.lua");
        lua << "DefineMission(902, \"Safe result flag\", 880)\n"
               "MisBeginTalk(\"Item delivery\")\n"
               "MisBeginAction(AddMission,880)\n"
               "MisBeginAction(SetFlag,880,1)\n"
               "MisNeed(MIS_NEED_ITEM,5001,1)\n"
               "MisResultCondition(HasMission,880)\n"
               "MisResultCondition(HasFlag,880,1)\n"
               "MisResultCondition(NoFlag,880,10)\n"
               "MisResultCondition(HasItem,5001,1)\n"
               "MisResultAction(TakeItem,5001,1)\n"
               "MisResultAction(SetFlag,880,10)\n"
               "MisResultAction(ClearMission,880)\n"
               "MisResultAction(SetRecord,880)\n"
               "MisResultAction(AddExp,11,11)\n"
               "DefineMission(903, \"Unknown event flag\", 881)\n"
               "MisBeginTalk(\"Item delivery\")\n"
               "MisBeginAction(AddMission,881)\n"
               "MisBeginAction(SetFlag,881,1)\n"
               "MisBeginAction(AddTrigger,8811,TE_GETITEM,5001,1)\n"
               "MisNeed(MIS_NEED_ITEM,5001,1)\n"
               "MisResultCondition(HasMission,881)\n"
               "MisResultCondition(HasFlag,881,1)\n"
               "MisResultAction(TakeItem,5001,1)\n"
               "MisResultAction(ClearMission,881)\n";
    }
    {
        std::ofstream custom(fixture / "Client/scripts/offline/custom-missions.tsv");
        custom << "900001\tTest collection\tCollect three apples for the board.\t1\titem:8:3\t50\t100\t15:1\n"
                  "900002\tTest hunt\tDefeat two creatures and deliver a hide.\t5\tkill:268:2;item:4474:1\t75\t125\t\n"
                  "900003\tBad objective\tThis row must be rejected.\t1\tteleport:1:1\t0\t0\t\n";
    }
    fs::current_path(fixture);
    top::NpcSystem synthetic;
    top::HistoricalMission safe,unsafe;
    const bool good=synthetic.mission(880,safe) && safe.turnInRequired &&
        safe.gate.unsupported.empty() && safe.beginFlags==std::vector<int>{1} &&
        safe.resultHasFlags==std::vector<int>{1} &&
        safe.resultNoFlags==std::vector<int>{10} && safe.rewardExp==11;
    const bool blocked=synthetic.mission(881,unsafe) && !unsafe.turnInRequired &&
        !unsafe.gate.unsupported.empty();
    top::HistoricalMission authored,authoredHunt;
    std::vector<int> authoredIds;
    const bool customMissions=synthetic.missionByDisplay(900001,authored) &&
        synthetic.mission(900002,authoredHunt) &&
        synthetic.missionIds("TOP_CUSTOM_MISSIONS",authoredIds) &&
        authored.title=="Test collection" && authored.neededItems.size()==1 &&
        authored.neededItems[0].id==8 && authored.rewardItems.size()==1 &&
        authored.rewardItems[0].id==15 && authoredHunt.eventTriggers.size()==2 &&
        authoredIds==std::vector<int>({900001,900002});
    fs::current_path(previous);
    fs::remove_all(fixture);
    if(!good || !blocked || !customMissions) {
        std::fprintf(stderr,"05-AQ: synthetic result conditions not safely recognized\n");return 1;
    }
    return 0;
}
