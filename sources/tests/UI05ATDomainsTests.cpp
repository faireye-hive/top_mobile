#include "top/MissionCatalog.h"
#include <cassert>
int main() {
    top::MissionCatalog missions;
    top::HistoricalMission offer;
    top::HistoricalMissionStep side;
    assert(!missions.mission(-1,offer));
    assert(!missions.missionStep(-1,side));
    assert(missions.missionByDisplay(702,offer));
    assert(offer.id==701 && offer.recordId==701);
    assert(missions.missionStep(703,side));
    assert(side.recordId==701 && side.resultingFlag==10);
    assert(!missions.missionByDisplay(703,offer));
    auto idx=missions.missionRecordIndex();
    assert(!idx[701].empty());
    missions.reset();
    assert(missions.mission(701,offer) && offer.id==701);
}
