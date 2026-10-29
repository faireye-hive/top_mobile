#include "top/HistoricalEffTimeline.h"

#include <cassert>
#include <vector>

struct ModelTrack { top::legacy_eff::Subeffect subeffect; };

int main() {
    using namespace top;
    std::vector<ModelTrack> tracks(3);
    for (auto& track : tracks) {
        track.subeffect.keys.resize(2);
        track.subeffect.keys[0].time = 1.f;
        track.subeffect.keys[1].time = 1.f;
    }

    auto& coord = tracks[0].subeffect;
    coord.type = 2;
    coord.coordFrameTime = .1f;
    coord.coordFrames.resize(2);
    coord.coordFrames[0][0][0] = 0.f;
    coord.coordFrames[1][0][0] = 1.f;
    std::vector<legacy_eff::Subeffect::UV> meshUvs(4);
    meshUvs[0][0] = 0.f;
    coord.meshCoordFrames = {meshUvs, meshUvs};
    coord.meshCoordFrames[1][0][0] = 1.f;

    auto& region = tracks[1].subeffect;
    region.type = 3;
    region.regionFrameTime = .1f;
    region.regions.resize(2);
    region.regions[0][0][0] = .2f;
    region.regions[1][0][0] = .8f;

    auto& texture = tracks[2].subeffect;
    texture.type = 1;
    texture.textureFrameTime = .1f;
    texture.frames = {"one.tga", "two.tga"};

    historical_eff_timeline::Group group;
    group.advance(.1f, tracks, false);
    // DX9's strict greater-than keeps the first texture/region at equality;
    // UV interpolation reaches the next coordinate at t=1.
    assert(group.textureFrame(2) == 0);
    assert(group.sampleQuad(0, coord)[0][0] > .99f);
    assert(group.sampleQuad(1, region)[0][0] == .2f);

    group.advance(.01f, tracks, false);
    assert(group.textureFrame(2) == 1);
    assert(group.sampleQuad(1, region)[0][0] == .8f);
    assert(group.sampleQuad(0, coord)[0][0] == 1.f); // final UV set holds

    group.reset(tracks);
    group.advance(.35f, tracks, false);
    // At most one controller step per update; excess time is discarded.
    assert(group.textureFrame(2) == 1);
    assert(group.tracks[2].textureClock.elapsed == 0.f);
    group.advance(.35f, tracks, false);
    assert(group.textureFrame(2) == 0); // wraps one frame at a time

    // Controller clocks are independent per child .eff part.
    group.reset(tracks);
    group.advance(.11f, tracks, false);
    assert(group.textureFrame(2) == 1);
    assert(group.tracks[0].coordClock.index == 1);
    assert(group.tracks[1].regionClock.index == 1);
}
