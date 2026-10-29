#pragma once
// Fonte de verdade das MISSOES e seus scripts Lua historicos. Sem NPCs,
// entidades 3D, UI, ou plataforma grafica. Nao executa scripts arbitrarios.
#include "top/MissionTypes.h"
#include <string>
#include <unordered_map>
#include <vector>
namespace top {
class MissionCatalog {
public:
    void reset();
    bool mission(int recordId, HistoricalMission& out) const;
    bool missionByDisplay(int displayId, HistoricalMission& out) const;
    std::vector<int> customMissionIds() const;
    bool missionStep(int displayId, HistoricalMissionStep& out) const;
    std::unordered_map<int,std::vector<int>> missionRecordIndex() const;
private:
    // 05-AK.1: indice de deslocamentos dos blocos DefineMission em MissionScript*.lua.
    // Nao executar Lua e nao reler megabytes para cada icone ! no NPC.
    void ensureMissionIndex() const;
    void ensureCustomMissionIndex() const;
    struct MissionLocation { std::string path; long offset = -1; int recordId = 0; };
    mutable std::unordered_map<int, std::vector<int>> displayByRecord_;
    mutable bool missionIndexReady_ = false;
    mutable std::unordered_map<int, MissionLocation> missionIndex_;
    mutable std::unordered_map<int,int> recordByDisplay_;
    mutable std::unordered_map<int, MissionLocation> stepIndex_;
    mutable std::unordered_map<int, std::vector<int>> stepsByRecord_;
    mutable std::unordered_map<int, HistoricalMissionStep> stepCache_;
    mutable std::unordered_map<int, HistoricalMission> missionCache_;
    mutable bool customMissionIndexReady_ = false;
    mutable std::unordered_map<int, HistoricalMission> customMissions_;
};
} // namespace top
