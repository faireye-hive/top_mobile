#pragma once
// 05-AE: extracao conservadora dos servicos do NPC a partir dos menus Lua.
// Nao executa o script: somente chamadas Text(..., OpenForge/OpenUnite/OpenMilling/OpenRepair/OpenGetStone)
// com a acao como TERCEIRO argumento podem habilitar uma aba da bancada.
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace top {
enum NpcForgeService : unsigned {
    kNpcForge = 1u, kNpcCombine = 2u, kNpcSocket = 4u, kNpcRepair = 16u,
    kNpcExtractGem = 32u, kNpcTiChun = 64u, kNpcUpgrade = 128u
};

namespace npc_forge_detail {
struct Token {
    std::string value;
    bool identifier = false;
    std::string literal; // 05-AJ: texto Lua original para dialogos (sem executar Lua)
};

inline bool nameStart(unsigned char c) { return std::isalpha(c) || c == '_'; }
inline bool nameBody(unsigned char c) { return std::isalnum(c) || c == '_'; }

// Lua [=[ ... ]=] tem um numero arbitrario de '='. Retorna -1 se '['
// nao introduzir string/comentario longo.
inline int longBracketLevel(const std::string& text, std::size_t at) {
    if (at >= text.size() || text[at] != '[') return -1;
    std::size_t pos = at + 1;
    while (pos < text.size() && text[pos] == '=') ++pos;
    if (pos == text.size() || text[pos] != '[') return -1;
    return static_cast<int>(pos - at - 1);
}

inline std::size_t skipLongBracket(const std::string& src, std::size_t start, int level) {
    std::string close = "]" + std::string(static_cast<std::size_t>(level), '=') + "]";
    const std::size_t data = start + static_cast<std::size_t>(level) + 2;
    const std::size_t end = src.find(close, data);
    return end == std::string::npos ? src.size() : end + close.size();
}

inline std::vector<Token> tokenize(const std::string& src) {
    std::vector<Token> tokens;
    tokens.reserve(std::min<std::size_t>(src.size() / 12, 20000));
    for (std::size_t i = 0; i < src.size();) {
        const unsigned char c = static_cast<unsigned char>(src[i]);
        if (std::isspace(c)) { ++i; continue; }
        if (src.compare(i, 2, "--") == 0) {
            const int level = longBracketLevel(src, i + 2);
            if (level >= 0) i = skipLongBracket(src, i + 2, level);
            else {
                const std::size_t end = src.find('\n', i + 2);
                i = end == std::string::npos ? src.size() : end + 1;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            const char quote = src[i++];
            std::string literal;
            bool terminated = false;
            while (i < src.size()) {
                if (src[i] == '\\' && i + 1 < src.size()) {
                    const char escaped = src[i+1];
                    literal.push_back(escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped);
                    i += 2; continue;
                }
                if (src[i] == quote) { ++i; terminated = true; break; }
                literal.push_back(src[i++]);
            }
            // Comentarios e conteudo de strings NUNCA viram tokens de codigo.
            if (!terminated) break;
            tokens.push_back({"<string>", false, std::move(literal)});
            continue;
        }
        if (c == '[') {
            const int level = longBracketLevel(src, i);
            if (level >= 0) {
                const std::size_t next = skipLongBracket(src, i, level);
                if (next == src.size() && (src.size() < 2 || src.back() != ']')) break;
                const std::size_t opening = i + static_cast<std::size_t>(level) + 2;
                const std::size_t closing = next - static_cast<std::size_t>(level) - 2;
                const std::string literal = closing >= opening ? src.substr(opening, closing-opening) : std::string{};
                i = next;
                tokens.push_back({"<string>", false, literal});
                continue;
            }
        }
        if (nameStart(c)) {
            const std::size_t begin = i++;
            while (i < src.size() && nameBody(static_cast<unsigned char>(src[i]))) ++i;
            tokens.push_back({src.substr(begin, i - begin), true});
            continue;
        }
        if (std::isdigit(c)) {
            const std::size_t begin = i++;
            while (i < src.size() && (nameBody(static_cast<unsigned char>(src[i])) || src[i] == '.')) ++i;
            tokens.push_back({src.substr(begin, i - begin), false});
            continue;
        }
        tokens.push_back({src.substr(i++, 1), false});
    }
    return tokens;
}

// Rejeita chamadas onde a palavra OpenForge apareca somente no rotulo,
// em outro argumento, em expressao composta ou em uma funcao auxiliar.
inline unsigned serviceFromTextCall(const std::vector<Token>& ts, std::size_t at) {
    if (at > 0 && (ts[at - 1].value == "." || ts[at - 1].value == ":")) return 0;
    if (at + 1 >= ts.size() || ts[at + 1].value != "(") return 0;
    std::vector<std::pair<std::size_t, std::size_t>> args;
    std::size_t start = at + 2;
    int paren = 1, square = 0, curly = 0;
    for (std::size_t j = start; j < ts.size(); ++j) {
        const std::string& t = ts[j].value;
        if (t == "(") ++paren;
        else if (t == ")" && --paren == 0) {
            args.emplace_back(start, j);
            break;
        } else if (t == "[") ++square;
        else if (t == "]") --square;
        else if (t == "{") ++curly;
        else if (t == "}") --curly;
        else if (t == "," && paren == 1 && square == 0 && curly == 0) {
            args.emplace_back(start, j);
            start = j + 1;
        }
    }
    if (args.size() != 3 || args[2].second != args[2].first + 1) return 0;
    const Token& action = ts[args[2].first];
    if (!action.identifier) return 0;
    if (action.value == "OpenForge") return kNpcForge;
    if (action.value == "OpenUnite") return kNpcCombine;
    if (action.value == "OpenMilling") return kNpcSocket;
    if (action.value == "OpenRepair") return kNpcRepair;
    if (action.value == "OpenGetStone") return kNpcExtractGem;
    if (action.value == "OpenItemTiChun") return kNpcTiChun;
    if (action.value == "OpenUpgrade") return kNpcUpgrade;
    return 0;
}
} // namespace npc_forge_detail

// found (opcional) separa funcao existente sem servicos de funcao ausente.
// Isso evita conceder servicos de outra definicao homonima em outro arquivo.
inline unsigned readNpcForgeServices(const std::string& lua, const std::string& functionName,
                                     bool* found = nullptr) {
    if (found) *found = false;
    if (functionName.empty()) return 0;
    using namespace npc_forge_detail;
    const auto ts = tokenize(lua);
    std::vector<char> blocks;
    bool target = false;
    unsigned services = 0;
    int nestedFunctions = 0;
    for (std::size_t i = 0; i < ts.size(); ++i) {
        const Token& t = ts[i];
        if (!t.identifier) continue;
        if (t.value == "function") {
            if (!target && blocks.empty() && i + 2 < ts.size() &&
                ts[i + 1].identifier && ts[i + 1].value == functionName && ts[i + 2].value == "(") {
                target = true;
                if (found) *found = true;
            } else if (target) ++nestedFunctions;
            blocks.push_back('f');
        } else if (t.value == "if") blocks.push_back('i');
        else if (t.value == "do") blocks.push_back('d');
        else if (t.value == "repeat") blocks.push_back('r');
        else if (t.value == "end" || t.value == "until") {
            if (blocks.empty()) continue;
            if (t.value == "until" && blocks.back() != 'r') continue;
            if (t.value == "end" && blocks.back() == 'r') continue;
            const char closing = blocks.back();
            blocks.pop_back();
            if (closing == 'f' && target) {
                if (nestedFunctions > 0) --nestedFunctions;
                else return services;
            }
        } else if (target && nestedFunctions == 0 && t.value == "Text") {
            services |= serviceFromTextCall(ts, i);
        }
    }
    // Arquivo parcial ou funcao sem fechamento: negar permissao (fail closed).
    if (found) *found = false;
    return 0;
}
} // namespace top
