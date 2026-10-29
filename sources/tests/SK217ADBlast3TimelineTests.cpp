#include "top/HistoricalBlast3Timeline.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace top::historical_blast3_timeline;
static bool close(float a,float b){return std::fabs(a-b)<1e-5f;}
int main(){
    unsetenv("TOP_HISTORICAL_BLAST3_TIMELINE");
    assert(enabled());
    setenv("TOP_HISTORICAL_BLAST3_TIMELINE","1",1);
    assert(enabled());
    const V3 direction{{0.f,0.f,0.f}},acceleration{{1.f,0.f,0.f}};
    SystemTimeline timeline;
    timeline.advance(4,.25f,2.f,4,2.f,direction,acceleration);
    auto sample=timeline.snapshot();
    assert(sample.slots.size()==4 && sample.slots[0].alive);
    assert(close(sample.slots[0].position[0],.5f));
    assert(close(sample.slots[0].position[1],.5f));
    assert(close(sample.slots[0].position[2],-.5f));
    assert(close(sample.slots[1].position[0],-.5f)); // D3DX row-vector rotation
    assert(close(sample.slots[1].position[1],.5f));
    assert(close(sample.slots[0].normalized,.125f));

    // DX9 moves with m_vOldPos first, then adds acceleration * dt.
    timeline.advance(4,.25f,2.f,4,2.f,direction,acceleration);
    sample=timeline.snapshot();
    assert(close(sample.slots[0].position[0],1.125f));
    assert(close(sample.slots[0].normalized,.25f));

    // GetCurFrame advances at most one frame and drops overshoot. A 1.25s
    // update on a 0.5s frame therefore remains alive at frame 1.
    SystemTimeline largeDelta;
    largeDelta.advance(1,1.25f,1.f,2,1.f,direction,{{0.f,0.f,0.f}});
    auto large=largeDelta.snapshot();
    assert(large.slots[0].alive && close(large.slots[0].normalized,.5f));
    largeDelta.advance(1,.5f,1.f,2,1.f,direction,{{0.f,0.f,0.f}});
    assert(!largeDelta.snapshot().slots[0].alive);

    unsetenv("TOP_HISTORICAL_BLAST3_TIMELINE");
    assert(enabled());
    std::cout<<"SK2.17-AD BLAST3 update timeline rules OK\n";
}
