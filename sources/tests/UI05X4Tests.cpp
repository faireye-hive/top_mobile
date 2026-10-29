#include "top/ActorFrameCache.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using top::actor_frame_cache::IndexedVertices;
using top::actor_frame_cache::Vertex;

namespace {
bool close(float a, float b) { return std::fabs(a - b) <= 0.0001f; }
}

int main() {
    IndexedVertices cache;
    // Two triangles share vertices 0 and 2; the historical triangle-list
    // emitter still needs six output vertices, but only four skinnings.
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 2, 1, 3};
    int skinCalls = 0, worldCalls = 0;
    float animationOffset = 0.f, worldX = 10.f, worldY = 20.f;
    const auto skin = [&](std::size_t i, Vertex& v) {
        ++skinCalls;
        v.x = static_cast<float>(i);
        v.y = static_cast<float>(i) * 2.f + animationOffset;
        v.z = 3.f;
        v.nx = 0.f; v.ny = 0.f; v.nz = 1.f;
    };
    const auto world = [&](const Vertex& src, Vertex& dst) {
        ++worldCalls;
        dst = src;
        dst.x += worldX;
        dst.y += worldY;
    };
    const auto emit = [&]() {
        std::vector<Vertex> triangles;
        for (auto i : indices) triangles.push_back(cache.get(i, skin, world));
        assert(triangles.size() == 6);
        assert(close(triangles[0].x, worldX));
        assert(close(triangles[3].x, worldX + 2.f));
        assert(close(triangles[1].y, worldY + 2.f + animationOffset));
        assert(close(triangles[2].nz, 1.f));
        assert(close(triangles[2].x, triangles[3].x));
    };
    cache.begin(4, 8);
    emit();
    assert(skinCalls == 4 && worldCalls == 4);
    // Same pose on a new render frame; node/world transform can still change.
    worldX = -4.f;
    cache.begin(4, 8);
    emit();
    assert(skinCalls == 4 && worldCalls == 8);
    // A new keyframe must invalidate only pose-space vertices.
    animationOffset = 6.f;
    cache.begin(4, 9);
    emit();
    assert(skinCalls == 8 && worldCalls == 12);
    // Two subsets may share the same vertex indices in one build.
    emit();
    assert(skinCalls == 8 && worldCalls == 12);
    // Explicit invalidation (equipment swap/boat model) with the same count
    // must not reuse stale vertex data from the previous model.
    cache.invalidate();
    cache.begin(4, 9);
    emit();
    assert(skinCalls == 12 && worldCalls == 16);
    // Geometry resize invalidates both caches safely.
    cache.begin(6, 9);
    (void)cache.get(5, skin, world);
    assert(cache.size() == 6);
    assert(skinCalls == 13 && worldCalls == 17);
    // The model may have no indexed triangles at all.
    cache.begin(0, 0);
    assert(cache.size() == 0);
    std::cout << "05-X4: skinned vertices cached across triangles and frames; world updated every draw\n";
    return 0;
}
