#pragma once
#include <algorithm>
#include <cmath>

namespace top::classic_camera {
// The reference client keeps the camera well above the avatar, aimed down at
// the terrain. Lift = eye height / horizontal distance (tan of elevation).
constexpr float kDefaultDistance = 15.f;
constexpr float kDefaultLift = 1.85f; // ~62 degrees above the horizon
constexpr float kMinDistance = 4.5f; // scroll para aproximar, sem liberar camera baixa
constexpr float kMaxDistance = 22.f;
constexpr float kMinLift = 1.35f; // ~53 degrees: never becomes a free camera
constexpr float kMaxLift = 2.4f;  // ~67 degrees: still shows actors standing
inline float distance(float v) { return std::isfinite(v) ? std::clamp(v,kMinDistance,kMaxDistance):kDefaultDistance; }
inline float lift(float v) { return std::isfinite(v) ? std::clamp(v,kMinLift,kMaxLift):kDefaultLift; }
inline float fromSavedLift(float v) {
    // 05-W1 used values 0.36..0.60. Upgrade the old configuration to the
    // classic preset instead of preserving the unwanted low-angle view.
    return (!std::isfinite(v) || v < .90f) ? kDefaultLift : lift(v);
}
} // namespace top::classic_camera
