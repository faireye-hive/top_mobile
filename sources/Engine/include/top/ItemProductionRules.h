#pragma once

#include <array>
#include <string>
#include <vector>

namespace top::item_production {

enum class Kind { Manufacturing, Crafting, Cooking };

struct Recipe {
    Kind kind = Kind::Manufacturing;
    int index = 0;
    bool active = false;
    int itemId = 0;
    int itemLevel = 0;
    std::array<int, 3> materials{};
    int weight = 0;
    int goodItem = 0;
};

// Two legacy crafting rows point at IDs absent from ItemInfo, while their
// authored comments and item levels match these canonical Client records.
// Keep the source row intact and resolve only when a blueprint is generated.
int canonicalOutputItemId(const Recipe& recipe);

class Rules {
public:
    bool load(const std::string& path);
    std::size_t count(Kind kind, bool activeOnly = false) const;
    const Recipe* find(Kind kind, int index) const;
    int totalWeight(Kind kind, int energy) const;
    const Recipe* recipeForRoll(Kind kind, int energy, int oneBasedRoll) const;

private:
    std::vector<Recipe> recipes_;
};

} // namespace top::item_production
