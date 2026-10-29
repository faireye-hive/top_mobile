#include "top/HistoricalModelTimeline.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

int main() {
    using top::historical_model_timeline::Timeline;
    using top::historical_model_timeline::controllerDelta;
    unsetenv("TOP_HISTORICAL_MODEL_FRAME_STEP");
    assert(top::historical_model_timeline::enabled());
    assert(controllerDelta(0.f,.1f,.15f)==0.f);
    assert(controllerDelta(.1f,.1f,.15f)==0.f);
    assert(std::fabs(controllerDelta(.2f,.1f,.15f)-.1f)<1e-6f);
    assert(std::fabs(controllerDelta(0.f,.1f,0.f)-.1f)<1e-6f);
    setenv("TOP_HISTORICAL_MODEL_FRAME_STEP","1",1);
    assert(top::historical_model_timeline::enabled());

    Timeline timeline;
    timeline.advance(.24f,1.f,4);
    assert(timeline.initialized && timeline.frame==0);
    assert(std::fabs(timeline.phase()-.24f)<1e-6f);
    timeline.advance(.02f,1.f,4);
    assert(timeline.frame==1 && timeline.frameTime==0.f);
    assert(std::fabs(timeline.phase()-.25f)<1e-6f);

    // GetCurFrame advances once and discards the remaining .55 seconds.
    timeline.advance(.8f,1.f,4);
    assert(timeline.frame==2 && timeline.frameTime==0.f);
    assert(std::fabs(timeline.phase()-.5f)<1e-6f);
    timeline.advance(.25f,1.f,4);
    assert(timeline.frame==3);
    assert(std::fabs(timeline.phase()-.75f)<1e-6f);

    // _FrameMoveModel resets through _CreateModel and returns without
    // rewriting the last size/color; keep that last phase for this update.
    timeline.advance(.25f,1.f,4);
    assert(timeline.frame==0 && timeline.holdingLastFrame);
    assert(std::fabs(timeline.phase()-.75f)<1e-6f);
    timeline.advance(.1f,1.f,4);
    assert(!timeline.holdingLastFrame && timeline.frame==0);
    assert(std::fabs(timeline.phase()-.1f)<1e-6f);

    const float before=timeline.phase();
    timeline.advance(-1.f,1.f,4);
    assert(std::fabs(timeline.phase()-before)<1e-6f);
    unsetenv("TOP_HISTORICAL_MODEL_FRAME_STEP");
}
