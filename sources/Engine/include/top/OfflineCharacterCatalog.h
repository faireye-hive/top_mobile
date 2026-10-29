#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace top {

struct OfflineCharacterProfile {
    int id = 0;          // Identidade do save, nunca o ID de raça/modelo.
    int raceId = 0;      // characterinfo/ActorSkin: Lance=1, Carsise=2, Phyllis=3, Ami=4.
    std::string name;
    std::string birthCity;
    std::string birthMap;
    int birthX = 0;
    int birthY = 0;
};

struct BirthPoint {
    std::string city;
    std::string map;
    int x = 0;
    int y = 0;
};

inline bool loadHistoricalBirthPoints(const std::string& path, std::vector<BirthPoint>& out) {
    std::ifstream file(path);
    if (!file) return false;
    std::vector<BirthPoint> parsed;
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("AddBirthPoint(") == std::string::npos) continue;
        char city[49] = {}, map[49] = {};
        int x = -1, y = -1;
        if (std::sscanf(line.c_str(), "AddBirthPoint(\"%48[^\"]\", \"%48[^\"]\", %d, %d)",
                        city, map, &x, &y) != 4) continue;
        BirthPoint point{city, map, x, y};
        if (!point.city.empty() && !point.map.empty() && x >= 0 && y >= 0 &&
            point.city.find_first_of("\t\r\n") == std::string::npos &&
            point.map.find_first_of("\t\r\n") == std::string::npos)
            parsed.push_back(std::move(point));
    }
    if (parsed.empty()) return false;
    out = std::move(parsed);
    return true;
}

// Perfil offline independente do login/servidor. O formato é TSV versionado,
// deliberadamente pequeno e legível para facilitar recuperação manual do save.
class OfflineCharacterCatalog {
public:
    static constexpr std::size_t kMaxCharacters = 4; // MAX_CHA_NUM do cliente original.

    const std::vector<OfflineCharacterProfile>& profiles() const { return profiles_; }
    int activeId() const { return activeId_; }
    const OfflineCharacterProfile* find(int id) const {
        const auto it = std::find_if(profiles_.begin(), profiles_.end(),
            [id](const OfflineCharacterProfile& profile) { return profile.id == id; });
        return it == profiles_.end() ? nullptr : &*it;
    }

    bool load(const std::string& root) {
        root_ = root;
        profiles_.clear();
        activeId_ = 0;
        std::ifstream file(root_ + "/user/offline-characters.tsv");
        if (!file) return true; // Catálogo ainda não criado.
        std::string line;
        if (!std::getline(file, line) || line != "TOP_OFFLINE_CHARACTERS\t1") return false;
        while (std::getline(file, line)) {
            OfflineCharacterProfile profile;
            std::vector<std::string> columns;
            std::size_t start = 0;
            for (;;) {
                const std::size_t tab = line.find('\t', start);
                columns.push_back(line.substr(start, tab == std::string::npos ? tab : tab - start));
                if (tab == std::string::npos) break;
                start = tab + 1;
            }
            if (columns.size() != 7 || !parseInt(columns[0], profile.id) ||
                !parseInt(columns[1], profile.raceId) || !parseInt(columns[5], profile.birthX) ||
                !parseInt(columns[6], profile.birthY)) return false;
            profile.name = columns[2];
            profile.birthCity = columns[3];
            profile.birthMap = columns[4];
            if (!valid(profile) || findIn(profiles_, profile.id) || profiles_.size() >= kMaxCharacters)
                return false;
            profiles_.push_back(std::move(profile));
        }
        std::ifstream active(root_ + "/user/offline-profile.txt");
        if (active) {
            std::string value;
            if (!(active >> value) || !parseInt(value, activeId_)) return false;
            if (activeId_ != 0 && !find(activeId_)) return false;
        }
        return true;
    }

    // Crea com a primeira coordenada cadastrada para a cidade na birth_conf
    // histórica; a etapa de entrada no mapa ainda valida/pousa o ponto no chão.
    bool create(int raceId, const std::string& name, const std::string& city,
                const std::vector<BirthPoint>& birthPoints, int& createdId) {
        createdId = 0;
        if (profiles_.size() >= kMaxCharacters || raceId < 1 || raceId > 4 ||
            !validName(name) || !validText(city, 48)) return false;
        if (std::any_of(profiles_.begin(), profiles_.end(), [&](const auto& p) { return p.name == name; }))
            return false;
        const auto point = std::find_if(birthPoints.begin(), birthPoints.end(),
            [&](const BirthPoint& candidate) { return candidate.city == city; });
        if (point == birthPoints.end() || !validText(point->map, 48)) return false;
        int id = 1001;
        for (;;) {
            std::ifstream orphanedSave(root_ + "/user/offline-save-" + std::to_string(id) + ".txt");
            if (!find(id) && !orphanedSave) break;
            if (id == 2147483647) return false;
            ++id;
        }
        OfflineCharacterProfile profile{id, raceId, name, city, point->map, point->x, point->y};
        profiles_.push_back(std::move(profile));
        createdId = id;
        return true;
    }

