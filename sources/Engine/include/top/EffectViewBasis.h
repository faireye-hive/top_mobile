#pragma once
#include <array>

namespace top {
// Rotation of the inverse view matrix, as CMPResManger::_MatBBoard.
// World coordinates here are the port's Y-up axes.
struct EffectViewBasis {
    std::array<float,3> right{{1.f,0.f,0.f}};
    std::array<float,3> up{{0.f,1.f,0.f}};

    static EffectViewBasis fromView(const float* view) {
        return {{{view[0],view[4],view[8]}},{{view[1],view[5],view[9]}}};
    }
    std::array<float,3> normal() const {
        return {{right[1]*up[2]-right[2]*up[1],
                 right[2]*up[0]-right[0]*up[2],
                 right[0]*up[1]-right[1]*up[0]}};
    }
};
}
