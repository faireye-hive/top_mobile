#pragma once
// 05-AJ: leitura PASSIVA dos dados de NPC. Nunca executa Lua.
// Apenas chamadas diretas dentro da funcao Lua solicitada sao analisadas;
// comentarios, textos, funcoes locais e funcoes de prefixo semelhante nao
// concedem missoes nem itens de loja.

#include "top/NpcForgeServices.h"
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>

namespace top {
struct NpcLuaData {
    struct DialogOption {
        std::string label;
        int nextPage = -1;
        bool close = false;
        int triggerId = 0;
        bool teamStarTransfer = false;
        std::string directAction;
        int actionParameter = 0;
        std::string actionTarget;
    };
    struct DialogPage {
        std::string text;
        std::vector<DialogOption> options;
    };
    struct ExchangeRule {
        int inputId = 0;
        int inputCount = 0;
        int outputId = 0;
        int outputCount = 0;
        int event = 0;
    };
    std::string firstTalk;
    std::vector<int> missionIds;
    std::vector<int> tradeItems;
    std::vector<ExchangeRule> exchangeRules;
    bool bankService = false;
    std::map<int, DialogPage> dialogPages;
};

namespace npc_lua_detail {
using Token = npc_forge_detail::Token;

inline bool singlePositiveInteger(const std::vector<Token>& ts, std::size_t a, std::size_t b, int& value) {
    if (b != a + 1 || a >= ts.size() || ts[a].identifier) return false;
    const std::string& str = ts[a].value;
    if (str.empty() || str.size() > 9) return false;
    for (unsigned char ch : str) if (ch < '0' || ch > '9') return false;
    const long n = std::strtol(str.c_str(), nullptr, 10); // IDs Lua antigos 0290 sao decimais.
    if (n <= 0 || n > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(n);
    return true;
}

inline bool arguments(const std::vector<Token>& ts, std::size_t i,
                      std::vector<std::pair<std::size_t,std::size_t>>& out) {
    out.clear();
    if (i > 0 && (ts[i-1].value == "." || ts[i-1].value == ":")) return false;
    if (i+1 >= ts.size() || ts[i+1].value != "(") return false;
    int paren = 1, square = 0, curly = 0;
    std::size_t start = i+2;
    for (std::size_t j = start; j < ts.size(); ++j) {
        const std::string& s = ts[j].value;
        if (s == "(") ++paren;
        else if (s == ")") {
            if (--paren == 0) {
                if (j > start || !out.empty()) out.emplace_back(start, j);
                return true;
            }
        } else if (s == "[") ++square;
        else if (s == "]") --square;
        else if (s == "{") ++curly;
        else if (s == "}") --curly;
        else if (s == "," && paren == 1 && square == 0 && curly == 0) {
            out.emplace_back(start, j);
            start = j+1;
        }
    }
    return false; // chamada incompleta
}

inline std::string displayText(const std::string& original) {
    std::string out;
    out.reserve(original.size());
    for (unsigned char c : original) if (c >= 32 && c < 127) out.push_back(static_cast<char>(c));
    return out;
}
} // namespace npc_lua_detail

inline bool readNpcLuaData(const std::string& lua, const std::string& functionName, NpcLuaData& out) {
    out = NpcLuaData{};
    if (functionName.empty() || functionName.size() > 128) return false;
    const auto ts = npc_forge_detail::tokenize(lua);
    std::vector<char> blocks;
    bool target = false, trade = false, exchange = false;
    int nestedFunctions = 0;
    NpcLuaData data;
    using namespace npc_lua_detail;
    for (std::size_t i = 0; i < ts.size(); ++i) {
        const Token& t = ts[i];
        if (!t.identifier) continue;
        if (t.value == "function") {
            if (!target && blocks.empty() && i+2 < ts.size() && ts[i+1].identifier &&
                ts[i+1].value == functionName && ts[i+2].value == "(") {
                target = true;
            } else if (target) ++nestedFunctions;
            blocks.push_back('f');
        } else if (t.value == "if") blocks.push_back('i');
        else if (t.value == "do") blocks.push_back('d');
        else if (t.value == "repeat") blocks.push_back('r');
        else if (t.value == "end" || t.value == "until") {
            if (blocks.empty() || (t.value == "until" && blocks.back() != 'r') ||
                (t.value == "end" && blocks.back() == 'r')) continue;
            const char closing = blocks.back();
            blocks.pop_back();
            if (closing == 'f' && target) {
                if (nestedFunctions > 0) --nestedFunctions;
                else { out = std::move(data); return true; }
            }
        } else if (target && nestedFunctions == 0) {
            if (t.value != "Talk" && t.value != "Text" && t.value != "InitTrade" &&
                t.value != "InitExchangeX" && t.value != "ExchangeDataX" && t.value != "TriggerAction" &&
                t.value != "AddNpcMission" && t.value != "Weapon" &&
                t.value != "Defence" && t.value != "Other" && t.value != "Boat" &&
                t.value != "Pet" && t.value != "Item") continue;
            std::vector<std::pair<std::size_t,std::size_t>> args;
            if (!arguments(ts, i, args)) continue;
            if (t.value == "InitTrade" && args.empty()) { trade = true; continue; }
            if (t.value == "InitExchangeX" && args.empty()) { exchange = true; continue; }
            if (exchange && t.value == "ExchangeDataX" && args.size() == 5 && data.exchangeRules.size() < 4096) {
                int values[5]{};
                bool valid = true;
                for (int field = 0; field < 5; ++field)
                    valid = valid && singlePositiveInteger(ts, args[static_cast<std::size_t>(field)].first,
                                                           args[static_cast<std::size_t>(field)].second,
                                                           values[field]);
                if (valid && values[1] <= 1000000 && values[3] <= 1000000)
                    data.exchangeRules.push_back({values[0], values[1], values[2], values[3], values[4]});
                continue;
            }
            if (t.value == "TriggerAction" && args.size() >= 2 &&
                args.back().second == args.back().first + 1 &&
                ts[args.back().first].identifier && ts[args.back().first].value == "OpenBank") {
                data.bankService = true;
            }
            if (t.value == "Talk" && data.firstTalk.empty() && args.size() >= 2 &&
                args[1].second == args[1].first + 1 &&
                ts[args[1].first].value == "<string>") {
                data.firstTalk = displayText(ts[args[1].first].literal);
                int pageId = 0;
                if (args[0].second == args[0].first + 1 && singlePositiveInteger(ts, args[0].first, args[0].second, pageId))
                    data.dialogPages[pageId].text = data.firstTalk;
            } else if (t.value == "Talk" && args.size() >= 2 &&
                       args[0].second == args[0].first + 1 &&
                       args[1].second == args[1].first + 1 &&
                       ts[args[1].first].value == "<string>") {
                int pageId = 0;
                if (singlePositiveInteger(ts, args[0].first, args[0].second, pageId) && pageId <= 4096)
                    data.dialogPages[pageId].text = displayText(ts[args[1].first].literal);
            } else if (t.value == "Text" && args.size() >= 3 && data.dialogPages.size() < 128 &&
                       args[1].second == args[1].first + 1 && ts[args[1].first].value == "<string>" &&
                       args[2].second == args[2].first + 1 && ts[args[2].first].identifier) {
                int pageId = 0, nextPage = 0;
                if (singlePositiveInteger(ts, args[0].first, args[0].second, pageId) && pageId <= 4096) {
                    const std::string& action = ts[args[2].first].value;
                    if (action == "JumpPage" && args.size() >= 4 &&
                        singlePositiveInteger(ts, args.back().first, args.back().second, nextPage) && nextPage <= 4096) {
                        data.dialogPages[pageId].options.push_back({displayText(ts[args[1].first].literal), nextPage, false});
                    } else if (action == "CloseTalk") {
                        data.dialogPages[pageId].options.push_back({displayText(ts[args[1].first].literal), -1, true});
                    } else if (action == "MultiTrigger" && args.size() >= 5 &&
                               singlePositiveInteger(ts, args.back().first, args.back().second, nextPage)) {
                        data.dialogPages[pageId].options.push_back({displayText(ts[args[1].first].literal), -1, false, nextPage, false});
                    } else if (action == "Transfer_TeamStar") {
                        data.dialogPages[pageId].options.push_back({displayText(ts[args[1].first].literal), -1, false, 0, true});
                    } else if ((action == "GetChaName_4" || action == "GetChaName_5" || action == "TransferDiamond") && args.size() >= 4) {
                        int parameter = 0;
                        if (singlePositiveInteger(ts, args.back().first, args.back().second, parameter)) {
                            data.dialogPages[pageId].options.push_back(
                                {displayText(ts[args[1].first].literal), -1, false, 0, false, action, parameter});
                        }
                    } else if ((action == "Teleport" || action == "SetSpawnPos") && args.size() >= 4 &&
                               ts[args.back().first].value == "<string>" &&
                               !ts[args.back().first].literal.empty()) {
                        data.dialogPages[pageId].options.push_back(
                            {displayText(ts[args[1].first].literal), -1, false, 0, false, action, 0,
                             displayText(ts[args.back().first].literal)});
                    }
                }
            } else if (t.value == "AddNpcMission" && args.size() == 1 && data.missionIds.size() < 2048) {
                int id = 0;
                if (singlePositiveInteger(ts, args[0].first, args[0].second, id)) data.missionIds.push_back(id);
            } else if (trade && args.size() == 1 && data.tradeItems.size() < 4096 &&
                       (t.value == "Weapon" || t.value == "Defence" || t.value == "Other" ||
                        t.value == "Boat" || t.value == "Pet" || t.value == "Item")) {
                int id = 0;
                if (singlePositiveInteger(ts, args[0].first, args[0].second, id)) data.tradeItems.push_back(id);
            }
        }
    }
    return false; // funcao nao encontrada ou truncada: nao usar dados parciais
}
} // namespace top

// 05-AK.3: percorre os tokens de um NpcScript apenas uma vez e extrai
// todas as funcoes Lua de nivel superior. Preserva a semantica passiva
// de readNpcLuaData, sem executar Lua ou inferir chamadas de comentarios.
namespace top {
inline void readNpcLuaDataIndex(const std::string& lua,
                                std::unordered_map<std::string, NpcLuaData>& output) {
    const auto ts = npc_forge_detail::tokenize(lua);
    using namespace npc_lua_detail;
    std::vector<char> blocks;
    std::string current;
    int nestedFunctions = 0;
    bool trade = false;
    bool exchange = false;
    NpcLuaData data;
    for (std::size_t i = 0; i < ts.size(); ++i) {
        const Token& t = ts[i];
        if (!t.identifier) continue;
        if (t.value == "function") {
            if (blocks.empty() && i + 2 < ts.size() && ts[i+1].identifier &&
                ts[i+2].value == "(" && ts[i+1].value.size() <= 128) {
                current = ts[i+1].value;
                data = NpcLuaData{};
                trade = false;
                exchange = false;
                nestedFunctions = 0;
            } else if (!current.empty()) {
                ++nestedFunctions;
            }
            blocks.push_back('f');
        } else if (t.value == "if") blocks.push_back('i');
        else if (t.value == "do") blocks.push_back('d');
        else if (t.value == "repeat") blocks.push_back('r');
        else if (t.value == "end" || t.value == "until") {
            if (blocks.empty() || (t.value == "until" && blocks.back() != 'r') ||
                (t.value == "end" && blocks.back() == 'r')) continue;
            const char closing = blocks.back();
            blocks.pop_back();
            if (closing == 'f' && !current.empty()) {
                if (nestedFunctions > 0) --nestedFunctions;
                else {
                    output.emplace(current, std::move(data));
                    current.clear();
                    trade = false;
                    exchange = false;
                }
            }
        } else if (!current.empty() && nestedFunctions == 0) {
            if (t.value != "Talk" && t.value != "Text" && t.value != "InitTrade" &&
                t.value != "InitExchangeX" && t.value != "ExchangeDataX" && t.value != "TriggerAction" &&
                t.value != "AddNpcMission" && t.value != "Weapon" &&
                t.value != "Defence" && t.value != "Other" && t.value != "Boat" &&
                t.value != "Pet" && t.value != "Item") continue;
            std::vector<std::pair<std::size_t,std::size_t>> args;
            if (!arguments(ts, i, args)) continue;
            if (t.value == "InitTrade" && args.empty()) { trade = true; continue; }
            if (t.value == "InitExchangeX" && args.empty()) { exchange = true; continue; }
            if (exchange && t.value == "ExchangeDataX" && args.size() == 5 && data.exchangeRules.size() < 4096) {
                int values[5]{};
                bool valid = true;
                for (int field = 0; field < 5; ++field)
                    valid = valid && singlePositiveInteger(ts, args[static_cast<std::size_t>(field)].first,
                                                           args[static_cast<std::size_t>(field)].second,
                                                           values[field]);
                if (valid && values[1] <= 1000000 && values[3] <= 1000000)
                    data.exchangeRules.push_back({values[0], values[1], values[2], values[3], values[4]});
                continue;
            }
            if (t.value == "TriggerAction" && args.size() >= 2 &&
                args.back().second == args.back().first + 1 &&
                ts[args.back().first].identifier && ts[args.back().first].value == "OpenBank") {
                data.bankService = true;
            }
            if (t.value == "Talk" && data.firstTalk.empty() && args.size() >= 2 &&
                args[1].second == args[1].first + 1 &&
                ts[args[1].first].value == "<string>") {
                data.firstTalk = displayText(ts[args[1].first].literal);
                int pageId = 0;
                if (args[0].second == args[0].first + 1 && singlePositiveInteger(ts, args[0].first, args[0].second, pageId))
                    data.dialogPages[pageId].text = data.firstTalk;
            } else if (t.value == "Talk" && args.size() >= 2 &&
                       args[0].second == args[0].first + 1 &&
                       args[1].second == args[1].first + 1 &&
                       ts[args[1].first].value == "<string>") {
                int pageId = 0;
                if (singlePositiveInteger(ts, args[0].first, args[0].second, pageId) && pageId <= 4096)
                    data.dialogPages[pageId].text = displayText(ts[args[1].first].literal);
            } else if (t.value == "Text" && args.size() >= 3 && data.dialogPages.size() < 128 &&
                       args[1].second == args[1].first + 1 && ts[args[1].first].value == "<string>" &&
                       args[2].second == args[2].first + 1 && ts[args[2].first].identifier) {
                int pageId = 0, nextPage = 0;
                if (singlePositiveInteger(ts, args[0].first, args[0].second, pageId) && pageId <= 4096) {
                    const std::string& action = ts[args[2].first].value;
                    if (action == "JumpPage" && args.size() >= 4 &&
                        singlePositiveInteger(ts, args.back().first, args.back().second, nextPage) && nextPage <= 4096) {
                        data.dialogPages[pageId].options.push_back({displayText(ts[args[1].first].literal), nextPage, false});
                    } else if (action == "CloseTalk") {
                        data.dialogPages[pageId].options.push_back({displayText(ts[args[1].first].literal), -1, true});
                    } else if (action == "MultiTrigger" && args.size() >= 5 &&
                               singlePositiveInteger(ts, args.back().first, args.back().second, nextPage)) {
                        data.dialogPages[pageId].options.push_back({displayText(ts[args[1].first].literal), -1, false, nextPage, false});
                    } else if (action == "Transfer_TeamStar") {
                        data.dialogPages[pageId].options.push_back({displayText(ts[args[1].first].literal), -1, false, 0, true});
                    } else if ((action == "GetChaName_4" || action == "GetChaName_5" || action == "TransferDiamond") && args.size() >= 4) {
                        int parameter = 0;
                        if (singlePositiveInteger(ts, args.back().first, args.back().second, parameter)) {
                            data.dialogPages[pageId].options.push_back(
                                {displayText(ts[args[1].first].literal), -1, false, 0, false, action, parameter});
                        }
                    } else if ((action == "Teleport" || action == "SetSpawnPos") && args.size() >= 4 &&
                               ts[args.back().first].value == "<string>" &&
                               !ts[args.back().first].literal.empty()) {
                        data.dialogPages[pageId].options.push_back(
                            {displayText(ts[args[1].first].literal), -1, false, 0, false, action, 0,
                             displayText(ts[args.back().first].literal)});
                    }
                }
            } else if (t.value == "AddNpcMission" && args.size() == 1 && data.missionIds.size() < 2048) {
                int id = 0;
                if (singlePositiveInteger(ts, args[0].first, args[0].second, id)) data.missionIds.push_back(id);
            } else if (trade && args.size() == 1 && data.tradeItems.size() < 4096 &&
                       (t.value == "Weapon" || t.value == "Defence" || t.value == "Other" ||
                        t.value == "Boat" || t.value == "Pet" || t.value == "Item")) {
                int id = 0;
                if (singlePositiveInteger(ts, args[0].first, args[0].second, id)) data.tradeItems.push_back(id);
            }
        }
    }
    // Funcoes sem 'end' nao produzem dados; identico ao leitor individual.
}
}  // namespace top
