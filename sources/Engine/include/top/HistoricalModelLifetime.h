#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace top { namespace historical_model_lifetime {

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

// DX9 UpdateDelay stops MODEL/STRIP at the controller's absolute playTime.
// Delay gates updates, but is not added to that stop time. Keep the old generic
// tail when this path is disabled, the controller is unbounded, or data is bad.
inline float expiry(float genericExpiry,int type,float delayTime,float playTime) {
    if(!enabled()||(type!=5&&type!=6)||!std::isfinite(delayTime)||
       !std::isfinite(playTime)||playTime<=0.f)return genericExpiry;
    return std::max(std::max(0.f,delayTime),playTime);
}

}} // namespace top::historical_model_lifetime
