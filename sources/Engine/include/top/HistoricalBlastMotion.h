#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace top { namespace historical_blast_motion {

using V3 = std::array<float, 3>;

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool actualDtEnabled() { return true; }

// DX9 moves with the current velocity, then increments that velocity by a*dt.
// This returns the correction from the existing continuous-time 0.5*a*t^2
// approximation for the opt-in nominal 60Hz replay.
inline V3 discreteAccelerationCorrection(const V3& acceleration, float age,
                                         float nominalStep = 1.f / 60.f) {
    V3 correction{{0.f, 0.f, 0.f}};
    if (!std::isfinite(age) || age <= 0.f || !std::isfinite(nominalStep) || nominalStep <= 0.f)
        return correction;
    const double step = nominalStep;
    const double elapsed = age;
    const double fullSteps = std::floor(elapsed / step);
    const double remainder = elapsed - fullSteps * step;
    const double discreteScale = step * step * fullSteps * (fullSteps - 1.0) * .5 +
                                 fullSteps * step * remainder;
    const double continuousScale = .5 * elapsed * elapsed;
    const double delta = discreteScale - continuousScale;
    for (std::size_t axis = 0; axis < correction.size(); ++axis)
        correction[axis] = static_cast<float>(static_cast<double>(acceleration[axis]) * delta);
    return correction;
}

struct SlotSample {
    bool alive = false;
    unsigned generation = 0;
};

// The DX9 integrator adds acceleration * dt to velocity after moving. Its
// acceleration displacement over actual frame deltas differs from the
// continuous 0.5*a*t^2 curve by -0.5*a*sum(dt_i^2).
struct SystemTimeline {
    struct Slot {
        bool active = false;
        unsigned generation = 0;
        double sumDeltaSquared = 0.0;
    };
    std::vector<Slot> slots;

    void advance(float dt, const std::vector<SlotSample>& samples) {
        if (!std::isfinite(dt) || dt < 0.f || samples.size() > 4096) return;
        if (slots.size() != samples.size()) slots.assign(samples.size(), Slot{});
        const double deltaSquared = static_cast<double>(dt) * dt;
        for (std::size_t i=0;i<samples.size();++i) {
            const auto& sample=samples[i];
            auto& slot=slots[i];
            if (!sample.alive) { slot.active=false; continue; }
            if (!slot.active || slot.generation != sample.generation) {
                slot.active=true;
                slot.generation=sample.generation;
                slot.sumDeltaSquared=deltaSquared;
            } else {
                slot.sumDeltaSquared+=deltaSquared;
            }
        }
    }

    V3 correction(const V3& acceleration, std::size_t index,
                  unsigned generation) const {
        V3 result{{0.f,0.f,0.f}};
        if (index>=slots.size()) return result;
        const auto& slot=slots[index];
        if (!slot.active || slot.generation!=generation) return result;
        const double scale=-0.5*slot.sumDeltaSquared;
        for(std::size_t axis=0;axis<result.size();++axis)
            result[axis]=static_cast<float>(static_cast<double>(acceleration[axis])*scale);
        return result;
    }
};

}} // namespace top::historical_blast_motion
