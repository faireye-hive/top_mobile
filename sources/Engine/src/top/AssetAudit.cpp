#include "top/AssetAudit.h"
#include "top/MapFile.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <unordered_map>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace top {
namespace {
namespace fs = std::filesystem;

bool safeMapName(const std::string& value) {
    if (value.empty() || value.size() > 48) return false;
    for (unsigned char c : value)
        if (!std::isalnum(c) && c != '_') return false;
    return true;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool regular(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}
bool directory(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

// Resolve apenas diferencas de caixa por segmento. Nunca faz procura recursiva
// global, para nao confundir dois recursos historicos de mesmo nome.
fs::path resolveCase(const fs::path& root, const fs::path& relative) {
    fs::path cursor = root;
    for (const auto& part : relative) {
        const std::string target = part.string();
        if (target == "." || target == ".." || target.empty()) return {};
        if (!directory(cursor)) return {};
        const fs::path exact = cursor / part;
        std::error_code ec;
        if (fs::exists(exact, ec)) { cursor = exact; continue; }
        std::vector<fs::path> matches;
        for (fs::directory_iterator it(cursor, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (lower(it->path().filename().string()) == lower(target)) matches.push_back(it->path());
        }
        if (matches.size() != 1) return {}; // ambiguidade em Linux = recurso nao deterministico
        cursor = matches.front();
    }
    return cursor;
}

std::string relativeName(const fs::path& root, const fs::path& real) {
    // lexically_relative preserva a grafia solicitada, mesmo com scripts/ em symlink.
    const fs::path rel = real.lexically_relative(root);
    return rel.empty() ? real.generic_string() : rel.generic_string();
}

std::size_t lineCount(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::size_t count = 0;
    std::string line;
    while (count < 100000 && std::getline(in, line)) {
        const std::size_t p = line.find_first_not_of(" \t\r");
        if (p == std::string::npos || line.compare(p, 2, "//") == 0 || line.compare(p, 2, "--") == 0)
            continue;
        ++count;
    }
    return count;
}

std::string jsonEscape(const std::string& input) {
    std::ostringstream out;
    const char* digits = "0123456789abcdef";
    for (unsigned char c : input) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) out << "\\u00" << digits[c >> 4] << digits[c & 15];
            else out << static_cast<char>(c);
        }
    }
    return out.str();
}

std::string csvEscape(const std::string& s) {
    std::string out = "\"";
    for (char c : s) { if (c == '"') out += '"'; out += c; }
    return out + '"';
}

bool plausibleMapHeader(const fs::path& file);

// Se mapName passar neste filtro, jamais podera acessar fora do Client.
void record(AssetAuditReport& report, const fs::path& root, const std::string& map,
            const std::string& category, const std::string& name,
            const std::string& importance, bool asDirectory = false,
            const std::string& alternative = {}) {
    AssetAuditEntry e;
    e.map = map;
    e.category = category;
    e.expected = name;
    e.importance = importance;
    fs::path chosen = resolveCase(root, fs::path(name));
    std::string actual = chosen.empty() ? "" : relativeName(root, chosen);
    bool good = !chosen.empty() && (asDirectory ? directory(chosen) : regular(chosen));
    if (!good && !alternative.empty()) {
        const fs::path backup = resolveCase(root, fs::path(alternative));
        if (!backup.empty() && (asDirectory ? directory(backup) : regular(backup))) {
            chosen = backup;
            actual = relativeName(root, backup);
            good = true;
            e.detail = "layout alternativo historico";
        }
    }
    if (!good) {
        e.status = "missing";
        if (importance == "required") ++report.missingRequired;
        else if (importance == "optional") ++report.missingOptional;
    } else {
        e.actual = actual;
        e.status = (actual == name || (!alternative.empty() && actual == alternative)) ? "ok" : "case_mismatch";
        if (e.status == "case_mismatch") ++report.caseMismatches;
        if (asDirectory) {
            std::size_t count = 0;
            std::error_code ec;
            for (fs::directory_iterator it(chosen, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; it.increment(ec)) ++count;
            e.detail += (e.detail.empty() ? "" : "; ") + std::to_string(count) + " entradas diretas";
            if (!count) {
                e.status = "empty";
                if (importance == "required") ++report.missingRequired;
                else if (importance == "optional") ++report.missingOptional;
            } // conteudo da subpasta nao foi testado
        } else {
            if (category == "npc" || category == "spawns" || category == "portals") {
                const auto n = lineCount(chosen);
                e.detail += (e.detail.empty() ? "" : "; ") + std::to_string(n) + " linhas nao comentadas (nao executadas)";
            }
        }
        if (category == "map" && e.status == "ok") {
            MapFile probe;
            if (!plausibleMapHeader(chosen) || !probe.open(chosen.string())) {
                e.status = "invalid";
                e.detail = "cabecalho/tabela de secoes .map invalido";
                ++report.invalidResources;
            } else {
                e.detail = std::to_string(probe.width()) + "x" + std::to_string(probe.height());
                ++report.validMapCount;
            }
        } else if (category == "map" && e.status == "case_mismatch") {
            MapFile probe;
            if (!plausibleMapHeader(chosen) || !probe.open(chosen.string())) {
                e.status = "invalid";
                e.detail = "cabecalho/tabela de secoes .map invalido; caixa divergente";
                ++report.invalidResources;
            } else {
                e.detail = std::to_string(probe.width()) + "x" + std::to_string(probe.height());
                ++report.validMapCount;
            }
        }
        if (category == "scene_obj" && e.status != "empty") {
            std::ifstream in(chosen, std::ios::binary);
            char header[20]{};
            in.read(header, sizeof header);
            const int version = static_cast<unsigned char>(header[16]) |
                                (static_cast<unsigned char>(header[17]) << 8) |
                                (static_cast<unsigned char>(header[18]) << 16) |
                                (static_cast<unsigned char>(header[19]) << 24);
            if (in.gcount() < 20 || std::string(header, 15) != "HF Object File!" || version != 600) {
                e.status = "invalid";
                e.detail = ".obj nao corresponde a versao 600 esperada";
                ++report.invalidResources;
            }
        }
    }
    report.entries.push_back(std::move(e));
}

// Validacao de limites antes de chamar o leitor historico, para evitar
// alocacoes gigantescas quando um cabecalho .map esta corrompido.
bool plausibleMapHeader(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::array<std::int32_t, 5> h{};
    if (!in.read(reinterpret_cast<char*>(h.data()), sizeof h)) return false;
    if (h[0] != 780627 || h[1] <= 0 || h[2] <= 0 ||
        h[1] > 16384 || h[2] > 16384 || h[3] != 8 || h[4] != 8) return false;
    const std::uint64_t sx = (static_cast<std::uint64_t>(h[1]) + 7) / 8;
    const std::uint64_t sy = (static_cast<std::uint64_t>(h[2]) + 7) / 8;
    std::error_code ec;
    return fs::file_size(file, ec) >= 20 + sx * sy * 4 && !ec;
}

void catalogReferences(AssetAuditReport& r, const fs::path& root) {
    const fs::path file = resolveCase(root, "scripts/table/sceneobjinfo.txt");
    std::ifstream in(file);
    if (!in) return;
    std::set<std::string> seen;
    std::string line;
    while (seen.size() < 20000 && std::getline(in, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos || line.empty() || !std::isdigit(static_cast<unsigned char>(line[0]))) continue;
        const auto next = line.find('\t', tab + 1);
        std::string name = line.substr(tab + 1, next == std::string::npos ? std::string::npos : next - tab - 1);
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
        if (name.size() <= 4 || lower(name.substr(name.size() - 4)) != ".lmo" ||
            name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
            name.find("..") != std::string::npos || !seen.insert(lower(name)).second) continue;
        // Referencia global; nao afirma que o modelo seja usado em todos os mapas.
        record(r, root, "", "scene_catalog", "model/scene/" + name, "diagnostic");
    }
}


// 05-AH: os .obj de cada mapa apontam para IDs de sceneobjinfo.txt.
// Inventariar TODO o catalogo como "modelo ausente em jogo" gera falsos positivos:
// so uma referencia usada por um .obj pode afetar visualmente aquele mapa.
struct SceneUse {
    std::size_t instances = 0;
    std::size_t visual = 0;
    std::size_t helper = 0;
    std::set<int> ids;
};
struct SceneDefinition {
    std::string file;
    int kind = 0;
};

std::unordered_map<int, SceneDefinition> readSceneDefinitions(const fs::path& root) {
    std::unordered_map<int, SceneDefinition> definitions;
    const fs::path file = resolveCase(root, "scripts/table/sceneobjinfo.txt");
    std::ifstream in(file, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '/') continue;
        std::vector<std::string> cols;
        std::size_t at = 0;
        while (at <= line.size()) {
            const auto end = line.find('\t', at);
            cols.push_back(line.substr(at, end == std::string::npos ? end : end - at));
            if (end == std::string::npos) break;
            at = end + 1;
        }
        if (cols.size() < 2) continue;
        const int id = std::atoi(cols[0].c_str()); // mesma chave do SceneWorld
        if (id < 0 || id > 100000 || cols[1].empty()) continue;
        std::string fileName = cols[1];
        while (!fileName.empty() && std::isspace(static_cast<unsigned char>(fileName.back()))) fileName.pop_back();
        if (fileName.empty()) continue;
        definitions[id] = {fileName, cols.size() > 3 ? std::atoi(cols[3].c_str()) : 0};
    }
    return definitions;
}

std::uint32_t objU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

// Os offsets, index por setor e registros de 20 bytes correspondem a
// SceneWorld::open/build. Os mesmos 25 objetos por setor sao considerados
// (limite real do renderizador), evitando superestimar o que ele desenha.
bool readSceneMapUse(const fs::path& obj, const std::unordered_map<int, SceneDefinition>& defs,
                     std::map<std::string, SceneUse>& used,
                     std::map<int, std::size_t>& unresolved,
                     std::size_t& instances, std::size_t& invalidSections,
                     std::size_t& ignoredByCap) {
    std::error_code ec;
    const auto length = fs::file_size(obj, ec);
    if (ec || length < 44 || length > 64u * 1024u * 1024u) return false;
    std::ifstream in(obj, std::ios::binary);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return false;
    const auto* p = bytes.data();
    if (std::memcmp(p, "HF Object File!", 15) || objU32(p + 16) != 600) return false;
    const std::uint32_t sx = objU32(p + 24), sy = objU32(p + 28);
    const std::uint32_t sw = objU32(p + 32), sh = objU32(p + 36);
    if (!sx || !sy || !sw || !sh || sx > 16384 || sy > 16384) return false;
    const std::uint64_t count = static_cast<std::uint64_t>(sx) * sy;
    if (count > (bytes.size() - 44u) / 8u) return false;
    for (std::uint64_t section = 0; section < count; ++section) {
        const auto* index = p + 44 + section * 8;
        const std::int32_t pos = static_cast<std::int32_t>(objU32(index));
        const std::int32_t declared = static_cast<std::int32_t>(objU32(index + 4));
        if (pos <= 0 || declared <= 0) continue;
        // SceneWorld limita a 25; registra a parte ignorada pelo proprio cliente.
        const auto n = std::min(declared, 25);
        ignoredByCap += static_cast<std::size_t>(declared - n);
        if (static_cast<std::uint64_t>(pos) + static_cast<std::uint64_t>(n) * 20u > bytes.size()) {
            ++invalidSections;
            continue;
        }
        for (int i = 0; i < n; ++i) {
            const auto* rec = p + pos + static_cast<std::size_t>(i) * 20;
            const std::uint16_t typeId = static_cast<std::uint16_t>(rec[0] | (rec[1] << 8));
            if (typeId >> 14) continue; // objetos nao-modelo, ignorados pelo SceneWorld
            const int id = typeId & 0x3fff;
            ++instances;
            const auto def = defs.find(id);
            if (def == defs.end()) {
                ++unresolved[id];
                continue;
            }
            const auto& fileName = def->second.file;
            // Sem caminhos arbitrarios vindos dos arquivos historicos.
            if (fileName.size() <= 4 || lower(fileName.substr(fileName.size() - 4)) != ".lmo" ||
                fileName.find_first_of("/\\") != std::string::npos || fileName.find("..") != std::string::npos) {
                ++unresolved[id];
                continue;
            }
            SceneUse& item = used[lower(fileName)];
            ++item.instances;
            if (def->second.kind == 0) ++item.visual;
            else ++item.helper;
            item.ids.insert(id);
        }
    }
    return true;
}

void auditSceneUsage(AssetAuditReport& report, const fs::path& root) {
    const auto definitions = readSceneDefinitions(root);
    std::set<std::string> usedGlobally, missingGlobally, caseGlobally;
    // As entradas de scene_obj ja receberam verificacao de existencia/case.
    // Iteracao por indice: record() adiciona entradas no final do vetor.
    const std::size_t previous = report.entries.size();
    for (std::size_t index = 0; index < previous; ++index) {
        auto& obj = report.entries[index];
        if (obj.category != "scene_obj" || (obj.status != "ok" && obj.status != "case_mismatch")) continue;
        const std::string map = obj.map;
        const fs::path path = root / obj.actual;
        std::map<std::string, SceneUse> used;
        std::map<int, std::size_t> unresolved;
        std::size_t instances = 0, invalidSections = 0, ignored = 0;
        if (!readSceneMapUse(path, definitions, used, unresolved, instances, invalidSections, ignored)) {
            obj.status = "invalid";
            obj.detail = "estrutura .obj invalida ou acima de 64 MiB";
            ++report.invalidResources;
            continue;
        }
        ++report.sceneMapsScanned;
        SceneMapImpact impact;
        impact.map = map;
        impact.instances = instances;
        impact.distinctModels = used.size();
        impact.invalidSections = invalidSections;
        for (const auto& pair : unresolved) impact.unresolvedInstances += pair.second;
        report.sceneInstances += instances;
        report.sceneUnresolvedInstances += [&]{
            std::size_t sum = 0;
            for (const auto& pair : unresolved) sum += pair.second;
            return sum;
        }();
        report.sceneInvalidSections += invalidSections;
        report.sceneIgnoredByCap += ignored;
        // Nao guarda referencias a report.entries: record() pode realocar o vetor.
        report.entries[index].detail += (report.entries[index].detail.empty() ? "" : "; ") +
            std::to_string(instances) + " modelos colocados; " + std::to_string(used.size()) +
            " nomes distintos; " + std::to_string(invalidSections) + " secoes invalidas";
        for (const auto& pair : used) {
            const auto& use = pair.second;
            // A tabela usa ID, mas a carga de modelo usa o nome do arquivo.
            std::string filename;
            for (int id : use.ids) {
                const auto it = definitions.find(id);
                if (it != definitions.end()) { filename = it->second.file; break; }
            }
            if (filename.empty()) continue;
            const std::string resource = "model/scene/" + filename;
            record(report, root, map, "scene_used", resource, "diagnostic");
            auto& entry = report.entries.back();
            entry.detail = "instancias=" + std::to_string(use.instances) +
                           "; visuais=" + std::to_string(use.visual) +
                           "; auxiliares=" + std::to_string(use.helper) + "; ids=";
            bool first = true;
            for (int id : use.ids) {
                if (!first) entry.detail += ',';
                entry.detail += std::to_string(id);
                first = false;
            }
            usedGlobally.insert(pair.first);
            if (entry.status == "missing") {
                missingGlobally.insert(pair.first);
                ++impact.missingModels;
                impact.missingInstances += use.instances;
                impact.missingVisual += use.visual;
                impact.missingHelpers += use.helper;
            }
            if (entry.status == "case_mismatch") {
                caseGlobally.insert(pair.first);
                // "case_mismatches" e o campo legado da 05-AF: nao contar
                // novamente a mesma referencia em cada mapa individual.
                --report.caseMismatches;
            }
        }
        report.sceneMissingInstances += impact.missingInstances;
        if (impact.missingModels) ++report.sceneMapsWithMissingModels;
        report.sceneImpact.push_back(std::move(impact));
        for (const auto& [id, n] : unresolved) {
            AssetAuditEntry e;
            e.map = map;
            e.category = "scene_unmapped";
            e.expected = "sceneobjinfo_id:" + std::to_string(id);
            e.status = "unmapped";
            e.importance = "diagnostic";
            e.detail = std::to_string(n) + " instancias no .obj sem modelo valido na tabela";
            report.entries.push_back(std::move(e));
        }
    }
    report.sceneModelsUsed = usedGlobally.size();
    report.sceneModelsMissingUsed = missingGlobally.size();
    report.sceneModelsCaseMismatchUsed = caseGlobally.size();
    for (std::size_t i = 0; i < previous; ++i) {
        auto& entry = report.entries[i];
        if (entry.category == "scene_catalog") {
            const auto name = entry.expected.substr(std::string("model/scene/").size());
            if (!report.sceneMapsScanned) {
                entry.detail = "uso indeterminado (nenhum .obj analisado)";
            } else if (usedGlobally.count(lower(name)) == 0) {
                ++report.sceneCatalogNotReferenced;
                entry.detail = "sem referencia nos .obj analisados";
            } else {
                entry.detail = "referenciado por ao menos um .obj analisado";
            }
        }
    }
}

bool found(const AssetAuditReport& report, const std::string& map, const std::string& category) {
    for (auto it = report.entries.rbegin(); it != report.entries.rend(); ++it)
        if (it->map == map && it->category == category)
            return it->status == "ok" || it->status == "case_mismatch";
    return false;
}

} // namespace

