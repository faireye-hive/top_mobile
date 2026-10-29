#pragma once

#include <cstdlib>

namespace top { namespace historical_strip_alpha {

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

// DX9 calls CMPModelEff::SetAlpha only when the type-6 parent alpha is < 1.
// SetAlpha replaces each child controller's alpha; it does not multiply it.
inline float effectiveAlpha(int particleType, float parentAlpha, float childAlpha) {
    return particleType == 6 && parentAlpha < 1.f ? parentAlpha : childAlpha;
}

}} // namespace top::historical_strip_alpha
