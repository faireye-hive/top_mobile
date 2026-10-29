#pragma once

// SK2.17-AM: per-FrameMove ROUND orbit and curve cursor.
// DX9 updates one shared orbit angle and lets each particle's GetCurFrame
// advance at most once per update, discarding frame-time overshoot.
#include "top/HistoricalModelTimeline.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace top { namespace historical_round_timeline {

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

struct Sample {
    bool alive=false;
    float normalized=0.f;
    float orbitAngle=0.f;
    std::array<float,3> position{{0.f,0.f,0.f}};
};

struct SystemTimeline {
    bool initialized=false;
    float angle=0.f;
    float angularSpeed=0.f;
    float life=0.f;
    unsigned frameCount=0;
    std::vector<historical_model_timeline::Timeline> slots;
    std::vector<float> slotAngles;

    void reset(int count,float authoredLife,unsigned authoredFrameCount,float speed) {
        initialized=true;
        angle=0.f;
        angularSpeed=speed;
        life=authoredLife;
        frameCount=authoredFrameCount;
        slots.assign(static_cast<std::size_t>(count),{});
        slotAngles.assign(static_cast<std::size_t>(count),0.f);
    }

    void advance(int count,float dt,float authoredLife,unsigned authoredFrameCount,float speed) {
        if(count<1||!std::isfinite(dt)||dt<0.f||!std::isfinite(authoredLife)||
           authoredLife<=0.f||authoredFrameCount<1||!std::isfinite(speed))return;
        if(!initialized||slots.size()!=static_cast<std::size_t>(count)||
           life!=authoredLife||frameCount!=authoredFrameCount||angularSpeed!=speed)
            reset(count,authoredLife,authoredFrameCount,speed);

        angle+=speed*dt;
        // _FrameMoveRound subtracts one full turn at most once per update.
        if(angle>=6.283185f)angle-=6.283185f;
        for(std::size_t index=0;index<slots.size();++index){
            auto& slot=slots[index];
            slot.advance(dt,authoredLife,authoredFrameCount);
            // At the cycle boundary DX9 resets the frame cursor then returns
            // without rewriting this particle's previous position/curve.
            if(!slot.holdingLastFrame)slotAngles[index]=angle;
        }
    }

    Sample sample(int slot,const std::array<float,3>& range)const {
        Sample result;
        if(!initialized||slot<0||static_cast<std::size_t>(slot)>=slots.size())return result;
        const auto index=static_cast<std::size_t>(slot);
        const auto& frame=slots[index];
        result.alive=true;
        result.normalized=frame.phase();
        result.orbitAngle=slotAngles[index];
        const float theta=6.2831853071795864769f*static_cast<float>(slot)/
                          static_cast<float>(slots.size())+result.orbitAngle;
        // D3DXMatrixRotationZ rotates (0, range.y, 0); the DX9 center adds
        // range/2. X/Y are mapped to GL x/z by EffectSystem at the boundary.
        result.position={{range[0]*.5f-range[1]*std::sin(theta),
                          range[1]*.5f+range[1]*std::cos(theta),
                          range[2]*.5f}};
        return result;
    }
};

}} // namespace top::historical_round_timeline
