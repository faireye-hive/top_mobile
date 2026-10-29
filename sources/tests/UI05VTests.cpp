// 05-V: PDEF, CRT e leitura das colunas historicas; sem dependencia OpenGL.
#include "top/HistoricalStats.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    using namespace top::historical_stats;
    const Monster hunter = monsterFromColumns(68, 4540, 0, 434, 651, 114, 229, 277, 27, 10);
    assert(hunter.hp == 4540 && hunter.physicalResistance == 27 && hunter.criticalPercent == 10);
    assert(physicalDamage(500, 100, 0) == 400);
    assert(physicalDamage(500, 100, 27) == 265); // floor(500*.73)-100
    assert(physicalDamage(500, 100, 70) == 166); // floor(500*(1-70/150))-100
    assert(physicalDamage(1000, 0, 500) == 150); // teto historico: 85%
    assert(physicalDamage(10, 500, 30) == 1);    // minimo offline preservado
    assert(criticalPermille(10, 68, 68) == 100);
    assert(criticalPermille(10, 73, 68) == 250);
    assert(criticalPermille(10, 68, 73) == 100);
    assert(criticalPermille(100, 99, 1) == 1000);
    assert(critical(10, 68, 68, 99));
    assert(!critical(10, 68, 68, 100));
    assert(missPermille(80, 90) == 200);
    assert(monsterFromColumns(1, 20, 0, 1, 2, 0, 0, 0, -7, 900).physicalResistance == 0);
    assert(monsterFromColumns(1, 20, 0, 1, 2, 0, 0, 0, -7, 900).criticalPercent == 100);

    // Ficha real, se Client foi instalado no diretorio do CTest.
    for (const char* root : {"Client", "../Client", "../../Client"}) {
        std::ifstream f(std::string(root) + "/scripts/table/characterinfo.txt", std::ios::binary);
        if (!f) continue;
        std::string line;
        bool found = false;
        while (std::getline(f, line)) {
            std::vector<std::string> fields;
            std::stringstream stream(line);
            std::string cell;
            while (std::getline(stream, cell, '\t')) fields.push_back(cell);
            if (fields.size() < 72 || fields[0] != "201") continue;
            assert(std::stoi(fields[67]) == 27 && std::stoi(fields[71]) == 10);
            found = true;
            break;
        }
        assert(found);
        break;
    }
}
