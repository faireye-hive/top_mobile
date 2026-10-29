#pragma once
// Characterinfo.txt field 20 (Action ID) is a shared animation-set ID.
// It is NOT the character/monster ID. Many creatures reuse one set.
#include <cstdlib>
#include <string>
#include <vector>

namespace top {
inline int characterPoseSetId(int characterId, const std::vector<std::string>& columns) {
    if (characterId <= 0 || columns.size() <= 20) return characterId;
    const std::string& raw = columns[20];
    if (raw.empty()) return characterId;
    char* after = nullptr;
    const long value = std::strtol(raw.c_str(), &after, 10);
    if (after == raw.c_str() || value < 1 || value > 100000) return characterId;
    // Some client releases allow comma-separated alternative IDs.
    // The leading ID is the standard model's pose set.
    if (*after != '\0' && *after != ',' && *after != ' ' && *after != '\t') return characterId;
    return static_cast<int>(value);
}

inline int poseFrameOnce(int start, int end, float cursor, int maxFrames) {
    if (maxFrames <= 0 || start < 0 || end <= start || start >= maxFrames) return 0;
    const float value = cursor < 0.f ? 0.f : cursor;
    const int offset = value >= static_cast<float>(end - start) ? end - start : static_cast<int>(value);
    const int frame = start + offset;
    return frame >= maxFrames ? maxFrames - 1 : frame;
}
} // namespace top
