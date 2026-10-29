#pragma once
// SK2.15-B: CMPPartSys PARTTICLE_STRIP (type 6), not CMPStrip (controller tail).
// DX9 _FrameMoveStrip uses one living particle with the FIRST authored parent
// size/colour/angle, but CMPModelEff::FrameMove advances the nested .eff clip.
// A default looping controller starts the nested CMPModelEff in looping mode;
// short nested clips must not disappear while their owning STRIP is alive.
#include <cmath>
#include <cstdlib>

namespace top { namespace legacy_strip_system {

inline constexpr bool historicalCyclesEnabled() { return true; }

inline float nestedModelAge(int particleType, bool isFlight, float age,
                            float groupCycleLength, bool historicStrip = true) {
    if (!std::isfinite(age) || age < 0.f) return 0.f;
    // Type=6 (PARTTICLE_STRIP) uses its FIRST parent key, but this does not
    // freeze the child .eff. DX9 CMPPartSys::Play(0) calls _CPPart->Play(0),
    // then CMPModelEff restarts the GROUP after all subeffects have stopped.
    // Preserve the other particle types' pre-SK2.15-B execution policy.
    const bool repeat = (particleType == 6 && historicStrip) ||
                        isFlight || (particleType != 5 && particleType != 6);
    if (!repeat || !std::isfinite(groupCycleLength) || groupCycleLength <= 0.f)
        return age;
    return std::fmod(age, groupCycleLength);
}

}} // top::legacy_strip_system
