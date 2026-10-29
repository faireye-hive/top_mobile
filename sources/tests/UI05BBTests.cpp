#include "top/OfflineCharacterCatalog.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    const std::string root = "/tmp/top-offline-characters-" + std::to_string(::getpid());
    if (::mkdir(root.c_str(), 0700) != 0) return 1;
    if (::mkdir((root + "/user").c_str(), 0700) != 0) return 2;
    const std::vector<top::BirthPoint> births = {
        {"Argent City", "garner", 2232, 2787},
        {"Shaitan City", "magicsea", 891, 3575},
        {"Icicle Castle", "darkblue", 1316, 536},
    };
    const std::string birthFixture = root + "/birth_conf.lua";
    {
        std::ofstream points(birthFixture);
        points << "AddBirthPoint(\"Argent City\", \"garner\", 2232, 2787)\n"
               << "AddBirthPoint(\"Shaitan City\", \"magicsea\", 891, 3575)\n"
               << "-- AddBirthPoint(\"Ignored\", \"garner\", 1, 2)\n";
    }
    std::vector<top::BirthPoint> parsedBirths;
    if (!top::loadHistoricalBirthPoints(birthFixture, parsedBirths) || parsedBirths.size() != 2 ||
        parsedBirths[1].city != "Shaitan City" || parsedBirths[1].x != 891) return 8;
    std::remove(birthFixture.c_str());
    parsedBirths.clear();
    if (!top::loadHistoricalBirthPoints("Client/scripts/offline/maps/script/birth/birth_conf.lua", parsedBirths))
        return 9;
    const auto hasBirth = [&](const std::string& city, const std::string& map, int x, int y) {
        for (const auto& point : parsedBirths)
            if (point.city == city && point.map == map && point.x == x && point.y == y) return true;
        return false;
    };
    if (!hasBirth("Argent City", "garner", 2232, 2787) ||
        !hasBirth("Shaitan City", "magicsea", 891, 3575) ||
        !hasBirth("Icicle Castle", "darkblue", 1316, 536)) return 10;
    top::OfflineCharacterCatalog catalog;
    if (!catalog.load(root)) return 3;
    {
        std::ofstream orphan(root + "/user/offline-save-1001.txt");
        orphan << "kept after profile deletion\n";
    }
    int lance = 0, carsise = 0, phyllis = 0, ami = 0, extra = 0;
    if (!catalog.create(1, "Offline Lance", "Argent City", births, lance) ||
        !catalog.create(2, "Offline Carsise", "Shaitan City", births, carsise) ||
        !catalog.create(3, "Offline Phyllis", "Icicle Castle", births, phyllis) ||
        !catalog.create(4, "Offline Ami", "Argent City", births, ami) ||
        catalog.create(3, "Fifth Character", "Argent City", births, extra) ||
        catalog.create(1, "Offline Lance", "Argent City", births, extra) ||
        catalog.create(1, "   ", "Argent City", births, extra) || lance != 1002) return 4;
    const auto* p = catalog.find(phyllis);
    if (!p || p->raceId != 3 || p->birthMap != "darkblue" || p->birthX != 1316 ||
        !catalog.select(phyllis) || !catalog.save()) return 5;

    top::OfflineCharacterCatalog restored;
    if (!restored.load(root) || restored.profiles().size() != 4 ||
        restored.activeId() != phyllis || !restored.find(ami) || !restored.erase(phyllis) ||
        restored.activeId() == phyllis || !restored.save()) return 6;

    // Malformed rows must fail closed instead of silently creating a shared save identity.
    {
        std::ofstream corrupt(root + "/user/offline-characters.tsv", std::ios::trunc);
        corrupt << "TOP_OFFLINE_CHARACTERS\t1\n1001\t3\tBad\tCity\tdarkblue\t-1\t20\n";
    }
    top::OfflineCharacterCatalog invalid;
    const bool rejected = !invalid.load(root);
    std::remove((root + "/user/offline-characters.tsv").c_str());
    std::remove((root + "/user/offline-profile.txt").c_str());
    std::remove((root + "/user/offline-save-1001.txt").c_str());
    ::rmdir((root + "/user").c_str());
    ::rmdir(root.c_str());
    if (!rejected) return 7;

    const std::string legacyRoot = root + "-legacy";
    if (::mkdir(legacyRoot.c_str(), 0700) != 0 || ::mkdir((legacyRoot + "/user").c_str(), 0700) != 0)
        return 11;
    top::OfflineCharacterCatalog legacy;
    if (!legacy.load(legacyRoot) || !legacy.importLegacy(1, "Lance", births.front()) ||
        legacy.activeId() != 1 || !legacy.save()) return 12;
    top::OfflineCharacterCatalog imported;
    if (!imported.load(legacyRoot) || imported.activeId() != 1 || !imported.find(1) ||
        imported.find(1)->raceId != 1) return 13;
    std::remove((legacyRoot + "/user/offline-characters.tsv").c_str());
    std::remove((legacyRoot + "/user/offline-profile.txt").c_str());
    ::rmdir((legacyRoot + "/user").c_str());
    ::rmdir(legacyRoot.c_str());
    return 0;
}
