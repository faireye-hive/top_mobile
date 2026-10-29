// 05-AG: caminhos historicos com capitalizacao Windows, sem renomear Client/.
#include "top/HistoricalPaths.h"
#include "top/NpcSystem.h"
#include "top/PortalSystem.h"
#include "top/MonsterSpawnSystem.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
static void put(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    assert(f);
    f << content;
}
struct RestoreCwd {
    fs::path path = fs::current_path();
    ~RestoreCwd() { fs::current_path(path); }
};
int main() {
    const fs::path sandbox = fs::temp_directory_path() /
        ("top05ag_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(sandbox);
    RestoreCwd restore;
    fs::current_path(sandbox);
    put("Client/map/DreamIsland.map", "fixture");
    put("Client/map/PKmap.map", "fixture");
    put("Client/map/sdBoss.map", "fixture");
    put("Client/scripts/offline/maps/DreamIsland/DreamIslandnpc.txt",
        "1\tGoldie\t0\t1\t1\t219300,270600\t0\t0\tArgent\t0\t0\tr_trade01\n");
    put("Client/scripts/offline/maps/DreamIsland/DreamIslandswhmap.txt",
        "1\t193\t1\t178614,264696\t-1\tgarner\t22967,2878\t1\tSilver Mine\n");
    put("Client/scripts/offline/maps/DreamIsland/DreamIslandChaSpn.txt",
        "2\t11000,12000\t14000,15000\t-1\t96,218\t2,2\t100\t10\t40\t0\tArea\n");
    put("Client/scripts/offline/script/monster/mlist.lua",
        "m = SetCurMap(\"DreamIsland\")\nif m==1 then\n PM(\"dream_mobs\")\nend\n");
    put("Client/scripts/offline/script/monster/dream_mobs.lua",
        "CreateCha(103, 12000, 13100, 280, 120)\n");
    assert(top::historicalFilePath("Client/map", "dreamisland.map").find("DreamIsland.map") != std::string::npos);
    assert(top::historicalFilePath("Client/map", "pkmap.map").find("PKmap.map") != std::string::npos);
    assert(top::historicalFilePath("Client/map", "sdboss.map").find("sdBoss.map") != std::string::npos);
    assert(top::historicalFilePath("Client/map", "../../etc/passwd").empty());
    assert(top::historicalFilePath("Client/map", "/etc/passwd").empty());
    assert(top::historicalFilePath("Client/map", "not_there.map").empty());
    top::NpcSystem npcs;
    assert(npcs.load("dreamisland") && npcs.npcs().size() == 1);
    assert(!npcs.load("../../invalid"));
    top::PortalSystem portals;
    assert(portals.load("dreamisland") && portals.portals().size() == 1);
    top::MonsterSpawnSystem monsters;
    assert(monsters.load("dreamisland") && monsters.regions().size() == 1);
    assert(monsters.scriptFileCount() == 1 && monsters.scripted().size() == 1);
    // Evita escolha arbitraria caso existam dois nomes com a mesma caixa logica.
    put("Client/map/DiFF.map", "first");
    put("Client/map/diff.map", "second");
    assert(top::historicalFilePath("Client/map", "DIFF.map").empty());
    fs::current_path(restore.path);
    fs::remove_all(sandbox);
    std::cout << "05-AG PASS: arquivos Windows casefold; NPC, portal e traversal seguros\n";
}
