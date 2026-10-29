#pragma once

#include "top/ActorSkin.h"

#include <string>
#include <vector>

namespace top {

struct MonsterSpawnRegion {
    int serial = 0;
    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    std::vector<int> ids;
    std::vector<int> counts;
    std::string name;
};

// 05-S: registros literais CreateCha dos scripts aprovados em monster/mlist.lua.
// Nao executa Lua nem interpreta chamadas dinamicas.
struct ScriptMonsterSpawn {
    int id = 0;
    float x = 0.f;
    float y = 0.f;
    int facing = 0;          // graus historicos
    int respawnSeconds = 0;
    std::string script;
};

class MonsterSpawnSystem {
public:
    bool load(const std::string& mapName);
    // So regioes cuja borda esta a ate radius metros. O monstro fica na regiao,
    // nao e trazido ate o pe do jogador.
    std::vector<MonsterSlot> nearest(float x, float y, int limit = 12, float radius = 72.f) const;
    const std::vector<MonsterSpawnRegion>& regions() const { return regions_; }
    const std::vector<ScriptMonsterSpawn>& scripted() const { return scripted_; }
    std::size_t scriptFileCount() const { return scriptFiles_; }
    // Distancia da pessoa ate a borda da regiao historica mais proxima.
    bool closestRegion(float x, float y, MonsterSpawnRegion& out, float& distance) const;
    static bool selfTest();

private:
    static bool parse(const std::string& line, MonsterSpawnRegion& out);
    std::vector<MonsterSpawnRegion> regions_;
    std::vector<ScriptMonsterSpawn> scripted_;
    std::size_t scriptFiles_ = 0;
};

}  // namespace top
