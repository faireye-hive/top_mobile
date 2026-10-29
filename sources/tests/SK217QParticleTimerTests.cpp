#include "top/HistoricalParticlePool.h"

#include <cmath>
#include <cassert>

int main() {
    using namespace top::historical_particle_pool;
    SystemTimeline fire;

    // DX9 calls _CreateFire once per dead slot. The shared timer accumulates
    // dt at each call, emits at the second slot, then resets and drops overshoot.
    fire.advance(2, 2, .016f, 1.f, .025f, -1.f, 7u, 1, 2, true);
    auto sample = fire.snapshot(2);
    assert(fire.initialized && sample.slots.size() == 2);
    assert(!sample.slots[0].alive && sample.slots[1].alive);
    assert(sample.slots[1].age == 0.f && sample.slots[1].generation == 0u);

    fire.advance(2, 2, .016f, 1.f, .025f, -1.f, 7u, 1, 2, true);
    sample = fire.snapshot(2);
    assert(!sample.slots[0].alive && sample.slots[1].alive);
    fire.advance(2, 2, .004f, 1.f, .025f, -1.f, 7u, 1, 2, true);
    sample = fire.snapshot(2);
    assert(!sample.slots[0].alive); // accumulated .020, below _fStep
    fire.advance(2, 2, .005f, 1.f, .025f, -1.f, 7u, 1, 2, true);
    sample = fire.snapshot(2);
    assert(sample.slots[0].alive && sample.slots[1].alive);
    assert(sample.slots[0].age == 0.f && sample.slots[0].generation == 0u);

    // A just-dead slot is not recycled until a later FrameMove call.
    SystemTimeline life;
    life.advance(2, 1, .02f, .06f, .02f, -1.f, 9u, 1, 3, true);
    assert(life.snapshot(3).slots[0].alive);
    life.advance(2, 1, .02f, .06f, .02f, -1.f, 9u, 1, 3, true);
    life.advance(2, 1, .02f, .06f, .02f, -1.f, 9u, 1, 3, true);
    life.advance(2, 1, .02f, .06f, .02f, -1.f, 9u, 1, 3, true);
    assert(!life.snapshot(3).slots[0].alive);
    life.advance(2, 1, .02f, .06f, .02f, -1.f, 9u, 1, 3, true);
    assert(life.snapshot(3).slots[0].alive);
    assert(life.snapshot(3).slots[0].generation == 1u);

    // Stop suppresses creation while preserving updates to already-live slots.
    SystemTimeline stopped;
    stopped.advance(2, 1, .02f, 1.f, .01f, .01f, 3u, 1, 4, true, false);
    assert(!stopped.snapshot(4).slots[0].alive);

    // Snow resets its shared timer even when _CreateSnow rejects zero Z motion.
    SystemTimeline snow;
    snow.advance(1, 1, .02f, 1.f, .02f, -1.f, 3u, 1, 4, false);
    assert(!snow.snapshot(4).slots[0].alive);
    snow.advance(1, 1, .01f, 1.f, .02f, -1.f, 3u, 1, 4, true);
    assert(!snow.snapshot(4).slots[0].alive);
    snow.advance(1, 1, .01f, 1.f, .02f, -1.f, 3u, 1, 4, true);
    assert(snow.snapshot(4).slots[0].alive);

    // SK2.17-U: opt-in random acceleration is applied once per live update,
    // uses that update's dt, and is reproducible for the same slot/generation.
    const std::array<float,3> acceleration{{2.f,0.f,-4.f}};
    SystemTimeline jitterA, jitterB;
    jitterA.advance(2,1,.1f,2.f,.01f,-1.f,55u,1,4,true,true,acceleration,true);
    jitterB.advance(2,1,.1f,2.f,.01f,-1.f,55u,1,4,true,true,acceleration,true);
    auto jitterSampleA=jitterA.snapshot(4);
    auto jitterSampleB=jitterB.snapshot(4);
    assert(jitterSampleA.slots[0].alive && jitterSampleA.slots[0].randomAccelerationOffset[0]==0.f);
    jitterA.advance(2,1,.125f,2.f,.01f,-1.f,55u,1,4,true,true,acceleration,true);
    jitterB.advance(2,1,.125f,2.f,.01f,-1.f,55u,1,4,true,true,acceleration,true);
    jitterSampleA=jitterA.snapshot(4);
    jitterSampleB=jitterB.snapshot(4);
    assert(jitterSampleA.slots[0].randomAccelerationOffset==jitterSampleB.slots[0].randomAccelerationOffset);
    assert(std::fabs(std::fabs(jitterSampleA.slots[0].randomAccelerationOffset[0])-.25f)<1e-6f);
    assert(std::fabs(std::fabs(jitterSampleA.slots[0].randomAccelerationOffset[2])-.5f)<1e-6f);
}
