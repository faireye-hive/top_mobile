#pragma once

// NPCs offline lidos dos <mapa>npc.txt do GameServer antigo. Os scripts
// associados sao preservados como identificadores, mas nunca executados.

#include "top/NpcLuaData.h"
#include "top/MissionCatalog.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace top {

struct Npc {
    int serial = -1;
    std::string name;
    int characterId = 0;
    int service = 0;
    float x = 0.f;
    float y = 0.f;
    float yaw = 0.f;
    std::string region;
    std::string script;
};


class NpcSystem {
public:
    bool load(const std::string& mapName);
    bool closest(float x, float y, int service, Npc& out) const;
    std::vector<Npc> visible(float x, float y, float radius) const;
    // Extrai Weapon/Defence/Other/etc. do bloco InitTrade do script indicado.
    // E um parser de dados, nao um interpretador Lua.
    bool catalog(const std::string& script, std::vector<int>& itemIds) const;
    bool exchangeRules(const std::string& script, std::vector<NpcLuaData::ExchangeRule>& rules) const;
    // Detecta somente a acao TriggerAction(..., OpenBank) da funcao historica.
    bool bankService(const std::string& script) const;
    // Primeira fala Talk(...) do NPC, usada pelo dialogo offline inicial.
    bool dialog(const std::string& script, std::string& text) const;
    bool dialogPage(const std::string& script, int page, NpcLuaData::DialogPage& out) const;
    // 05-AD: leitura passiva das acoes Text(... OpenForge/Unite/Milling)
    // declaradas na funcao Lua original deste NPC.
    unsigned forgeServices(const std::string& script) const;
    bool missionIds(const std::string& script, std::vector<int>& ids) const;
    bool mission(int id, HistoricalMission& mission) const;
    const MissionCatalog& missions() const { return missionCatalog_; }
    // Adaptadores legados: novos usuarios podem acessar missions() diretamente.
    // Resolve AddNpcMission(displayId) to the AddMission(recordId) progress.
    bool missionByDisplay(int displayId, HistoricalMission& mission) const;
    bool missionStep(int displayId, HistoricalMissionStep& step) const;
    std::unordered_map<int,std::vector<int>> missionRecordIndex() const;
    static bool selfTest();
    const std::vector<Npc>& npcs() const { return npcs_; }

private:
    static bool parse(const std::string& line, Npc& out);
    bool readScriptData(const std::string& script, NpcLuaData& out) const;
    MissionCatalog missionCatalog_; // parsing/caching Lua de missoes isolado
    mutable bool scriptIndexReady_ = false;
    mutable std::unordered_map<std::string, NpcLuaData> scriptDataCache_;
    std::vector<Npc> npcs_;
};

}  // namespace top
