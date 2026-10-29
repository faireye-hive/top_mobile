#pragma once
// SK2.17-W: discrete frame clock for the single MODEL particle.
// DX9 CMPParticle::GetCurFrame advances at most once per FrameMove and drops
// the remaining elapsed time when a frame boundary is crossed.
#include <cmath>
#include <cstdlib>

namespace top { namespace historical_model_timeline {
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }
inline float controllerDelta(float previousAge,float dt,float delay) {
    if(!std::isfinite(previousAge)||!std::isfinite(dt)||dt<0.f||
       !std::isfinite(delay))return 0.f;
    // DX9 checks IsDelay before FrameUpdate; Render advances the delay clock
    // later in the same frame. Do not consume the crossing update partially.
    return previousAge>=delay?dt:0.f;
}
struct Timeline {
    bool initialized=false;
    bool holdingLastFrame=false;
    unsigned frame=0;
    unsigned frameCount=0;
    float age=0.f;
    float life=0.f;
    float frameTime=0.f;
    float lastPhase=0.f;

    void reset(float authoredLife,unsigned authoredFrameCount) {
        initialized=true;
        holdingLastFrame=false;
        frame=0;
        frameCount=authoredFrameCount;
        age=0.f;
        life=authoredLife;
        frameTime=0.f;
        lastPhase=0.f;
    }
    void advance(float dt,float authoredLife,unsigned authoredFrameCount) {
        if(!std::isfinite(dt)||dt<0.f||!std::isfinite(authoredLife)||
           authoredLife<=0.f||authoredFrameCount<1)return;
        if(!initialized||life!=authoredLife||frameCount!=authoredFrameCount)
            reset(authoredLife,authoredFrameCount);
        // The prior update reached the last frame and DX9's _CreateModel
        // reset the cursor without changing the already-rendered curve values.
        holdingLastFrame=false;
        age+=dt;
        frameTime+=dt;
        const float perFrame=life/static_cast<float>(frameCount);
        if(frameTime>=perFrame){
            ++frame;
            frameTime=0.f; // GetCurFrame discards overshoot and advances once.
            if(frame>=frameCount){
                frame=0;
                holdingLastFrame=true;
                return;
            }
        }
        lastPhase=phase();
    }
    float phase() const {
        if(holdingLastFrame)return lastPhase;
        if(!initialized||frameCount==0||life<=0.f)return 0.f;
        const float perFrame=life/static_cast<float>(frameCount);
        const float within=perFrame>0.f?frameTime/perFrame:0.f;
        return (static_cast<float>(frame)+within)/static_cast<float>(frameCount);
    }
};
}} // namespace top::historical_model_timeline
