#include "top/NpcSystem.h"
#include <cstdio>
int main() {
    top::NpcSystem npcs;
    top::HistoricalMission leaves;
    // This only runs with archived scripts mounted under Client; unit tests
    // never invent a mission or run arbitrary Lua.
    if(!npcs.mission(721,leaves)) {
        std::fprintf(stderr,"05-AP: historic MissionScript01.lua unavailable (fixture required)\n");
        return 1;
    }
    if(leaves.id!=721 || leaves.recordId!=721 || !leaves.turnInRequired ||
       !leaves.gate.unsupported.empty() || leaves.rewardGold!=100 ||
       leaves.rewardExp!=40 || leaves.neededItems.size()!=2 ||
       leaves.neededItems[0].id!=1573 || leaves.neededItems[0].count!=10 ||
       leaves.neededItems[1].id!=1574 || leaves.neededItems[1].count!=3) {
        std::fprintf(stderr,"05-AP: historic Leaves Collection parsing mismatch\n");return 1;
    }
    // 705 uses unimplemented flags and multi-kill requirements; it cannot
    // be exposed as a claimable quest until those mechanics are implemented.
    top::HistoricalMission flags;
    if(npcs.mission(705,flags) && flags.gate.unsupported.empty()) {
        std::fprintf(stderr,"05-AP: complex historic result was offered unsafely\n");return 1;
    }
    top::HistoricalMission another;
    if(!npcs.mission(726,another) || !another.turnInRequired ||
       !another.gate.unsupported.empty() || another.rewardGold!=200 ||
       another.rewardExp!=70 || another.neededItems.size()!=3) {
        std::fprintf(stderr,"05-AP: Decoction Recipe hand-in was not recovered\n");return 1;
    }
    return 0;
}
