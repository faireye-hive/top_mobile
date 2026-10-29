#pragma once
// 05-AA: declaracoes literais de variable.lua e regras de can_milling_item_main.
// O script Lua nao e executado. Sem configuracao historica valida, nao perfurar.
#include <algorithm>
#include <fstream>
#include <regex>
#include <string>

namespace top::socket_rules {
struct Config {
    int limit = 0;
    int cost = 0;
    bool valid() const { return limit >= 1 && limit <= 3 && cost > 0 && cost <= 100000000; }
};
inline Config load(const std::string& path) {
    std::ifstream input(path);
    Config rules;
    static const std::regex limitRx(R"(^\s*Server\.Socket\.Limit\s*=\s*(\d+)\b)");
    static const std::regex costRx(R"(^\s*Server\.Socket\.Cost\s*=\s*(\d+)\b)");
    std::string line;
    for (; std::getline(input,line);) {
        std::smatch match;
        if (std::regex_search(line,match,limitRx)) {
            try { rules.limit=std::stoi(match[1]); } catch (...) { rules.limit=0; }
        } else if (std::regex_search(line,match,costRx)) {
            try { rules.cost=std::stoi(match[1]); } catch (...) { rules.cost=0; }
        }
    }
    return rules.valid() ? rules : Config{};
}
inline int cost(const Config& rules, int existing, int maxSockets, int maxDurability) {
    if (!rules.valid() || existing < 0 || maxSockets <= 0 ||
        existing >= std::min(rules.limit,maxSockets) ||
        maxDurability == 25000) return -1; // forge.lua: apparel proibido
    return (existing+1)*rules.cost;
}
inline bool materials(int id1, int id2) {
    // forge.lua Check_Jiaguji + Check_Cuihuafen, em qualquer ordem.
    return (id1==890 && id2==891) || (id1==891 && id2==890);
}
inline constexpr int successChancePer10000(int sockets) {
    // forge.lua Check_CG_damo: 0,1,2 encaixes = Percentage_Random(1.0).
    return sockets>=0 && sockets<3 ? 10000 : 0;
}
} // namespace top::socket_rules
