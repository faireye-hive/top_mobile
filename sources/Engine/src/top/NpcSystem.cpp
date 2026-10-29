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

std::string extractQuotedNpcText(const std::string& line) {
    const std::size_t first = line.find_first_of("\"'");
    if (first == std::string::npos) return {};
    const char quote = line[first];
    bool escaped = false;
    for (std::size_t at = first + 1; at < line.size(); ++at) {
        if (!escaped && line[at] == quote) return line.substr(first + 1, at - first - 1);
        if (!escaped && line[at] == '\\') escaped = true;
        else escaped = false;
    }
    return {};
}

std::string plainMissionText(const std::string& encoded) {
    std::string out;
    bool tag = false;
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        const char c = encoded[i];
        if (c == '<') {
            tag = true;
            if (encoded.compare(i, 3, "<n>") == 0 && !out.empty() && out.back() != ' ') out.push_back(' ');
            continue;
        }
        if (tag) {
            if (c == '>') tag = false;
            continue;
        }
        if (c == '\\' && i + 1 < encoded.size() && encoded[i + 1] == '\'') continue;
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte >= 32 && byte < 127) out.push_back(c);
    }
    return out;
}


std::string trimGate(std::string value) {
    const auto first=value.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return {};
    const auto last=value.find_last_not_of(" \t\r\n");
    value=value.substr(first,last-first+1);
    if(value.size()>=2 && (value.front()=='"'||value.front()=='\'' ) && value.back()==value.front())
        value=value.substr(1,value.size()-2);
    return value;
}
std::vector<std::string> gateArguments(const std::string& line) {
    const auto begin=line.find('('),end=line.rfind(')');
    if(begin==std::string::npos||end==std::string::npos||end<=begin)return {};
    std::vector<std::string> args;
    std::string text=line.substr(begin+1,end-begin-1);
    std::size_t pos=0;
    while(pos<=text.size()) {
        auto comma=text.find(',',pos);
        if(comma==std::string::npos)comma=text.size();
        args.push_back(trimGate(text.substr(pos,comma-pos)));
        if(comma==text.size())break;
        pos=comma+1;
    }
    return args;
}
int gateInteger(const std::string& text) {
    if(text.empty())return -1;
    char* end=nullptr;
    long value=std::strtol(text.c_str(),&end,10);
    if(end==text.c_str()||*end||value<0||value>100000000)return -1;
    return static_cast<int>(value);
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
    missionIndexReady_ = false;
    missionIndex_.clear();
    recordByDisplay_.clear();
    stepIndex_.clear();
    stepsByRecord_.clear();
    stepCache_.clear();
    displayByRecord_.clear();
    missionCache_.clear();
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
    NpcLuaData data;
    if (!readScriptData(script, data)) return false;
    itemIds = std::move(data.tradeItems);
    return !itemIds.empty();
}

bool NpcSystem::dialog(const std::string& script, std::string& text) const {
    text.clear();
    NpcLuaData data;
    if (!readScriptData(script, data)) return false;
    text = std::move(data.firstTalk);
    return !text.empty();
}

bool NpcSystem::missionIds(const std::string& script, std::vector<int>& ids) const {
    ids.clear();
    NpcLuaData data;
    if (!readScriptData(script, data)) return false;
    ids = std::move(data.missionIds);
    return !ids.empty();
}

