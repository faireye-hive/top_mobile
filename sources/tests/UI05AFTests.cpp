#include "top/AssetAudit.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

static void put(const fs::path& path, const std::string& value = "// fixture\n1\t NPC\n") {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out << value;
}

static void mapFile(const fs::path& path, bool valid) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    int hdr[] = {valid ? 780627 : 999, 8, 8, 8, 8, 0};
    out.write(reinterpret_cast<const char*>(hdr), sizeof hdr);
}

static const top::AssetAuditEntry& get(const top::AssetAuditReport& r, const std::string& map,
                                       const std::string& category) {
    for (const auto& e : r.entries)
        if (e.map == map && e.category == category) return e;
    std::abort();
}

int main() {
    const fs::path folder = fs::temp_directory_path() /
        ("top_05af_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); }
    } cleanup{folder};
    const fs::path root = folder / "Client";
    fs::create_directories(root);
    mapFile(root / "map/Garner.map", true); // diferencia de case no Linux
    mapFile(root / "map/hell.map", false);   // cabecalho corrompido
    put(root / "scripts/offline/maps/garner/garnernpc.txt", "// foo\n1\t a\n2\t b\n");
    put(root / "scripts/offline/maps/garner/garnerChaSpn.txt");
    put(root / "scripts/offline/hell/hellswhmap.txt"); // layout legado
    put(root / "scripts/table/skillinfo.txt");
    put(root / "scripts/table/sceneobjinfo.txt", "1\texample.lmo\tobject\n2\tabsent.lmo\tobject\n");
    put(root / "model/scene/example.lmo");
    put(root / "texture/scene/example.bmp");
    const auto r = top::auditClientAssets(root.string());
    assert(r.mapCount == 2 && r.validMapCount == 1);
    assert(r.mapsWithNpcData == 1 && r.mapsWithSpawnData == 1 && r.mapsWithPortalData == 1);
    assert(get(r, "garner", "map").status == "case_mismatch");
    assert(get(r, "hell", "map").status == "invalid");
    assert(get(r, "garner", "npc").detail.find("2 linhas") != std::string::npos);
    assert(get(r, "hell", "portals").status == "ok");
    assert(get(r, "hell", "portals").actual == "scripts/offline/hell/hellswhmap.txt");
    assert(get(r, "hell", "npc").status == "missing");
    assert(get(r, "", "scene_catalog").status == "ok");
    bool catalogMissing = false;
    for (const auto& e : r.entries)
        if (e.category == "scene_catalog" && e.expected == "model/scene/absent.lmo")
            catalogMissing = e.status == "missing" && e.importance == "diagnostic";
    assert(catalogMissing);
    assert(r.missingRequired > 0 && r.caseMismatches > 0 && r.invalidResources > 0);
    assert(top::auditClientAssets(root.string(), "garner").mapCount == 1);
    assert(top::auditClientAssets(root.string(), "garner").validMapCount == 1);
    assert(top::auditClientAssets(root.string(), "unknown").mapCount == 1);
    const fs::path json = folder / "report.json";
    const fs::path csv = folder / "report.csv";
    assert(top::writeAssetAuditJson(r, json.string()));
    assert(top::writeAssetAuditCsv(r, csv.string()));
    std::ifstream j(json);
    const std::string s((std::istreambuf_iterator<char>(j)), {});
    assert(s.find("top-assets-05-af-v1") != std::string::npos);
    assert(s.find("case_mismatch") != std::string::npos);
    assert(s.find("\"maps\": 2") != std::string::npos);
    std::ifstream c(csv);
    std::string line;
    std::getline(c, line);
    assert(line == "map,category,expected,actual,status,importance,detail");
    std::cout << "05-AF PASS: 2 mapas, distincao de caixa, .map invalido, variantes de script e JSON/CSV\n";
}
