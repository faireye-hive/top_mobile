#include "top/HistoricalSkillExecution.h"

#include <algorithm>
#include <cmath>

namespace top::historical_skill {
InputMode inputMode(const Descriptor& skill) {
    if (skill.type == 2) return InputMode::Passive;
    // skillinfo target=4 => enemy; helpful=0. Other target modes include
    // self, ally, ground, building and utility, and need different handlers.
    if (skill.target == 4 && skill.helpful == 0 &&
        (skill.type == 0 || skill.type == 1)) return InputMode::SelectEnemy;
    return InputMode::Direct;
}

float reachMeters(const Descriptor& skill, float playerRadius, float foeRadius) {
    const float distance = std::max(0, skill.distanceCm) / 100.f;
    return std::max(.1f, distance + std::max(0.f, playerRadius) + std::max(0.f, foeRadius));
}

TargetStep targetStep(const Descriptor& skill, bool validFoe, float distance,
                      float playerRadius, float foeRadius) {
    if (inputMode(skill) != InputMode::SelectEnemy) return TargetStep::Invalid;
    if (!validFoe || !std::isfinite(distance)) return TargetStep::AwaitSelection;
    return distance <= reachMeters(skill, playerRadius, foeRadius) ?
        TargetStep::Ready : TargetStep::Approach;
}

float approachStopMeters(float reach) {
    if (!std::isfinite(reach)) return 0.2f;
    const float pad = std::min(.45f, reach * .35f);
    return std::max(.2f, reach - pad);
}
} // namespace top::historical_skill
