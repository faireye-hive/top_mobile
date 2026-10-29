#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace top::item_stat_rules {

enum class Attribute { Strength, Constitution, Agility, Accuracy, Spirit };

struct Rule {
    std::string name;
    int amount = 0;
    Attribute attribute = Attribute::Strength;
};

// Reads only the literal Server.Restore.Attr entries used by ItemStat.
class Rules {
public:
    bool load(const std::string& variablePath);
    bool find(int itemId, Rule& out) const;
    std::size_t size() const { return rules_.size(); }

private:
    std::unordered_map<int, Rule> rules_;
};

} // namespace top::item_stat_rules
