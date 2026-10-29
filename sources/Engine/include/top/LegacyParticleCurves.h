#pragma once
// 05-H: Interpolacao das curvas que ja sao decodificadas de CMPPartCtrl (.par).
// Nao confundir numero de keyframes de tamanho/cor com quadros de atlas UV.
// Nao descreve nem inventa a hierarquia/geometria de CMPModelEff (.eff).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace top {
namespace legacyfx {

struct CurveSample {
    float size = 1.f;
    float color[4] = {1.f, 1.f, 1.f, 1.f};
    float rotation = 0.f; // componente Z do track de rotacao (radianos)
};

inline float sanitize(float v, float fallback, float low, float high) {
    return std::isfinite(v) ? std::max(low, std::min(high, v)) : fallback;
}

inline float sampleTrack(const std::vector<float>& track, std::size_t stride,
                         std::size_t channel, float t, float fallback) {
    if (!stride || channel >= stride || track.size() < stride || track.size() % stride) return fallback;
    t = sanitize(t, 0.f, 0.f, 1.f);
    const std::size_t frames = track.size() / stride;
    // CMPParticle::m_fFrameTime = life / frameCount. The last interval
    // holds the final key; it does not interpolate for life/(N-1).
    const float cursor = t * static_cast<float>(frames);
    const std::size_t first = std::min(frames - 1, static_cast<std::size_t>(cursor));
    const std::size_t second = std::min(frames - 1, first + 1);
    const float a = track[first * stride + channel];
    const float b = track[second * stride + channel];
    if (!std::isfinite(a) || !std::isfinite(b)) return fallback;
    return a + (b - a) * (cursor - static_cast<float>(first));
}

inline CurveSample sample(const std::vector<float>& sizes,
                          const std::vector<float>& colors,
                          const std::vector<float>& rotations, float normalizedAge) {
    CurveSample out;
    out.size = sanitize(sampleTrack(sizes, 1, 0, normalizedAge, 1.f), 1.f, 0.f, 50.f);
    for (std::size_t channel = 0; channel < 4; ++channel)
        out.color[channel] = sanitize(sampleTrack(colors, 4, channel, normalizedAge, 1.f),
                                      1.f, 0.f, 1.f);
    out.rotation = sanitize(sampleTrack(rotations, 3, 2, normalizedAge, 0.f),
                            0.f, -1000.f, 1000.f);
    return out;
}

} // namespace legacyfx
} // namespace top
