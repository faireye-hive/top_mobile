#include "top/TexturePaths.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace top {
namespace {
bool exists(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}
std::string key(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
std::vector<std::string> csvFields(const std::string& line) {
    std::vector<std::string> cols;
    std::string word;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') { word += '"'; ++i; }
            else quoted = !quoted;
        } else if (c == ',' && !quoted) { cols.push_back(word); word.clear(); }
        else if (c != '\r') word += c;
    }
    cols.push_back(word);
    return cols;
}
struct Manifest {
    bool loaded = false;
    std::unordered_map<std::string, std::string> files;
};
}
std::string resolveTexturePath(const std::string& historicalPath) {
    std::string normalized = historicalPath;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const std::string low = key(normalized);
    const std::size_t marker = low.rfind("/texture/");
    const std::size_t start = marker != std::string::npos ? marker + 1 : (low.rfind("texture/", 0) == 0 ? 0 : std::string::npos);
    if (start == std::string::npos) return exists(historicalPath) ? historicalPath : "";
    const std::string root = normalized.substr(0, start + 8); // inclui "texture/"
    const std::string relative = normalized.substr(start + 8);
    if (relative.empty()) return {};
    static std::unordered_map<std::string, Manifest> caches;
    Manifest& manifest = caches[root];
    if (!manifest.loaded) {
        manifest.loaded = true;
        std::ifstream in(root + "texture_manifest.csv");
        std::string line;
        std::getline(in, line); // cabecalho
        while (std::getline(in, line)) {
            const auto cols = csvFields(line);
            if (cols.size() >= 3 && !cols[0].empty() && !cols[1].empty()) {
                manifest.files.emplace(key(cols[0]), cols[1]);
            }
        }
    }
    const auto it = manifest.files.find(key(relative));
    if (it != manifest.files.end()) {
        const std::string mapped = root + it->second;
        if (exists(mapped)) return mapped;
    }
    if (exists(historicalPath)) return historicalPath;
    // Compatibilidade com conversoes sem manifesto (nao resolve colisoes).
    if (exists(historicalPath + ".png")) return historicalPath + ".png";
    // Linux filesystems are case-sensitive. Older client textures have
    // mixed-case names (Ice1.TGA.png) while .eff records store "ice1".
    // Only look for an exact case-insensitive filename inside the same
    // effect-texture directory; do not guess replacements by similar names.
    if (key(relative).rfind("effect/", 0) == 0) {
        const std::filesystem::path expected(historicalPath + ".png");
        std::error_code ec;
        const auto dir=expected.parent_path();
        if (std::filesystem::is_directory(dir,ec)) {
            const std::string wanted=key(expected.filename().string());
            std::string match;
            for (std::filesystem::directory_iterator it(dir,ec),end;!ec && it!=end;it.increment(ec)) {
                if (!it->is_regular_file(ec)) continue;
                if (key(it->path().filename().string()) != wanted) continue;
                const std::string candidate=it->path().string();
                if (!match.empty()) { match.clear(); break; } // ambiguous case collision
                match=candidate;
            }
            if (!ec && !match.empty()) return match;
        }
    }
    return {};
}
} // namespace top
