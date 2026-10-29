#pragma once
// SK2.17-A: CMPPartSys::Load in the original DX9 MPParticleSys.cpp.
// _iRoadom selects [life/divisor, life] and SNOW's velocity interval.
// Before .par v13 it was inferred from the system type; newer files store it.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
namespace top { namespace historical_particle_random {
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool randfMappingEnabled() { return true; }
inline int defaultDivisor(int type) {
    return type == 1 || type == 2 || type == 16 ? 4 : 2;
}
inline int decodedDivisor(unsigned version, int type, int serialized = 0) {
    if (version <= 12) return defaultDivisor(type);
    // A zero/negative value would divide by zero in the original client.
    // Treat an invalid field as the constructor default (2), not as 1.
    return serialized > 0 && serialized <= 256 ? serialized : 2;
}

// DX9's Randf(float) is not a continuous uniform sample: MPParticleSys.h
// combines rand()%integer-range with a rand()%1000 fractional part (or a
// smaller modulo for a fractional maximum). Reproduce that mapping over the
// existing deterministic per-particle stream. This preserves the authored
// distribution and quantization, but not the process-global MSVC rand order.
template<class NextRand>
inline unsigned randModulo(unsigned limit, NextRand&& nextRand) {
    return limit == 0 ? 0u : nextRand() % limit;
}

template<class NextRand>
inline float dx9Randf(float maximum, NextRand&& nextRand) {
    if (!std::isfinite(maximum) || maximum <= 0.f ||
        static_cast<double>(maximum) >
            static_cast<double>(std::numeric_limits<int>::max())) return 0.f;
    if (maximum == 1.f)
        return static_cast<float>(nextRand() % 1000u) / 1000.f;

    const int whole = static_cast<int>(maximum);
    const float fraction = maximum - static_cast<float>(whole);
    const unsigned integer = randModulo(static_cast<unsigned>(whole), nextRand);
    unsigned fractional = 0;
    if (fraction > 0.f && static_cast<int>(fraction) == 0) {
        const auto fractionalLimit = static_cast<unsigned>(fraction * 1000.f);
        fractional = randModulo(fractionalLimit, nextRand);
    } else {
        fractional = nextRand() % 1000u;
    }
    return static_cast<float>(integer) + static_cast<float>(fractional) / 1000.f;
}

inline std::uint32_t deterministicRandBits(std::uint32_t seed, unsigned lane) {
    std::uint32_t value = seed ^ (0x9e3779b9u * (lane + 1u));
    value ^= value >> 16; value *= 0x7feb352du; value ^= value >> 15;
    value *= 0x846ca68bu; value ^= value >> 16;
    return value;
}

inline unsigned deterministicRandModulo(unsigned limit, std::uint32_t seed,
                                        unsigned lane = 0) {
    return limit == 0 ? 0u : deterministicRandBits(seed, lane) % limit;
}

inline float deterministicRandf(float maximum, std::uint32_t seed,
                                unsigned firstLane = 0) {
    unsigned draw = 0;
    auto nextRand = [&]() { return deterministicRandBits(seed, firstLane + draw++); };
    return dx9Randf(maximum, nextRand);
}

inline float deterministicRandfBetween(float minimum, float maximum,
                                       std::uint32_t seed, unsigned firstLane = 0) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || maximum < minimum) return 0.f;
    if (minimum == maximum) return maximum;
    return minimum + deterministicRandf(maximum - minimum, seed, firstLane);
}

inline float snowFireLife(float life, std::uint32_t seed, int slot,
                          unsigned generation, int divisor) {
    if (!(std::isfinite(life) && life > 0.f)) return 0.f;
    const float minimum = life / static_cast<float>(divisor > 0 ? divisor : 2);
    const std::uint32_t particleSeed = seed ^ (0x9e3779b9u *
        static_cast<std::uint32_t>(slot + 1)) ^ (generation * 0x85ebca6bu);
    return deterministicRandfBetween(minimum, life, particleSeed);
}
}} // namespace top::historical_particle_random
