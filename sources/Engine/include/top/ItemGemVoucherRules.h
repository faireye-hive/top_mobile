#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace top::item_gem_vouchers {

// Literal reward lists from GemVouchers in the historical ItemEffect.lua.
// roll is one-based, matching Lua's math.random(1, #list).
class Rules {
public:
    bool load(const char* itemEffectPath);
    bool supports(int voucherId) const;
    std::size_t rewardCount(int voucherId) const;
    bool rewardForRoll(int voucherId, std::uint64_t roll, int& rewardId) const;

private:
    std::unordered_map<int, std::vector<int>> rewards_;
};

} // namespace top::item_gem_vouchers
