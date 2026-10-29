#include "top/ItemTicketRules.h"
namespace top::item_tickets {
std::optional<Destination> destinationFor(int id, const std::string& callback) {
    if (callback != "Ticket") return std::nullopt;
    // From GameServer TicketSys.Ticket + birth_conf.lua; first historical
    // birthpoint selected deterministically offline (server chose at random).
    switch (id) {
        case 332: return Destination{332, "Spring Town", "darkblue", 3323.0f, 2507.0f};
        case 563: return Destination{563, "Summer Island", "darkblue", 3681.0f, 2653.0f};
        case 583: return Destination{583, "Autumn Island", "darkblue", 2635.0f, 3004.0f};
        case 2445: return Destination{2445, "Caribbean", "jialebi", 620.0f, 951.0f};
        case 2447: return Destination{2447, "Skeletar Isle", "jialebi", 609.0f, 624.0f};
        case 2491: return Destination{2491, "Naval Base", "jialebi", 1084.0f, 206.0f};
        case 2844: return Destination{2844, "Abaddon 4", "jialebi", 155.0f, 907.0f};
        case 3048: return Destination{3048, "Thundoria Castle", "garner", 736.0f, 1592.0f};
        case 3049: return Destination{3049, "Thundoria Harbor", "garner", 990.0f, 1263.0f};
        case 3050: return Destination{3050, "Sacred Snow Mountain", "garner", 914.0f, 1072.0f};
        case 3051: return Destination{3051, "Andes Forest Haven", "garner", 1007.0f, 2966.0f};
        case 3052: return Destination{3052, "Oasis Haven", "magicsea", 787.0f, 3121.0f};
        case 3053: return Destination{3053, "Icespire Haven", "darkblue", 2681.0f, 647.0f};
        case 3054: return Destination{3054, "Lone Tower Entrance", "garner", 271.0f, 1775.0f};
        case 3055: return Destination{3055, "Barren Cavern Entrance", "magicsea", 1369.0f, 3033.0f};
        case 3056: return Destination{3056, "Abandoned Mine 2", "eastgoaf", 116.0f, 28.0f};
        case 3057: return Destination{3057, "Silver Mine 2", "eastgoaf", 238.0f, 27.0f};
        case 3058: return Destination{3058, "Silver Mine 3", "eastgoaf", 382.0f, 123.0f};
        case 3059: return Destination{3059, "Lone Tower 2", "lonetower", 35.0f, 21.0f};
        case 3060: return Destination{3060, "Lone Tower 3", "lonetower", 120.0f, 406.0f};
        case 3070: return Destination{3070, "Lone Tower 4", "lonetower", 320.0f, 175.0f};
        case 3071: return Destination{3071, "Lone Tower 5", "lonetower", 483.0f, 121.0f};
        case 3072: return Destination{3072, "Lone Tower 6", "lonetower", 494.0f, 297.0f};
        case 3073: return Destination{3073, "Abandon Mine 1", "garner", 1642.0f, 2698.0f};
        case 3076: return Destination{3076, "Spring Town", "darkblue", 3323.0f, 2507.0f};
        case 3828: return Destination{3828, "Thundoria Castle", "garner", 736.0f, 1592.0f};
        case 3829: return Destination{3829, "Thundoria Harbor", "garner", 990.0f, 1263.0f};
        case 3830: return Destination{3830, "Sacred Snow Mountain", "garner", 914.0f, 1072.0f};
        case 3831: return Destination{3831, "Andes Forest Haven", "garner", 1007.0f, 2966.0f};
        case 3832: return Destination{3832, "Oasis Haven", "magicsea", 787.0f, 3121.0f};
        case 3833: return Destination{3833, "Icespire Haven", "darkblue", 2681.0f, 647.0f};
        case 3834: return Destination{3834, "Lone Tower Entrance", "garner", 271.0f, 1775.0f};
        case 3835: return Destination{3835, "Barren Cavern Entrance", "magicsea", 1369.0f, 3033.0f};
        case 3836: return Destination{3836, "Abandoned Mine 2", "eastgoaf", 116.0f, 28.0f};
        case 3837: return Destination{3837, "Silver Mine 2", "eastgoaf", 238.0f, 27.0f};
        case 3838: return Destination{3838, "Silver Mine 3", "eastgoaf", 382.0f, 123.0f};
        case 3839: return Destination{3839, "Lone Tower 2", "lonetower", 35.0f, 21.0f};
        case 3840: return Destination{3840, "Lone Tower 3", "lonetower", 120.0f, 406.0f};
        case 3841: return Destination{3841, "Lone Tower 4", "lonetower", 320.0f, 175.0f};
        case 3842: return Destination{3842, "Lone Tower 5", "lonetower", 483.0f, 121.0f};
        case 3843: return Destination{3843, "Lone Tower 6", "lonetower", 494.0f, 297.0f};
        case 3847: return Destination{3847, "Abandon Mine 1", "garner", 1642.0f, 2698.0f};
        case 4602: return Destination{4602, "Argent City", "garner", 2232.0f, 2787.0f};
        case 4603: return Destination{4603, "Shaitan City", "magicsea", 891.0f, 3575.0f};
        case 4604: return Destination{4604, "Icicle Castle", "darkblue", 1316.0f, 536.0f};
        default: return std::nullopt; // e.g. 3141 has empty destination.
    }
}
Rejection check(const Conditions& c) {
    if (c.level < 1) return Rejection::InvalidLevel;
    if (c.boat) return Rejection::Boat;
    if (c.maxHp <= 0 || c.maxSp <= 0 || c.hp < c.maxHp * 0.5f || c.sp < c.maxSp * 0.5f)
        return Rejection::Health;
    if (c.currentMap == "garner2" || c.currentMap == "prisonisland")
        return Rejection::ProhibitedMap;
    return Rejection::None;
}
} // namespace top::item_tickets
