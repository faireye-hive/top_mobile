#include "top/NpcSystem.h"
#include <cstdio>
#include <vector>

int main() {
    top::NpcSystem scripts;
    top::HistoricalMission main;
    top::HistoricalMissionStep goldie;
    if(!scripts.mission(701,main) || main.recordId!=701 || !main.turnInRequired ||
       !main.gate.unsupported.empty() || main.beginItems.size()!=1 ||
       main.beginItems[0].id!=3950 || main.beginItems[0].count!=1 ||
       main.resultHasFlags!=std::vector<int>{10} || main.rewardExp!=9 ||
       !scripts.missionStep(703,goldie) || goldie.recordId!=701 ||
       goldie.absentFlag!=10 || goldie.resultingFlag!=10 ||
       goldie.takeItems.size()!=1 || goldie.takeItems[0].id!=3950 ||
       goldie.takeItems[0].count!=1) {
       std::fprintf(stderr,"05-AR: Senna -> Goldie -> Senna script contract mismatch\n");return 1;
    }
    top::HistoricalMission sennaOffer,wrongOffer;
    if(!scripts.missionByDisplay(702,sennaOffer) || sennaOffer.id!=701 ||
       sennaOffer.beginItems.size()!=1 || sennaOffer.beginItems[0].id!=3950 ||
       scripts.missionByDisplay(703,wrongOffer)) {
       std::fprintf(stderr,"05-AR: AddNpcMission display ID incorrectly resolved to record\n");return 1;
    }
    std::vector<int> goldieIds,sennaIds;
    if(!scripts.missionIds("r_trade01",goldieIds) ||
       !scripts.missionIds("BT_NewUser001",sennaIds)) {
        std::fprintf(stderr,"05-AR: real NPC dialogue scripts missing\n");return 1;
    }
    auto contains=[](const std::vector<int>& v,int n) {
        for(int x:v)if(x==n)return true;return false;
    };
    if(!contains(goldieIds,703) || !contains(sennaIds,702)) {
        std::fprintf(stderr,"05-AR: side action attached to wrong NPC\n");return 1;
    }
    top::HistoricalMissionStep wrong;
    if(scripts.missionStep(702,wrong) || scripts.missionStep(999999,wrong)) {
        std::fprintf(stderr,"05-AR: accepted mission step without COMPLETE_SHOW\n");return 1;
    }
    return 0;
}
