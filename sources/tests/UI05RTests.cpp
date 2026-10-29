#include "top/MonsterSpawnSystem.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

namespace fs = std::filesystem;

void put(const std::string& map, const std::string& lines) {
    const fs::path dir = fs::path("Client/scripts/offline/maps") / map;
    fs::create_directories(dir);
    std::ofstream file(dir / (map + "ChaSpn.txt"), std::ios::binary);
    assert(file);
    file << lines;
    assert(file.good());
}

int main(int argc, char** argv) {
    using top::MonsterSlot;
    using top::MonsterSpawnRegion;
    using top::MonsterSpawnSystem;
    assert(MonsterSpawnSystem::selfTest());
    const fs::path testDir = "Client/scripts/offline/maps";
    put("test05ra", "// heading\n"
        "1\t10000,10000\t14000,14000\t-1\t234,228,233\t3,3,2\t100,100,100\t10\t40\t0\tRegion A\n"
        "2\t10000,10000\t14000,14000\t-1\t46,32\t2,2\t100,100\t10\t40\t0\tRegion B\n"
        "3\t10000,10000\t14000,14000\t-1\t999\t0\t100\t10\t40\t0\tDisabled\n");
    put("test05rb", "1\t10000,10000\t14000,14000\t-1\t96,218\t2,2\t100\t10\t40\t0\tShaitan\n");
    put("test05rc", "// mapa historico sem ChaSpn\n");

    MonsterSpawnSystem system;
    assert(system.load("test05ra"));
    assert(system.regions().size() == 3);
    const auto first = system.nearest(120.f, 120.f, 20, 84.f);
    assert(!first.empty());
    std::set<int> types;
    for (const MonsterSlot& mob : first) {
        assert(mob.x >= 100.f && mob.x <= 140.f);
        assert(mob.y >= 100.f && mob.y <= 140.f);
        assert(mob.id != 999); // quantidade zero, spawn desativado
        types.insert(mob.id);
    }
    assert(types.size() == 5); // diversidade nao dominada por um unico tipo
    assert(first.size() == 12); // respeitar quantidade historica e teto 3/tipo
    // A posicao nao desliza quando o jogador se move dentro da mesma celula 8 m.
    const auto stable = system.nearest(121.f, 121.f, 20, 84.f);
    assert(first.size() == stable.size());
    // Ordenacao por proximidade pode variar; os monstros e suas coordenadas nao.
    for (const auto& mob : first) {
        bool found = false;
        for (const auto& candidate : stable)
            found |= mob.id == candidate.id && mob.x == candidate.x && mob.y == candidate.y;
        assert(found);
    }
    MonsterSpawnRegion region;
    float distance = 0.f;
    assert(system.closestRegion(200.f, 200.f, region, distance));
    assert(std::abs(distance - std::sqrt(7200.f)) < .01f);
    assert(system.nearest(400.f, 400.f, 20, 84.f).empty());
    assert(system.nearest(120.f, 120.f, 0, 84.f).empty());
    assert(system.nearest(NAN, 120.f, 20, 84.f).empty());
    assert(system.nearest(120.f, 120.f, 20, NAN).empty());
    assert(system.nearest(120.f, 120.f, 20, 84.f).size() == first.size());

    // Carregar outro mapa limpa a populacao anterior e usa apenas seus IDs.
    assert(system.load("test05rb"));
    const auto second = system.nearest(120.f, 120.f, 20, 84.f);
    assert(second.size() == 4);
    for (const auto& mob : second) assert(mob.id == 96 || mob.id == 218);
    assert(!system.load("test05rc"));
    assert(system.regions().empty());
    assert(system.nearest(120.f, 120.f).empty());
    assert(!system.closestRegion(120.f, 120.f, region, distance));
    assert(!system.load("test05r_missing"));
    assert(system.regions().empty());

    fs::remove_all(testDir / "test05ra");
    fs::remove_all(testDir / "test05rb");
    fs::remove_all(testDir / "test05rc");

    if (argc > 1) {
        const fs::path before = fs::current_path();
        fs::current_path(argv[1]);
        std::set<int> families;
        assert(system.load("darkblue") && system.regions().size() > 100);
        for (const auto& row : system.regions())
            for (int id : row.ids) families.insert(id);
        assert(families.count(234) && families.count(228));
        assert(system.load("magicsea") && system.regions().size() > 70);
        families.clear();
        for (const auto& row : system.regions())
            for (int id : row.ids) families.insert(id);
        assert(families.count(96) && families.count(218));
        assert(!system.load("jialebi") && system.regions().empty());
        fs::current_path(before);
    }
    std::puts("05-R: regioes por mapa, IDs reais, variedade, spawn vazio e streaming OK");
}
