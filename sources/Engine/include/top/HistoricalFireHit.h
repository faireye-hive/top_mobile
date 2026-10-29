#pragma once

// SK2.17-H: authored FIRE path-end hit events from CMPPartSys.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace top { namespace historical_fire_hit {

struct Event {
    bool trigger = false;
    float stopAt = -1.f; // elapsed time after _fDelayTime, for the FIRE pool
};

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

inline Event at(float controllerAge, float delay, float playTime,
                float pathVelocity, const std::vector<float>& segmentDistances,
                bool hasHitEffect, bool hasMap, bool alreadyReported) {
    Event result;
    if (alreadyReported || !hasHitEffect || !hasMap || segmentDistances.empty() ||
        !std::isfinite(controllerAge) || !std::isfinite(delay) ||
        !std::isfinite(playTime) || !std::isfinite(pathVelocity) ||
        controllerAge < 0.f || delay < 0.f || pathVelocity <= 0.f) return result;

    float totalDistance = 0.f;
    for (float distance : segmentDistances) {
        if (!std::isfinite(distance) || distance < 0.f) return result;
        totalDistance += distance;
    }
    if (!std::isfinite(totalDistance) || totalDistance <= 0.f) return result;

    const float firstWrap = delay + totalDistance / pathVelocity;
    // CMPPartSys::UpdateDelay stops the controller at _fPlayTime. A positive
    // stop time before the first path wrap prevents _FrameMoveFire's event.
    if (playTime > 0.f && firstWrap > playTime) return result;
    if (controllerAge < firstWrap) return result;
    result.trigger = true;
    result.stopAt = totalDistance / pathVelocity;
    return result;
}

} } // namespace top::historical_fire_hit
