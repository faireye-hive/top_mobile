// 05-U: consulta atributos base da tabela original, sem OpenGL ou servidor.
#include "top/HistoricalStats.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;
    std::size_t begin = 0;
    for (;;) {
        const auto end = line.find('\t', begin);
        result.push_back(line.substr(begin, end == std::string::npos ? end : end-begin));
        if (end == std::string::npos) return result;
        begin = end + 1;
    }
}
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Uso: top_stats_inspect Client <monster-id> [outro-id...]\n";
        return 2;
    }
    const std::string root = argv[1];
    const std::string table = root + "/scripts/table/characterinfo.txt";
    std::ifstream file(table, std::ios::binary);
    if (!file) { std::cerr << "Arquivo ausente: " << table << '\n'; return 1; }
    std::vector<int> ids;
    for (int i = 2; i < argc; ++i) {
        const int id = std::atoi(argv[i]);
        if (id < 1 || id > 100000) { std::cerr << "ID invalido\n"; return 2; }
        ids.push_back(id);
    }
    std::string line;
    int found = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '/') continue;
        const auto row = fields(line);
        if (row.size() <= 71) continue;
        const int id = std::atoi(row[0].c_str());
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) continue;
        const auto m = top::historical_stats::monsterFromColumns(
            std::atoi(row[60].c_str()), std::atoi(row[61].c_str()),
            std::atoi(row[62].c_str()), std::atoi(row[65].c_str()),
            std::atoi(row[66].c_str()), std::atoi(row[68].c_str()),
            std::atoi(row[69].c_str()), std::atoi(row[70].c_str()),
            std::atoi(row[67].c_str()), std::atoi(row[71].c_str()));
        std::cout << "ID " << id << ' ' << row[1] << ": nivel " << m.level
                  << " HP " << m.hp << " ATK " << m.minAttack << '-' << m.maxAttack
                  << " DEF " << m.defense << " HIT " << m.hit << " FLEE " << m.dodge
                  << " PDEF " << m.physicalResistance << " CRT " << m.criticalPercent
                  << " EXP_COL62 " << m.experience
                  << (m.experience == 0 ? " (fallback EXP offline)" : "") << '\n';
        ++found;
    }
    top::historical_stats::Experience exp;
    if (exp.load(root + "/scripts/offline/calculate/variable.lua")) {
        std::cout << "DEXP historica: Lv1->2 " << exp.needed(1)
                  << ", Lv4->5 " << exp.needed(4)
                  << ", Lv9->10 " << exp.needed(9)
                  << ", Lv50->51 " << exp.needed(50) << '\n';
    } else std::cout << "DEXP nao encontrada; fallback de progressao ativo\n";
    if (found != static_cast<int>(ids.size())) {
        std::cerr << "Encontrados " << found << "/" << ids.size() << " IDs\n";
        return 1;
    }
    return 0;
}
