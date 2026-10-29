#include "top/ItemRandomRewardRules.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace top::item_random_rewards {
namespace {
bool readFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream stream;
    stream << file.rdbuf();
    out = stream.str();
    return file.good() || file.eof();
}

bool integerField(const std::string& row, const char* name, int& value) {
    // Mesmo padrão ECMAScript de antes. Compilá-lo em cada campo de variable.lua
    // recriava a expressão cerca de 12 mil vezes e segurava a abertura.
    static const std::regex active(R"((?:^|[^A-Za-z0-9_])Active\s*=\s*(-?\d+))");
    static const std::regex itemId(R"((?:^|[^A-Za-z0-9_])ItemID\s*=\s*(-?\d+))");
    static const std::regex quantity(R"((?:^|[^A-Za-z0-9_])Quantity\s*=\s*(-?\d+))");
    static const std::regex quality(R"((?:^|[^A-Za-z0-9_])Quality\s*=\s*(-?\d+))");
    static const std::regex rad(R"((?:^|[^A-Za-z0-9_])Rad\s*=\s*(-?\d+))");
    static const std::regex goodItem(R"((?:^|[^A-Za-z0-9_])GoodItem\s*=\s*(-?\d+))");
    std::smatch match;
    auto matches = [&](const std::regex& pattern) {
        return std::regex_search(row, match, pattern);
    };
    bool matched = false;
    if (std::strcmp(name, "Active") == 0) matched = matches(active);
    else if (std::strcmp(name, "ItemID") == 0) matched = matches(itemId);
    else if (std::strcmp(name, "Quantity") == 0) matched = matches(quantity);
    else if (std::strcmp(name, "Quality") == 0) matched = matches(quality);
    else if (std::strcmp(name, "Rad") == 0) matched = matches(rad);
    else if (std::strcmp(name, "GoodItem") == 0) matched = matches(goodItem);
    else {
        const std::regex fallback(std::string("(?:^|[^A-Za-z0-9_])") + name + R"(\s*=\s*(-?\d+))");
        matched = matches(fallback);
    }
    if (!matched) return false;
    try {
        const long long parsed = std::stoll(match[1].str());
        if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}
} // namespace

bool Rules::load(const std::string& itemEffectPath, const std::string& variablePath) {
    tables_.clear();
    itemTables_.clear();
    jobItemTables_.clear();
    std::string effects;
    std::string variables;
    if (!readFile(itemEffectPath, effects) || !readFile(variablePath, variables)) return false;

    const std::regex functionPattern(R"(function\s+(GiveRandomItems|BlackMarket|ItemUse_JingLingBOX|ItemUse_YingbiBox|ItemUse_HYBOX)\s*\([^\n]*\)([\s\S]*?)(?=\n\s*function\s+|\s*$))");
    const std::regex mappingPattern(R"((rItem|Market)\s*\[\s*(\d+)\s*\]\s*=\s*([A-Za-z_]\w*))");
    std::unordered_set<std::string> requiredTables;
    // The three ChestYmazes items select a literal four-item table by
    // promoted class. Job IDs and BaoXiang table names come from the
    // historical skill/class and reward resources.
    struct ClassChestTable { int itemId; int job; const char* table; };
    static constexpr ClassChestTable classChestTables[] = {
        {9618,9,"BaoXiang_ZSSJ"}, {9618,8,"BaoXiang_ZSJS"},
        {9618,12,"BaoXiang_ZSJJ"}, {9618,13,"BaoXiang_ZSSZ"},
        {9618,14,"BaoXiang_ZSFY"}, {9618,16,"BaoXiang_ZSHH"},
        {9619,9,"BaoXiang_HLSJ"}, {9619,8,"BaoXiang_HLJS"},
        {9619,12,"BaoXiang_HLJJ"}, {9619,13,"BaoXiang_HLSZ"},
        {9619,14,"BaoXiang_HLFY"}, {9619,16,"BaoXiang_HLHH"},
        {9620,9,"BaoXiang_MSJ"}, {9620,8,"BaoXiang_MJS"},
        {9620,12,"BaoXiang_MJJ"}, {9620,13,"BaoXiang_MSZ"},
        {9620,14,"BaoXiang_MFY"}, {9620,16,"BaoXiang_MHH"},
    };
    for (const ClassChestTable& chest : classChestTables) {
        jobItemTables_[chest.itemId][chest.job] = chest.table;
        requiredTables.insert(chest.table);
    }
    for (auto function = std::sregex_iterator(effects.begin(), effects.end(), functionPattern);
         function != std::sregex_iterator(); ++function) {
        const std::string functionName = (*function)[1].str();
        if (functionName == "ItemUse_YingbiBox") {
            itemTables_[1872] = "BaoXiang_CHESTCOIN";
            requiredTables.insert("BaoXiang_CHESTCOIN");
            continue;
        }
        if (functionName == "ItemUse_HYBOX") {
            itemTables_[3901] = "BaoXiang_HYBOX";
            requiredTables.insert("BaoXiang_HYBOX");
            continue;
        }
        if (functionName == "ItemUse_JingLingBOX") {
            itemTables_[262] = "BaoXiang_JingLingBOX";
            requiredTables.insert("BaoXiang_JingLingBOX");
            continue;
        }
        const std::string expectedContainer = functionName == "BlackMarket" ? "Market" : "rItem";
        const std::string functionBody = (*function)[2].str();
        for (auto it = std::sregex_iterator(functionBody.begin(), functionBody.end(), mappingPattern);
             it != std::sregex_iterator(); ++it) {
            try {
                if ((*it)[1].str() != expectedContainer) continue;
                const int itemId = std::stoi((*it)[2].str());
                const std::string tableName = (*it)[3].str();
                if (itemId > 0) {
                    itemTables_[itemId] = tableName;
                    requiredTables.insert(tableName);
                }
            } catch (...) {
                tables_.clear();
                itemTables_.clear();
                jobItemTables_.clear();
                return false;
            }
        }
    }
    if (itemTables_.empty()) return false;

    struct IndexedReward { int index; Reward reward; };
    std::unordered_map<std::string, std::vector<IndexedReward>> indexed;
    const std::regex rowPattern(R"(([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]\s*=\s*\{([^}]*)\})");
    for (auto it = std::sregex_iterator(variables.begin(), variables.end(), rowPattern);
         it != std::sregex_iterator(); ++it) {
        const std::string tableName = (*it)[1].str();
        if (!requiredTables.count(tableName)) continue;
        try {
            const int index = std::stoi((*it)[2].str());
            const std::string row = (*it)[3].str();
            int active = 0;
            Reward reward;
            if (!integerField(row, "Active", active) || active != 1) continue;
            if (!integerField(row, "ItemID", reward.itemId) ||
                !integerField(row, "Quantity", reward.quantity) ||
                !integerField(row, "Quality", reward.quality) ||
                !integerField(row, "Rad", reward.weight)) continue;
            int goodItem = 0;
            if (!integerField(row, "GoodItem", goodItem)) continue;
            reward.announces = goodItem == 1;
            if (index <= 0 || reward.itemId <= 0 || reward.quantity <= 0 || reward.weight <= 0) continue;
            indexed[tableName].push_back({index, reward});
        } catch (...) {
            tables_.clear();
            itemTables_.clear();
            jobItemTables_.clear();
            return false;
        }
    }

    for (const std::string& tableName : requiredTables) {
        auto rows = indexed.find(tableName);
        if (rows == indexed.end() || rows->second.empty()) {
            tables_.clear();
            itemTables_.clear();
            jobItemTables_.clear();
            return false;
        }
        std::sort(rows->second.begin(), rows->second.end(), [](const IndexedReward& a, const IndexedReward& b) {
            return a.index < b.index;
        });
        std::vector<Reward>& rewards = tables_[tableName];
        std::uint64_t sum = 0;
        for (const IndexedReward& row : rows->second) {
            if (sum > std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(row.reward.weight)) {
                tables_.clear();
                itemTables_.clear();
                jobItemTables_.clear();
                return false;
            }
            sum += static_cast<std::uint64_t>(row.reward.weight);
            rewards.push_back(row.reward);
        }
    }
    for (const auto& entry : itemTables_) {
        if (!tables_.count(entry.second)) {
            tables_.clear();
            itemTables_.clear();
            jobItemTables_.clear();
            return false;
        }
    }
    for (const auto& byItem : jobItemTables_) {
        for (const auto& byJob : byItem.second) {
            if (!tables_.count(byJob.second)) {
                tables_.clear();
                itemTables_.clear();
                jobItemTables_.clear();
                return false;
            }
        }
    }
    return true;
}

