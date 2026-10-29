#pragma once

// sceneffectinfo.txt maps cast/target scene-effect IDs to particle controllers.
// These IDs are a separate namespace from magicsingleinfo.txt.
#include "top/MagicEffectCatalog.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace top {

struct SceneEffectDefinition {
    std::string particle;
    std::string name;
};

class SceneEffectCatalog {
public:
    bool load(const std::string& filename) {
        entries_.clear();
        std::ifstream in(filename);
        if (!in) return false;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '/' || line[0] == '#' || line.size() > 16384) continue;
            std::string idText;
            std::string particle;
            std::string name;
            const auto first = line.find('\t');
            if (first == std::string::npos) continue;
            const auto second = line.find('\t', first + 1);
            if (second == std::string::npos) continue;
            idText = line.substr(0, first);
            particle = line.substr(first + 1, second - first - 1);
            const auto third = line.find('\t', second + 1);
            name = line.substr(second + 1, third == std::string::npos ? std::string::npos : third - second - 1);
            if (idText.empty() || !std::all_of(idText.begin(), idText.end(), [](unsigned char c) {
                    return std::isdigit(c) != 0;
                })) continue;
            int id = 0;
            try { id = std::stoi(idText); } catch (...) { continue; }
            if (id <= 0 || id > 10000000 || !MagicEffectCatalog::safeName(particle)) continue;
            entries_[id] = {std::move(particle), std::move(name)};
        }
        return !entries_.empty();
    }

    const SceneEffectDefinition* find(int id) const {
        const auto found = entries_.find(id);
        return found == entries_.end() ? nullptr : &found->second;
    }
    std::size_t size() const { return entries_.size(); }

private:
    std::unordered_map<int, SceneEffectDefinition> entries_;
};

} // namespace top
