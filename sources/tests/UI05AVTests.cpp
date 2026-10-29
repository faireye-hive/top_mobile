#include "top/MissionJournal.h"
#include <cstdio>
#include <string>
#include <vector>
int main() {
    using namespace top;
    const MissionEventTrigger pickup{7211,MissionEventKind::GetItem,1573,10,1};
    const MissionEventTrigger kill{7361,MissionEventKind::Kill,104,5,10};
    if(missionEventProgress(pickup,6)!="Coletar Item 1573 6/10")return 1;
    if(missionEventProgress(pickup,20)!="Coletar Item 1573 10/10")return 2;
    if(missionEventProgress(pickup,-5)!="Coletar Item 1573 0/10")return 3;
    if(missionEventProgress(kill,3)!="Derrotar Monstro 104 3/5")return 4;
    if(missionEventProgress(pickup,1,"Shroom Leaves").find("Shroom Leaves") == std::string::npos)return 5;
    std::vector<MissionEventTrigger> triggers{pickup,kill};
    std::vector<int> counts{6,3},flags;
    if(!applyMissionEvent(triggers,counts,flags,MissionEventKind::GetItem,1573,4) ||
       counts!=std::vector<int>({10,3}) || flags!=std::vector<int>({10}))return 6;
    if(missionEventProgress(pickup,counts[0])!="Coletar Item 1573 10/10")return 7;
    std::vector<std::string> details={"title", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "NPC:", "Senna"};
    limitMissionJournalDetails(details,9,2);
    if(details.size()!=9 || details[6]!="... +5 objetivos" ||
       details[7]!="NPC:" || details[8]!="Senna")return 8;
    return 0;
}
