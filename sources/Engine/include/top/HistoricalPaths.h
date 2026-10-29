#pragma once
// 05-AG: o cliente historico do Windows nao diferencia maiusculas/minusculas.
// No Linux, procurar cada segmento apenas quando o caminho exato nao existe.
// Nunca aceita segmentos '..' no caminho relativo, nem escolhe entre
// multiplas correspondencias equivalentes: o resultado deve ser deterministico.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>

namespace top {
namespace historical_paths_detail {
inline std::string lowerAscii(std::string str) {
    for (char& c : str) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return str;
}
} // namespace historical_paths_detail

inline std::string historicalFilePath(const std::filesystem::path& root,
                                      const std::filesystem::path& relative) {
    namespace fs = std::filesystem;
    if (relative.empty() || relative.is_absolute()) return {};
    fs::path current = root;
    for (const auto& part : relative) {
        const std::string target = part.string();
        if (target.empty() || target == "." || target == "..") return {};
        std::error_code ec;
        const fs::path exact = current / part;
        if (fs::exists(exact, ec) && !ec) {
            current = exact;
            continue;
        }
        if (!fs::is_directory(current, ec) || ec) return {};
        fs::path only;
        unsigned matches = 0;
        for (fs::directory_iterator it(current, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (historical_paths_detail::lowerAscii(it->path().filename().string()) ==
                historical_paths_detail::lowerAscii(target)) {
                if (++matches > 1) return {}; // ambiguidade, nao escolher ao acaso
                only = it->path();
            }
        }
        if (ec || matches != 1) return {};
        current = only;
    }
    std::error_code ec;
    return fs::is_regular_file(current, ec) && !ec ? current.string() : std::string{};
}
} // namespace top
