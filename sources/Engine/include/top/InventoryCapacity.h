#pragma once
#include <algorithm>

namespace top::inventory_rules {
// Historical iteminfo.txt: 3088 -> 28, 3089 -> 32, ... 3093 -> 48.
inline constexpr int kBaseBagCapacity = 24;
inline constexpr int kMaxBagCapacity = 48;
inline int upgradeTarget(int itemId) {
    return itemId >= 3088 && itemId <= 3093 ? 28 + (itemId - 3088) * 4 : 0;
}
inline bool canUpgrade(int itemId, int currentCapacity) {
    return upgradeTarget(itemId) == currentCapacity + 4;
}
inline int validatedCapacity(int capacity) {
    return (capacity >= kBaseBagCapacity && capacity <= kMaxBagCapacity && capacity % 4 == 0)
        ? capacity : kBaseBagCapacity;
}
} // namespace top::inventory_rules