void NpcSystem::ensureMissionIndex() const {
    if (missionIndexReady_) return;
    {
        const char* names[] = {"MissionScript01.lua", "MissionScript02.lua", "MissionScript03.lua", "MissionScript04.lua",
                               "MissionScript05.lua", "MissionScript06.lua", "MissionScript07.lua", "MissionScript08.lua"};
        const char* roots[] = {"Client/scripts/offline/script/MisScript/",
                               "../Client/scripts/offline/script/MisScript/",
                               "../../Client/scripts/offline/script/MisScript/"};
        for (const char* root : roots) {
            for (const char* name : names) {
                const std::string path = std::string(root) + name;
                FILE* source = std::fopen(path.c_str(), "rb");
                if (!source) continue;
                char raw[16384];
                long offset = 0;
                int currentMission=0;
                while ((offset = std::ftell(source)) >= 0 && std::fgets(raw, sizeof raw, source)) {
                    std::string line(raw);
                    const std::size_t first = line.find_first_not_of(" \t");
                    if (first != std::string::npos) line.erase(0, first);
                    if(line.rfind("MisBeginAction",0)==0 && currentMission>0 && line.find("AddMission")!=std::string::npos) {
                        const auto args=gateArguments(line);
                        if(args.size()>1&&args[0]=="AddMission" && gateInteger(args[1])>0) {
                            const int record=gateInteger(args[1]);
                            displayByRecord_[record].push_back(currentMission);
                        }
                    }
                    if (line.rfind("DefineMission", 0) != 0) continue;
                    currentMission=0;
                    const bool side = line.find("COMPLETE_SHOW") != std::string::npos;
                    const auto topArgs=gateArguments(line);
                    if(side && topArgs.size()>=3) {
                        const int display=gateInteger(topArgs[0]), record=gateInteger(topArgs[2]);
                        if(display>0 && record>0) {
                            stepIndex_.emplace(display,MissionLocation{path,offset,record});
                            stepsByRecord_[record].push_back(display);
                        }
                        continue;
                    }
                    const std::string title = extractQuotedNpcText(line);
                    const std::size_t quoteEnd = title.empty() ? std::string::npos : line.find(title) + title.size() + 1;
                    const std::size_t comma = quoteEnd == std::string::npos ? std::string::npos : line.find(',', quoteEnd);
                    const int record = comma == std::string::npos ? 0 : std::atoi(line.c_str() + comma + 1);
                    currentMission=record;
                    const int display=topArgs.empty()?-1:gateInteger(topArgs[0]);
                    if(record>0) {
                        missionIndex_.emplace(record, MissionLocation{path,offset,0});
                        if(display>0) recordByDisplay_.emplace(display,record);
                    }
                }
                std::fclose(source);
            }
        }
        missionIndexReady_ = true;
    }
}

