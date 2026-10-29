#include "top/NpcSystem.h"
#include "top/HistoricalPaths.h"
#include "top/NpcForgeServices.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstring>

namespace top {
namespace {

std::vector<std::string> tabs(const std::string& line) {
    std::vector<std::string> cols;
    std::size_t at = 0;
    while (at <= line.size()) {
        const std::size_t end = line.find('\t', at);
        if (end == std::string::npos) {
            cols.push_back(line.substr(at));
            break;
        }
        cols.push_back(line.substr(at, end - at));
        at = end + 1;
    }
    return cols;
}

bool point(const std::string& text, float& x, float& y) {
    int cmX = 0;
    int cmY = 0;
    if (std::sscanf(text.c_str(), "%d,%d", &cmX, &cmY) != 2) return false;
    x = static_cast<float>(cmX) / 100.f;
    y = static_cast<float>(cmY) / 100.f;
    return true;
}

bool testVendorScript(const std::string& script) {
    return script.rfind("TOP_TEST_",0)==0;
}

std::vector<int> testVendorStock(const std::string& script) {
    enum class Shelf { Apparel, Weapons, Chests, General };
    Shelf shelf;
    if(script=="TOP_TEST_APPAREL")shelf=Shelf::Apparel;
    else if(script=="TOP_TEST_WEAPONS")shelf=Shelf::Weapons;
    else if(script=="TOP_TEST_CHESTS")shelf=Shelf::Chests;
    else if(script=="TOP_TEST_GENERAL")shelf=Shelf::General;
    else return {};

    // As quatro prateleiras usam o mesmo iteminfo. Uma leitura preenche todas;
    // a classificação de cada linha permanece a das varreduras separadas.
    struct Shelves {
        std::vector<int> apparel, weapons, chests, general;
    };
    static Shelves shelves;
    static bool ready=false;
    if(!ready) {
        FILE* file=nullptr;
        for(const char* root:{"Client","../Client","../../Client"}) {
            const std::string actual=historicalFilePath(root,"scripts/table/iteminfo.txt");
            if(!actual.empty() && (file=std::fopen(actual.c_str(),"rb")))break;
        }
        if(!file)return {};
        Shelves built;
        char raw[16384];
        while(std::fgets(raw,sizeof raw,file)) {
            const auto cols=tabs(raw);
            if(cols.size()<29)continue;
            char* end=nullptr;
            const long id=std::strtol(cols[0].c_str(),&end,10);
            if(end==cols[0].c_str() || *end || id<=0 || id>1000000)continue;
            const std::string& slots=cols[28];
            bool weapon=false;
            bool wearable=false;
            for(std::size_t i=0;i<slots.size();) {
                const std::size_t begin=i;
                while(i<slots.size() && slots[i]!=',')++i;
                char* slotEnd=nullptr;
                const std::string field=slots.substr(begin,i-begin);
                const long parsed=std::strtol(field.c_str(),&slotEnd,10);
                if(slotEnd==field.c_str() || *slotEnd || parsed<1 || parsed>1000) {
                    if(i<slots.size())++i;
                    continue;
                }
                const int slot=static_cast<int>(parsed);
                if(slot==6 || slot==9)weapon=true;
                else if(slot>0)wearable=true;
                if(i<slots.size())++i;
            }
            std::string lower=cols[1];
            std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            const bool chest=lower.find("chest")!=std::string::npos || lower.find("treasure")!=std::string::npos ||
                             lower.find("box")!=std::string::npos || lower.find("crate")!=std::string::npos;
            const int itemId=static_cast<int>(id);
            if(wearable&&!weapon)built.apparel.push_back(itemId);
            if(weapon)built.weapons.push_back(itemId);
            if(chest)built.chests.push_back(itemId);
            if(!wearable&&!weapon)built.general.push_back(itemId);
        }
        std::fclose(file);
        // Materiais diretamente usados pela promocao de Crusader e pelo caminho
        // historico do Champion ficam no vendedor geral para testes de missao.
        for(int id:{15,295,4368,4453,4454,4456,4474})
            if(std::find(built.general.begin(),built.general.end(),id)==built.general.end())
                built.general.push_back(id);
        shelves=std::move(built);
        ready=true;
    }
    switch(shelf) {
        case Shelf::Apparel: return shelves.apparel;
        case Shelf::Weapons: return shelves.weapons;
        case Shelf::Chests: return shelves.chests;
        case Shelf::General: return shelves.general;
    }
    return {};
}


}  // namespace

bool NpcSystem::parse(const std::string& line, Npc& out) {
    if (line.empty() || line.rfind("//", 0) == 0) return false;
    const std::vector<std::string> cols = tabs(line);
    if (cols.size() < 12) return false;
    out = Npc{};
    out.serial = std::atoi(cols[0].c_str());
    out.name = cols[1];
    out.characterId = std::atoi(cols[3].c_str());
    out.service = std::atoi(cols[4].c_str());
    if (out.serial < 0 || out.characterId <= 0 || !point(cols[5], out.x, out.y)) return false;
    out.yaw = static_cast<float>(std::atof(cols[7].c_str()));
    out.region = cols[8];
    out.script = cols[11];
    while (!out.script.empty() && (out.script.back() == '\r' || out.script.back() == '\n')) out.script.pop_back();
    return !out.name.empty();
}

bool NpcSystem::load(const std::string& mapName) {
    npcs_.clear();
    scriptDataCache_.clear();
    scriptIndexReady_ = false;
    missionCatalog_.reset();
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
            const std::string relative = layout + mapName + "/" + mapName + "npc.txt";
            const std::string actual = historicalFilePath(root, relative);
            if (!actual.empty() && (file = std::fopen(actual.c_str(), "rb"))) break;
        }
    }
    if (!file) return false;
    char line[8192];
    while (std::fgets(line, sizeof line, file)) {
        Npc npc;
        if (parse(line, npc)) npcs_.push_back(std::move(npc));
    }
    std::fclose(file);
    if(mapName=="garner") {
        // Additional QA vendors in Argent; historical NPC rows and stocks are
        // intentionally untouched. Reuse known NPC actor models from the map.
        constexpr int actorModels[]={17,20,18,13};
        constexpr const char* names[]={"Test Apparel & Armor","Test Weapons","Test Chests","Test General & Quest Items"};
        constexpr const char* scripts[]={"TOP_TEST_APPAREL","TOP_TEST_WEAPONS","TOP_TEST_CHESTS","TOP_TEST_GENERAL"};
        constexpr float locations[][2]={{2224.50f,2784.50f},{2227.00f,2784.50f},
                                        {2224.50f,2787.00f},{2227.00f,2787.00f}};
        for(int i=0;i<4;++i) {
            Npc npc;
            npc.serial=10000+i; npc.name=names[i]; npc.characterId=actorModels[i]; npc.service=1;
            npc.x=locations[i][0]; npc.y=locations[i][1]; npc.yaw=180.f;
            npc.region="Argent City"; npc.script=scripts[i];
            npcs_.push_back(std::move(npc));
        }
        Npc board;
        board.serial=10004;board.name="Argent Mission Board";board.characterId=11;board.service=1;
        board.x=2225.75f;board.y=2785.75f;board.yaw=180.f;board.region="Argent City";
        board.script="TOP_CUSTOM_MISSIONS";
        npcs_.push_back(std::move(board));
        Npc trainer;
        trainer.serial=10005;trainer.name="Class Test Trainer";trainer.characterId=13;trainer.service=1;
        trainer.x=2222.50f;trainer.y=2786.50f;trainer.yaw=180.f;trainer.region="Argent City";
        trainer.script="TOP_TEST_CLASS";
        npcs_.push_back(std::move(trainer));
    }
    return !npcs_.empty();
}

