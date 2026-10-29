#include "top/ItemRandomRewardRules.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

int main() {
    const std::vector<std::pair<std::string,std::string>> roots = {
        {"Client/scripts/offline/script/calculate/ItemEffect.lua", "Client/scripts/offline/calculate/variable.lua"},
        {"../Client/scripts/offline/script/calculate/ItemEffect.lua", "../Client/scripts/offline/calculate/variable.lua"},
        {"../../Client/scripts/offline/script/calculate/ItemEffect.lua", "../../Client/scripts/offline/calculate/variable.lua"}
    };
    top::item_random_rewards::Rules rules;
    bool loaded = false;
    for (const auto& root : roots) {
        if (rules.load(root.first, root.second)) { loaded = true; break; }
    }
    if (!loaded || rules.mappedItems() != 56 || !rules.supports(3905) ||
        !rules.supports(1094) || !rules.supports(9600) || rules.supports(999999)) {
        std::fprintf(stderr, "05-BC: GiveRandomItems historical mapping failed to load (loaded=%d mappings=%zu)\n",
                     loaded ? 1 : 0, rules.mappedItems());
        return 1;
    }
    if (rules.totalWeight(3905) != 10564 || rules.totalWeight(1094) != 3063 ||
        rules.totalWeight(3903) != 9200 || rules.totalWeight(9600) != 2630) {
        std::fprintf(stderr, "05-BC: historical weight totals differ from Lua tables\n");
        return 2;
    }
    top::item_random_rewards::Reward reward;
    if (!rules.rewardForRoll(3905, 1, reward) || reward.itemId != 3909 ||
        reward.quantity != 1 || reward.quality != 5 || reward.announces ||
        !rules.rewardForRoll(1094, 1, reward) || reward.itemId != 3098 ||
        reward.quantity != 4 || !rules.rewardForRoll(3903, 1, reward) ||
        reward.itemId != 3851 || !rules.rewardForRoll(1815, 1, reward) ||
        reward.itemId != 1787 || reward.quantity != 2 || reward.quality != 7 ||
        !rules.rewardForRoll(9600, 1, reward) || reward.itemId != 7002 || reward.quality != 4 ||
        !rules.rewardForRoll(3905, 10564, reward) ||
        reward.itemId != 3135 || rules.rewardForRoll(3905, 0, reward) ||
        rules.rewardForRoll(3905, 10565, reward)) {
        std::fprintf(stderr, "05-BC: weighted reward boundaries differ from Lua order\n");
        return 3;
    }
    bool sawAnnouncement = false;
    for (std::uint64_t roll = 1; roll <= rules.totalWeight(1094); ++roll) {
        if (rules.rewardForRoll(1094, roll, reward) && reward.announces) {
            sawAnnouncement = true;
            break;
        }
    }
    if (!sawAnnouncement) {
        std::fprintf(stderr, "05-BC: GoodItem flags were not preserved\n");
        return 4;
    }
    return 0;
}
