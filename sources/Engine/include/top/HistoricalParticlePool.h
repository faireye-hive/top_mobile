#pragma once
// SK2.16-A: deterministic snapshot evaluation of CMPPartSys pool births/lifetimes.
// The original SNOW/FIRE create one particle at a time and recycle dead slots,
// while BLAST spawns a whole group and (when Play(0) loops) restarts that group.
// DX9 uses rand(), frame-dependent emit timing and random acceleration; this
// evaluator reconstructs the lifecycle, NOT an exact RNG or movement replay.
// Used by default; visual comparison against the D3D client is still pending.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>
#include "top/HistoricalParticleRandom.h"

namespace top { namespace historical_particle_pool {
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool sharedEmitterTimerEnabled() { return true; }
inline float rand01(std::uint32_t k) {
    k ^= k >> 16; k *= 0x7feb352du; k ^= k >> 15;
    k *= 0x846ca68bu; k ^= k >> 16;
    return static_cast<float>(k & 0x00ffffffu) / 16777216.f;
}
struct Sample {
    bool alive = false;
    float age = 0.f;
    float life = 0.f;
    float normalized = 0.f;
    unsigned generation = 0;
    std::array<float,3> randomAccelerationOffset{{0.f,0.f,0.f}};
};
struct SystemSample {
    std::vector<Sample> slots;
};
struct TimelineSlot {
    bool alive = false;
    float age = 0.f;
    float life = 0.f;
    float frameTime = 0.f;
    unsigned frame = 0;
    unsigned generation = 0;
    std::array<float,3> randomAccelerationOffset{{0.f,0.f,0.f}};
    unsigned randomAccelerationStep = 0;
};
inline float individualLife(float life, std::uint32_t seed, int slot,
                            unsigned generation, int divisor);
// Stateful replay driven by the actual EffectSystem::step(dt) calls. It keeps
// CMPPartSys's one shared _fCurTime accumulator and visits slots in index order.
// Random values remain deterministic per slot because the DX9 rand() stream is
// global and is not available to the GL renderer.
struct SystemTimeline {
    bool initialized = false;
    float emitterTime = 0.f;
    std::vector<TimelineSlot> slots;
    std::vector<unsigned> attempts;

    void reset(int count) {
        initialized = true;
        emitterTime = 0.f;
        slots.assign(static_cast<std::size_t>(std::max(0,count)), TimelineSlot{});
        attempts.assign(slots.size(), 0u);
    }

    void advance(int type, int count, float dt, float authoredLife, float step,
                 float stopAt, std::uint32_t seed, int divisor, int frameCount,
                 bool snowCanSpawn, bool canEmit = true,
                 const std::array<float,3>& acceleration = {{0.f,0.f,0.f}},
                 bool randomAcceleration = false) {
        if ((type != 1 && type != 2) || count < 1 ||
            !std::isfinite(dt) || dt < 0.f ||
            !std::isfinite(authoredLife) || authoredLife <= 0.f ||
            !std::isfinite(step) || !std::isfinite(stopAt) || frameCount < 1)
            return;
        if (!initialized || slots.size() != static_cast<std::size_t>(count)) reset(count);
        for (int index = 0; index < count; ++index) {
            TimelineSlot& slot = slots[static_cast<std::size_t>(index)];
            if (slot.alive) {
                slot.age += dt;
                slot.frameTime += dt;
                const float perFrame = slot.life / static_cast<float>(frameCount);
                if (slot.frameTime >= perFrame) {
                    ++slot.frame;
                    slot.frameTime = 0.f;
                    if (slot.frame >= static_cast<unsigned>(frameCount)) slot.alive = false;
                }
                if (slot.alive && randomAcceleration &&
                    (type == 1 || type == 2)) {
                    const std::uint32_t key = seed ^
                        (0x85ebca6bu * static_cast<std::uint32_t>(index + 1)) ^
                        (0xc2b2ae35u * slot.generation) ^
                        (0x9e3779b9u * (slot.randomAccelerationStep + 1u));
                    const float sign = historical_particle_random::randfMappingEnabled() ?
                        (historical_particle_random::deterministicRandModulo(2u,key) == 0u ? -1.f : 1.f) :
                        (rand01(key) < 0.5f ? -1.f : 1.f);
                    for (std::size_t axis = 0; axis < acceleration.size(); ++axis)
                        slot.randomAccelerationOffset[axis] += acceleration[axis] * dt * sign;
                    ++slot.randomAccelerationStep;
                }
                continue; // DX9 does not recycle a just-dead slot in this update.
            }
            if (!canEmit) continue;
            emitterTime += dt;
            if (step > 0.f && emitterTime < step) continue;
            emitterTime = 0.f; // DX9 resets; it does not retain interval overshoot.
            const unsigned generation = attempts[static_cast<std::size_t>(index)]++;
            if (type == 1 && !snowCanSpawn) continue; // _CreateSnow may reject zero Z velocity.
            const float life = historical_particle_random::randfMappingEnabled() ?
                historical_particle_random::snowFireLife(authoredLife,seed,index,generation,divisor) :
                individualLife(authoredLife,seed,index,generation,divisor);
            slot = {true, 0.f, life, 0.f, 0u, generation};
        }
    }

