#pragma once
// SK2.17-X: stateful replay of CMPPartSys RIPPLE slot births and frame life.
// The slot order/shared timer/life interval follow DX9; random values remain
// deterministic per slot because the original consumes the global rand().
#include "top/HistoricalParticlePool.h"

namespace top { namespace historical_ripple {
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

struct SystemTimeline {
    bool initialized=false;
    float emitterTime=0.f;
    std::vector<historical_particle_pool::TimelineSlot> slots;
    std::vector<unsigned> attempts;

    void reset(int count,float step) {
        initialized=true;
        // CMPPartSys::Play primes the shared timer to _fStep so the first
        // FrameMove can create a slot immediately.
        emitterTime=std::max(0.f,step);
        slots.assign(static_cast<std::size_t>(std::max(0,count)),{});
        attempts.assign(slots.size(),0u);
    }

    void advance(int count,float dt,float life,float step,float stopAt,
                 unsigned seed,
                 int divisor,int frameCount,bool canEmit=true) {
        if(count<1||count>100||!std::isfinite(dt)||dt<0.f||
           !std::isfinite(life)||life<=0.f||!std::isfinite(step)||step<0.f||
           !std::isfinite(stopAt)||divisor<1||frameCount<1||frameCount>256)return;
        if(!initialized||slots.size()!=static_cast<std::size_t>(count))reset(count,step);
        for(int index=0;index<count;++index){
            auto& slot=slots[static_cast<std::size_t>(index)];
            if(slot.alive){
                slot.age+=dt;
                slot.frameTime+=dt;
                const float perFrame=slot.life/static_cast<float>(frameCount);
                if(slot.frameTime>=perFrame){
                    ++slot.frame;
                    slot.frameTime=0.f; // DX9 advances once and drops excess.
                    if(slot.frame>=static_cast<unsigned>(frameCount))slot.alive=false;
                }
                continue; // a slot that just died waits until the next update
            }
            if(!canEmit)continue;
            emitterTime+=dt;
            if(step>0.f&&emitterTime<step)continue;
            emitterTime=0.f;
            const unsigned generation=attempts[static_cast<std::size_t>(index)]++;
            const float slotLife=historical_particle_pool::individualLife(
                life,seed,index,generation,divisor);
            slot={true,0.f,slotLife,0.f,0u,generation};
        }
    }

    historical_particle_pool::SystemSample snapshot(int frameCount) const {
        historical_particle_pool::SystemSample sample;
        sample.slots.resize(slots.size());
        for(std::size_t index=0;index<slots.size();++index){
            const auto& slot=slots[index];
            if(!slot.alive)continue;
            const float perFrame=slot.life/static_cast<float>(std::max(1,frameCount));
            const float within=perFrame>0.f?slot.frameTime/perFrame:0.f;
            const float normalized=std::clamp(
                (static_cast<float>(slot.frame)+within)/static_cast<float>(std::max(1,frameCount)),
                0.f,1.f);
            sample.slots[index]={true,slot.age,slot.life,normalized,slot.generation};
        }
        return sample;
    }
};
}} // namespace top::historical_ripple
