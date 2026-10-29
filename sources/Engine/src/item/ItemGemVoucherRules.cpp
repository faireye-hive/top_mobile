#include "top/ItemGemVoucherRules.h"

#include <fstream>
#include <limits>
#include <regex>
#include <sstream>

namespace top::item_gem_vouchers {
namespace {
bool readFile(const char* path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream stream;
    stream << file.rdbuf();
    out = stream.str();
    return file.good() || file.eof();
}

bool positiveDecimal(const std::string& text, int& out) {
    try {
        std::size_t used = 0;
        const long long value = std::stoll(text, &used, 10);
        if (used != text.size() || value <= 0 || value > std::numeric_limits<int>::max()) return false;
        out = static_cast<int>(value);
        return true;
    } catch (...) {
        return false;
    }
}
} // namespace

bool Rules::load(const char* itemEffectPath) {
    rewards_.clear();
    std::string source;
    if (!itemEffectPath || !readFile(itemEffectPath, source)) return false;

    const std::regex functionPattern(
        R"(function\s+GemVouchers\s*\([^\n]*\)([\s\S]*?)(?=\n\s*function\s+|\s*$))");
    std::smatch functionMatch;
    if (!std::regex_search(source, functionMatch, functionPattern)) return false;
    const std::string body = functionMatch[1].str();
    const std::regex tablePattern(R"(VoucherVar\s*\[\s*(\d+)\s*\]\s*=\s*\{([^}]*)\})");
    const std::regex rewardPattern(R"((\d+))");
    for (auto table = std::sregex_iterator(body.begin(), body.end(), tablePattern);
         table != std::sregex_iterator(); ++table) {
        int voucher = 0;
        if (!positiveDecimal((*table)[1].str(), voucher)) {
            rewards_.clear();
            return false;
        }
        std::vector<int> ids;
        const std::string row = (*table)[2].str();
        for (auto item = std::sregex_iterator(row.begin(), row.end(), rewardPattern);
             item != std::sregex_iterator(); ++item) {
            int id = 0;
            if (!positiveDecimal((*item)[1].str(), id)) {
                rewards_.clear();
                return false;
            }
            ids.push_back(id);
        }
        if (ids.empty() || !rewards_.emplace(voucher, std::move(ids)).second) {
            rewards_.clear();
            return false;
        }
    }
    return !rewards_.empty();
}

bool Rules::supports(int voucherId) const {
    return rewards_.find(voucherId) != rewards_.end();
}

std::size_t Rules::rewardCount(int voucherId) const {
    const auto it = rewards_.find(voucherId);
    return it == rewards_.end() ? 0u : it->second.size();
}

bool Rules::rewardForRoll(int voucherId, std::uint64_t roll, int& rewardId) const {
    const auto it = rewards_.find(voucherId);
    if (it == rewards_.end() || roll == 0 || roll > it->second.size()) return false;
    rewardId = it->second[static_cast<std::size_t>(roll - 1)];
    return true;
}

} // namespace top::item_gem_vouchers
