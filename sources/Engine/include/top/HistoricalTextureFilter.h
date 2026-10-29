#pragma once
// SK2.15-C: D3DTEXTUREFILTERTYPE values authored by CMPPartSys (.par).
// None/unsupported filters retain the engine's default linear sampling.
// This policy is CPU-only: no OpenGL dependency and no impact on UI/terrain.
#include <cstdlib>
namespace top { namespace historical_filter {
struct Pair { int min = 0, mag = 0; };
// D3DTEXF_POINT=1; D3DTEXF_LINEAR=2; 0 means unspecified/default.
constexpr int supported(int value) { return value == 1 || value == 2 ? value : 0; }
constexpr Pair choose(int min, int mag, bool enabled) {
    return enabled ? Pair{supported(min), supported(mag)} : Pair{};
}
constexpr bool same(Pair a, Pair b) { return a.min == b.min && a.mag == b.mag; }
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }
inline Pair effective(int min, int mag) { return choose(min, mag, enabled()); }
}} // namespace top::historical_filter
