#pragma once

// Builds the terrain-conforming grid used by DX9 CMPShadeMap::MoveTo.
// The caller supplies the OpenGL terrain height sampler and applies texture,
// blend state and the particle's interpolated tint.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace top { namespace historical_shade {

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool terrainGridEnabled() { return true; }

struct Vertex {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float u = 0.f;
    float v = 0.f;
};

template <class HeightAt>
std::vector<Vertex> build(float centerX, float centerZ, float radius,
                          float fallbackHeight, HeightAt&& heightAt) {
    std::vector<Vertex> triangles;
    if (!std::isfinite(centerX) || !std::isfinite(centerZ) ||
        !std::isfinite(radius) || radius <= 0.f || !std::isfinite(fallbackHeight))
        return triangles;

    // DX9: TILESIZE=1, _fGridMax=radius/TILESIZE+1, rounded up to
    // _iGridCrossNum in CMPShadeMap::CreateShadeMap.
    const float gridMax = radius + 1.f;
    // CMPShadeMap's source vertex arrays have MAX_SHADER_VERNUM=300 slots.
    if (gridMax > 16.f) return triangles;
    const int gridCross = static_cast<int>(std::ceil(gridMax));
    if (gridCross < 1 || (gridCross + 1) * (gridCross + 1) > 300) return triangles;
    const int startX = static_cast<int>(centerX - radius * .5f);
    const int startZ = static_cast<int>(centerZ - radius * .5f);
    triangles.reserve(static_cast<std::size_t>(gridCross) *
                      static_cast<std::size_t>(gridCross) * 6u);

    auto sample = [&](int x, int z) {
        const float worldX = static_cast<float>(startX + x);
        const float worldZ = static_cast<float>(startZ + z);
        float height = heightAt(worldX, worldZ, x == gridCross, z == gridCross);
        if (!std::isfinite(height)) height = fallbackHeight;
        const float u = std::clamp((worldX - centerX) / gridMax + .5f, 0.f, 1.f);
        const float v = std::clamp((worldZ - centerZ) / gridMax + .5f, 0.f, 1.f);
        return Vertex{worldX, height + .015f, worldZ, u, v};
    };

    for (int z = 0; z < gridCross; ++z) {
        for (int x = 0; x < gridCross; ++x) {
            const Vertex a = sample(x, z);
            const Vertex b = sample(x + 1, z);
            const Vertex c = sample(x, z + 1);
            const Vertex d = sample(x + 1, z + 1);
            triangles.insert(triangles.end(), {a, b, c, b, d, c});
        }
    }
    return triangles;
}

}} // namespace top::historical_shade
