#pragma once
#include "top/GfxDevice.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace top::selection_ring {
// Construir triangulos finos no plano XZ (OpenGL Y-up).
inline std::vector<MeshVertex> build(float x, float y, float height, float radius,
                                      int segments=48) {
    radius = std::clamp(radius, .4f, 4.f);
    segments = std::clamp(segments, 12, 128);
    std::vector<MeshVertex> out;
    out.reserve(static_cast<std::size_t>(segments * 6));
    constexpr float tau=6.2831853071795864769f;
    auto vert = [&](float angle, float r) {
        MeshVertex v{};
        v.x = x + std::cos(angle) * r;
        v.y = height + .065f;
        v.z = y + std::sin(angle) * r;
        v.r=.08f;v.g=.32f;v.b=1.f;v.a=.80f;
        return v;
    };
    for (int i=0;i<segments;++i) {
        const float a=tau*i/segments,b=tau*(i+1)/segments;
        auto a0=vert(a,radius-.085f),a1=vert(a,radius+.085f);
        auto b0=vert(b,radius-.085f),b1=vert(b,radius+.085f);
        out.insert(out.end(),{a0,a1,b0,a1,b1,b0});
    }
    return out;
}
} // namespace top::selection_ring
