#include "top/PortalSystem.h"
#include "top/HistoricalPaths.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace top {
namespace {

std::vector<std::string> tabs(const std::string& line) {
    std::vector<std::string> cols;
    std::size_t at = 0;
    while (at <= line.size()) {
        const std::size_t end = line.find('\t', at);
        if (end == std::string::npos) {
            cols.push_back(line.substr(at));
            break;
        }
        cols.push_back(line.substr(at, end - at));
        at = end + 1;
    }
    return cols;
}

bool point(const std::string& text, float& x, float& y) {
    int cmX = 0;
    int cmY = 0;
    if (std::sscanf(text.c_str(), "%d,%d", &cmX, &cmY) != 2) return false;
    x = static_cast<float>(cmX) / 100.f;
    y = static_cast<float>(cmY) / 100.f;
    return true;
}

}  // namespace

bool PortalSystem::parse(const std::string& line, Portal& out) {
    if (line.empty() || line.rfind("//", 0) == 0) return false;
    const std::vector<std::string> cols = tabs(line);
    if (cols.size() < 9) return false;
    out = Portal{};
    out.id = std::atoi(cols[0].c_str());
    if (out.id <= 0 || !point(cols[3], out.x, out.y) || !point(cols[6], out.targetX, out.targetY)) return false;
    out.targetMap = cols[5];
    out.name = cols[8];
    while (!out.name.empty() && (out.name.back() == '\r' || out.name.back() == '\n')) out.name.pop_back();
    return !out.targetMap.empty();
}

bool PortalSystem::load(const std::string& mapName) {
    portals_.clear();
    // Os scripts antigos podem usar DreamIsland/PKmap na pasta fisica,
    // enquanto o identificador interno do mapa e lowercase.
    if (mapName.empty() || mapName.size() > 48 ||
        !std::all_of(mapName.begin(), mapName.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '_';
        })) return false;
    FILE* file = nullptr;
    // Mantem a preferencia do layout maps/, seguida do layout antigo.
    for (const std::string& layout : {"scripts/offline/maps/", "scripts/offline/"}) {
        if (file) break;
        for (const char* root : {"Client", "../Client", "../../Client"}) {
            const std::string relative = layout + mapName + "/" + mapName + "swhmap.txt";
            const std::string actual = historicalFilePath(root, relative);
            if (!actual.empty() && (file = std::fopen(actual.c_str(), "rb"))) break;
        }
    }
    if (!file) return false;
    char line[4096];
    while (std::fgets(line, sizeof line, file)) {
        Portal portal;
        if (parse(line, portal)) portals_.push_back(std::move(portal));
    }
    std::fclose(file);
    return !portals_.empty();
}

bool PortalSystem::near(float x, float y, float radius, Portal& out) const {
    const float limit = radius * radius;
    for (const Portal& portal : portals_) {
        const float dx = portal.x - x;
        const float dy = portal.y - y;
        if (dx * dx + dy * dy <= limit) {
            out = portal;
            return true;
        }
    }
    return false;
}

bool PortalSystem::selfTest() {
    Portal portal;
    if (!parse("1\t193\t1\t178614,264696\t-1\teastgoaf\t22967,2878\t1\tSilver Mine", portal)) return false;
    if (portal.id != 1 || portal.targetMap != "eastgoaf" || portal.name != "Silver Mine") return false;
    if (portal.x < 1786.13f || portal.x > 1786.15f || portal.targetY < 28.77f || portal.targetY > 28.79f) return false;
    PortalSystem system;
    system.portals_.push_back(portal);
    Portal found;
    return system.near(1786.14f, 2646.96f, 0.1f, found) && found.id == 1;
}

}  // namespace top
