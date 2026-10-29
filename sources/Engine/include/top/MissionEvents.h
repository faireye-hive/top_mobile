#pragma once
// Eventos de missao offline, sem NPC, OpenGL ou interpretador Lua.
// Somente regras extraidas e verificadas pelo MissionCatalog podem chegar aqui.
#include <algorithm>
#include <vector>
namespace top {
enum class MissionEventKind { Kill, GetItem };
struct MissionEventTrigger {
    int triggerId = 0;
    MissionEventKind kind = MissionEventKind::Kill;
    int targetId = 0;
    int required = 0;
    int firstFlag = 0;
};
// A unica flag produzida por um contador completo e a flag final do
// AddNextFlag (ex.: 10,5 => 14). Contadores intermediarios NAO sao
// persistidos como flags, evitando ultrapassar o limite Q4 de 32 flags.
inline bool applyMissionEvent(const std::vector<MissionEventTrigger>& triggers,
                             std::vector<int>& counts, std::vector<int>& flags,
                             MissionEventKind kind, int targetId, int amount=1) {
    if(amount<=0 || targetId<=0 || counts.size()!=triggers.size())return false;
    bool changed=false;
    for(std::size_t i=0;i<triggers.size();++i) {
        const auto& rule=triggers[i];
        if(rule.kind!=kind || rule.targetId!=targetId || counts[i]>=rule.required)continue;
        const int next=counts[i]+std::min(amount,rule.required-counts[i]);
        if(next==counts[i])continue;
        counts[i]=next;
        if(next==rule.required && rule.firstFlag>0) {
            const int finalFlag=rule.firstFlag+rule.required-1;
            if(std::find(flags.begin(),flags.end(),finalFlag)==flags.end())flags.push_back(finalFlag);
        }
        changed=true;
    }
    return changed;
}
} // namespace top
