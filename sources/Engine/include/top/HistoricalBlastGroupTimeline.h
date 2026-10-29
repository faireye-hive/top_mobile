#pragma once
// SK2.17-AK: replay BLAST's group-wide restart after every slot has died.
// The original CMagicCtrl starts particle controllers with Play(0), which
// makes BLAST loop. Its deterministic random stream still differs from the
// DX9 process-wide rand() sequence.
#include "top/HistoricalParticlePool.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace top { namespace historical_blast_group {
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

struct SystemSample {
    std::vector<historical_particle_pool::Sample> slots;
    bool stopped=false;
};

struct SystemTimeline {
    struct Slot {
        bool alive=true;
        unsigned frame=0;
        float frameTime=0.f;
        float age=0.f;
        float life=0.f;
        unsigned generation=0;
    };
    bool initialized=false;
    bool stopped=false;
    int frameCount=0;
    int divisor=2;
    float authoredLife=0.f;
    std::uint32_t seed=0;
    std::vector<Slot> slots;

    void reset(int count,float life,int frames,int lifeDivisor,std::uint32_t randomSeed,
               unsigned generation=0) {
        initialized=true;stopped=false;frameCount=frames;divisor=lifeDivisor;
        authoredLife=life;seed=randomSeed;
        slots.assign(static_cast<std::size_t>(std::max(0,count)),Slot{});
        for(int i=0;i<count;++i) {
            Slot& slot=slots[static_cast<std::size_t>(i)];
            slot.generation=generation;
            slot.life=historical_particle_pool::individualLife(
                life,seed,i,generation,divisor);
        }
    }

    void advance(int count,float dt,float life,int frames,int lifeDivisor,
                 std::uint32_t randomSeed,bool loop,bool mayRestart) {
        if(count<1||count>4096||!std::isfinite(dt)||dt<0.f||
           !std::isfinite(life)||life<=0.f||frames<1||frames>512)return;
        if(!initialized||slots.size()!=static_cast<std::size_t>(count)||
           frameCount!=frames||authoredLife!=life||seed!=randomSeed||divisor!=lifeDivisor)
            reset(count,life,frames,lifeDivisor,randomSeed);
        if(stopped)return;
        unsigned dead=0;
        for(const Slot& slot:slots)if(!slot.alive)++dead;
        for(std::size_t i=0;i<slots.size();++i) {
            Slot& slot=slots[i];
            if(!slot.alive) {
                if(dead==slots.size()) {
                    if(loop&&mayRestart) {
                        const unsigned generation=slots.front().generation+1u;
                        reset(count,life,frames,lifeDivisor,randomSeed,generation);
                        // DX9's loop recreates the whole group inside the
                        // current slot update; later slots still receive dt.
                        for(std::size_t next=i+1;next<slots.size();++next)
                            updateSlot(slots[next],dt);
                    } else stopped=true;
                    return;
                }
                continue;
            }
            updateSlot(slot,dt);
            if(!slot.alive)++dead;
            if(dead==slots.size()) {
                if(loop&&mayRestart) {
                    const unsigned generation=slot.generation+1u;
                    reset(count,life,frames,lifeDivisor,randomSeed,generation);
                    // _FrameMoveBlast continues its for-loop after _CreateBlast.
                    for(std::size_t next=i+1;next<slots.size();++next)
                        updateSlot(slots[next],dt);
                } else stopped=true;
                return;
            }
        }
    }

    SystemSample snapshot() const {
        SystemSample out;out.stopped=stopped;out.slots.resize(slots.size());
        for(std::size_t i=0;i<slots.size();++i) {
            const Slot& slot=slots[i];if(!slot.alive)continue;
            const float perFrame=slot.life/static_cast<float>(std::max(1,frameCount));
            const float normalized=std::clamp((static_cast<float>(slot.frame)+
                (perFrame>0.f?slot.frameTime/perFrame:0.f))/
                static_cast<float>(std::max(1,frameCount)),0.f,1.f);
            out.slots[i]={true,slot.age,slot.life,normalized,slot.generation,{}};
        }
        return out;
    }

private:
    void updateSlot(Slot& slot,float dt) {
        if(!slot.alive)return;
        slot.frameTime+=dt;
        const float perFrame=slot.life/static_cast<float>(frameCount);
        if(slot.frameTime>=perFrame) {
            ++slot.frame;slot.frameTime=0.f;
            if(slot.frame>=static_cast<unsigned>(frameCount))slot.alive=false;
        }
        if(slot.alive)slot.age+=dt;
    }
};
}} // namespace top::historical_blast_group
