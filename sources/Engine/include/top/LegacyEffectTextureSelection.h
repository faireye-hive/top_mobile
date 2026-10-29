#pragma once
// SK2.16-D: DX9 CMPModelEff::Render() binds the subeffect's SetTexture().
// External .lgo materials provide a fallback, not precedence over the
// explicitly-authored .eff texture. Animated .eff frame texture wins first.
#include <cstdlib>
#include <string>

namespace top { namespace legacy_effect_texture {

enum class Origin { None, Animated, Effect, Material };
struct Choice {
    std::string path;
    Origin origin = Origin::None;
};
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool historicalEnabled() { return true; }
inline Choice choose(const std::string& effectTexture,
                     const std::string& materialTexture,
                     const std::string& animatedFrame,
                     bool historical) {
    if (!animatedFrame.empty()) return {animatedFrame, Origin::Animated};
    if (historical && !effectTexture.empty()) return {effectTexture, Origin::Effect};
    if (!materialTexture.empty()) return {materialTexture, Origin::Material};
    if (!effectTexture.empty()) return {effectTexture, Origin::Effect};
    return {};
}
}} // namespace top::legacy_effect_texture
