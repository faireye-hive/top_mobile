#pragma once
namespace top {
inline const char* jobName(int job) {
    static constexpr const char* names[] = {
        "Newbie", "Swordsman", "Hunter", "Sailor", "Explorer", "Herbalist",
        "Artisan", "Merchant", "Champion", "Crusader", "White Knight",
        "Animal Tamer", "Sharpshooter", "Cleric", "Seal Master", "Captain",
        "Voyager", "Upstart", "Engineer"
    };
    return job >= 0 && job < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[job] : "Unknown";
}

} // namespace top
