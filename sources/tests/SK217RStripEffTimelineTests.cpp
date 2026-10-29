#include "top/HistoricalEffTimeline.h"

#include <cassert>
#include <vector>

struct ModelTrack { top::legacy_eff::Subeffect subeffect; };

int main() {
    using namespace top;
    std::vector<ModelTrack> tracks(2);
    for (auto& track : tracks) {
        track.subeffect.keys.resize(2);
        track.subeffect.keys[0].time = .1f;
        track.subeffect.keys[1].time = .2f;
        track.subeffect.keys[0].position[0] = 0.f;
        track.subeffect.keys[1].position[0] = 10.f;
    }

    historical_eff_timeline::Group group;
    group.advance(.05f, tracks, false);
    auto phase = group.phase(0, tracks);
    assert(!phase.finished && phase.from == 0 && phase.to == 1);
    assert(phase.fraction > .49f && phase.fraction < .51f);
    const auto halfway = legacy_eff::samplePhase(tracks[0].subeffect, phase);
    assert(halfway.position[0] > 4.9f && halfway.position[0] < 5.1f);

    // One long DX9 FrameMove advances one key only and drops residual time.
    group.reset(tracks);
    group.advance(.25f, tracks, false);
    phase = group.phase(0, tracks);
    assert(!phase.finished && phase.from == 1 && phase.to == 0);
    assert(phase.fraction == 0.f);

    // The group repeats only when every nested controller has stopped.
    group.advance(.2f, tracks, true);
    phase = group.phase(0, tracks);
    assert(!phase.finished && phase.from == 0 && phase.fraction == 0.f);
    assert(group.generation == 1);

    // A shorter child remains stopped while its sibling continues.
    tracks[0].subeffect.keys[0].time = .1f;
    tracks[0].subeffect.keys[1].time = .1f;
    tracks[1].subeffect.keys[0].time = .3f;
    tracks[1].subeffect.keys[1].time = .3f;
    group.reset(tracks);
    group.advance(.1f, tracks, true);
    group.advance(.1f, tracks, true);
    assert(group.phase(0, tracks).finished);
    assert(!group.phase(1, tracks).finished);
}
