#include "top/ItemStatRules.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace top::item_stat_rules {

bool Rules::load(const std::string& variablePath) {
    rules_.clear();
    std::ifstream file(variablePath, std::ios::binary);
    if (!file) return false;
    std::ostringstream stream;
    stream << file.rdbuf();
    const std::string data = stream.str();
    const std::regex row(R"REGEX(Server\.Restore\.Attr\s*\[\s*(\d+)\s*\]\s*=\s*\{\s*Name\s*=\s*"([^"]+)"\s*,\s*Amount\s*=\s*(-?\d+)\s*,\s*Attribute\s*=\s*(ATTR_B(?:STR|CON|AGI|DEX|STA))\s*\})REGEX");
    for (auto it = std::sregex_iterator(data.begin(), data.end(), row);
         it != std::sregex_iterator(); ++it) {
        try {
            const int id = std::stoi((*it)[1].str());
            Rule rule;
            rule.name = (*it)[2].str();
            rule.amount = std::stoi((*it)[3].str());
            const std::string attribute = (*it)[4].str();
            if (attribute == "ATTR_BSTR") rule.attribute = Attribute::Strength;
            else if (attribute == "ATTR_BCON") rule.attribute = Attribute::Constitution;
            else if (attribute == "ATTR_BAGI") rule.attribute = Attribute::Agility;
            else if (attribute == "ATTR_BDEX") rule.attribute = Attribute::Accuracy;
            else if (attribute == "ATTR_BSTA") rule.attribute = Attribute::Spirit;
            else continue;
            if (id > 0 && rule.amount != 0) rules_[id] = std::move(rule);
        } catch (...) {
            rules_.clear();
            return false;
        }
    }
    return !rules_.empty();
}

bool Rules::find(int itemId, Rule& out) const {
    const auto it = rules_.find(itemId);
    if (it == rules_.end()) return false;
    out = it->second;
    return true;
}

} // namespace top::item_stat_rules
