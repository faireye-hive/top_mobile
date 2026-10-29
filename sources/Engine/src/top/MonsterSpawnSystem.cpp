#include "top/MonsterSpawnSystem.h"
#include "top/HistoricalPaths.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <regex>
#include <set>
#include <cctype>

namespace top {
namespace {

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> result;
    std::size_t at = 0;
    while (at <= text.size()) {
        const std::size_t end = text.find(separator, at);
        result.push_back(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
        if (end == std::string::npos) break;
        at = end + 1;
    }
    return result;
}

bool point(const std::string& text, float& x, float& y) {
    int cx = 0;
    int cy = 0;
    if (std::sscanf(text.c_str(), "%d,%d", &cx, &cy) != 2) return false;
    x = static_cast<float>(cx) / 100.f;
    y = static_cast<float>(cy) / 100.f;
    return true;
}

float clamp(float value, float low, float high) { return std::max(low, std::min(value, high)); }

std::string trim(std::string s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(first, end - first + 1);
}

bool safeToken(const std::string& s) {
    if (s.empty() || s.size() > 48) return false;
    for (unsigned char ch : s)
        if (!std::isalnum(ch) && ch != '_') return false;
    return true;
}

FILE* openRelative(const std::string& relative) {
    for (const char* prefix : {"", "../", "../../", "../../../"}) {
        FILE* file = std::fopen((std::string(prefix) + relative).c_str(), "rb");
        if (file) return file;
    }
    return nullptr;
}

std::string stripLuaComment(const char* line) {
    std::string s(line);
    // Os scripts CreateCha/mlist originais nao usam '--' em strings.
    const std::size_t comment = s.find("--");
    if (comment != std::string::npos) s.resize(comment);
    return trim(std::move(s));
}

// Somente declaracoes literais; nao executa Lua, dofile, calculos ou expressoes.
// Aceita 'local cha = CreateCha(...)' e 'Nome = CreateCha(...)'.
bool literalCreateCha(const std::string& line, ScriptMonsterSpawn& out) {
    const std::size_t pos = line.find("CreateCha(");
    if (pos == std::string::npos) return false;
    const std::string prefix = trim(line.substr(0, pos));
    static const std::regex assignment(R"(^(local\s+)?[a-zA-Z_][a-zA-Z_0-9]*\s*=\s*$)");
    if (!prefix.empty() && !std::regex_match(prefix, assignment)) return false;
    const std::string tail = line.substr(pos);
    int id = 0, cmX = 0, cmY = 0, facing = 0, respawn = 0, n = 0;
    if (std::sscanf(tail.c_str(), "CreateCha( %d , %d , %d , %d , %d ) %n",
                    &id, &cmX, &cmY, &facing, &respawn, &n) != 5 || n == 0) return false;
    if (!trim(tail.substr(static_cast<std::size_t>(n))).empty()) return false;
    if (id < 1 || id > 100000 || cmX < 0 || cmY < 0 ||
        cmX > 10000000 || cmY > 10000000 || facing < -360 || facing > 360 ||
        respawn < 0 || respawn > 864000) return false;
    out.id = id;
    out.x = static_cast<float>(cmX) * .01f;
    out.y = static_cast<float>(cmY) * .01f;
    out.facing = facing;
    out.respawnSeconds = respawn;
    return true;
}

std::vector<std::string> selectedLuaFiles(const std::string& mapName) {
    std::vector<std::string> names;
    FILE* file = openRelative("Client/scripts/offline/script/monster/mlist.lua");
    if (!file) return names;
    char raw[4096];
    std::string currentMap;
    int depth = 0;
    std::set<std::string> seen;
    static const std::regex mapCall(R"(^(local\s+)?m\s*=\s*SetCurMap\s*\(\s*['\"]([a-zA-Z0-9_]+)['\"]\s*\)\s*$)");
    static const std::regex includeCall(R"(^PM\s*\(\s*['\"]([a-zA-Z0-9_]+)['\"]\s*\)\s*$)");
    std::smatch match;
    int lines = 0;
    while (std::fgets(raw, sizeof raw, file) && ++lines < 4000) {
        const std::string line = stripLuaComment(raw);
        if (line.empty()) continue;
        if (std::regex_match(line, match, mapCall)) {
            currentMap = match[2].str();
            depth = 0;
        } else if (line == "if m==1 then" || line == "if m == 1 then") {
            depth = 1;
        } else if (line.compare(0, 3, "if ") == 0 || line.compare(0, 3, "if(") == 0) {
            if (depth > 0) ++depth;
        } else if (line == "end") {
            if (depth > 0) --depth;
        } else if (depth == 1 && historical_paths_detail::lowerAscii(currentMap) ==
            historical_paths_detail::lowerAscii(mapName) && std::regex_match(line, match, includeCall)) {
            const std::string name = match[1].str();
            if (safeToken(name) && seen.insert(name).second && names.size() < 96) names.push_back(name);
        }
    }
    std::fclose(file);
    return names;
}

std::size_t loadScriptSpawns(const std::string& mapName, std::vector<ScriptMonsterSpawn>& output) {
    std::size_t opened = 0;
    std::set<std::tuple<int, int, int>> already;
    for (const std::string& name : selectedLuaFiles(mapName)) {
        FILE* file = openRelative("Client/scripts/offline/script/monster/" + name + ".lua");
        if (!file) continue;
        ++opened;
        char raw[4096];
        int lines = 0;
        while (std::fgets(raw, sizeof raw, file) && ++lines <= 30000 && output.size() < 30000) {
            ScriptMonsterSpawn spawn;
            if (!literalCreateCha(stripLuaComment(raw), spawn)) continue;
            // Chaves em centimetros, sem perder a precisao das coordenadas.
            const auto key = std::make_tuple(spawn.id, static_cast<int>(std::lround(spawn.x * 100.f)),
                                             static_cast<int>(std::lround(spawn.y * 100.f)));
            if (already.insert(key).second) {
                spawn.script = name;
                output.push_back(std::move(spawn));
            }
        }
        std::fclose(file);
    }
    return opened;
}


}  // namespace

bool MonsterSpawnSystem::parse(const std::string& line, MonsterSpawnRegion& out) {
    if (line.empty() || line.rfind("//", 0) == 0) return false;
    const std::vector<std::string> cols = split(line, '\t');
    if (cols.size() < 6) return false;
    out = MonsterSpawnRegion{};
    out.serial = std::atoi(cols[0].c_str());
    if (out.serial <= 0 || !point(cols[1], out.x0, out.y0) || !point(cols[2], out.x1, out.y1)) return false;
    if (out.x0 > out.x1) std::swap(out.x0, out.x1);
    if (out.y0 > out.y1) std::swap(out.y0, out.y1);
    for (const std::string& id : split(cols[4], ',')) {
        const int value = std::atoi(id.c_str());
        if (value > 0) out.ids.push_back(value);
    }
    for (const std::string& count : split(cols[5], ',')) out.counts.push_back(std::max(0, std::atoi(count.c_str())));
    if (cols.size() > 10) out.name = cols[10];
    while (!out.name.empty() && (out.name.back() == '\r' || out.name.back() == '\n')) out.name.pop_back();
    return !out.ids.empty();
}

bool MonsterSpawnSystem::load(const std::string& mapName) {
    regions_.clear();
    scripted_.clear();
    scriptFiles_ = 0;
    if (!safeToken(mapName)) return false;
    scriptFiles_ = loadScriptSpawns(mapName, scripted_);
    // Os scripts antigos podem usar DreamIsland/PKmap na pasta fisica,
    // enquanto o identificador interno do mapa e lowercase.
    if (mapName.empty() || mapName.size() > 48 ||
        !std::all_of(mapName.begin(), mapName.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '_';
        })) return false;
    FILE* file = nullptr;
    // Mantem a preferencia do layout maps/, seguida do layout antigo.
    for (const std::string& layout : {"scripts/offline/maps/", "scripts/offline/"}) {
        if (file) break;
        for (const char* root : {"Client", "../Client", "../../Client"}) {
            const std::string relative = layout + mapName + "/" + mapName + "ChaSpn.txt";
            const std::string actual = historicalFilePath(root, relative);
            if (!actual.empty() && (file = std::fopen(actual.c_str(), "rb"))) break;
        }
    }
    if (!file) return !scripted_.empty();
    char raw[8192];
    while (std::fgets(raw, sizeof raw, file)) {
        MonsterSpawnRegion region;
        if (parse(raw, region)) regions_.push_back(std::move(region));
    }
    std::fclose(file);
    return !regions_.empty() || !scripted_.empty();
}