unsigned NpcSystem::forgeServices(const std::string& script) const {
    if (script.empty()) return 0;
    constexpr const char* names[] = {"NpcScript01.lua", "NpcScript02.lua", "NpcScript03.lua", "NpcScript04.lua",
                                     "NpcScript05.lua", "NpcScript06.lua", "NpcScript07.lua", "NpcScript08.lua"};
    constexpr const char* roots[] = {"Client/scripts/offline/script/MisScript/",
                                     "../Client/scripts/offline/script/MisScript/",
                                     "../../Client/scripts/offline/script/MisScript/"};
    for (const char* root : roots) {
        for (const char* name : names) {
            FILE* f = std::fopen((std::string(root) + name).c_str(), "rb");
            if (!f) continue;
            std::string content;
            char chunk[8192];
            std::size_t n = 0;
            while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0 && content.size() < 2u*1024u*1024u)
                content.append(chunk, n);
            std::fclose(f);
            bool scriptFound = false;
            const unsigned services = readNpcForgeServices(content, script, &scriptFound);
            // Nao confundir r_trade01 com r_trade01_extra, nem conceder
            // servicos de outra funcao/arquivo se este script existe.
            if (scriptFound) return services;
        }
    }
    return 0;
}

bool NpcSystem::closest(float x, float y, int service, Npc& out) const {
    float best = std::numeric_limits<float>::max();
    bool found = false;
    for (const Npc& npc : npcs_) {
        if (service >= 0 && npc.service != service) continue;
        const float dx = npc.x - x;
        const float dy = npc.y - y;
        const float distance = dx * dx + dy * dy;
        if (distance >= best) continue;
        best = distance;
        out = npc;
        found = true;
    }
    return found;
}

std::vector<Npc> NpcSystem::visible(float x, float y, float radius) const {
    std::vector<Npc> result;
    const float limit = radius * radius;
    for (const Npc& npc : npcs_) {
        const float dx = npc.x - x;
        const float dy = npc.y - y;
        if (dx * dx + dy * dy <= limit) result.push_back(npc);
    }
    return result;
}

