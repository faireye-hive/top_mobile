#include "top/HistoricalRoundTimeline.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

int main() {
    using top::historical_round_timeline::SystemTimeline;
    unsetenv("TOP_HISTORICAL_ROUND_TIMELINE");
    assert(top::historical_round_timeline::enabled());
    setenv("TOP_HISTORICAL_ROUND_TIMELINE","1",1);
    assert(top::historical_round_timeline::enabled());

    SystemTimeline timeline;
    timeline.advance(4,.8f,1.f,4,2.f);
    assert(timeline.initialized && timeline.slots.size()==4);
    assert(timeline.slots[0].frame==1 && timeline.slots[0].frameTime==0.f);
    assert(std::fabs(timeline.slots[0].phase()-.25f)<1e-6f);
    const auto range=std::array<float,3>{{4.f,2.f,1.f}};
    const auto first=timeline.sample(0,range);
    assert(first.alive);
    assert(std::fabs(first.orbitAngle-1.6f)<1e-6f);
    assert(std::fabs(first.position[0]-(2.f-2.f*std::sin(1.6f)))<1e-6f);
    assert(std::fabs(first.position[1]-(1.f+2.f*std::cos(1.6f)))<1e-6f);

    // GetCurFrame advances at most one key on a long update and discards
    // the remaining .55 seconds, unlike continuous age sampling.
    timeline.advance(4,.8f,1.f,4,2.f);
    assert(timeline.slots[0].frame==2 && timeline.slots[0].frameTime==0.f);
    assert(std::fabs(timeline.slots[0].phase()-.5f)<1e-6f);

    // ROUND wraps its curve cursor but holds the previous position/curve for
    // the update in which DX9 resets the particle.
    SystemTimeline wrap;
    wrap.advance(1,.26f,.5f,2,1.f);
    const auto before=wrap.sample(0,range);
    assert(wrap.slots[0].frame==1);
    wrap.advance(1,.26f,.5f,2,1.f);
    const auto held=wrap.sample(0,range);
    assert(wrap.slots[0].frame==0 && wrap.slots[0].holdingLastFrame);
    assert(std::fabs(held.normalized-before.normalized)<1e-6f);
    assert(std::fabs(held.orbitAngle-before.orbitAngle)<1e-6f);
    wrap.advance(1,.01f,.5f,2,1.f);
    assert(!wrap.slots[0].holdingLastFrame);

    const float angleBefore=timeline.angle;
    timeline.advance(4,4.f,1.f,4,2.f);
    assert(std::fabs(timeline.angle-(angleBefore+8.f-6.283185f))<2e-5f);
    unsetenv("TOP_HISTORICAL_ROUND_TIMELINE");
}
