#include "top/HistoricalSkillExecution.h"
#include "top/SkillVisual.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> cols;
    std::size_t start = 0;
    for (;;) {
        const auto end = line.find('\t', start);
        cols.push_back(line.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return cols;
}
int number(const std::string& text) { return std::atoi(text.c_str()); }
const char* label(top::historical_skill::InputMode mode) {
    using top::historical_skill::InputMode;
    switch (mode) {
        case InputMode::SelectEnemy: return "select_enemy";
        case InputMode::Passive: return "passive";
        case InputMode::Direct: return "other_target";
    }
    return "unknown";
}
std::string quoted(const std::string& raw) {
    std::string result = "\"";
    for (const char c: raw) { if (c == '"') result += '"'; result += c; }
    result += '"';
    return result;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3 || (argc == 3 && std::string(argv[2]) != "--csv")) {
        std::fprintf(stderr, "uso: top_skill_execution_audit Client/scripts/table/skillinfo.txt [--csv]\n");
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) { std::fprintf(stderr, "skillinfo nao encontrado: %s\n", argv[1]); return 1; }
    top::SkillVisualCatalog visuals;
    const bool visualLoaded = visuals.load(argv[1]);
    const bool csv = argc == 3;
    if (csv) std::puts("id,name,type,helpful,target,range_cm,mode,visual_channels,flight,hit");
    int entries = 0, enemy = 0, passive = 0, other = 0, visual = 0;
    int withFlight = 0, withImpact = 0, noVisual = 0;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '/' || line[0] == '#') continue;
        const auto c = splitTabs(line);
        if (c.size() <= 17 || number(c[0]) <= 0) continue;
        const int id = number(c[0]);
        const top::historical_skill::Descriptor rule{number(c[9]), number(c[10]),
                                                     number(c[17]), number(c[16])};
        const auto mode = top::historical_skill::inputMode(rule);
        if (mode == top::historical_skill::InputMode::SelectEnemy) ++enemy;
        else if (mode == top::historical_skill::InputMode::Passive) ++passive;
        else ++other;
        const top::SkillVisual* fx = visualLoaded ? visuals.find(id) : nullptr;
        const int channels = fx ? static_cast<int>(fx->channels.size()) : 0;
        const bool flight = fx && fx->flight > 0;
        const bool impact = fx && (fx->target > 0 || fx->ground > 0 || fx->water > 0);
        visual += channels > 0;
        noVisual += channels == 0;
        withFlight += flight;
        withImpact += impact;
        ++entries;
        if (csv)
            std::printf("%d,%s,%d,%d,%d,%d,%s,%d,%d,%d\n", id,
                quoted(c[1]).c_str(), rule.type, rule.helpful, rule.target,
                rule.distanceCm, label(mode), channels, flight, impact);
    }
    if (!csv) {
        std::printf("Registros historicos: %d\n", entries);
        std::printf("Alvo inimigo (novo fluxo): %d\n", enemy);
        std::printf("Passivas: %d\n", passive);
        std::printf("Outros alvos/funcionalidades (fluxo anterior): %d\n", other);
        std::printf("Metadados visuais: %d com canais, %d sem canais\n", visual, noVisual);
        std::printf("Com efeito de voo: %d | com impacto: %d\n", withFlight, withImpact);
        std::puts("ATENCAO: canais declarados != efeito renderizado; auditar .par/.eff separadamente.");
    }
    return 0;
}