    SystemSample snapshot(int frameCount) const {
        SystemSample sample;
        sample.slots.resize(slots.size());
        for (std::size_t index = 0; index < slots.size(); ++index) {
            const TimelineSlot& slot = slots[index];
            if (!slot.alive) continue;
            const float perFrame = slot.life / static_cast<float>(std::max(1,frameCount));
            const float withinFrame = perFrame > 0.f ? slot.frameTime / perFrame : 0.f;
            const float normalized = std::clamp(
                (static_cast<float>(slot.frame) + withinFrame) /
                    static_cast<float>(std::max(1,frameCount)), 0.f, 1.f);
            sample.slots[index] = {true, slot.age, slot.life, normalized,
                                   slot.generation, slot.randomAccelerationOffset};
        }
        return sample;
    }
};
inline float individualLife(float life, std::uint32_t seed, int slot,
                            unsigned generation, int divisor = 2) {
    // DX9 chooses Randf(life / _iRoadom, life). In older .par versions
    // _iRoadom is type-dependent, and v13+ records it explicitly.
    if (historical_particle_random::randfMappingEnabled())
        return historical_particle_random::snowFireLife(
            life,seed,slot,generation,divisor);
    const float lower = 1.f / static_cast<float>(divisor > 0 ? divisor : 2);
    return life * (lower + (1.f - lower) * rand01(seed ^ (0x9e3779b9u *
                static_cast<std::uint32_t>(slot + 1)) ^ (generation * 0x85ebca6bu)));
}
// stopAt is measured relative to the end of _fDelayTime, and -1 means
// no explicit emission stop. Once stopped, existing particles finish their life.
inline Sample at(int type, int slot, int count, float seconds, float authoredLife,
                 float step, float stopAt, std::uint32_t seed,
                 int divisor = 2) {
    Sample empty;
    if (slot < 0 || count < 1 || slot >= count ||
        !(std::isfinite(seconds) && std::isfinite(authoredLife) &&
          std::isfinite(step) && std::isfinite(stopAt)) ||
        seconds < 0.f || authoredLife <= 0.f) return empty;
    const float life = std::max(authoredLife, 0.0001f);
    const float interval = std::max(0.f, step);
    auto stopped = [&](float birth) { return stopAt >= 0.f && birth >= stopAt; };
    if (type == 3) { // BLAST: entire pool born at once, Play(0) loops the group.
        float maxLife = 0.f;
        for (int n = 0; n < count; ++n)
            maxLife = std::max(maxLife, individualLife(life,seed,n,0,divisor));
        if (!(maxLife > 0.f)) return empty;
        const unsigned gen = static_cast<unsigned>(std::min(100000.0,
                                          std::floor(static_cast<double>(seconds)/maxLife)));
        const float birth = static_cast<float>(gen) * maxLife;
        if (stopped(birth)) return empty;
        const float myLife = individualLife(life,seed,slot,gen,divisor);
        const float age = seconds-birth;
        if (age >= myLife) return empty; // dead slot awaits the next group cycle
        return {true,age,myLife,age/myLife,gen};
    }
    if (type != 1 && type != 2) return empty;
    // SNOW/FIRE: individual slots. A first pass emits across the pool,
    // subsequent generations recycle individual slots after death. The D3D
    // _fStep accumulator depends on frame cadence; this is an FPS-independent
    // approximation that preserves staggered births and per-particle curves.
    float birth = static_cast<float>(slot) * interval;
    unsigned gen = 0;
    if (seconds < birth || stopped(birth)) return empty;
    for (; gen < 10000; ++gen) {
        const float myLife = historical_particle_random::randfMappingEnabled() ?
            historical_particle_random::snowFireLife(life,seed,slot,gen,divisor) :
            individualLife(life,seed,slot,gen,divisor);
        if (seconds < birth + myLife)
            return {true,seconds-birth,myLife,(seconds-birth)/myLife,gen};
        const float next = birth + std::max(myLife,interval * static_cast<float>(count));
        if (!std::isfinite(next) || next <= birth || next > seconds || stopped(next))
            return empty;
        birth = next;
    }
    return empty;
}

// Replays the shared _fCurTime accumulator used by _CreateSnow/_CreateFire.
// DX9 calls the creator once for each dead slot in index order; every attempt
// adds dt, and a successful interval resets the accumulator to zero (it does
// not subtract the interval). This snapshot uses a nominal fixed rate because
// EffectSystem renders from age snapshots and does not retain the DX9 frame RNG.
inline SystemSample snowFireSystemAt(int type, int count, float seconds,
        float authoredLife, float step, float stopAt, std::uint32_t seed,
        int divisor, int frameCount, bool snowCanSpawn, float hz = 60.f) {
    SystemSample result;
    if ((type != 1 && type != 2) || count < 1 || count > 100 ||
        !std::isfinite(seconds) || !std::isfinite(authoredLife) ||
        !std::isfinite(step) || !std::isfinite(stopAt) || !std::isfinite(hz) ||
        seconds < 0.f || seconds > 60.f || authoredLife <= 0.f ||
        frameCount < 1 || frameCount > 256 || hz < 1.f || hz > 240.f)
        return result;

    result.slots.resize(static_cast<std::size_t>(count));
    std::vector<unsigned> deathFrame(static_cast<std::size_t>(count));
    std::vector<unsigned> nextGeneration(static_cast<std::size_t>(count));
    std::vector<unsigned> birthFrame(static_cast<std::size_t>(count));
    float emitterTimer = 0.f;
    const float tick = 1.f / hz;
    const unsigned ticks = static_cast<unsigned>(std::floor(seconds * hz + .00001f));
    for (unsigned frame = 1; frame <= ticks; ++frame) {
        const float frameAge = static_cast<float>(frame) * tick;
        for (int index = 0; index < count; ++index) {
            Sample& slot = result.slots[static_cast<std::size_t>(index)];
            if (slot.alive) {
                if (frame >= deathFrame[static_cast<std::size_t>(index)])
                    slot.alive = false;
                continue;
            }
            // The first slot attempt occurs only after the controller's first
            // FrameMove; a stopped controller never calls its creator again.
            if (stopAt >= 0.f && frameAge >= stopAt) continue;
            emitterTimer += tick;
            if (step > 0.f && emitterTimer < step) continue;
            emitterTimer = 0.f;
            if (type == 1 && !snowCanSpawn) continue;

            const unsigned generation = nextGeneration[static_cast<std::size_t>(index)]++;
            const float life = individualLife(authoredLife,seed,index,generation,divisor);
            const float keyTime = life / static_cast<float>(frameCount);
            const unsigned ticksPerKey = static_cast<unsigned>(std::max(1.0,
                std::ceil(static_cast<double>(keyTime) * hz - 1e-5)));
            const unsigned lifeTicks = ticksPerKey * static_cast<unsigned>(frameCount);
            birthFrame[static_cast<std::size_t>(index)] = frame;
            deathFrame[static_cast<std::size_t>(index)] = frame + lifeTicks;
            slot = {true,0.f,life,0.f,generation};
        }
    }
    for (int index = 0; index < count; ++index) {
        Sample& slot = result.slots[static_cast<std::size_t>(index)];
        if (!slot.alive) continue;
        const float bornAt = static_cast<float>(birthFrame[static_cast<std::size_t>(index)]) * tick;
        slot.age = std::max(0.f,seconds-bornAt);
        slot.normalized = std::clamp(slot.age / std::max(slot.life,0.0001f),0.f,1.f);
    }
    return result;
}
}} // namespace top::historical_particle_pool
