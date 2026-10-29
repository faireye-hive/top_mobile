#include "top/HistoricalShrink.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>

int main() {
    using namespace top::historical_shrink;
    unsetenv("TOP_HISTORICAL_SHRINK");
    assert(enabled());
    setenv("TOP_HISTORICAL_SHRINK", "1", 1);
    assert(enabled());

    // DX9 PointPointRange is a strict per-axis box, not a Euclidean radius.
    assert(insideCollisionBounds({{0.49f, 0.49f, 0.49f}}));
    assert(!insideCollisionBounds({{0.5f, 0.f, 0.f}}));
    assert(!insideCollisionBounds({{0.f, -0.5f, 0.f}}));

    const auto initial = at({{10.f, 10.f, 10.f}}, 2.f, 0.f, 123u, 0);
    const auto repeat = at({{10.f, 10.f, 10.f}}, 2.f, 0.f, 123u, 0);
    assert(initial.alive && repeat.alive);
    assert(initial.position == repeat.position);
    for (float coordinate : initial.position)
        assert(coordinate >= -5.f && coordinate <= 5.f);

    const auto moved = at({{10.f, 10.f, 10.f}}, 2.f, 1.f / 60.f, 123u, 0);
    const float length = std::sqrt(initial.position[0] * initial.position[0] +
                                   initial.position[1] * initial.position[1] +
                                   initial.position[2] * initial.position[2]);
    assert(moved.alive);
    for (unsigned axis = 0; axis < 3; ++axis) {
        const float towardCenter = length > 1e-8f ? -initial.position[axis] / length : 0.f;
        const float expected = initial.position[axis] + towardCenter * (2.f / 60.f);
        assert(std::abs(moved.position[axis] - expected) < 1e-5f);
    }

    // With zero travel, a point inside the DX9 collision box dies on the first
    // update; with no elapsed frame it remains alive just after creation.
    const auto stationary = at({{0.f, 0.f, 0.f}}, 0.f, 0.f, 123u, 0);
    assert(stationary.alive);
    const auto afterOneUpdate = at({{0.f, 0.f, 0.f}}, 0.f, 1.f / 60.f, 123u, 0);
    assert(!afterOneUpdate.alive);

    SystemTimeline timeline;
    timeline.advance(1,.04f,{{10.f,10.f,10.f}},2.f,1.f,.1f,2,4,123u);
    timeline.advance(1,.04f,{{10.f,10.f,10.f}},2.f,1.f,.1f,2,4,123u);
    assert(!timeline.sample(0,4).alive);
    timeline.advance(1,.04f,{{10.f,10.f,10.f}},2.f,1.f,.1f,2,4,123u);
    const auto born=timeline.sample(0,4);
    assert(born.alive && born.age==0.f && born.generation==0u);
    const auto bornPosition=born.position;
    timeline.advance(1,.3f,{{10.f,10.f,10.f}},2.f,1.f,.1f,2,4,123u);
    const auto advanced=timeline.sample(0,4);
    assert(advanced.alive && std::abs(advanced.age-.3f)<1e-6f);
    assert(std::abs(advanced.normalized-.25f)<1e-6f); // one frame; residual dt is dropped.
    const float initialLength=std::sqrt(bornPosition[0]*bornPosition[0]+
        bornPosition[1]*bornPosition[1]+bornPosition[2]*bornPosition[2]);
    for(unsigned axis=0;axis<3;++axis) {
        const float direction=initialLength>1e-8f?-bornPosition[axis]/initialLength:0.f;
        assert(std::abs(advanced.position[axis]-(bornPosition[axis]+direction*.6f))<1e-5f);
    }

    SystemTimeline recycle;
    recycle.advance(1,.05f,{{0.f,0.f,0.f}},0.f,1.f,.05f,2,4,55u);
    assert(recycle.sample(0,4).alive);
    recycle.advance(1,.01f,{{0.f,0.f,0.f}},0.f,1.f,.05f,2,4,55u);
    assert(!recycle.sample(0,4).alive);
    recycle.advance(1,.05f,{{0.f,0.f,0.f}},0.f,1.f,.05f,2,4,55u);
    const auto recycled=recycle.sample(0,4);
    assert(recycled.alive && recycled.generation==1u);

    const auto invalid = at({{1.f, 1.f, 1.f}}, 1.f,
                            std::numeric_limits<float>::quiet_NaN(), 1u, 0);
    assert(!invalid.alive);
    unsetenv("TOP_HISTORICAL_SHRINK");
}