// 05-AJ: todas as consultas a mesma funcao usam os MESMOS tokens de codigo
// Lua, com cache por instancia de NpcSystem. Comentarios, strings e funcoes
// vizinhas nao podem criar lojas ou ofertas ficticias.
bool NpcSystem::readScriptData(const std::string& script, NpcLuaData& out) const {
    out = NpcLuaData{};
    if (script.empty() || script.size() > 128 ||
        !std::all_of(script.begin(), script.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '_';
        })) return false;
    // 05-AK.3: a versao anterior tokenizava o mesmo NpcScript01.lua
    // repetidamente (uma vez para cada NPC). Agora tokenizamos cada arquivo
    // somente uma vez e construimos o catalogo completo de funcoes.
    if (!scriptIndexReady_) {
        constexpr const char* names[] = {"NpcScript01.lua", "NpcScript02.lua", "NpcScript03.lua", "NpcScript04.lua",
                                         "NpcScript05.lua", "NpcScript06.lua", "NpcScript07.lua", "NpcScript08.lua"};
        for (const char* root : {"Client", "../Client", "../../Client"}) {
            for (const char* name : names) {
                const std::string actual = historicalFilePath(root, std::string("scripts/offline/script/MisScript/") + name);
                if (actual.empty()) continue;
                FILE* file = std::fopen(actual.c_str(), "rb");
                if (!file) continue;
                std::string content;
                char chunk[8192];
                std::size_t count = 0;
                while ((count = std::fread(chunk, 1, sizeof chunk, file)) != 0 && content.size() < 4u*1024u*1024u)
                    content.append(chunk, count);
                const bool tooLarge = content.size() > 4u*1024u*1024u;
                std::fclose(file);
                if (tooLarge) continue;
                readNpcLuaDataIndex(content, scriptDataCache_);
            }
        }
        scriptIndexReady_ = true;
    }
    const auto cached = scriptDataCache_.find(script);
    if (cached == scriptDataCache_.end()) return false;
    out = cached->second;
    return true;
}

bool NpcSystem::catalog(const std::string& script, std::vector<int>& itemIds) const {
    itemIds.clear();
    if(testVendorScript(script)) {
        itemIds=testVendorStock(script);
        return !itemIds.empty();
    }
    NpcLuaData data;
    if (!readScriptData(script, data)) return false;
    itemIds = std::move(data.tradeItems);
    return !itemIds.empty();
}

bool NpcSystem::exchangeRules(const std::string& script, std::vector<NpcLuaData::ExchangeRule>& rules) const {
    rules.clear();
    NpcLuaData data;
    if (!readScriptData(script, data)) return false;
    rules = std::move(data.exchangeRules);
    return !rules.empty();
}

bool NpcSystem::bankService(const std::string& script) const {
    NpcLuaData data;
    return readScriptData(script, data) && data.bankService;
}

bool NpcSystem::dialog(const std::string& script, std::string& text) const {
    text.clear();
    NpcLuaData data;
    if (!readScriptData(script, data)) return false;
    text = std::move(data.firstTalk);
    return !text.empty();
}

bool NpcSystem::dialogPage(const std::string& script, int page, NpcLuaData::DialogPage& out) const {
    out = NpcLuaData::DialogPage{};
    if (page <= 0 || page > 4096) return false;
    NpcLuaData data;
    if (!readScriptData(script, data)) return false;
    const auto found = data.dialogPages.find(page);
    if (found == data.dialogPages.end()) return false;
    out = found->second;
    return true;
}

bool NpcSystem::missionIds(const std::string& script, std::vector<int>& ids) const {
    ids.clear();
    if(script=="TOP_CUSTOM_MISSIONS") {
        ids=missionCatalog_.customMissionIds();
        return !ids.empty();
    }
    NpcLuaData data;
    if (!readScriptData(script, data)) return false;
    ids = std::move(data.missionIds);
    return !ids.empty();
}

// Facade conservado para codigo e testes historicos. A logica e indices
// residem somente no catalogo de missoes.
bool NpcSystem::mission(int id, HistoricalMission& out) const {
    return missionCatalog_.mission(id, out);
}
bool NpcSystem::missionByDisplay(int id, HistoricalMission& out) const {
    return missionCatalog_.missionByDisplay(id, out);
}
bool NpcSystem::missionStep(int id, HistoricalMissionStep& out) const {
    return missionCatalog_.missionStep(id, out);
}
std::unordered_map<int,std::vector<int>> NpcSystem::missionRecordIndex() const {
    return missionCatalog_.missionRecordIndex();
}

bool NpcSystem::selfTest() {
    Npc npc;
    if (!parse("2\tBlacksmith - Goldie\t1\t20\t1\t219312,270612\t219312,270612\t114\tArgent City\t1\t0\tr_trade01\t0", npc))
        return false;
    if (npc.serial != 2 || npc.characterId != 20 || npc.service != 1 || npc.script != "r_trade01") return false;
    if (npc.x < 2193.11f || npc.x > 2193.13f || npc.y < 2706.11f || npc.y > 2706.13f) return false;
    NpcSystem system;
    system.npcs_.push_back(npc);
    Npc found;
    return system.closest(2200.f, 2700.f, 1, found) && found.name == "Blacksmith - Goldie" &&
           system.visible(2193.12f, 2706.12f, 0.1f).size() == 1;
}

}  // namespace top
