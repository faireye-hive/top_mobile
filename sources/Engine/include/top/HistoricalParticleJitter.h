#pragma once
// SK2.17-B: the historical _FrameMoveFire/_FrameMoveSnow uses the same
// randomly flipped acceleration vector for each frame, NOT a persistent
// 0.5 * acceleration * age^2 force. The DX9 implementation adds +/-
// _vAccel * dt directly to the per-frame displacement (NOT dt squared). This deterministic fixed-60Hz emulator
// deliberately does not claim to reproduce the DX9 rand() sequence or FPS.
#include "top/HistoricalParticlePool.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace top { namespace historical_particle_jitter {
using V3=std::array<float,3>;
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool frameAccelerationEnabled() { return true; }
inline V3 offset(const V3& acceleration,float age,std::uint32_t seed,
                 int slot,unsigned generation=0) {
    V3 displacement{{0.f,0.f,0.f}};
    if(!std::isfinite(age) || age<=0.f) return displacement;
    constexpr float dt=1.f/60.f;
    // Life spans in the original content are normally short. Prevent a
    // malformed emitter with days of age from doing unbounded per-frame work.
    const int ticks=std::min(1800,static_cast<int>(std::ceil(std::min(age,30.f)/dt)));
    const std::uint32_t key=seed ^ (0x85ebca6bu*std::uint32_t(slot+1)) ^
                            (0xc2b2ae35u*generation);
    for(int i=0;i<ticks;++i) {
        const float elapsed=dt*float(i);
        const float step=std::min(dt,age-elapsed);
        if(step<=0.f)break;
        const bool plus=historical_particle_random::randfMappingEnabled() ?
            historical_particle_random::deterministicRandModulo(
                2u,key,static_cast<unsigned>(i)) != 0u :
            historical_particle_pool::rand01(key ^
                (0x9e3779b9u*std::uint32_t(i+1))) < .5f;
        const float factor=(plus?1.f:-1.f)*step;
        for(int c=0;c<3;++c) displacement[c]+=acceleration[c]*factor;
    }
    return displacement;
}
}} // namespace top::historical_particle_jitter
