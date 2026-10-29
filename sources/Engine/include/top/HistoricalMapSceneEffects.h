#pragma once

#include <cstdint>
#include <cmath>
#include <cstdlib>

namespace top {

struct MapSceneEffectPlacement {
    std::uint64_t key = 0;
    int effectId = 0;
    float x = 0.f;
    float y = 0.f;
    float height = 0.f;
    float yaw = 0.f; // DX9 yaw/100 radians, around the vertical world axis.
};

namespace historical_map_scene_effects {
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }
inline void rotateHorizontal(float x, float z, float yaw, float& outX, float& outZ) {
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    outX = x * c - z * s;
    outZ = x * s + z * c;
}
} // namespace historical_map_scene_effects

} // namespace top
