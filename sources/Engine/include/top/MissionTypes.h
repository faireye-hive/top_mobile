#pragma once
// Contratos de missao independentes dos NPCs, UI e OpenGL.
#include "top/MissionEligibility.h"
#include "top/MissionEvents.h"
#include <string>
#include <vector>
namespace top {
struct HistoricalMission {
    struct ItemAmount { int id = 0; int count = 0; };
    int id = 0;
    std::string title;
    std::string beginText;
    std::string objective;
    int targetId = 0;
    int targetCount = 0;
    float targetX = -1.f;
    float targetY = -1.f;
    int rewardExp = 0;
    int rewardGold = 0;
    int minLevel = 1;
    int maxLevel = 0;
    int recordId = 0;
    // 05-AP: true only for a proven NPC result path (HasMission + ClearMission).
    bool turnInRequired = false;
    // 05-AQ: per-record flags owned by the active mission in historical GameServer.
    // Result flags are enabled only when their source state can be proved.
    std::vector<int> beginFlags, resultHasFlags, resultNoFlags;
    MissionGate gate;
    std::vector<ItemAmount> neededItems;
    std::vector<ItemAmount> rewardItems;
    // 05-AR: items given as part of accepting a historical mission.
    std::vector<ItemAmount> beginItems;
    // 05-AU: triggers validados por assinatura TE_GETITEM/TE_KILL +
    // IsItem/IsMonster + AddNextFlag, independentes dos NPCs.
    std::vector<MissionEventTrigger> eventTriggers;
};

// COMPLETE_SHOW side interaction: never offered as a new mission.
// A specific NPC can change the flags of an existing mission after
// receiving the exact items from the historical Lua script.
struct HistoricalMissionStep {
    int displayId = 0, recordId = 0, absentFlag = 0, resultingFlag = 0;
    std::vector<HistoricalMission::ItemAmount> takeItems;
};

} // namespace top
