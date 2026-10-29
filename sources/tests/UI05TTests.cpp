#include "top/MonsterLifecycle.h"
#include "top/MonsterSpawnSystem.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

namespace fs = std::filesystem;
static void writeFile(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    assert(out);
    out << text;
    assert(out.good());
}
int main() {
    using namespace top;
    using namespace top::monster_lifecycle;
    assert(respawnDelay(0) == 8.f);
    assert(respawnDelay(120) == 120.f);
    assert(respawnDelay(864001) == 864000.f);
    Registry saved;
    int hp=0; float x=0, y=0, yaw=0, deathElapsed=0;
    // Um mob foi derrotado aos t=20, ja passou 1 s da animacao de morte.
    saved.save("garner", 101, 20., 0, 120.f, 130.f, 1.f, 1.f, 120);
    assert(saved.sizeFor("garner") == 1);
    assert(saved.restore("garner", 101, 40., 100, 120.f, 130.f, 120, hp, x,y,yaw,deathElapsed));
    assert(hp == 0 && deathElapsed > 20.f && deathElapsed < 22.f);
    assert(saved.restore("garner", 101, 138.9, 100, 120.f, 130.f, 120, hp,x,y,yaw,deathElapsed));
    assert(hp == 0);
    assert(saved.restore("garner", 101, 139., 100, 120.f, 130.f, 120, hp,x,y,yaw,deathElapsed));
    assert(hp == 100 && x == 120.f && y == 130.f && deathElapsed == 0.f);
    // Mesma especie em outro spawn e outro mapa nao herda a morte.
    assert(!saved.restore("garner", 102, 40., 100, 0,0, 120, hp,x,y,yaw,deathElapsed));
    assert(!saved.restore("darkblue", 101, 40., 100, 0,0, 120, hp,x,y,yaw,deathElapsed));
    saved.save("garner", 102, 10., 23, 122.f, 131.f, 2.f, 0.f, 120);
    assert(saved.restore("garner", 102, 15., 100, 121.f, 131.f, 120, hp,x,y,yaw,deathElapsed));
    assert(hp == 23 && x == 122.f && y == 131.f && yaw == 2.f);
    saved.save("garner", 103, 10., 0, 123.f, 131.f, 0.f, 0.f, 0);
    assert(saved.restore("garner", 103, 17.9, 100, 123.f, 131.f, 0, hp,x,y,yaw,deathElapsed));
    assert(hp == 0);
    assert(saved.restore("garner", 103, 18., 100, 123.f, 131.f, 0, hp,x,y,yaw,deathElapsed));
    assert(hp == 100);
    saved.clear();
    assert(saved.sizeFor("garner") == 0);

    const fs::path fixture = fs::current_path()/"05t_fixture";
    fs::remove_all(fixture);
    const fs::path previous = fs::current_path();
    const auto scripts = fixture/"Client/scripts/offline/script/monster";
    writeFile(scripts/"mlist.lua", "m = SetCurMap(\"map05t\")\nif m==1 then\n PM(\"map05t_mobs\")\nend\n");
    writeFile(scripts/"map05t_mobs.lua", "CreateCha(103, 12000, 13100, 280, 120)\nCreateCha(103, 12050, 13100, 90, 240)\n");
    writeFile(fixture/"Client/scripts/offline/maps/map05t/map05tChaSpn.txt",
              "2\t11000,12000\t14000,15000\t-1\t96,218\t2,2\t100\t10\t40\t0\tArea\n");
    fs::current_path(fixture);
    MonsterSpawnSystem system;
    assert(system.load("map05t"));
    auto first = system.nearest(120.f, 131.f, 20, 20.f);
    auto next = system.nearest(127.f, 131.f, 20, 20.f);
    std::set<std::uint64_t> keys;
    int lua = 0, table = 0;
    for (const MonsterSlot& mob : first) {
        assert(mob.spawnKey != 0);
        assert(keys.insert(mob.spawnKey).second);
        if (mob.respawnSeconds) {
            ++lua;
            assert(mob.id == 103);
            assert(mob.respawnSeconds == 120 || mob.respawnSeconds == 240);
        } else { ++table; }
    }
    assert(lua == 2 && table > 0);
    for (const MonsterSlot& mob : next) {
        if (!mob.respawnSeconds) continue;
        assert(keys.count(mob.spawnKey)); // mesmo CreateCha nao muda com o streaming
    }
    fs::current_path(previous);
    fs::remove_all(fixture);
    std::puts("05-T: ciclos de respawn, isolamento de mapas e identidade de CreateCha/ChaSpn OK");
}
