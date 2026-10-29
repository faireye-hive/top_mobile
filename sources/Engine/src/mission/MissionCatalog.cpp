#include "top/MissionCatalog.h"
#include "top/HistoricalPaths.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
namespace top {
namespace {
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

std::vector<std::string> splitDelimited(const std::string& text,char delimiter) {
    std::vector<std::string> fields;
    std::size_t at=0;
    while(at<=text.size()) {
        const auto end=text.find(delimiter,at);
        fields.push_back(text.substr(at,end==std::string::npos?std::string::npos:end-at));
        if(end==std::string::npos)break;
        at=end+1;
    }
    return fields;
}

bool parsePositive(const std::string& text,int& value,int max=1000000) {
    char* end=nullptr;
    const long parsed=std::strtol(text.c_str(),&end,10);
    if(text.empty() || end==text.c_str() || *end || parsed<0 || parsed>max)return false;
    value=static_cast<int>(parsed);
    return true;
}

} // namespace

void MissionCatalog::reset() {
    missionIndexReady_ = false;
    missionIndex_.clear(); recordByDisplay_.clear();
    stepIndex_.clear(); stepsByRecord_.clear(); stepCache_.clear();
    displayByRecord_.clear(); missionCache_.clear();
    customMissionIndexReady_=false;customMissions_.clear();
}

void MissionCatalog::ensureCustomMissionIndex() const {
    if(customMissionIndexReady_)return;
    customMissionIndexReady_=true;
    FILE* file=nullptr;
    for(const char* root:{".","..","../.."}) {
        const std::string path=historicalFilePath(root,"sources/Tools/custom-missions.tsv");
        if(!path.empty() && (file=std::fopen(path.c_str(),"rb")))break;
    }
    for(const char* root:{"Client","../Client","../../Client"}) {
        if(file)break;
        const std::string path=historicalFilePath(root,"scripts/offline/custom-missions.tsv");
        if(!path.empty() && (file=std::fopen(path.c_str(),"rb")))break;
    }
    if(!file)return;
    char raw[8192];
    while(std::fgets(raw,sizeof raw,file)) {
        std::string line(raw);
        while(!line.empty() && (line.back()=='\n'||line.back()=='\r'))line.pop_back();
        if(line.empty() || line[0]=='#')continue;
        const auto columns=splitDelimited(line,'\t');
        // id, title, description, min level, objectives, exp, gold, item rewards
        if(columns.size()!=8)continue;
        int id=0,minLevel=0,rewardExp=0,rewardGold=0;
        if(!parsePositive(columns[0],id,1000000) || id<900000 ||
           !parsePositive(columns[3],minLevel,100) || minLevel<1 ||
           !parsePositive(columns[5],rewardExp,1000000000) ||
           !parsePositive(columns[6],rewardGold,1000000000) ||
           columns[1].empty() || columns[2].empty() || customMissions_.count(id))continue;
        HistoricalMission mission;
        mission.id=id;mission.recordId=id;mission.title=columns[1];
        mission.beginText=columns[2];mission.objective=columns[2];mission.minLevel=minLevel;
        mission.rewardExp=rewardExp;mission.rewardGold=rewardGold;mission.turnInRequired=true;
        mission.gate.unsupported.clear();
        bool valid=true;
        int nextFlag=1,triggerId=1,killObjectives=0;
        for(const auto& spec:splitDelimited(columns[4],';')) {
            const auto fields=splitDelimited(spec,':');
            int target=0,count=0;
            if(fields.size()!=3 || !parsePositive(fields[1],target) || target<1 ||
               !parsePositive(fields[2],count,255) || count<1) {valid=false;break;}
            MissionEventKind kind;
            if(fields[0]=="kill")kind=MissionEventKind::Kill;
            else if(fields[0]=="item")kind=MissionEventKind::GetItem;
            else {valid=false;break;}
            if(nextFlag+count-1>255) {valid=false;break;}
            mission.eventTriggers.push_back({triggerId++,kind,target,count,nextFlag});
            const int finalFlag=nextFlag+count-1;
            nextFlag=finalFlag+1;
            if(kind==MissionEventKind::Kill) {
                ++killObjectives;
                if(killObjectives==1) {mission.targetId=target;mission.targetCount=count;}
                mission.resultHasFlags.push_back(finalFlag);
            } else mission.neededItems.push_back({target,count});
        }
        if(!valid || mission.eventTriggers.empty())continue;
        if(killObjectives!=1)mission.targetId=mission.targetCount=0;
        if(!columns[7].empty())for(const auto& item:splitDelimited(columns[7],';')) {
            const auto fields=splitDelimited(item,':');
            int itemId=0,count=0;
            if(fields.size()!=2 || !parsePositive(fields[0],itemId) || itemId<1 ||
               !parsePositive(fields[1],count,1000000) || count<1) {valid=false;break;}
            mission.rewardItems.push_back({itemId,count});
        }
        if(valid)customMissions_.emplace(id,std::move(mission));
    }
    std::fclose(file);
}

std::vector<int> MissionCatalog::customMissionIds() const {
    ensureCustomMissionIndex();
    std::vector<int> ids;ids.reserve(customMissions_.size());
    for(const auto& entry:customMissions_)ids.push_back(entry.first);
    std::sort(ids.begin(),ids.end());
    return ids;
}
void MissionCatalog::ensureMissionIndex() const {
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
bool MissionCatalog::missionStep(int displayId, HistoricalMissionStep& step) const {
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

bool MissionCatalog::missionByDisplay(int displayId, HistoricalMission& result) const {
    ensureCustomMissionIndex();
    if(customMissions_.count(displayId))return mission(displayId,result);
    ensureMissionIndex();
    const auto found=recordByDisplay_.find(displayId);
    if(found==recordByDisplay_.end()) {result=HistoricalMission{};return false;}
    return mission(found->second,result);
}

std::unordered_map<int,std::vector<int>> MissionCatalog::missionRecordIndex() const {
    ensureMissionIndex();
    return displayByRecord_;
}

bool MissionCatalog::mission(int id, HistoricalMission& out) const {
    out = HistoricalMission{};
    if (id <= 0) return false;
    ensureCustomMissionIndex();
    const auto custom=customMissions_.find(id);
    if(custom!=customMissions_.end()) {out=custom->second;return true;}
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
            // 05-AU: declared triggers must agree exactly with their Lua
            // InitTrigger/IsItem|IsMonster/AddNextFlag/RegCurTrigger block.
            std::unordered_map<int,MissionEventTrigger> declared, registered;
            bool invalidTrigger=false, triggerBlock=false, conditionSeen=false, actionSeen=false;
            MissionEventTrigger currentCondition, currentAction;
            std::vector<HistoricalMission::ItemAmount> killObjectives;
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
                // This is a passive structural recognizer, NOT a Lua runtime.
                // A registration belongs to the current DefineMission block.
                if (line.rfind("InitTrigger",0)==0) {
                    if(triggerBlock)invalidTrigger=true;
                    triggerBlock=true;conditionSeen=false;actionSeen=false;
                } else if(line.rfind("TriggerCondition",0)==0) {
                    const auto args=gateArguments(line);
                    if(!triggerBlock || conditionSeen || args.size()!=3 || args[0]!="1" ||
                       (args[1]!="IsItem" && args[1]!="IsMonster") || gateInteger(args[2])<=0)
                        invalidTrigger=true;
                    else {
                        conditionSeen=true;
                        currentCondition.kind=args[1]=="IsItem"?MissionEventKind::GetItem:MissionEventKind::Kill;
                        currentCondition.targetId=gateInteger(args[2]);
                    }
                } else if(line.rfind("TriggerAction",0)==0) {
                    const auto args=gateArguments(line);
                    if(!triggerBlock || actionSeen || args.size()!=5 || args[0]!="1" ||
                       args[1]!="AddNextFlag" || gateInteger(args[2])!=out.recordId ||
                       gateInteger(args[3])<=0 || gateInteger(args[4])<=0 ||
                       gateInteger(args[3])+gateInteger(args[4])-1>255)
                        invalidTrigger=true;
                    else {
                        actionSeen=true;
                        currentAction.firstFlag=gateInteger(args[3]);
                        currentAction.required=gateInteger(args[4]);
                    }
                } else if(line.rfind("RegCurTrigger",0)==0) {
                    const auto args=gateArguments(line);
                    const int id=args.size()==1?gateInteger(args[0]):-1;
                    const auto definition=declared.find(id);
                    if(!triggerBlock || !conditionSeen || !actionSeen || id<=0 ||
                       definition==declared.end() || registered.count(id) ||
                       definition->second.kind!=currentCondition.kind ||
                       definition->second.targetId!=currentCondition.targetId ||
                       definition->second.required!=currentAction.required)
                        invalidTrigger=true;
                    else {
                        auto trigger=definition->second;
                        trigger.firstFlag=currentAction.firstFlag;
                        registered.emplace(id,trigger);
                    }
                    triggerBlock=false;
                }
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
                    } else if(!args.empty() && args[0]=="AddTrigger") {
                        hasExternalTriggers=true;
                        const int id=args.size()==5?gateInteger(args[1]):-1;
                        const int target=args.size()==5?gateInteger(args[3]):-1;
                        const int count=args.size()==5?gateInteger(args[4]):-1;
                        if(id<=0 || target<=0 || count<=0 || count>255 ||
                           (args[2]!="TE_GETITEM" && args[2]!="TE_KILL") || declared.count(id))
                            invalidTrigger=true;
                        else declared.emplace(id,MissionEventTrigger{
                            id,args[2]=="TE_GETITEM"?MissionEventKind::GetItem:MissionEventKind::Kill,
                            target,count,0});
                    }
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
                            if(out.targetId>0 && out.targetCount>0)
                                killObjectives.push_back({out.targetId,out.targetCount});
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
            if(hasExternalTriggers) {
                if(triggerBlock || invalidTrigger || declared.size()!=registered.size()) invalidTrigger=true;
                // Every declared TE_GETITEM/TE_KILL must correspond to one
                // MisNeed objective with the SAME required event count.
                for(const auto& entry:declared) {
                    const auto it=registered.find(entry.first);
                    if(it==registered.end()) {invalidTrigger=true;continue;}
                    const auto& rule=it->second;
                    const auto& objectives=rule.kind==MissionEventKind::Kill?killObjectives:out.neededItems;
                    if(std::count_if(objectives.begin(),objectives.end(),[&](const auto& item) {
                        return item.id==rule.targetId && item.count==rule.required;
                    })!=1)invalidTrigger=true;
                }
                // Prevent overlapping flag ranges and conflicting streams.
                for(const auto& a:registered)for(const auto& b:registered)if(a.first<b.first) {
                    const auto& x=a.second;const auto& y=b.second;
                    if(x.firstFlag<=y.firstFlag+y.required-1 &&
                       y.firstFlag<=x.firstFlag+x.required-1)invalidTrigger=true;
                    if(x.kind==y.kind && x.targetId==y.targetId)invalidTrigger=true;
                }
                if(!invalidTrigger) {
                    for(const auto& entry:registered)out.eventTriggers.push_back(entry.second);
                    std::sort(out.eventTriggers.begin(),out.eventTriggers.end(),
                         [](const auto& a,const auto& b){return a.triggerId<b.triggerId;});
                    // Multi-target hunting cannot reuse the historical one-
                    // target fields: those are retained for legacy quests.
                    if(killObjectives.size()>1)out.targetId=out.targetCount=0;
                }
            }
            if(resultPresent) {
                // 05-AQ: a flag belongs to the active record, not the visible
                // mission id. We cannot claim dynamic trigger flags before
                // interpreting the corresponding TE_GETITEM/TE_KILL scripts.
                // Only accept flags whose value is provable from begin actions
                // and which cannot change through an unimplemented trigger.
                if(!out.resultHasFlags.empty() || !out.resultNoFlags.empty()) {
                    if(hasExternalTriggers && invalidTrigger) resultUnsupported=true;
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
                        if(!supported)for(const auto& trigger:out.eventTriggers)
                            if(trigger.firstFlag+trigger.required-1==flag) {supported=true;break;}
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
                if(!resultHasMission || !resultClearMission || resultUnsupported ||
                   (hasExternalTriggers && invalidTrigger)) {
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

} // namespace top
