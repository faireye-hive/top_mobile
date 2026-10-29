#pragma once

// PERF-01: identities from historical CreateCha / ChaSpn, not model IDs.
// Identical character IDs can be present at different spawn points.
#include <cmath>
#include <cstdint>

namespace top::population_identity {
struct Spawn {
    int characterId = 0;
    float homeX = 0.f;
    float homeY = 0.f;
    bool passive = false;
    std::uint64_t key = 0;
};

inline bool same(const Spawn& a, const Spawn& b) {
    if (a.characterId != b.characterId || a.passive != b.passive) return false;
    if (a.key && b.key) return a.key == b.key;
    if (a.key || b.key) return false;
    return std::fabs(a.homeX - b.homeX) < .001f &&
           std::fabs(a.homeY - b.homeY) < .001f;
}
}  // namespace top::population_identity
