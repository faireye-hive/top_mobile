#include "top/ItemGemVoucherRules.h"

#include <cassert>
#include <cstdio>

int main() {
    top::item_gem_vouchers::Rules rules;
    assert(rules.load("Client/scripts/offline/script/calculate/ItemEffect.lua"));
    assert(rules.rewardCount(9612) == 3);
    assert(rules.rewardCount(9613) == 5);
    assert(rules.rewardCount(9614) == 5);
    assert(rules.rewardCount(9615) == 5);
    assert(rules.rewardCount(9616) == 5);
    assert(rules.rewardCount(9617) == 3);
    int reward = 0;
    assert(rules.rewardForRoll(9612, 1, reward) && reward == 6832);
    assert(rules.rewardForRoll(9612, 3, reward) && reward == 6834);
    assert(rules.rewardForRoll(9616, 1, reward) && reward == 860);
    assert(rules.rewardForRoll(9616, 5, reward) && reward == 1012);
    assert(rules.rewardForRoll(9617, 3, reward) && reward == 866);
    assert(!rules.rewardForRoll(9617, 0, reward));
    assert(!rules.rewardForRoll(9617, 4, reward));
    assert(!rules.supports(9618));
    std::puts("historical GemVouchers reward lists passed");
}
