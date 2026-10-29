#pragma once
// SK2.17-D/E: RANGE2 collision and recycled-slot replay from DX9.
// DX9 samples terrain after advancing in D3D X/Y/Z-up coordinates. The GL
// adapter supplies the equivalent terrain height sampler at the boundary.
#include "top/HistoricalParticleExtended.h"
#include "top/HistoricalParticleRandom.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <vector>

namespace top { namespace historical_range2_collision {
using V3=std::array<float,3>;
using GroundHeight=std::function<float(int,int)>;
inline float randomRange(float range,std::uint32_t seed,int slot,int axis,
                         unsigned lane) {
    return historical_particle_random::randfMappingEnabled() ?
        historical_particle_random::deterministicRandf(range,
            seed^(0x85ebca6bu*static_cast<std::uint32_t>(slot+1)),lane) :
        range*historical_particle_extended::random01(seed,slot,axis);
}
struct Sample {
    bool alive=false;
    bool hit=false;
    float age=0.f;
    float hitAge=0.f;
    V3 position{{0.f,0.f,0.f}}; // absolute DX9 world X/Y/Z-up
};
struct SlotSample {
    bool alive=false;
    unsigned generation=0;
    float age=0.f;
    V3 position{{0.f,0.f,0.f}};
};
struct HitEvent {
    int slot=0;
    unsigned generation=0;
    float age=0.f; // time since collision at the requested sample
    V3 position{{0.f,0.f,0.f}};
};
struct SystemSample {
    std::vector<SlotSample> slots;
    std::vector<HitEvent> hits;
};
inline constexpr bool enabled() { return true; }
inline constexpr bool actualDtEnabled() { return true; }
inline Sample at(float seconds,int slot,int count,float controllerLife,
                 float step,float speed,const V3& base,const V3& range,
                 const V3& direction,std::uint32_t seed,
                 const GroundHeight& groundHeight={},bool hasMap=false,
                 float hz=60.f) {
    Sample result;
    if(!std::isfinite(seconds) || !std::isfinite(controllerLife) ||
       !std::isfinite(step) || !std::isfinite(speed) || !std::isfinite(hz) ||
       seconds<0.f || controllerLife<=0.f || slot<0 || slot>=count ||
       count<=0 || count>100 || hz<1.f || hz>240.f || seconds>60.f) return result;
    for(float value:base)if(!std::isfinite(value))return result;
    for(float value:range)if(!std::isfinite(value))return result;
    for(float value:direction)if(!std::isfinite(value))return result;
    const float interval=std::max(step,1.f/hz);
    const float birth=interval*static_cast<float>(slot+1);
    if(birth>=controllerLife || seconds<birth)return result;
    const auto randomPosition=[&](int axis) {
        return randomRange(range[axis],seed,slot,axis,
                           static_cast<unsigned>(axis)*4u);
    };
    V3 pos{{base[0]+randomPosition(0),base[1]+randomPosition(1),base[2]+range[2]}};
    const float norm=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+
                               direction[2]*direction[2]);
    const float tick=1.f/hz;
    const int completeTicks=static_cast<int>(std::floor((seconds-birth)*hz+0.00001f));
    int collidedAt=-1;
    float collisionHeight=0.f;
    for(int n=1;n<=completeTicks;++n) {
        if(norm>1e-8f)for(int axis=0;axis<3;++axis)
            pos[axis]+=direction[axis]*(speed*tick/norm);
        const float terrain=hasMap && groundHeight ?
            groundHeight(static_cast<int>(pos[0]),static_cast<int>(pos[1])) : 0.f;
        if(pos[2]<terrain || pos[2]>50.f) {
            collidedAt=n;
            collisionHeight=terrain;
            break;
        }
    }
    if(collidedAt>=0) {
        result.hit=true;
        result.hitAge=std::max(0.f,seconds-(birth+static_cast<float>(collidedAt)*tick));
        // The mapped DX9 branch clamps to GetGridHeight; its mapless branch
        // only kills/sends the current position and leaves the overshoot.
        if(hasMap)pos[2]=collisionHeight;
    } else result.alive=true;
    result.position=pos;
    result.age=seconds-birth;
    return result;
}

// Replays the shared RANGE2 pool in source order. DX9 calls _CreateRange2
// once for each dead slot, and each call advances the same emitter timer.
// `reportedGeneration[slot]` suppresses impact events already delivered by
// the caller while keeping the replay stateless and deterministic.
inline SystemSample simulate(float seconds,int count,float controllerLife,
                             float step,float speed,const V3& base,const V3& range,
                             const V3& direction,std::uint32_t seed,
                             const GroundHeight& groundHeight={},bool hasMap=false,
                             const std::vector<unsigned>& reportedGeneration={},
                             float hz=60.f) {
    SystemSample result;
    if(!std::isfinite(seconds) || !std::isfinite(controllerLife) ||
       !std::isfinite(step) || !std::isfinite(speed) || !std::isfinite(hz) ||
       seconds<0.f || seconds>60.f || controllerLife<=0.f || count<=0 ||
       count>100 || hz<1.f || hz>240.f) return result;
    for(float value:base)if(!std::isfinite(value))return result;
    for(float value:range)if(!std::isfinite(value))return result;
    for(float value:direction)if(!std::isfinite(value))return result;

    struct State {
        SlotSample sample;
        float bornAt=0.f;
    };
    std::vector<State> state(static_cast<std::size_t>(count));
    result.slots.resize(static_cast<std::size_t>(count));
    const float tick=1.f/hz;
    const int ticks=static_cast<int>(std::floor(seconds*hz+0.00001f));
    const float norm=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+
                               direction[2]*direction[2]);
    float emitterTime=0.f;
    bool stopped=false;
    for(int frame=1;frame<=ticks;++frame) {
        const float now=static_cast<float>(frame)*tick;
        if(now>controllerLife)stopped=true;
        for(int slot=0;slot<count;++slot) {
            State& particle=state[static_cast<std::size_t>(slot)];
            if(particle.sample.alive) {
                particle.sample.age=now-particle.bornAt;
                if(norm>1e-8f)for(int axis=0;axis<3;++axis)
                    particle.sample.position[axis]+=direction[axis]*(speed*tick/norm);
                const float terrain=hasMap && groundHeight ?
                    groundHeight(static_cast<int>(particle.sample.position[0]),
                                 static_cast<int>(particle.sample.position[1])) : 0.f;
                if(particle.sample.position[2]<terrain || particle.sample.position[2]>50.f) {
                    particle.sample.alive=false;
                    if(hasMap)particle.sample.position[2]=terrain;
                    const unsigned reported=static_cast<std::size_t>(slot)<reportedGeneration.size() ?
                        reportedGeneration[static_cast<std::size_t>(slot)] : 0u;
                    if(particle.sample.generation>reported) {
                        result.hits.push_back({slot,particle.sample.generation,
                            std::max(0.f,seconds-now),particle.sample.position});
                    }
                    continue;
                }
            } else if(!stopped) {
                // This timer is intentionally shared by the whole pool.
                // A dead slot calls _CreateRange2 and contributes one dt.
                emitterTime+=tick;
                if(emitterTime>=step) {
                    emitterTime=0.f;
                    const unsigned generation=++particle.sample.generation;
                    const std::uint32_t generationSeed=seed+
                        (generation-1u)*0x9e3779b9u;
                    particle.sample.position={{
                        base[0]+range[0]*historical_particle_extended::random01(generationSeed,slot,0),
                        base[1]+range[1]*historical_particle_extended::random01(generationSeed,slot,1),
                        base[2]+range[2]}};
                    particle.sample.alive=true;
                    particle.sample.age=0.f;
                    particle.bornAt=now;
                }
            }
        }
    }
    for(std::size_t slot=0;slot<state.size();++slot) {
        if(state[slot].sample.alive)
            state[slot].sample.age=std::max(0.f,static_cast<float>(ticks)*tick-state[slot].bornAt);
        result.slots[slot]=state[slot].sample;
    }
    return result;
}

