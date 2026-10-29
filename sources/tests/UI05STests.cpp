#include "top/MonsterSpawnSystem.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
namespace fs = std::filesystem;
static void write(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p); assert(out); out << data; assert(out.good());
}
int main(int argc, char** argv) {
    const fs::path dir = fs::current_path() / "05s_fixture";
    const fs::path before = fs::current_path();
    fs::remove_all(dir);
    const fs::path monster = dir / "Client/scripts/offline/script/monster";
    write(monster / "mlist.lua", R"LUA(function PM(monster)
  dofile(GetResPath(monster))
end
local m = SetCurMap("test05s")
if m==1 then
  PM("test05s01")
  -- PM("test05s_holiday")
  if os.date("%m") == "12" then
    PM("test05s_holiday")
  end
  PM("test05s02")
end
m = SetCurMap("not_test05s")
if m==1 then
  PM("bad_map")
end
)LUA");
    write(monster / "test05s01.lua", R"LUA(-- CreateCha(999, 12000, 12000, 90, 20)
CreateCha(207, 12000, 13000, 280, 120)
local cha=CreateCha(469, 12050, 13000, 145, 240)
CreateCha(207, 12000, 13000, 280, 120) -- duplicado
SetChaSideID(cha, 1)
CreateCha(999, tonumber("12000"), 13000, 90, 120)
CreateCha(999, -10, 13000, 90, 120)
)LUA");
    write(monster / "test05s02.lua", "GroupOne = CreateCha(738, 12000, 13100, 145, 700)\n");
    write(monster / "test05s_holiday.lua", "CreateCha(888, 12000, 13000, 90, 10)\n");
    write(monster / "bad_map.lua", "CreateCha(777, 12000, 13000, 90, 10)\n");
    write(dir / "Client/scripts/offline/maps/test05s/test05sChaSpn.txt",
          "1\t11000,12000\t14000,15000\t-1\t96,218\t2,2\t100\t10\t40\t0\tB\n");
    fs::current_path(dir);
    top::MonsterSpawnSystem sp;
    assert(sp.load("test05s"));
    assert(sp.scriptFileCount() == 2);
    assert(sp.scripted().size() == 3);
    assert(sp.regions().size() == 1);
    const auto nearby = sp.nearest(120.f, 130.f, 12, 15.f);
    assert(nearby.size() >= 3);
    std::set<int> found;
    for (const auto& mob : nearby) {
        found.insert(mob.id);
        assert(mob.id != 999 && mob.id != 888 && mob.id != 777);
    }
    assert(found.count(207) && found.count(469) && found.count(738));
    assert(std::abs(sp.scripted().front().x - 120.f) < .001f);
    assert(std::abs(sp.scripted().front().y - 130.f) < .001f);
    assert(sp.scripted().front().respawnSeconds == 120);
    assert(sp.scripted().front().facing == 280);
    const auto again = sp.nearest(121.f, 130.f, 12, 15.f);
    for (const auto& mob : nearby) {
        if (mob.id != 207 && mob.id != 469 && mob.id != 738) continue;
        bool matched = false;
        for (const auto& other : again)
            if (other.id == mob.id && other.x == mob.x && other.y == mob.y) matched = true;
        assert(matched);
    }
    top::MonsterSpawnRegion nearest;
    float distance = 0.f;
    assert(sp.closestRegion(120.f, 130.f, nearest, distance));
    assert(distance <= .01f);
    assert(nearest.name.find("Lua") != std::string::npos);
    assert(!sp.load("not_test05s") || sp.scripted().size() == 1);
    assert(sp.scripted().size() == 1 && sp.scripted().front().id == 777);
    assert(!sp.load("missing05s"));
    assert(sp.scripted().empty() && sp.regions().empty());
    assert(!sp.load("../test05s"));
    // Regressao: o clamp nao pode empilhar 20 monstros na quina da regiao.
    write(dir / "Client/scripts/offline/maps/edge05s/edge05sChaSpn.txt",
          "1\t1300,1000\t14800,14700\t-1\t75,93,218,253,88,99,82\t3,3,3,3,3,3,3\t100\t10\t40\t0\tB\n");
    assert(sp.load("edge05s"));
    const auto edge = sp.nearest(2.5f, 2.5f, 20, 84.f);
    assert(edge.size() >= 10);
    std::set<std::pair<int, int>> positions;
    for (const auto& mob : edge) {
        assert(mob.x >= 13.f && mob.x <= 148.f);
        assert(mob.y >= 10.f && mob.y <= 147.f);
        positions.insert({static_cast<int>(mob.x * 10), static_cast<int>(mob.y * 10)});
    }
    assert(positions.size() >= 8);
    assert(!positions.count({130, 100}));
    fs::current_path(before);
    fs::remove_all(dir);
    if (argc > 1) {
        fs::current_path(argv[1]);
        for (const std::string name : {"garner", "magicsea", "darkblue", "eastgoaf", "secretgarden"}) {
            assert(sp.load(name));
            assert(sp.scriptFileCount() > 0);
            assert(sp.scripted().size() > 0);
            std::printf("05-S %s: %zu scripts, %zu registros Lua\n", name.c_str(),
                        sp.scriptFileCount(), sp.scripted().size());
        }
        assert(sp.load("secretgarden"));
        assert(sp.regions().empty()); // monstros exclusivamente CreateCha
        assert(!sp.nearest(220.f, 70.f, 20, 50.f).empty());
        fs::current_path(before);
    }
    std::puts("05-S: Lua CreateCha literal por mlist, posicoes reais e isolamento de mapas OK");
}
