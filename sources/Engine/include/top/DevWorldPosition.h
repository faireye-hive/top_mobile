#pragma once

// Offline-only checkpoint / last location. Kept separate from Play's versioned
// inventory/quest save so older clients and recovery checksums are untouched.
#include <cmath>
#include <cstdio>
#include <string>
#include <algorithm>

namespace top::dev_world {
struct Location {
    std::string map;
    float x = 0.f;
    float y = 0.f;
    float yaw = 0.f;
    bool boat = false;
};
inline bool valid(const Location& p) {
    if (p.map.empty() || p.map.size() > 48 || !std::isfinite(p.x) ||
        !std::isfinite(p.y) || !std::isfinite(p.yaw) || p.x < 0.f ||
        p.y < 0.f || p.x >= 16384.f || p.y >= 16384.f ||
        std::abs(p.yaw) > 100.f) return false;
    for (unsigned char c : p.map)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
inline std::string path(const std::string& root, int identity, bool manual = false) {
    if (identity <= 0 || identity > 999999) return {};
    return root + "/user/offline-" + (manual ? "checkpoint-" : "location-") +
           std::to_string(identity) + ".txt";
}
inline bool read(const std::string& file, Location& out) {
    if (file.empty()) return false;
    FILE* in = std::fopen(file.c_str(), "rb");
    if (!in) return false;
    char magic[16]{}, map[64]{}, trailer = 0;
    Location tmp;
    int boat = -1;
    // Extra trailing tokens indicate a malformed or unexpected version.
    const int n = std::fscanf(in, "%15s %63s %f %f %f %d %c", magic, map,
                              &tmp.x, &tmp.y, &tmp.yaw, &boat, &trailer);
    std::fclose(in);
    tmp.map = map;
    tmp.boat = boat == 1;
    if (n != 6 || std::string(magic) != "TOPLOC1" || (boat != 0 && boat != 1) ||
        !valid(tmp)) return false;
    out = tmp;
    return true;
}
inline bool write(const std::string& file, const Location& p) {
    if (file.empty() || !valid(p)) return false;
    const std::string temporary = file + ".tmp";
    FILE* out = std::fopen(temporary.c_str(), "wb");
    if (!out) return false;
    const bool okay = std::fprintf(out, "TOPLOC1 %s %.4f %.4f %.6f %d\n",
                                   p.map.c_str(), p.x, p.y, p.yaw, p.boat ? 1 : 0) > 0 &&
                      std::fflush(out) == 0;
    const bool closed = std::fclose(out) == 0;
    if (!okay || !closed || std::rename(temporary.c_str(), file.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}
}