// 05-AR: read the *display ID* of a COMPLETE_SHOW-only NPC step.
// An entry must be a simple, complete HasMission/NoRecord/NoFlag +
// HasItem/TakeItem -> SetFlag transaction. All other Lua actions fail closed.
bool NpcSystem::missionStep(int displayId, HistoricalMissionStep& step) const {
    step=HistoricalMissionStep{};
    ensureMissionIndex();
    const auto cached=stepCache_.find(displayId);
    if(cached!=stepCache_.end()) {step=cached->second;return true;}
    const auto found=stepIndex_.find(displayId);
    if(found==stepIndex_.end())return false;
    FILE* file=std::fopen(found->second.path.c_str(),"rb");
    if(!file)return false;
    if(std::fseek(file,found->second.offset,SEEK_SET)!=0){std::fclose(file);return false;}
    HistoricalMissionStep candidate;
    candidate.displayId=displayId;candidate.recordId=found->second.recordId;
    bool hasMission=false,noRecord=false,alwaysFailure=false,valid=true,inside=false;
    std::vector<HistoricalMission::ItemAmount> hasItems;
    char raw[16384];
    while(std::fgets(raw,sizeof raw,file)) {
        std::string line(raw);
        const auto first=line.find_first_not_of(" \t");
        if(first!=std::string::npos)line.erase(0,first);
        if(line.rfind("DefineMission",0)==0) {
            if(inside)break;
            inside=true;continue;
        }
        if(!inside || line.rfind("--",0)==0)continue;
        if(line.rfind("MisBeginCondition",0)==0) {
            const auto args=gateArguments(line);
            if(args.size()==1 && args[0]=="AlwaysFailure")alwaysFailure=true;
            else valid=false;
        }
        if(line.rfind("MisResultCondition",0)==0 || line.rfind("MisResultAction",0)==0) {
            const bool condition=line.rfind("MisResultCondition",0)==0;
            const auto args=gateArguments(line);
            const std::string name=args.empty()?std::string{}:args[0];
            const int a=args.size()>1?gateInteger(args[1]):-1;
            const int b=args.size()>2?gateInteger(args[2]):-1;
            const int record=candidate.recordId;
            if(condition) {
                if(name=="HasMission" && a==record)hasMission=true;
                else if(name=="NoRecord" && a==record)noRecord=true;
                else if(name=="NoFlag" && a==record && b>0 && b<=255 && !candidate.absentFlag)candidate.absentFlag=b;
                else if(name=="HasItem" && a>0 && b>0 && b<=1000000)hasItems.push_back({a,b});
                else valid=false;
            } else {
                if(name=="TakeItem" && a>0 && b>0 && b<=1000000)candidate.takeItems.push_back({a,b});
                else if(name=="SetFlag" && a==record && b>0 && b<=255 && !candidate.resultingFlag)candidate.resultingFlag=b;
                else valid=false;
            }
        }
    }
    std::fclose(file);
    auto counts=[](const std::vector<HistoricalMission::ItemAmount>& items) {
        std::unordered_map<int,int> out;
        for(const auto& item:items)out[item.id]+=item.count;
        return out;
    };
    if(!valid || !alwaysFailure || !hasMission || !noRecord ||
       candidate.absentFlag==0 || candidate.absentFlag!=candidate.resultingFlag ||
       candidate.takeItems.empty() || counts(hasItems)!=counts(candidate.takeItems))return false;
    stepCache_.emplace(displayId,candidate);
    step=candidate;return true;
}

bool NpcSystem::missionByDisplay(int displayId, HistoricalMission& result) const {
    ensureMissionIndex();
    const auto found=recordByDisplay_.find(displayId);
    if(found==recordByDisplay_.end()) {result=HistoricalMission{};return false;}
    return mission(found->second,result);
}

std::unordered_map<int,std::vector<int>> NpcSystem::missionRecordIndex() const {
    ensureMissionIndex();
    return displayByRecord_;
}

