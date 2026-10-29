#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace top { namespace historical_dummy {

struct V3 { float x=0.f, y=0.f, z=0.f; };
struct Slot {
    V3 position{};
    V3 direction{};
    float age=0.f;
    float life=0.f;
    bool alive=false;
};
struct Timeline {
    float lastAge=0.f;
    float sharedEmitterTime=0.f;
    bool initialized=false;
    std::vector<Slot> slots;
};

inline float unit(unsigned value) {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / 16777216.f;
}

// DX9 DUMMY uses a pool: each dead slot is recreated on the next update.
// The item endpoints are supplied in original DX9 XYZ order by the caller.
inline void advance(Timeline& timeline, float age, const V3& dummy1,
                    const V3& dummy2, int count, float life, float step, int randomDivisor,
                    float velocity, const V3& acceleration, unsigned seed) {
    if (!std::isfinite(age) || !std::isfinite(life) || life <= 0.f || count <= 0)
        return;
    const float dt = timeline.initialized ? std::max(0.f, age - timeline.lastAge) :
                                           std::max(0.f, age);
    timeline.initialized = true;
    timeline.lastAge = age;
    if (timeline.slots.size() != static_cast<std::size_t>(count))
        timeline.slots.resize(static_cast<std::size_t>(count));
    const V3 delta{dummy1.x-dummy2.x,dummy1.y-dummy2.y,dummy1.z-dummy2.z};
    const float distance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
    const V3 direction=distance>0.f?V3{delta.x/distance,delta.y/distance,delta.z/distance}:V3{};
    const float divisor=static_cast<float>(std::max(1,randomDivisor));
    for (std::size_t i=0;i<timeline.slots.size();++i) {
        Slot& slot=timeline.slots[i];
        const bool deadAtUpdateStart=!slot.alive;
        if (slot.alive) {
            slot.age += dt;
            if (slot.age >= slot.life) slot.alive=false;
            else {
                const float sign=unit(seed+static_cast<unsigned>(i*2u)+
                                      static_cast<unsigned>(age*60000.f)) < .5f ? -1.f : 1.f;
                slot.position.x += slot.direction.x*velocity*dt + acceleration.x*dt*sign;
                slot.position.y += slot.direction.y*velocity*dt + acceleration.y*dt*sign;
                slot.position.z += slot.direction.z*velocity*dt + acceleration.z*dt*sign;
            }
        }
        if (deadAtUpdateStart && dt > 0.f) {
            timeline.sharedEmitterTime+=dt;
            if(timeline.sharedEmitterTime<step) continue;
            timeline.sharedEmitterTime=0.f;
            const float rLife=unit(seed+static_cast<unsigned>(i*17u)+
                                   static_cast<unsigned>(age*100000.f));
            slot.life=life/divisor+(life-life/divisor)*rLife;
            // MPParticleSys.cpp calls Randf(dummyDistance, particleCount).
            // Preserve the authored argument order; the client RNG itself is
            // global and is therefore only approximated by this deterministic seed.
            const float rDistance=unit(seed+static_cast<unsigned>(i*31u)+
                                       static_cast<unsigned>(age*200000.f));
            const float travel=distance+(static_cast<float>(count)-distance)*rDistance;
            slot.position={dummy2.x+direction.x*travel,
                           dummy2.y+direction.y*travel,
                           dummy2.z+direction.z*travel};
            slot.direction=direction;
            slot.age=0.f;
            slot.alive=true;
        }
    }
}

}} // namespace top::historical_dummy