    bool importLegacy(int raceId, const std::string& name, const BirthPoint& birth) {
        if (profiles_.size() >= kMaxCharacters || raceId < 1 || raceId > 4 ||
            !validName(name) || !validText(birth.city, 48) ||
            !validText(birth.map, 48) || birth.city.empty() || birth.map.empty() ||
            birth.x < 0 || birth.y < 0 || find(raceId)) return false;
        profiles_.push_back({raceId, raceId, name, birth.city, birth.map, birth.x, birth.y});
        activeId_ = raceId; // Mantém offline-save-{raceId}.txt sem copiar nem renomear.
        return true;
    }

    bool erase(int id) {
        const auto it = std::find_if(profiles_.begin(), profiles_.end(),
            [id](const OfflineCharacterProfile& profile) { return profile.id == id; });
        if (it == profiles_.end()) return false;
        profiles_.erase(it);
        if (activeId_ == id) activeId_ = profiles_.empty() ? 0 : profiles_.front().id;
        return true;
    }

    bool select(int id) {
        if (id != 0 && !find(id)) return false;
        activeId_ = id;
        return true;
    }

    bool save() const {
        if (root_.empty()) return false;
        const std::string path = root_ + "/user/offline-characters.tsv";
        const std::string temp = path + ".tmp";
        FILE* file = std::fopen(temp.c_str(), "wb");
        if (!file) return false;
        bool ok = std::fprintf(file, "TOP_OFFLINE_CHARACTERS\t1\n") > 0;
        for (const auto& profile : profiles_) {
            if (!valid(profile)) { ok = false; break; }
            if (std::fprintf(file, "%d\t%d\t%s\t%s\t%s\t%d\t%d\n", profile.id,
                    profile.raceId, profile.name.c_str(), profile.birthCity.c_str(),
                    profile.birthMap.c_str(), profile.birthX, profile.birthY) < 0) {
                ok = false;
                break;
            }
        }
        if (std::fflush(file) != 0) ok = false;
        if (std::fclose(file) != 0) ok = false;
        if (!ok || std::rename(temp.c_str(), path.c_str()) != 0) {
            std::remove(temp.c_str());
            return false;
        }
        const std::string activePath = root_ + "/user/offline-profile.txt";
        const std::string activeTemp = activePath + ".tmp";
        file = std::fopen(activeTemp.c_str(), "wb");
        if (!file) return false;
        ok = std::fprintf(file, "%d\n", activeId_) > 0 && std::fflush(file) == 0;
        if (std::fclose(file) != 0) ok = false;
        if (!ok || std::rename(activeTemp.c_str(), activePath.c_str()) != 0) {
            std::remove(activeTemp.c_str());
            return false;
        }
        return true;
    }

private:
    static bool parseInt(const std::string& text, int& value) {
        if (text.empty()) return false;
        char* end = nullptr;
        const long parsed = std::strtol(text.c_str(), &end, 10);
        if (!end || *end || parsed < 0 || parsed > 2147483647L) return false;
        value = static_cast<int>(parsed);
        return true;
    }
    static bool validText(const std::string& value, std::size_t maxBytes) {
        if (value.size() > maxBytes) return false;
        return value.find_first_of("\t\r\n") == std::string::npos;
    }
    static bool validName(const std::string& name) {
        if (name.empty() || !validText(name, 24)) return false;
        bool hasAlphaNumeric = false;
        for (unsigned char ch : name) {
            const bool alnum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                               (ch >= '0' && ch <= '9');
            if (!alnum && ch != ' ' && ch != '_' && ch != '-') return false;
            hasAlphaNumeric = hasAlphaNumeric || alnum;
        }
        return hasAlphaNumeric;
    }
    static bool valid(const OfflineCharacterProfile& profile) {
        return profile.id > 0 && profile.raceId >= 1 && profile.raceId <= 4 &&
            validName(profile.name) &&
            !profile.birthCity.empty() && validText(profile.birthCity, 48) &&
            !profile.birthMap.empty() && validText(profile.birthMap, 48) &&
            profile.birthX >= 0 && profile.birthY >= 0;
    }
    static bool findIn(const std::vector<OfflineCharacterProfile>& profiles, int id) {
        return std::any_of(profiles.begin(), profiles.end(),
            [id](const auto& profile) { return profile.id == id; });
    }

    std::string root_;
    std::vector<OfflineCharacterProfile> profiles_;
    int activeId_ = 0;
};

} // namespace top
