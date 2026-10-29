#pragma once

#include <algorithm>
#include <cstdlib>

namespace top { namespace historical_particle_count {

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

inline int resolve(int type, int authoredCount, bool singleInstance,
                   bool useAuthoredSnowFireCount, int fallbackLimit = 24) {
    if (singleInstance) return 1;
    if (authoredCount < 1) return 1;
    if (useAuthoredSnowFireCount && (type == 1 || type == 2))
        return authoredCount;
    return std::min(authoredCount, std::max(1, fallbackLimit));
}

} } // namespace top::historical_particle_count
