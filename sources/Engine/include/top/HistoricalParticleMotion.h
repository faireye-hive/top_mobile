#pragma once
// SK2.16-B: position evaluator for the historical CMPPartSys SNOW/FIRE/BLAST.
// Coordinate order here is D3D (X,Y horizontal, Z up). The caller converts
// to GL (X,Z horizontal, Y up) at the boundary.
//
// Original Randf(), random acceleration signs and
// emitter stepping are frame-dependent. Our seeded samples permit repeatable
// comparisons without pretending to reproduce an unknown DX9 rand() stream.
#include "top/HistoricalParticlePool.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace top { namespace historical_particle_motion {
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }
using V3 = std::array<float,3>;
struct Pose {
    V3 position{{0.f,0.f,0.f}}; // local DX9 displacement, includes spawn range
    V3 velocity{{0.f,0.f,0.f}};
};
inline float sample(std::uint32_t seed, unsigned channel) {
    return historical_particle_pool::rand01(seed ^ (0x9e3779b9u * (channel+1)));
}
inline float sign(std::uint32_t seed, unsigned channel) {
    return sample(seed,channel)<0.5f?-1.f:1.f;
}
inline V3 unit(V3 v) {
    const float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(n<=1e-8f || !std::isfinite(n))return {{0.f,0.f,0.f}};
    for(float& x:v)x/=n;
    return v;
}
// Authored range is Randf(range[i]) in the DX9 constructor, NOT centered
// around the origin. SNOW/FIRE use the same modulo/0.001 quantization as DX9;
// their seeded per-slot stream does not reproduce global MSVC rand ordering.
inline Pose at(int type, const V3& range, const V3& direction,
               const V3& acceleration, float speed, float age,
               std::uint32_t seed, int slot, unsigned generation=0,
               int divisor=2) {
    Pose p;
    if(type!=1 && type!=2 && type!=3)return p;
    if(!(std::isfinite(age) && std::isfinite(speed) && age>=0.f))return p;
    const std::uint32_t particleSeed=seed ^ (0x85ebca6bu*std::uint32_t(slot+1)) ^
                                   (0xc2b2ae35u*generation);
    const bool useDx9Randf=historical_particle_random::randfMappingEnabled();
    for(unsigned c=0;c<3;++c)
        p.position[c]=useDx9Randf ?
            historical_particle_random::deterministicRandf(range[c],particleSeed,c*4u) :
            range[c]*sample(particleSeed,c);
    if(type==2) {
        // _CreateFire normalizes _vDir; _FrameMoveFire translates by
        // normalized direction * _fVecl * dt, plus random acceleration.
        p.velocity=unit(direction);
        for(float& value:p.velocity)value*=speed;
    } else if(type==1) {
        // _CreateSnow samples [speed/_iRoadom,speed] per axis, with a sign
        // flip on horizontal axes. The divisor comes from the .par version.
        const float lower=1.f/static_cast<float>(divisor>0?divisor:2);
        for(unsigned c=0;c<3;++c) {
            const float magnitude=useDx9Randf ?
                historical_particle_random::deterministicRandfBetween(
                    lower*speed,speed,particleSeed,12u+c*4u) :
                speed*(lower+(1.f-lower)*sample(particleSeed,3+c));
            const float horizontalSign=c==2?1.f:useDx9Randf?
                (historical_particle_random::deterministicRandModulo(2u,particleSeed,24u+c)?1.f:-1.f):
                sign(particleSeed,6+c);
            p.velocity[c]=magnitude*direction[c]*horizontalSign;
        }
        // A zero vertical component makes _CreateSnow reject the particle.
        // The caller can decide whether to suppress the sample entirely.
    } else { // type 3 BLAST
        // _CreateBlast: Randf(speed), -Randf(speed), Randf(speed), then
        // direction components and random horizontal signs.
        for(unsigned c=0;c<3;++c) {
            const float magnitude=useDx9Randf ?
                historical_particle_random::deterministicRandf(
                    speed,particleSeed,16u+c*4u) : speed*sample(particleSeed,3+c);
            const float randomSign=useDx9Randf ?
                (historical_particle_random::deterministicRandModulo(
                    2u,particleSeed,32u+c)?1.f:-1.f) : sign(particleSeed,6+c);
            p.velocity[c]=magnitude*direction[c]*
                          (c==2?1.f:(c==1?-1.f:1.f)*randomSign);
        }
    }
    for(unsigned c=0;c<3;++c) {
        p.position[c]+=p.velocity[c]*age;
        if(type==3) {
            // _FrameMoveBlast integrates vOldPos += acceleration*dt.
            // Keep the continuous-time default approximation; the opt-in
            // HistoricalBlastMotion layer corrects its nominal-tick drift.
            p.position[c]+=0.5f*acceleration[c]*age*age;
        }
        // FIRE/SNOW choose a new random +/- acceleration EVERY FRAME in DX9.
        // Its expected displacement is zero, so adding a fixed quadratic
        // acceleration here would invent a persistent directional force.
    }
    return p;
}
}} // namespace top::historical_particle_motion
