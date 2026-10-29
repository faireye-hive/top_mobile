#pragma once

// 05-G: catalogue of effect resource IDs from the original
// Client/scripts/table/magicsingleinfo.txt. It maps a numeric ID to
// model .eff names and particle controller .par names. It does not imply
// that the (DirectX-era) .eff geometry is fully decoded by OpenGL.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace top {

struct MagicEffectDefinition {
    std::vector<std::string> models;
    std::vector<std::string> particles;
    std::string result;
    int renderMode = -1; // magicsingleinfo column 8, DX9 MagicList index
    float velocity = 0.f; // magicsingleinfo column 4; diagnostic for now
};

class MagicEffectCatalog {
public:
    static bool safeName(const std::string& name) {
        if (name.empty() || name.size() > 100 || name == "." || name == "..") return false;
        // An asset name is a filename, not a path supplied by the game data.
        for (unsigned char c : name) {
            if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == '(' || c == ')')) return false;
        }
        return name.find("..") == std::string::npos;
    }

    bool load(const std::string& filename) {
        entries_.clear();
        std::ifstream in(filename);
        if (!in) return false;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '/' || line[0] == '#') continue;
            if (line.size() > 16384 || entries_.size() >= 4096) continue;
            std::vector<std::string> cols;
            std::size_t start = 0;
            while (start <= line.size() && cols.size() < 12) {
                const auto stop = line.find('\t', start);
                cols.push_back(line.substr(start, stop == std::string::npos ? std::string::npos : stop - start));
                if (stop == std::string::npos) break;
                start = stop + 1;
            }
            if (cols.size() < 11) continue;
            const std::string& first = cols[0];
            if (first.empty() || !std::all_of(first.begin(), first.end(), [](unsigned char c){ return std::isdigit(c); })) continue;
            int id = 0;
            try { id = std::stoi(first); } catch (...) { continue; }
            if (id <= 0 || id > 10000000) continue;
            MagicEffectDefinition record;
            splitNames(cols[3], record.models);
            splitNames(cols[6], record.particles);
            try {
                const auto value=trim(cols[8]); std::size_t used=0;
                const int mode=std::stoi(value,&used);
                if(used==value.size() && mode>=0 && mode<=6) record.renderMode=mode;
            } catch (...) {}
            try {
                const auto value=trim(cols[4]); std::size_t used=0;
                const float velocity=std::stof(value,&used);
                if(used==value.size() && std::isfinite(velocity) &&
                   velocity>=0.f && velocity<10000.f) record.velocity=velocity;
            } catch (...) {}
            std::string result = trim(cols[10]);
            if (result != "0" && safeName(result)) record.result = result;
            entries_[id] = std::move(record);
        }
        return !entries_.empty();
    }

    const MagicEffectDefinition* find(int id) const {
        auto it = entries_.find(id);
        return it == entries_.end() ? nullptr : &it->second;
    }
    std::size_t size() const { return entries_.size(); }

private:
    static std::string trim(std::string s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    }
    static void splitNames(const std::string& value, std::vector<std::string>& out) {
        std::size_t at = 0;
        while (at < value.size() && out.size() < 16) {
            const auto end = value.find(',', at);
            const std::string name = trim(value.substr(at, end == std::string::npos ? std::string::npos : end - at));
            if (name != "0" && safeName(name)) out.push_back(name);
            if (end == std::string::npos) break;
            at = end + 1;
        }
    }
    std::unordered_map<int, MagicEffectDefinition> entries_;
};

} // namespace top
