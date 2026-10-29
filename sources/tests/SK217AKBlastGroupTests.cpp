#include "top/HistoricalBlastGroupTimeline.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
using namespace top::historical_blast_group;
int main() {
    unsetenv("TOP_HISTORICAL_BLAST_GROUP_CYCLE");
    assert(enabled());
    setenv("TOP_HISTORICAL_BLAST_GROUP_CYCLE","1",1);
    assert(enabled());

    SystemTimeline loop;
    loop.advance(2,.5f,1.f,2,1,77,true,true);
    auto state=loop.snapshot();
    assert(state.slots.size()==2&&state.slots[0].alive&&state.slots[1].alive);
    assert(state.slots[0].normalized==.5f);
    loop.advance(2,.5f,1.f,2,1,77,true,true);
    state=loop.snapshot();
    // Both slots expire; DX9 calls _CreateBlast for the whole group.
    assert(!state.stopped&&state.slots[0].alive&&state.slots[1].alive);
    assert(state.slots[0].generation==1&&state.slots[1].generation==1);
    assert(state.slots[0].age==0.f&&state.slots[1].age==0.f);

    SystemTimeline noRestart;
    noRestart.advance(2,.5f,1.f,2,1,77,true,false);
    noRestart.advance(2,.5f,1.f,2,1,77,true,false);
    state=noRestart.snapshot();
    assert(state.stopped&&!state.slots[0].alive&&!state.slots[1].alive);

    SystemTimeline discrete;
    discrete.advance(1,.8f,1.f,2,1,77,true,true);
    state=discrete.snapshot();
    assert(state.slots[0].alive&&state.slots[0].normalized==.5f);
    discrete.advance(1,.5f,1.f,2,1,77,true,true);
    assert(discrete.snapshot().slots[0].generation==1);
    unsetenv("TOP_HISTORICAL_BLAST_GROUP_CYCLE");
}
