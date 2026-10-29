#include "top/ItemProductionRules.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace top::item_production {
namespace {
bool integerAfter(const char* line, const char* key, int& value) {
    const char* at = std::strstr(line, key);
    if (!at) return false;
    at += std::strlen(key);
    while (*at == ' ' || *at == '\t') ++at;
    if (*at++ != '=') return false;
    while (*at == ' ' || *at == '\t') ++at;
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(at, &end, 10);
    if (end == at || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX) return false;
    value = static_cast<int>(parsed);
    return true;
}

bool indexAfter(const char* line, const char* prefix, int& index) {
    const char* at = std::strstr(line, prefix);
    if (!at) return false;
    at += std::strlen(prefix);
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(at, &end, 10);
    if (end == at || errno == ERANGE || parsed <= 0 || parsed > INT_MAX) return false;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end != ']') return false;
    index = static_cast<int>(parsed);
    return true;
}

bool rangeFor(Kind kind, int energy, int& first, int& last) {
    if (energy < 1 || energy > 10) return false;
    if (kind == Kind::Manufacturing) {
        first = (energy - 1) * 28 + 1;
        last = energy * 28;
        return true;
    }
    static constexpr int craftFirst[] = {1, 78, 167, 279, 435, 597, 733, 895};
    static constexpr int craftLast[] = {77, 166, 278, 434, 596, 732, 871, 902};
    static constexpr int cookFirst[] = {1, 8, 13, 18, 27, 32, 37, 39};
    static constexpr int cookLast[] = {7, 12, 17, 26, 31, 36, 38, 40};
    if (energy > 8) return false;
    const int slot = energy - 1;
    if (kind == Kind::Crafting) {
        first = craftFirst[slot];
        last = craftLast[slot];
    } else {
        first = cookFirst[slot];
        last = cookLast[slot];
    }
    return true;
}
} // namespace

bool Rules::load(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::vector<Recipe> parsed;
    char line[4096];
    while (std::fgets(line, sizeof line, file)) {
        Kind kind;
        const char* prefix = nullptr;
        if ((prefix = std::strstr(line, "Manufacturing["))) kind = Kind::Manufacturing;
        else if ((prefix = std::strstr(line, "Crafting["))) kind = Kind::Crafting;
        else if ((prefix = std::strstr(line, "Cooking["))) kind = Kind::Cooking;
        else continue;
        Recipe recipe;
        recipe.kind = kind;
        if (!indexAfter(prefix, kind == Kind::Manufacturing ? "Manufacturing[" :
                                kind == Kind::Crafting ? "Crafting[" : "Cooking[", recipe.index)) continue;
        int active = 0;
        if (!integerAfter(line, "Active", active) || !integerAfter(line, "ItemID", recipe.itemId) ||
            !integerAfter(line, "ItemLv", recipe.itemLevel) ||
            !integerAfter(line, "Material1", recipe.materials[0]) ||
            !integerAfter(line, "Material2", recipe.materials[1]) ||
            !integerAfter(line, "Material3", recipe.materials[2]) ||
            !integerAfter(line, "Rad", recipe.weight) || !integerAfter(line, "GoodItem", recipe.goodItem)) continue;
        recipe.active = active == 1;
        if (recipe.itemId <= 0 || recipe.itemLevel < 0 || recipe.weight < 0 ||
            recipe.materials[0] < 0 || recipe.materials[1] < 0 || recipe.materials[2] < 0) continue;
        const auto duplicate = std::find_if(parsed.begin(), parsed.end(), [&](const Recipe& old) {
            return old.kind == recipe.kind && old.index == recipe.index;
        });
        if (duplicate == parsed.end()) parsed.push_back(recipe);
        else *duplicate = recipe;
    }
    const bool okay = std::ferror(file) == 0;
    std::fclose(file);
    if (!okay || parsed.empty()) return false;
    recipes_.swap(parsed);
    return true;
}

int canonicalOutputItemId(const Recipe& recipe) {
    if (recipe.kind != Kind::Crafting) return recipe.itemId;
    if (recipe.index == 539 && recipe.itemId == 2984 && recipe.itemLevel == 55) return 6090;
    if (recipe.index == 688 && recipe.itemId == 3464 && recipe.itemLevel == 65) return 6091;
    return recipe.itemId;
}

std::size_t Rules::count(Kind kind, bool activeOnly) const {
    return static_cast<std::size_t>(std::count_if(recipes_.begin(), recipes_.end(), [&](const Recipe& recipe) {
        return recipe.kind == kind && (!activeOnly || recipe.active);
    }));
}

const Recipe* Rules::find(Kind kind, int index) const {
    const auto found = std::find_if(recipes_.begin(), recipes_.end(), [&](const Recipe& recipe) {
        return recipe.kind == kind && recipe.index == index;
    });
    return found == recipes_.end() ? nullptr : &*found;
}

int Rules::totalWeight(Kind kind, int energy) const {
    int first = 0, last = 0;
    if (!rangeFor(kind, energy, first, last)) return 0;
    long long total = 0;
    for (const Recipe& recipe : recipes_) {
        if (recipe.kind == kind && recipe.active && recipe.index >= first && recipe.index <= last)
            total += recipe.weight;
    }
    return total > INT_MAX ? 0 : static_cast<int>(total);
}

const Recipe* Rules::recipeForRoll(Kind kind, int energy, int oneBasedRoll) const {
    int first = 0, last = 0;
    if (!rangeFor(kind, energy, first, last)) return nullptr;
    int cursor = 0;
    for (const Recipe& recipe : recipes_) {
        if (recipe.kind != kind || !recipe.active || recipe.index < first || recipe.index > last) continue;
        if (recipe.weight <= 0 || cursor > INT_MAX - recipe.weight) continue;
        cursor += recipe.weight;
        if (oneBasedRoll > 0 && oneBasedRoll <= cursor) return &recipe;
    }
    return nullptr;
}

} // namespace top::item_production
