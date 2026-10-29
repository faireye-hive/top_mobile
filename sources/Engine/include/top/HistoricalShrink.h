#pragma once

#include "top/HistoricalParticlePool.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>

namespace top { namespace historical_shrink {

using V3 = std::array<float, 3>;

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool timelineEnabled() { return true; }

inline bool insideCollisionBounds(const V3& position, float halfExtent = 0.5f) {
    return std::isfinite(halfExtent) && halfExtent > 0.f &&
        std::isfinite(position[0]) && std::isfinite(position[1]) &&
        std::isfinite(position[2]) &&
        std::fabs(position[0]) < halfExtent &&
        std::fabs(position[1]) < halfExtent &&
        std::fabs(position[2]) < halfExtent;
}

struct Sample {
    bool alive = false;
    V3 position{{0.f, 0.f, 0.f}}; // DX9 displacement from the range midpoint.
    float age = 0.f;
    float normalized = 0.f;
    unsigned generation = 0;
};

// Deterministic nominal-60Hz replay of _CreateShrink/_FrameMoveShrink.
// Spawn coordinates use Randf(range), and motion uses the normalized vector
// toward range/2 multiplied by the authored velocity. The original calls
// PointPointRange after each move; no distance or renderer scale is introduced.
inline Sample at(const V3& range, float velocity, float age,
                 std::uint32_t seed, int slot, float step = 1.f / 60.f) {
    Sample result;
    if (slot < 0 || !std::isfinite(age) || age < 0.f ||
        !std::isfinite(velocity) || !std::isfinite(step) || step <= 0.f)
        return result;
    for (float extent : range)
        if (!std::isfinite(extent) || extent < 0.f) return result;

    const std::uint32_t key = seed ^ (0x85ebca6bu * std::uint32_t(slot + 1));
    for (unsigned axis = 0; axis < 3; ++axis) {
        const float unit = historical_particle_pool::rand01(
            key ^ (0x9e3779b9u * (axis + 1u)));
        result.position[axis] = (unit - 0.5f) * range[axis];
    }

    V3 direction{{-result.position[0], -result.position[1], -result.position[2]}};
    const float length = std::sqrt(direction[0] * direction[0] +
                                    direction[1] * direction[1] +
                                    direction[2] * direction[2]);
    if (length > 1e-8f && std::isfinite(length))
        for (float& component : direction) component /= length;
    else
        direction = {{0.f, 0.f, 0.f}};

    const double tickRatio = static_cast<double>(age) / static_cast<double>(step);
    if (!std::isfinite(tickRatio) ||
        tickRatio > static_cast<double>(std::numeric_limits<unsigned>::max()))
        return result;
    const double roundoff = 2.0 * std::numeric_limits<float>::epsilon() *
                            std::max(1.0, tickRatio);
    const unsigned ticks = static_cast<unsigned>(std::floor(tickRatio + roundoff));
    result.alive = true;
    for (unsigned tick = 0; tick < ticks; ++tick) {
        for (unsigned axis = 0; axis < 3; ++axis)
            result.position[axis] += direction[axis] * (velocity * step);
        if (insideCollisionBounds(result.position)) {
            result.alive = false;
            break;
        }
    }
    return result;
}

struct Slot {
    bool alive = false;
    V3 position{{0.f,0.f,0.f}};
    V3 direction{{0.f,0.f,0.f}};
    float age = 0.f;
    float life = 0.f;
    float frameTime = 0.f;
    float frameDuration = 0.f;
    unsigned frame = 0;
    unsigned generation = 0;
};

struct SystemTimeline {
    bool initialized = false;
    float emitterTime = 0.f;
    std::vector<Slot> slots;
    std::vector<unsigned> attempts;

    void reset(int count) {
        initialized = true;
        emitterTime = 0.f;
        slots.assign(static_cast<std::size_t>(std::max(0,count)),Slot{});
        attempts.assign(slots.size(),0u);
    }

    // Mirrors _FrameMoveShrink's slot order and shared _fCurTime. A dead slot
    // gets one creation attempt per update; excess timer is discarded, and a
    // newly created slot is not moved until the following update.
    void advance(int count, float dt, const V3& range, float velocity,
                 float authoredLife, float step, int divisor, int frameCount,
                 std::uint32_t seed, bool canEmit = true) {
        if (count < 1 || !std::isfinite(dt) || dt < 0.f ||
            !std::isfinite(velocity) || !std::isfinite(authoredLife) || authoredLife <= 0.f ||
            !std::isfinite(step) || !std::isfinite(emitterTime) || frameCount < 1)
            return;
        for (float extent : range)
            if (!std::isfinite(extent) || extent < 0.f) return;
        if (!initialized || slots.size() != static_cast<std::size_t>(count)) reset(count);

        for (int index=0; index<count; ++index) {
            Slot& slot=slots[static_cast<std::size_t>(index)];
            if (slot.alive) {
                slot.frameTime += dt;
                if (slot.frameTime >= slot.frameDuration) {
                    ++slot.frame;
                    slot.frameTime=0.f; // CMPParticle::GetCurFrame drops residual dt.
                    if (slot.frame >= static_cast<unsigned>(frameCount)) {
                        slot.alive=false;
                        continue;
                    }
                }
                slot.age += dt;
                for (unsigned axis=0;axis<3;++axis)
                    slot.position[axis] += slot.direction[axis]*(velocity*dt);
                if (insideCollisionBounds(slot.position)) slot.alive=false;
                continue;
            }

            if (!canEmit) continue;
            emitterTime += dt;
            if (step > 0.f && emitterTime < step) continue;
            emitterTime=0.f;

            const unsigned generation=attempts[static_cast<std::size_t>(index)]++;
            const std::uint32_t key=seed ^ (0x85ebca6bu*std::uint32_t(index+1)) ^
                                    (0xc2b2ae35u*generation);
            V3 position{};
            for (unsigned axis=0;axis<3;++axis) {
                const float random=historical_particle_pool::rand01(
                    key ^ (0x9e3779b9u*(axis+1u)));
                position[axis]=(random-.5f)*range[axis];
            }
            V3 direction{{-position[0],-position[1],-position[2]}};
            const float length=std::sqrt(direction[0]*direction[0]+
                                         direction[1]*direction[1]+
                                         direction[2]*direction[2]);
            if (length>1e-8f && std::isfinite(length))
                for(float& component:direction) component/=length;
            else direction={{0.f,0.f,0.f}};

            const float particleLife=historical_particle_pool::individualLife(
                authoredLife,seed,index,generation,divisor);
            const float frameDuration=particleLife/static_cast<float>(frameCount);
            slot={true,position,direction,0.f,particleLife,0.f,frameDuration,0u,generation};
        }
    }

    Sample sample(std::size_t index, int frameCount) const {
        Sample result;
        if(index>=slots.size()) return result;
        const Slot& slot=slots[index];
        if(!slot.alive) return result;
        const float within=slot.frameDuration>0.f?slot.frameTime/slot.frameDuration:0.f;
        result.alive=true;
        result.position=slot.position;
        result.age=slot.age;
        result.normalized=std::clamp(
            (static_cast<float>(slot.frame)+within)/static_cast<float>(std::max(1,frameCount)),
            0.f,1.f);
        result.generation=slot.generation;
        return result;
    }
};

}} // namespace top::historical_shrink
