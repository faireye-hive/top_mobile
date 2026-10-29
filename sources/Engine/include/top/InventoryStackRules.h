#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace top::inventory_rules {
// Keeps old save format (id,count), but validates complete insertions atomically.
// Every Stack must expose .id and .count fields.
template <typename Stack>
bool insert(std::vector<Stack>& stacks, int id, int count, int maxStack, int slots) {
    if (id <= 0 || count <= 0 || maxStack <= 0 || slots <= 0) return false;
    std::vector<Stack> next = stacks;
    for (Stack& item : next) {
        if (item.id != id || item.count >= maxStack) continue;
        const int moved = std::min(count, maxStack - item.count);
        item.count += moved;
        count -= moved;
        if (count == 0) break;
    }
    while (count > 0) {
        if (next.size() >= static_cast<std::size_t>(slots)) return false;
        const int moved = std::min(count, maxStack);
        next.push_back({id, moved});
        count -= moved;
    }
    stacks.swap(next);
    return true;
}
}  // namespace top::inventory_rules