bool MonsterSpawnSystem::closestRegion(float x, float y, MonsterSpawnRegion& out, float& distance) const {
    const MonsterSpawnRegion* closest = nullptr;
    float best = std::numeric_limits<float>::infinity();
    for (const MonsterSpawnRegion& region : regions_) {
        const float dx = clamp(x, region.x0, region.x1) - x;
        const float dy = clamp(y, region.y0, region.y1) - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; closest = &region; }
    }
    for (const ScriptMonsterSpawn& spawn : scripted_) {
        const float dx = spawn.x - x, dy = spawn.y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= best) {
            best = d2;
            closest = nullptr;
            out = MonsterSpawnRegion{};
            out.x0 = out.x1 = spawn.x;
            out.y0 = out.y1 = spawn.y;
            out.name = "Lua CreateCha [" + spawn.script + "]";
        }
    }
    if (!std::isfinite(best)) { distance = 0.f; return false; }
    if (!closest) { distance = std::sqrt(best); return true; }
    out = *closest;
    distance = std::sqrt(best);
    return true;
}

std::vector<MonsterSlot> MonsterSpawnSystem::nearest(float x, float y, int limit, float radius) const {
    // Spawn historico por mapa: nunca adicionar um monstro de outro mapa como fallback.
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || limit <= 0) return {};
    struct Candidate {
        const MonsterSpawnRegion* region;
        std::size_t kind;
        int copy;
        float x;
        float y;
        float distance2;
    };
    std::vector<Candidate> candidates;
    const float reach = std::max(8.f, radius);
    const float reach2 = reach * reach;
    // Posicao fica estavel por celula de 8 m, evitando que toda a populacao
    // deslize junto com o jogador a cada atualizacao de streaming.
    const float anchorX = std::floor(x / 8.f) * 8.f + 4.f;
    const float anchorY = std::floor(y / 8.f) * 8.f + 4.f;
    for (const MonsterSpawnRegion& region : regions_) {
        const float dx = clamp(x, region.x0, region.x1) - x;
        const float dy = clamp(y, region.y0, region.y1) - y;
        if (dx * dx + dy * dy > reach2) continue;
        for (std::size_t kind = 0; kind < region.ids.size(); ++kind) {
            const int historicalCount = kind < region.counts.size() ? region.counts[kind] : 0;
            // Quantidade zero significa spawn desativado, nao um monstro gratis.
            if (historicalCount <= 0) continue;
            const int copies = std::min(3, historicalCount);
            for (int copy = 0; copy < copies; ++copy) {
                // Distribuicao deterministica dentro da regiao. O jogador nao
                // altera as coordenadas continuamente, apenas a janela espacial.
                const unsigned seed = static_cast<unsigned>(region.serial) * 1103515245u +
                                      static_cast<unsigned>(kind) * 12345u +
                                      static_cast<unsigned>(copy) * 2654435761u;
                const float jitterX = (static_cast<float>(seed % 17u) - 8.f) * 1.5f;
                const float jitterY = (static_cast<float>((seed >> 8) % 17u) - 8.f) * 1.5f;
                float sx = clamp(anchorX + jitterX, region.x0, region.x1);
                float sy = clamp(anchorY + jitterY, region.y0, region.y1);
                // Se a janela comecar fora da regiao, clamp simples empilha
                // muitos monstros no mesmo pixel da borda (eastgoaf 13,10).
                // Espalhar deterministicamente pela faixa interna mais proxima.
                if (anchorX <= region.x0) sx = clamp(region.x0 + .5f +
                    static_cast<float>((seed >> 3) % 31u) * .4f, region.x0, region.x1);
                else if (anchorX >= region.x1) sx = clamp(region.x1 - .5f -
                    static_cast<float>((seed >> 3) % 31u) * .4f, region.x0, region.x1);
                if (anchorY <= region.y0) sy = clamp(region.y0 + .5f +
                    static_cast<float>((seed >> 11) % 31u) * .4f, region.y0, region.y1);
                else if (anchorY >= region.y1) sy = clamp(region.y1 - .5f -
                    static_cast<float>((seed >> 11) % 31u) * .4f, region.y0, region.y1);
                const float ox = sx - x;
                const float oy = sy - y;
                const float d2 = ox * ox + oy * oy;
                if (d2 <= reach2)
                    candidates.push_back({&region, kind, copy, sx, sy, d2});
            }
        }
    }
    // A 05-Q listava primeiro todos os exemplares do primeiro tipo na
    // primeira regiao; isso escondia a variedade do bestiario. Distribuir
    // primeiro uma copia de cada (regiao,tipo) antes dos duplicados.
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.copy != b.copy) return a.copy < b.copy;
        if (a.distance2 != b.distance2) return a.distance2 < b.distance2;
        if (a.region->serial != b.region->serial) return a.region->serial < b.region->serial;
        return a.kind < b.kind;
    });
    std::vector<MonsterSlot> result;
    const std::size_t maxSize = static_cast<std::size_t>(std::min(limit, 64));
    // Primeiro os pontos CreateCha exatos (Lua). Eles nao seguem a posicao
    // do jogador, nao sofrem clamp e nao executam scripts em tempo real.
    struct NearLua { const ScriptMonsterSpawn* spawn; float dist2; };
    std::vector<NearLua> nearLua;
    for (const ScriptMonsterSpawn& spawn : scripted_) {
        const float dx = spawn.x - x, dy = spawn.y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= reach2) nearLua.push_back({&spawn, d2});
    }
    std::sort(nearLua.begin(), nearLua.end(), [](const NearLua& a, const NearLua& b) {
        if (a.dist2 != b.dist2) return a.dist2 < b.dist2;
        if (a.spawn->id != b.spawn->id) return a.spawn->id < b.spawn->id;
        if (a.spawn->x != b.spawn->x) return a.spawn->x < b.spawn->x;
        return a.spawn->y < b.spawn->y;
    });
    std::set<int> emittedIds;
    auto pushLua = [&](const NearLua& near) {
        if (result.size() >= maxSize) return;
        const std::uint64_t index = static_cast<std::uint64_t>(near.spawn - scripted_.data()) + 1u;
        result.push_back({near.spawn->id, near.spawn->x, near.spawn->y, false,
                          static_cast<float>(near.spawn->facing),
                          0x1000000000000000ULL | index, near.spawn->respawnSeconds});
    };
    // Uma especie de cada antes dos duplicados, quando houver variedade.
    for (const NearLua& near : nearLua)
        if (emittedIds.insert(near.spawn->id).second) pushLua(near);
    for (const NearLua& near : nearLua) {
        if (result.size() >= maxSize) break;
        bool used = false;
        for (const MonsterSlot& existing : result)
            if (existing.id == near.spawn->id && std::fabs(existing.x - near.spawn->x) < .01f &&
                std::fabs(existing.y - near.spawn->y) < .01f) { used = true; break; }
        if (!used) pushLua(near);
    }
    for (const Candidate& candidate : candidates) {
        if (result.size() >= maxSize) break;
        const int id = candidate.region->ids[candidate.kind];
        bool duplicate = false;
        for (const MonsterSlot& existing : result) {
            if (existing.id == id && std::hypot(existing.x - candidate.x, existing.y - candidate.y) < 2.f) {
                duplicate = true; break;
            }
        }
        if (!duplicate) {
            const std::uint64_t key = 0x2000000000000000ULL |
                (static_cast<std::uint64_t>(static_cast<unsigned>(candidate.region->serial)) << 32) |
                (static_cast<std::uint64_t>(candidate.kind & 0xffffu) << 16) |
                static_cast<std::uint64_t>(candidate.copy + 1);
            result.push_back({id, candidate.x, candidate.y, false, 0.f, key, 0});
        }
    }
    return result;
}

bool MonsterSpawnSystem::selfTest() {
    MonsterSpawnRegion region;
    if (!parse("9\t201900,267500\t216200,278600\t-1\t75,185\t14,14\t100,100\t10,10\t40\t0\tArgent 3", region)) return false;
    return region.serial == 9 && region.ids.size() == 2 && region.ids[0] == 75 && region.counts[1] == 14 &&
           std::fabs(region.x0 - 2019.f) < .01f && region.name == "Argent 3";
}

}  // namespace top
