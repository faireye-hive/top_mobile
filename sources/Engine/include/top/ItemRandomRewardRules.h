#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace top::item_random_rewards {

struct Reward {
    int itemId = 0;
    int quantity = 0;
    int quality = 0;
    int weight = 0;
    bool announces = false;
};

// Reads only literal mappings and weighted rows used by historical
// GiveRandomItems and BlackMarket callbacks; this is not a Lua interpreter.
class Rules {
public:
    bool load(const std::string& itemEffectPath, const std::string& variablePath);
    bool supports(int itemId) const;
    bool supports(int itemId, int job) const;
    std::uint64_t totalWeight(int itemId) const;
    std::uint64_t totalWeight(int itemId, int job) const;
    bool rewardForRoll(int itemId, std::uint64_t roll, Reward& out) const;
    bool rewardForRoll(int itemId, int job, std::uint64_t roll, Reward& out) const;
    std::size_t mappedItems() const { return itemTables_.size(); }

private:
    std::unordered_map<std::string, std::vector<Reward>> tables_;
    std::unordered_map<int, std::string> itemTables_;
    std::unordered_map<int, std::unordered_map<int, std::string>> jobItemTables_;
};

} // namespace top::item_random_rewards
