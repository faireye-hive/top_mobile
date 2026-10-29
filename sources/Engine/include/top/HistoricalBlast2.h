#pragma once
// SK2.17-C: frame-stepped reconstruction of the unusual DX9
// CMPPartSys::_CreateBlast2 / _FrameMoveBlast2 controller.
// Original MPParticleSys.cpp: _vFontDir defaults to (0,1,0),
// the first frame advances at three times its normal frame rate, the
// velocity is tripled during that first frame, and frame one shifts X
// +angle[0].x, -angle[0].x, back to center across successive updates.
// This evaluator models a nominal 60 Hz clock; actual DX9 behaviour is
// frame-rate-dependent. Positions are local DX9 coordinates (Z-up).
#include <algorithm>
#include <array>
#include <cmath>

namespace top { namespace historical_blast2 {
using V3=std::array<float,3>;
struct Sample {
    bool alive=false;
    float age=0.f;
    float normalized=0.f;
    V3 position{{0.f,0.f,0.f}};
    int frame=0;
    int lateralState=0;
};
inline Sample at(float seconds,float life,int frames,float speed,
                 const V3& acceleration,float sideways,float hz=60.f) {
    Sample s;
    if(!std::isfinite(seconds) || !std::isfinite(life) ||
       !std::isfinite(speed) || !std::isfinite(sideways) ||
       !std::isfinite(hz) || seconds<0.f || life<=0.f ||
       frames<=0 || frames>256 || hz<1.f || hz>240.f) return s;
    for(float v:acceleration)if(!std::isfinite(v))return s;
    // _CreateBlast2 always initializes all slots together at _vPos,
    // with m_vOldPos = _vFontDir, not the serialized _vDir. Regular .par
    // uses default _vFontDir=(0,1,0). Font effects can override that
    // separately and are outside the scope of this controller.
    V3 velocity{{0.f,1.f,0.f}};
    float frameTime=life/static_cast<float>(frames);
    float frameClock=0.f;
    V3 anchor=s.position;
    float effectiveCursor=0.f;
    const float tick=1.f/hz;
    if(seconds>60.f)return {};
    // Fixed-step playback for repeatable random-access sample evaluation.
    // Stop as soon as GetCurFrame reaches the last frame; the original
    // controller calls _bPlay=false and returns instead of looping BLAST2.
    // No fractional update: a snapshot between fixed ticks must NOT
    // advance a second lateral state inside the same historical frame.
    const int completeTicks=static_cast<int>(std::floor(seconds*hz+0.00001f));
    for(int tickIndex=0;tickIndex<completeTicks; ++tickIndex) {
        const float dt=tick;
        frameClock+=dt;
        const float threshold=(s.frame==0 ? frameTime/3.f : frameTime);
        if(frameClock>=threshold) {
            ++s.frame;
            frameClock=0.f; // matches CMPParticle::GetCurFrame (no carry)
            if(s.frame>=frames)return {};
            if(s.frame==1)anchor=s.position;
        }
        if(s.frame==1) {
            if(s.lateralState==0) {
                s.position[0]=anchor[0]+sideways;
                s.lateralState=1;
            } else if(s.lateralState==1) {
                s.position[0]=anchor[0]-sideways;
                s.lateralState=2;
            } else if(s.lateralState==2) {
                s.position[0]=anchor[0];
                s.lateralState=3;
            }
            // wCurFrame==1 branches through goto _ret and skips motion,
            // acceleration, size and color updates in the original.
            continue;
        }
        effectiveCursor=float(s.frame) + frameClock/frameTime;
        const float vdt=speed*dt*(s.frame==0 ? 3.f : 1.f);
        for(int c=0;c<3;++c) {
            s.position[c]+=velocity[c]*vdt;
            velocity[c]+=acceleration[c]*dt;
        }
    }
    s.alive=true;
    s.age=seconds;
    s.normalized=std::clamp(effectiveCursor/static_cast<float>(frames),0.f,1.f);
    return s;
}
}} // namespace top::historical_blast2
