#include "top/ItemProductionRules.h"

#include <cstdio>

int main() {
    top::item_production::Rules rules;
    if (!rules.load("Client/scripts/offline/script/calculate/variable.lua") &&
        !rules.load("../Client/scripts/offline/script/calculate/variable.lua") &&
        !rules.load("../../Client/scripts/offline/script/calculate/variable.lua")) {
        std::fprintf(stderr, "05-BE: historical production tables did not load\n");
        return 1;
    }
    using top::item_production::Kind;
    if (rules.count(Kind::Manufacturing) != 280 || rules.count(Kind::Crafting) != 904 ||
        rules.count(Kind::Cooking) != 40) {
        std::fprintf(stderr, "05-BE: recipe counts M=%zu C=%zu K=%zu\n",
                     rules.count(Kind::Manufacturing), rules.count(Kind::Crafting), rules.count(Kind::Cooking));
        return 2;
    }
    const auto* first = rules.find(Kind::Manufacturing, 1);
    if (!first || !first->active || first->itemId != 1067 || first->itemLevel != 10 ||
        first->materials != std::array<int,3>{4418,3999,1677} || first->weight != 5) {
        std::fprintf(stderr, "05-BE: first manufacturing recipe was not parsed exactly\n");
        return 3;
    }
    const int total = rules.totalWeight(Kind::Manufacturing, 1);
    const auto* craft904 = rules.find(Kind::Crafting, 904);
    const int craftTierEight = rules.totalWeight(Kind::Crafting, 8);
    const auto* lastCraftRoll = rules.recipeForRoll(Kind::Crafting, 8, craftTierEight);
    if (total <= 0 || rules.recipeForRoll(Kind::Manufacturing, 1, 0) ||
        rules.recipeForRoll(Kind::Manufacturing, 1, total + 1) ||
        rules.recipeForRoll(Kind::Manufacturing, 1, 1) != first ||
        craftTierEight <= 0 || !craft904 || !craft904->active || !lastCraftRoll || lastCraftRoll->index > 902 ||
        rules.totalWeight(Kind::Cooking, 8) <= 0 ||
        rules.totalWeight(Kind::Cooking, 9) != 0) {
        std::fprintf(stderr, "05-BE: historical tier ranges/weighted selection are invalid\n");
        return 4;
    }
    return 0;
}