AssetAuditReport auditClientAssets(const std::string& clientRoot, const std::string& mapFilter) {
    AssetAuditReport r;
    const fs::path root = fs::path(clientRoot).lexically_normal();
    r.clientRoot = root.string();
    if (!directory(root)) return r;

    // Global: referencias que os leitores atuais realmente consultam.
    for (const auto& pair : std::vector<std::pair<std::string, std::string>>{
        {"scripts/table/terraininfo.txt", "required"},
        {"scripts/table/sceneobjinfo.txt", "required"},
        {"scripts/table/characterinfo.txt", "required"},
        {"scripts/table/iteminfo.txt", "required"},
        {"scripts/table/skillinfo.txt", "required"},
        {"scripts/txt/CharacterAction.tx", "required"},
        {"scripts/table/mapinfo.txt", "optional"},
        {"scripts/table/musicinfo.txt", "optional"},
        {"scripts/offline/calculate/variable.lua", "required"},
        {"scripts/offline/script/calculate/skilleffect.lua", "optional"},
        {"scripts/offline/script/monster/mlist.lua", "optional"},
        {"texture/terrain/alpha/total.tga", "required"},
    }) {
        record(r, root, "", "global_file", pair.first, pair.second);
    }
    for (const auto& category : std::vector<std::pair<std::string, std::string>>{
        {"model/scene", "required"}, {"model/character", "required"},
        {"model/item", "optional"}, {"animation", "required"},
        {"texture/scene", "required"},
        {"texture/character", "required"}, {"texture/item", "optional"},
        {"texture/effect", "optional"}, {"effect", "optional"},
        {"music", "optional"}
    }) record(r, root, "", "directory", category.first, category.second, true);

    catalogReferences(r, root);

    std::set<std::string> maps;
    const auto collect = [&](const fs::path& parent, bool fromMapFiles) {
        if (!directory(parent)) return;
        std::error_code ec;
        for (fs::directory_iterator it(parent, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            const std::string name = fromMapFiles ? it->path().stem().string() : it->path().filename().string();
            if (!safeMapName(name)) continue;
            if (fromMapFiles) {
                if (lower(it->path().extension().string()) == ".map" && regular(it->path())) maps.insert(lower(name));
            } else if (it->is_directory()) {
                if (name != "script" && name != "calculate" && name != "maps") maps.insert(lower(name));
            }
        }
    };
    collect(root / "map", true);
    collect(root / "scripts/offline/maps", false);
    collect(root / "scripts/offline", false);
    if (!mapFilter.empty()) maps = {mapFilter};
    r.mapCount = maps.size();

    for (const auto& m : maps) {
        record(r, root, m, "map", "map/" + m + ".map", "required");
        record(r, root, m, "scene_obj", "map/" + m + ".obj", "optional");
        // Ausencia de .obj e normal em mapas sem objetos: SceneWorld usa cena vazia.
        for (const auto& pair : std::vector<std::pair<std::string, std::string>>{
            {"npc", "npc.txt"}, {"spawns", "ChaSpn.txt"}, {"portals", "swhmap.txt"}
        }) {
            const std::string name = m + pair.second;
            record(r, root, m, pair.first, "scripts/offline/maps/" + m + "/" + name,
                   "optional", false, "scripts/offline/" + m + "/" + name);
        }
        r.mapsWithNpcData += found(r, m, "npc") ? 1u : 0u;
        r.mapsWithSpawnData += found(r, m, "spawns") ? 1u : 0u;
        r.mapsWithPortalData += found(r, m, "portals") ? 1u : 0u;
    }
    auditSceneUsage(r, root);
    return r;
}

bool writeAssetAuditJson(const AssetAuditReport& r, const std::string& output) {
    std::ofstream f(output, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << "{\n  \"schema\": \"top-assets-05-af-v1\",\n"
      << "  \"client_root\": \"" << jsonEscape(r.clientRoot) << "\",\n"
      << "  \"note\": \"Existencia de arquivos e cabecalhos nao comprova equivalencia DX9/OpenGL, nem execucao de scripts Lua.\",\n"
      << "  \"summary\": {\"maps\": " << r.mapCount << ", \"valid_maps\": " << r.validMapCount
      << ", \"maps_with_npcs\": " << r.mapsWithNpcData << ", \"maps_with_spawns\": " << r.mapsWithSpawnData
      << ", \"maps_with_portals\": " << r.mapsWithPortalData
      << ", \"missing_required\": " << r.missingRequired << ", \"missing_optional\": " << r.missingOptional
      << ", \"case_mismatches\": " << r.caseMismatches << ", \"invalid\": " << r.invalidResources
      << ", \"scene_maps_scanned\": " << r.sceneMapsScanned
      << ", \"scene_instances\": " << r.sceneInstances
      << ", \"scene_models_used\": " << r.sceneModelsUsed
      << ", \"scene_models_missing_used\": " << r.sceneModelsMissingUsed
      << ", \"scene_models_case_mismatch_used\": " << r.sceneModelsCaseMismatchUsed
      << ", \"scene_unresolved_instances\": " << r.sceneUnresolvedInstances
      << ", \"scene_catalog_not_referenced_in_scanned_maps\": " << r.sceneCatalogNotReferenced
      << ", \"scene_invalid_sections\": " << r.sceneInvalidSections
      << ", \"scene_records_ignored_by_renderer_cap\": " << r.sceneIgnoredByCap
      << ", \"scene_missing_instances\": " << r.sceneMissingInstances
      << ", \"scene_maps_with_missing_models\": " << r.sceneMapsWithMissingModels << "},\n"
      << "  \"entries\": [\n";
    for (std::size_t i = 0; i < r.entries.size(); ++i) {
        const auto& e = r.entries[i];
        f << "    {\"map\":\"" << jsonEscape(e.map) << "\",\"category\":\""
          << jsonEscape(e.category) << "\",\"expected\":\"" << jsonEscape(e.expected)
          << "\",\"actual\":\"" << jsonEscape(e.actual) << "\",\"status\":\""
          << jsonEscape(e.status) << "\",\"importance\":\"" << jsonEscape(e.importance)
          << "\",\"detail\":\"" << jsonEscape(e.detail) << "\"}";
        f << (i + 1 == r.entries.size() ? "\n" : ",\n");
    }
    f << "  ]\n}\n";
    return static_cast<bool>(f);
}

bool writeAssetAuditCsv(const AssetAuditReport& r, const std::string& output) {
    std::ofstream f(output, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << "map,category,expected,actual,status,importance,detail\n";
    for (const auto& e : r.entries)
        f << csvEscape(e.map) << ',' << csvEscape(e.category) << ',' << csvEscape(e.expected)
          << ',' << csvEscape(e.actual) << ',' << csvEscape(e.status) << ','
          << csvEscape(e.importance) << ',' << csvEscape(e.detail) << '\n';
    return static_cast<bool>(f);
}

bool writeSceneImpactCsv(const AssetAuditReport& r, const std::string& output) {
    std::ofstream f(output, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << "map,instances,distinct_models,missing_models,missing_instances,missing_visual,missing_helpers,unresolved_instances,invalid_sections,missing_percent\n";
    auto sorted = r.sceneImpact;
    std::sort(sorted.begin(), sorted.end(), [](const SceneMapImpact& a, const SceneMapImpact& b) {
        if (a.missingInstances != b.missingInstances) return a.missingInstances > b.missingInstances;
        return a.map < b.map;
    });
    for (const auto& m : sorted) {
        // Percentual de instancias (inclui referencias sem ID que nao foram classificadas como missing).
        const double percentage = m.instances ? (100.0 * m.missingInstances / m.instances) : 0.0;
        f << csvEscape(m.map) << ',' << m.instances << ',' << m.distinctModels << ','
          << m.missingModels << ',' << m.missingInstances << ',' << m.missingVisual << ','
          << m.missingHelpers << ',' << m.unresolvedInstances << ',' << m.invalidSections
          << ',' << static_cast<unsigned>(percentage * 100.0 + 0.5) / 100.0 << '\n';
    }
    return static_cast<bool>(f);
}

bool writeMissingSceneModelsTxt(const AssetAuditReport& r, const std::string& output) {
    std::ofstream f(output, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    // Uma linha por arquivo concreto, sem inventar nome ou modelo substituto.
    std::set<std::string> missing;
    for (const auto& e : r.entries) {
        if (e.category == "scene_used" && e.status == "missing") missing.insert(e.expected);
    }
    for (const auto& name : missing) f << name << '\n';
    return static_cast<bool>(f);
}

int runAssetAuditCli(int argc, char** argv) {
    const char* env = std::getenv("TOP_CLIENT_ROOT");
    std::string root = (env && *env) ? env : "Client";
    std::string mapFilter;
    std::string json = "top_assets_report.json";
    std::string csv = "top_assets_report.csv";
    std::string sceneImpactPath, missingListPath;
    bool strict = false;
    for (int i = 1; i < argc; ++i) {
        const std::string opt = argv[i];
        if (opt == "--audit-assets") continue; // compartilhado com top_gl
        if (opt == "--help" || opt == "-h") {
            std::cout << "Auditoria offline 05-AI: [--client CLIENT] [--map NOME]"
                         " [--json ARQUIVO] [--csv ARQUIVO] [--scene-impact CSV]"
                         " [--missing-models TXT] [--strict]\n"
                         "Sem --client usa TOP_CLIENT_ROOT ou Client; nao inicia GL.\n";
            return 0;
        }
        if ((opt == "--client" || opt == "--map" || opt == "--json" || opt == "--csv" ||
             opt == "--scene-impact" || opt == "--missing-models") && i + 1 < argc) {
            const std::string value = argv[++i];
            if (opt == "--client") root = value;
            if (opt == "--map") mapFilter = value;
            if (opt == "--json") json = value;
            if (opt == "--csv") csv = value;
            if (opt == "--scene-impact") sceneImpactPath = value;
            if (opt == "--missing-models") missingListPath = value;
        } else if (opt == "--strict") strict = true;
        else {
            std::cerr << "Parametro desconhecido ou incompleto: " << opt << '\n';
            return 2;
        }
    }
    if ((!mapFilter.empty() && !safeMapName(mapFilter)) || !directory(root)) {
        std::cerr << "Client inexistente ou nome de mapa invalido: " << root << '\n';
        return 2;
    }
    const AssetAuditReport report = auditClientAssets(root, mapFilter);
    if (!writeAssetAuditJson(report, json) || !writeAssetAuditCsv(report, csv) ||
        (!sceneImpactPath.empty() && !writeSceneImpactCsv(report, sceneImpactPath)) ||
        (!missingListPath.empty() && !writeMissingSceneModelsTxt(report, missingListPath))) {
        std::cerr << "Falha escrevendo os relatórios: " << json << " / " << csv << '\n';
        return 3;
    }
    std::cout << "05-AI inventario: " << report.mapCount << " mapas (" << report.validMapCount
              << " cabecalhos validos), " << report.mapsWithNpcData << " com NPC, "
              << report.mapsWithSpawnData << " com ChaSpn, " << report.mapsWithPortalData
              << " com portais\n"
              << "  faltas obrigatorias: " << report.missingRequired
              << "; opcionais: " << report.missingOptional
              << "; diferencas maiusculas/minusculas: " << report.caseMismatches
              << "; invalidos: " << report.invalidResources << '\n'
              << "  cena: " << report.sceneMapsScanned << " .obj analisados; "
              << report.sceneInstances << " instancias; " << report.sceneModelsUsed
              << " modelos distintos usados; " << report.sceneModelsMissingUsed
              << " modelos usados ausentes; " << report.sceneModelsCaseMismatchUsed
              << " usados com caixa diferente; " << report.sceneUnresolvedInstances
              << " instancias sem ID valido; " << report.sceneCatalogNotReferenced
              << " modelos catalogados nao referenciados nos .obj analisados\n"
              << "  impacto: " << report.sceneMissingInstances << " instancias ausentes em "
              << report.sceneMapsWithMissingModels << " mapas\n"
              << "  JSON: " << json << "\n  CSV: " << csv << '\n';
    if (!sceneImpactPath.empty()) std::cout << "  Impacto CSV: " << sceneImpactPath << '\n';
    if (!missingListPath.empty()) std::cout << "  Modelos ausentes TXT: " << missingListPath << '\n';
    if (strict && (report.missingRequired || report.invalidResources || report.caseMismatches)) return 1;
    return 0;
}

} // namespace top