// Stateful replay for the original _FrameMoveRange2 call order. The caller
// supplies one active frame delta at a time and the terrain sampler at the
// update boundary; no nominal-FPS reconstruction is used in this path.
struct SystemTimeline {
    bool initialized=false;
    bool stopped=false;
    bool finished=false;
    float controllerAge=0.f;
    float emitterTime=0.f;
    std::vector<SlotSample> slots;

    void reset(int count) {
        initialized=true;
        stopped=false;
        finished=false;
        controllerAge=0.f;
        emitterTime=0.f;
        slots.assign(static_cast<std::size_t>(std::max(0,count)),SlotSample{});
    }

    SystemSample advance(float dt,int count,float controllerLife,float step,float speed,
                         const V3& base,const V3& range,const V3& direction,
                         std::uint32_t seed,const GroundHeight& groundHeight={},
                         bool hasMap=false) {
        SystemSample result;
        if(!std::isfinite(dt)||dt<0.f||!std::isfinite(controllerLife)||
           controllerLife<=0.f||!std::isfinite(step)||!std::isfinite(speed)||
           count<=0||count>100||finished)return result;
        if(!initialized||slots.size()!=static_cast<std::size_t>(count))reset(count);
        for(float value:base)if(!std::isfinite(value))return result;
        for(float value:range)if(!std::isfinite(value))return result;
        for(float value:direction)if(!std::isfinite(value))return result;

        controllerAge+=dt;
        if(controllerAge>controllerLife)stopped=true;
        const float norm=std::sqrt(direction[0]*direction[0]+
            direction[1]*direction[1]+direction[2]*direction[2]);
        result.slots.resize(slots.size());
        bool anyAlive=false;
        for(std::size_t index=0;index<slots.size();++index) {
            SlotSample& particle=slots[index];
            if(particle.alive) {
                particle.age+=dt;
                if(norm>1e-8f)for(int axis=0;axis<3;++axis)
                    particle.position[axis]+=direction[axis]*(speed*dt/norm);
                const float terrain=hasMap&&groundHeight?
                    groundHeight(static_cast<int>(particle.position[0]),
                                 static_cast<int>(particle.position[1])):0.f;
                if(particle.position[2]<terrain||particle.position[2]>50.f) {
                    particle.alive=false;
                    if(hasMap)particle.position[2]=terrain;
                    result.hits.push_back({static_cast<int>(index),particle.generation,
                                           0.f,particle.position});
                    result.slots[index]=particle;
                    continue; // DX9 does not recreate a slot in its hit update.
                }
            } else if(!stopped) {
                // _CreateRange2 adds this frame's dt once for every dead slot,
                // shares the accumulator, and resets it instead of retaining
                // interval overshoot.
                emitterTime+=dt;
                if(emitterTime>=step) {
                    emitterTime=0.f;
                    ++particle.generation;
                    const std::uint32_t generationSeed=seed+
                        (particle.generation-1u)*0x9e3779b9u;
                    particle.position={{
                        base[0]+randomRange(range[0],generationSeed,
                            static_cast<int>(index),0,0u),
                        base[1]+randomRange(range[1],generationSeed,
                            static_cast<int>(index),1,4u),
                        base[2]+range[2]}};
                    particle.alive=true;
                    particle.age=0.f;
                }
            }
            anyAlive=anyAlive||particle.alive;
            result.slots[index]=particle;
        }
        if(stopped&&!anyAlive)finished=true;
        return result;
    }

    SystemSample snapshot() const {
        SystemSample result;
        result.slots=slots;
        return result;
    }
};
}} // namespace top::historical_range2_collision
