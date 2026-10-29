#include "top/HistoricalStats.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

int main() {
    using namespace top::historical_stats;
    const auto fixture = std::filesystem::temp_directory_path() / "top_05u_experience.lua";
    { std::ofstream f(fixture); assert(f);
      f << "DEXP = {}\nDEXP[1] = 0\nDEXP[2] = 5\nDEXP[3] = 15\n";
      for (int n = 4; n <= 100; ++n) f << "DEXP[" << n << "] = " << (15LL + 10LL * (n-3)) << "\n";
    }
    Experience synthetic;
    assert(synthetic.load(fixture.string()));
    assert(synthetic.needed(1) == 5 && synthetic.needed(2) == 10);
    assert(synthetic.needed(3) == 10 && synthetic.needed(100) == 0);
    std::filesystem::remove(fixture);
    // A tabela verdadeira inclui > INT32 nas experiencias cumulativas.
    Experience historical;
    const char* candidates[] = {
        "Client/scripts/offline/calculate/variable.lua",
        "../Client/scripts/offline/calculate/variable.lua",
        "../../Client/scripts/offline/calculate/variable.lua"
    };
    bool found = false;
    for (const char* candidate : candidates) if (historical.load(candidate)) { found = true; break; }
    if (found) {
        assert(historical.needed(1) == 5);
        assert(historical.needed(4) == 66);
        assert(historical.needed(9) == 1234);
        assert(historical.cumulative(100) == 4176709541LL);
    }
    const auto strong = monsterFromColumns(68, 4540, 1250, 434, 651, 114, 229, 277);
    assert(strong.hp == 4540 && strong.maxAttack == 651 && strong.defense == 114);
    assert(strong.hit == 229 && strong.dodge == 277 && strong.experience == 1250);
    assert(monsterFromColumns(51, 16760, 0, 548, 823, 163, 184, 200).hp == 16760);
    assert(missPermille(20, 100) == 900); // max 90% (legacy)
    assert(missPermille(100, 20) == 0);
    assert(missPermille(10, 10) == 100);
    assert(!misses(0, 0, 0));
    assert(physicalDamage(434, 114) == 320);
    assert(physicalDamage(10, 80) == 1);
    assert(physicalDamage(100, 0, 50) == 50);
    // Valida fichas reais, nao apenas valores codificados no teste.
    for (const auto& dirname : {std::string("Client"), std::string("../Client"),
                                std::string("../../Client")}) {
        std::ifstream source(dirname + "/scripts/table/characterinfo.txt", std::ios::binary);
        if (!source) continue;
        int bossCount = 0;
        std::string line;
        while (std::getline(source, line)) {
            if (line.size() < 4 || line[0] == '/') continue;
            std::vector<std::string> cols;
            std::stringstream tokens(line);
            std::string field;
            while (std::getline(tokens, field, '\t')) cols.push_back(field);
            if (cols.size() <= 70) continue;
            const int id = std::atoi(cols[0].c_str());
            if (id != 201 && id != 701) continue;
            const auto real = monsterFromColumns(std::atoi(cols[60].c_str()),
                std::atoi(cols[61].c_str()), std::atoi(cols[62].c_str()),
                std::atoi(cols[65].c_str()), std::atoi(cols[66].c_str()),
                std::atoi(cols[68].c_str()), std::atoi(cols[69].c_str()),
                std::atoi(cols[70].c_str()));
            if (id == 201) assert(real.level == 68 && real.hp == 4540 && real.minAttack == 434 && real.defense == 114);
            if (id == 701) assert(real.level == 51 && real.hp == 16760 && real.maxAttack == 823 && real.defense == 163);
            ++bossCount;
        }
        assert(bossCount == 2);
        break;
    }
    return 0;
}
