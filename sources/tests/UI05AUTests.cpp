#include "top/MissionCatalog.h"
#include "top/MissionEvents.h"
#include <algorithm>
#include <cstdio>
#include <vector>
int main() {
    top::MissionCatalog catalog;
    top::HistoricalMission collection, recipe, trial;
    if(!catalog.mission(721,collection) || collection.eventTriggers.size()!=2 ||
       !catalog.mission(726,recipe) || recipe.eventTriggers.size()!=3 ||
       !catalog.mission(736,trial) || trial.eventTriggers.size()!=3 ||
       trial.resultHasFlags!=std::vector<int>({14,29})){
        std::fprintf(stderr,"05-AU: historical trigger extraction failed\n");return 1;
    }
    // The original 736 has a profession-conversion gate that remains
    // unsupported; event parsing must not silently remove that gate.
    if(trial.gate.unsupported.empty())return 2;
    std::vector<int> counts(trial.eventTriggers.size(),0),flags={1};
    const auto apply=[&](top::MissionEventKind kind,int id,int amount=1){
        return top::applyMissionEvent(trial.eventTriggers,counts,flags,kind,id,amount);
    };
    if(!apply(top::MissionEventKind::Kill,104,5) || !apply(top::MissionEventKind::Kill,39,10) ||
       !apply(top::MissionEventKind::GetItem,1844,5) ||
       std::find(flags.begin(),flags.end(),14)==flags.end() ||
       std::find(flags.begin(),flags.end(),29)==flags.end() ||
       std::find(flags.begin(),flags.end(),34)==flags.end())return 3;
    if(apply(top::MissionEventKind::Kill,104,1) ||
       apply(top::MissionEventKind::GetItem,1844,1) ||
       apply(top::MissionEventKind::Kill,999,1))return 4;
    // Separate missions counting the same mob have independent progress.
    std::vector<top::MissionEventTrigger> duplicate{{11,top::MissionEventKind::Kill,104,2,10}};
    std::vector<int> a{0},b{0},af,bf;
    if(!top::applyMissionEvent(duplicate,a,af,top::MissionEventKind::Kill,104) ||
       !top::applyMissionEvent(duplicate,b,bf,top::MissionEventKind::Kill,104) ||
       a[0]!=1 || b[0]!=1 || !af.empty() || !bf.empty())return 5;
    if(!top::applyMissionEvent(duplicate,a,af,top::MissionEventKind::Kill,104) ||
       a[0]!=2 || b[0]!=1 || af!=std::vector<int>{11})return 6;
    return 0;
}
