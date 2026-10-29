#include "top/HistoricalModelEffTimeline.h"
#include "top/HistoricalEffTimeline.h"

#include <cassert>
#include <cstdlib>
#include <vector>

int main() {
    using top::historical_eff_timeline::Group;
    struct Track { top::legacy_eff::Subeffect subeffect; };
    unsetenv("TOP_HISTORICAL_MODEL_EFF_TIMELINE");
    assert(top::historical_model_eff_timeline::enabled());
    setenv("TOP_HISTORICAL_MODEL_EFF_TIMELINE","1",1);
    assert(top::historical_model_eff_timeline::enabled());

    std::vector<Track> source(1);
    source[0].subeffect.keys.resize(3);
    for(auto& key:source[0].subeffect.keys)key.time=.1f;
    Group group;
    group.advance(.35f,source,true);
    assert(group.initialized && group.tracks[0].frame==1);
    assert(group.tracks[0].frameTime==0.f); // one child frame per FrameMove; drop residual
    assert(group.phase(0,source).from==1);
    group.advance(.1f,source,true);
    assert(group.tracks[0].frame==2);
    group.advance(.1f,source,true);
    assert(group.generation==1 && group.tracks[0].frame==0);
    assert(group.phase(0,source).from==0);
    unsetenv("TOP_HISTORICAL_MODEL_EFF_TIMELINE");
}
