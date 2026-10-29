#pragma once

// Historical skill targeting/range policy. No Play, GL, UI or resources.
// skillinfo.txt: col 9 (type), 10 (helpful), 17 (target), 16 (distance cm).
namespace top::historical_skill {

struct Descriptor {
    int type = 0;
    int helpful = 0;
    int target = 0;
    int distanceCm = 0;
};

enum class InputMode { Direct, SelectEnemy, Passive };
enum class TargetStep { AwaitSelection, Approach, Ready, Invalid };

InputMode inputMode(const Descriptor& skill);
float reachMeters(const Descriptor& skill, float playerRadius, float foeRadius);
TargetStep targetStep(const Descriptor& skill, bool validFoe, float distance,
                      float playerRadius, float foeRadius);
// Stops *inside* reach, to avoid stuck casts at numerical boundaries.
float approachStopMeters(float reach);

} // namespace top::historical_skill