bool Rules::supports(int itemId) const {
    const auto mapped = itemTables_.find(itemId);
    return mapped != itemTables_.end() && tables_.count(mapped->second) != 0;
}

bool Rules::supports(int itemId, int job) const {
    const auto item = jobItemTables_.find(itemId);
    if (item == jobItemTables_.end()) return false;
    const auto table = item->second.find(job);
    return table != item->second.end() && tables_.count(table->second) != 0;
}

std::uint64_t Rules::totalWeight(int itemId) const {
    const auto mapped = itemTables_.find(itemId);
    if (mapped == itemTables_.end()) return 0;
    const auto table = tables_.find(mapped->second);
    if (table == tables_.end()) return 0;
    std::uint64_t total = 0;
    for (const Reward& reward : table->second) total += static_cast<std::uint64_t>(reward.weight);
    return total;
}

std::uint64_t Rules::totalWeight(int itemId, int job) const {
    const auto item = jobItemTables_.find(itemId);
    if (item == jobItemTables_.end()) return 0;
    const auto mapped = item->second.find(job);
    if (mapped == item->second.end()) return 0;
    const auto table = tables_.find(mapped->second);
    if (table == tables_.end()) return 0;
    std::uint64_t total = 0;
    for (const Reward& reward : table->second) total += static_cast<std::uint64_t>(reward.weight);
    return total;
}

bool Rules::rewardForRoll(int itemId, std::uint64_t roll, Reward& out) const {
    const auto mapped = itemTables_.find(itemId);
    if (mapped == itemTables_.end()) return false;
    const auto table = tables_.find(mapped->second);
    if (table == tables_.end()) return false;
    std::uint64_t cumulative = 0;
    for (const Reward& reward : table->second) {
        cumulative += static_cast<std::uint64_t>(reward.weight);
        if (roll >= 1 && roll <= cumulative) {
            out = reward;
            return true;
        }
    }
    return false;
}

bool Rules::rewardForRoll(int itemId, int job, std::uint64_t roll, Reward& out) const {
    const auto item = jobItemTables_.find(itemId);
    if (item == jobItemTables_.end()) return false;
    const auto mapped = item->second.find(job);
    if (mapped == item->second.end()) return false;
    const auto table = tables_.find(mapped->second);
    if (table == tables_.end()) return false;
    std::uint64_t cumulative = 0;
    for (const Reward& reward : table->second) {
        cumulative += static_cast<std::uint64_t>(reward.weight);
        if (roll >= 1 && roll <= cumulative) {
            out = reward;
            return true;
        }
    }
    return false;
}

} // namespace top::item_random_rewards