bool NpcSystem::mission(int id, HistoricalMission& out) const {
    out = HistoricalMission{};
    if (id <= 0) return false;
    // O menu de missoes e os marcadores ! consultam centenas de IDs na abertura.
    // Antes, cada missao varria novamente 8 arquivos inteiros (~2.8 MiB).
    // Um indice por instancia evita essa leitura quadratica sem mudar o parser.
    ensureMissionIndex();
    const auto cached = missionCache_.find(id);
    if (cached != missionCache_.end()) { out = cached->second; return true; }
    const auto found = missionIndex_.find(id);
    if (found == missionIndex_.end()) return false;
    FILE* file = std::fopen(found->second.path.c_str(), "rb");
    if (!file) return false;
    if (std::fseek(file, found->second.offset, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    {
            bool inside = false;
            bool alwaysFailure = false;
            bool resultPresent = false, resultHasMission = false, resultClearMission = false;
            bool resultUnsupported = false;
            bool hasExternalTriggers = false;
            std::vector<HistoricalMission::ItemAmount> resultTakeItems;
            std::vector<HistoricalMission::ItemAmount> resultHasItems;
            char raw[16384];
            while (std::fgets(raw, sizeof raw, file)) {
                std::string line(raw);
                const std::size_t first = line.find_first_not_of(" \t");
                if (first != std::string::npos) line.erase(0, first);
                if (line.rfind("DefineMission", 0) == 0) {
                    if (inside) break;
                    const std::string title = extractQuotedNpcText(line);
                    const std::size_t quoteEnd = title.empty() ? std::string::npos : line.find(title) + title.size() + 1;
                    const std::size_t comma = quoteEnd == std::string::npos ? std::string::npos : line.find(',', quoteEnd);
                    const int record = comma == std::string::npos ? 0 : std::atoi(line.c_str() + comma + 1);
                    if (record == id && line.find("COMPLETE_SHOW") == std::string::npos) {
                        inside = true;
                        out.id = id;
                        out.recordId = id;
                        out.title = plainMissionText(title);
                    }
                    continue;
                }
                if (!inside || line.rfind("--", 0) == 0) continue;
                // 05-AP: never execute Lua. Accept only a small, complete set of
                // historically verified result calls for NPC hand-in. Unknown
                // result operations block this mission's offline offer.
                if (line.rfind("MisResultCondition",0)==0 || line.rfind("MisResultAction",0)==0) {
                    resultPresent=true;
                    const bool condition=line.rfind("MisResultCondition",0)==0;
                    const auto args=gateArguments(line);
                    if (args.empty()) {resultUnsupported=true;continue;}
                    const int a=args.size()>1?gateInteger(args[1]):-1;
                    const int b=args.size()>2?gateInteger(args[2]):-1;
                    const auto& type=args[0];
                    // The script's AddMission record is the second/third
                    // DefineMission argument, NOT the display ID.
                    if(condition) {
                        if(type=="HasMission" && a==out.recordId) resultHasMission=true;
                        else if(type=="NoRecord" && a==out.recordId) {} // cannot already be complete
                        else if(type=="HasItem" && a>0 && b>0 && b<=1000000)
                            resultHasItems.push_back({a,b});
                        else if(type=="HasFlag" && a==out.recordId && b>0 && b<=255)
                            out.resultHasFlags.push_back(b);
                        else if(type=="NoFlag" && a==out.recordId && b>0 && b<=255)
                            out.resultNoFlags.push_back(b);
                        else resultUnsupported=true;
                    } else {
                        if(type=="ClearMission" && a==out.recordId) resultClearMission=true;
                        else if(type=="SetRecord" && a==out.recordId) {} // represented by completed progress
                        else if((type=="SetFlag" || type=="ClearFlag") && a==out.recordId && b>0 && b<=255) {
                            // Effects on flags of a mission being cleared need no durable state.
                        }
                        else if(type=="TakeItem" && a>0 && b>0 && b<=1000000)
                            resultTakeItems.push_back({a,b});
                        else if(type=="AddExp" && a>=0 && b>=a) {} // deterministic lower bound in 05-AO
                        else if(type=="AddMoney" && a>=0) {}
                        else if(type=="GiveItem" && a>0 && b>0 && b<=1000000) {}
                        else resultUnsupported=true;
                    }
                }
                // 05-AM: apenas condicoes de inicio da missao; condicoes de
                // entrega (MisResultCondition) nao bloqueiam a oferta.
                if (line.rfind("MisBeginCondition", 0) == 0) {
                    const auto args=gateArguments(line);
                    if(args.empty()){out.gate.unsupported="Condicao Lua sem argumentos";continue;}
                    const auto& type=args[0];
                    const int a=args.size()>1?gateInteger(args[1]):-1;
                    if(type=="AlwaysFailure")alwaysFailure=true;
                    else if(type=="HasRecord" && a>0)out.gate.needsCompleted.push_back(a);
                    else if(type=="NoRecord" && a>0)out.gate.needsNotCompleted.push_back(a);
                    else if(type=="HasMission" && a>0)out.gate.needsActive.push_back(a);
                    else if(type=="NoMission" && a>0)out.gate.needsNotActive.push_back(a);
                    else if(type=="PfEqual" && a>=0)out.gate.jobEqual=a;
                    else if(type=="NoPfEqual" && a>=0)out.gate.jobNotEqual=a;
                    else if(type=="HasItem" && a>0 && args.size()>2 && gateInteger(args[2])>0)
                        out.gate.needsItems.push_back({a,gateInteger(args[2])});
                    else if(type=="LvCheck" && args.size()>2) {
                        const int level=gateInteger(args[2]);
                        if(level>=0&&args[1]==">")out.minLevel=std::max(out.minLevel,level+1);
                        else if(level>=0&&args[1]==">=")out.minLevel=std::max(out.minLevel,level);
                        else if(level>=0&&args[1]=="<")out.maxLevel=out.maxLevel==0?level-1:std::min(out.maxLevel,level-1);
                        else if(level>=0&&args[1]=="<=")out.maxLevel=out.maxLevel==0?level:std::min(out.maxLevel,level);
                        else out.gate.unsupported="LvCheck: operador desconhecido";
                    }
                    else if(out.gate.unsupported.empty())out.gate.unsupported="Condicao Lua: "+type;
                    continue;
                }
                if (line.rfind("MisBeginAction",0)==0) {
                    const auto args=gateArguments(line);
                    if(args.size()>1 && args[0]=="AddMission" && gateInteger(args[1])>0)
                        out.recordId=gateInteger(args[1]);
                    else if(!args.empty() && args[0]=="SetFlag") {
                        if(args.size()>2 && gateInteger(args[1])==out.recordId &&
                           gateInteger(args[2])>0 && gateInteger(args[2])<=255)
                            out.beginFlags.push_back(gateInteger(args[2]));
                        else if(out.gate.unsupported.empty())out.gate.unsupported="SetFlag de inicio nao modelado";
                    } else if(!args.empty() && args[0]=="GiveItem") {
                        if(args.size()>=3 && gateInteger(args[1])>0 && gateInteger(args[2])>0 &&
                           gateInteger(args[2])<=1000000)
                            out.beginItems.push_back({gateInteger(args[1]),gateInteger(args[2])});
                        else if(out.gate.unsupported.empty())out.gate.unsupported="GiveItem de inicio invalido";
                    } else if(!args.empty() && args[0]=="AddTrigger") hasExternalTriggers=true;
                }
                if (line.rfind("MisBeginTalk", 0) == 0 && out.beginText.empty()) out.beginText = plainMissionText(extractQuotedNpcText(line));
                else if (line.rfind("MisNeed", 0) == 0) {
                    if (line.find("MIS_NEED_DESP") != std::string::npos) {
                        const std::string encoded = extractQuotedNpcText(line);
                        out.objective = plainMissionText(encoded);
                        const std::size_t pos = encoded.find("<j(");
                        int x = 0;
                        int y = 0;
                        if (pos != std::string::npos && std::sscanf(encoded.c_str() + pos, "<j(%d,%d)", &x, &y) == 2) {
                            out.targetX = static_cast<float>(x);
                            out.targetY = static_cast<float>(y);
                        }
                    }
                    else if (line.find("MIS_NEED_KILL") != std::string::npos) {
                        const std::size_t comma = line.find(',', line.find("MIS_NEED_KILL"));
                        if (comma != std::string::npos) {
                            out.targetId = std::atoi(line.c_str() + comma + 1);
                            const std::size_t next = line.find(',', comma + 1);
                            if (next != std::string::npos) out.targetCount = std::atoi(line.c_str() + next + 1);
                        }
                    } else if (line.find("MIS_NEED_ITEM") != std::string::npos) {
                        const std::size_t comma = line.find(',', line.find("MIS_NEED_ITEM"));
                        if (comma != std::string::npos) {
                            const int itemId = std::atoi(line.c_str() + comma + 1);
                            const std::size_t next = line.find(',', comma + 1);
                            const int count = next == std::string::npos ? 0 : std::atoi(line.c_str() + next + 1);
                            if (itemId > 0 && count > 0) out.neededItems.push_back({itemId, count});
                        }
                    }
                } else if (line.find("MisResultAction(AddExp") != std::string::npos) {
                    const std::size_t comma = line.find(',', line.find("AddExp"));
                    if (comma != std::string::npos) out.rewardExp = std::atoi(line.c_str() + comma + 1);
                } else if (line.find("MisResultAction(AddMoney") != std::string::npos) {
                    const std::size_t comma = line.find(',', line.find("AddMoney"));
                    if (comma != std::string::npos) out.rewardGold = std::atoi(line.c_str() + comma + 1);
                } else if (line.find("MisPrize(MIS_PRIZE_MONEY") != std::string::npos) {
                    const std::size_t comma = line.find(',', line.find("MIS_PRIZE_MONEY"));
                    if (comma != std::string::npos) out.rewardGold = std::atoi(line.c_str() + comma + 1);
                } else if (line.find("MisPrize(MIS_PRIZE_ITEM") != std::string::npos ||
                           line.find("MisResultAction(GiveItem") != std::string::npos) {
                    const std::size_t token = line.find("MIS_PRIZE_ITEM") != std::string::npos ?
                                              line.find("MIS_PRIZE_ITEM") : line.find("GiveItem");
                    const std::size_t comma = line.find(',', token);
                    if (comma != std::string::npos) {
                        const int itemId = std::atoi(line.c_str() + comma + 1);
                        const std::size_t next = line.find(',', comma + 1);
                        const int count = next == std::string::npos ? 0 : std::atoi(line.c_str() + next + 1);
                        if (itemId > 0 && count > 0) out.rewardItems.push_back({itemId, count});
                    }
                }
            }
            std::fclose(file);
            if(resultPresent) {
                // 05-AQ: a flag belongs to the active record, not the visible
                // mission id. We cannot claim dynamic trigger flags before
                // interpreting the corresponding TE_GETITEM/TE_KILL scripts.
                // Only accept flags whose value is provable from begin actions
                // and which cannot change through an unimplemented trigger.
                if(!out.resultHasFlags.empty() || !out.resultNoFlags.empty()) {
                    if(hasExternalTriggers) resultUnsupported=true;
                    for(int flag:out.resultHasFlags) {
                        if(std::find(out.beginFlags.begin(),out.beginFlags.end(),flag)!=out.beginFlags.end())continue;
                        // A flag nao nasce por proximidade: exige uma etapa
                        // COMPLETE_SHOW real vinculada ao mesmo record.
                        bool supported=false;
                        const auto source=stepsByRecord_.find(out.recordId);
                        if(source!=stepsByRecord_.end())for(int display:source->second) {
                            HistoricalMissionStep side;
                            if(missionStep(display,side) && side.resultingFlag==flag) {supported=true;break;}
                        }
                        if(!supported)resultUnsupported=true;
                    }
                    for(int flag:out.resultNoFlags)
                        if(std::find(out.beginFlags.begin(),out.beginFlags.end(),flag)!=out.beginFlags.end())
                            resultUnsupported=true;
                }
                // Do not accidentally consume unlisted items or duplicate a
                // MisNeed item. Item requirements and TakeItem actions must
                // match exactly for supported hand-in paths.
                auto aggregate=[](const std::vector<HistoricalMission::ItemAmount>& values) {
                    std::unordered_map<int,int> counts;
                    for(const auto& item:values)counts[item.id]+=item.count;
                    return counts;
                };
                const auto needed=aggregate(out.neededItems);
                if(needed!=aggregate(resultTakeItems) ||
                   (!resultHasItems.empty() && needed!=aggregate(resultHasItems)))
                    resultUnsupported=true;
                if(!resultHasMission || !resultClearMission || resultUnsupported) {
                    if(out.gate.unsupported.empty())out.gate.unsupported="Resultado Lua nao suportado com seguranca";
                } else {
                    out.turnInRequired=true;
                }
            }
            const bool valid = inside && !alwaysFailure && !out.title.empty() &&
                               (!out.beginText.empty() || !out.objective.empty());
            if (valid) missionCache_.emplace(id, out);
            else out = HistoricalMission{};
            return valid;
    }
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
