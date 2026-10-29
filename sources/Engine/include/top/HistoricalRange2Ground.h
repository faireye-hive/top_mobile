#pragma once

#include "top/MapFile.h"

#include <cstdlib>

namespace top { namespace historical_range2_ground {

inline constexpr bool subgridHeightEnabled() { return true; }

inline float sample(const MapFile& map, int x, int y) {
    if (subgridHeightEnabled()) return map.range2GroundHeightAt(x, y);
    return map.heightAt(static_cast<float>(x), static_cast<float>(y));
}

} } // namespace top::historical_range2_ground
