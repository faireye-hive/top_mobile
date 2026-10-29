#include "top/HistoricalRipple.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

int main() {
    using top::historical_ripple::SystemTimeline;
    unsetenv("TOP_HISTORICAL_RIPPLE_POOL");
    assert(top::historical_ripple::enabled());
    setenv("TOP_HISTORICAL_RIPPLE_POOL","1",1);
    assert(top::historical_ripple::enabled());

    SystemTimeline timeline;
    // DX9 Play() primes _fCurTime to _fStep; the first dead slot emits.
    timeline.advance(2,.1f,1.f,.5f,-1.f,2,4,17u);
    assert(timeline.initialized && timeline.slots.size()==2);
    assert(timeline.slots[0].alive && !timeline.slots[1].alive);
    assert(timeline.slots[0].life>=.5f && timeline.slots[0].life<1.f);
    assert(timeline.slots[0].generation==0);

    // While slot zero lives, the shared accumulator lets slot one emit next.
    timeline.advance(2,.5f,1.f,.5f,-1.f,2,4,17u);
    assert(timeline.slots[0].alive && timeline.slots[1].alive);
    assert(timeline.slots[1].generation==0);
    assert(timeline.slots[0].frame==1); // at most one frame despite dt > key interval
    auto snapshot=timeline.snapshot(4);
    assert(snapshot.slots[0].alive && snapshot.slots[1].alive);
    assert(std::fabs(snapshot.slots[0].normalized-.25f)<1e-6f);

    // Stopping emission does not kill existing particles.
    timeline.advance(2,.1f,1.f,.5f,.7f,2,4,17u,false);
    assert(timeline.slots[0].alive && timeline.slots[1].alive);

    const float age=timeline.slots[0].age;
    timeline.advance(2,-1.f,1.f,.5f,.7f,2,4,17u,false);
    assert(std::fabs(timeline.slots[0].age-age)<1e-6f);
    unsetenv("TOP_HISTORICAL_RIPPLE_POOL");
}
