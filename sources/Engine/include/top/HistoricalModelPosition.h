#pragma once
// DX9 _FrameMoveModel places its single particle at _vPos + range / 2.
#include <array>
#include <cstdlib>

namespace top { namespace historical_model_position {
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

inline std::array<float,3> midpointOffset(const std::array<float,3>& range) {
    return {{range[0] * 0.5f, range[1] * 0.5f, range[2] * 0.5f}};
}
}} // namespace top::historical_model_position
