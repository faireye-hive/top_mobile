#include "top/ItemStatRules.h"

#include <cstdio>

int main() {
    top::item_stat_rules::Rules rules;
    if (!rules.load("Client/scripts/offline/calculate/variable.lua") &&
        !rules.load("../Client/scripts/offline/calculate/variable.lua") &&
        !rules.load("../../Client/scripts/offline/calculate/variable.lua")) {
        std::fprintf(stderr, "05-BD: historical ItemStat table did not load\n");
        return 1;
    }
    if (rules.size() != 10) {
        std::fprintf(stderr, "05-BD: expected ten historical stat reset items\n");
        return 2;
    }
    const int ids[] = {899, 900, 901, 902, 903, 3109, 3110, 3111, 3112, 3113};
    const top::item_stat_rules::Attribute attrs[] = {
        top::item_stat_rules::Attribute::Strength,
        top::item_stat_rules::Attribute::Constitution,
        top::item_stat_rules::Attribute::Agility,
        top::item_stat_rules::Attribute::Accuracy,
        top::item_stat_rules::Attribute::Spirit
    };
    for (int i = 0; i < 10; ++i) {
        top::item_stat_rules::Rule rule;
        if (!rules.find(ids[i], rule) || rule.amount != -1 ||
            rule.attribute != attrs[i % 5] || rule.name.empty()) {
            std::fprintf(stderr, "05-BD: stat reset rule mismatch for item %d\n", ids[i]);
            return 3;
        }
    }
    top::item_stat_rules::Rule unknown;
    if (rules.find(904, unknown)) return 4;
    return 0;
}
