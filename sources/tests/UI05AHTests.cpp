#include "top/AssetAudit.h"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void put(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out << content;
}
static void write32(std::vector<std::uint8_t>& b, std::size_t off, std::uint32_t n) {
    assert(off + 4 <= b.size());
    for (int i = 0; i < 4; ++i) b[off + i] = static_cast<std::uint8_t>((n >> (i * 8)) & 255);
}
static void write16(std::vector<std::uint8_t>& b, std::size_t off, std::uint16_t n) {
    assert(off + 2 <= b.size());
    b[off] = static_cast<std::uint8_t>(n & 255);
    b[off + 1] = static_cast<std::uint8_t>(n >> 8);
}
static void writeObj(const fs::path& file, bool brokenSection = false) {
    // Replica o layout de SceneWorld::open/build: 44 bytes, 2 indices
    // (8 bytes cada), blocos de 20 bytes por objeto; setores 8x8.
    std::vector<std::uint8_t> b(44 + 16 + 6 * 20, 0);
    const char sig[] = "HF Object File!";
    for (int i = 0; i < 15; ++i) b[i] = sig[i];
    write32(b, 16, 600); // version
    write32(b, 24, 2);   // secX
    write32(b, 28, 1);   // secY
    write32(b, 32, 8);   // sectionW
    write32(b, 36, 8);   // sectionH
    write32(b, 44, 60);  // first sector offset
    write32(b, 48, 6);   // six records
    if (brokenSection) {
        write32(b, 52, 4000); // bad second sector offset
        write32(b, 56, 1);
    }
    // 1 house; 1 house; 2 statue (missing); 33 absent ID;
    // 3 helper with mesh; kind != 0 means non-model (ignored)
    for (int i = 0; i < 6; ++i) {
        const int id = (i == 0 || i == 1) ? 1 : i == 2 ? 2 : i == 3 ? 33 : i == 4 ? 3 : 1;
        write16(b, 60 + i * 20, static_cast<std::uint16_t>((i == 5 ? 1u << 14 : 0u) | id));
    }
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}
static const top::AssetAuditEntry* find(const top::AssetAuditReport& r,
    const std::string& map, const std::string& category, const std::string& path) {
    for (const auto& e : r.entries)
        if (e.map == map && e.category == category && e.expected == path) return &e;
    return nullptr;
}
int main() {
    const fs::path tmp = fs::temp_directory_path() /
        ("top_05ah_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code ec; fs::remove_all(p, ec); } } cleanup{tmp};
    const fs::path root = tmp / "Client";
    put(root / "scripts/table/sceneobjinfo.txt",
        "1\tHouse.lmo\tHouse\t0\n2\tStatue.lmo\tStatue\t0\n3\tHelper.lmo\tCollision helper\t2\n4\tUnused.lmo\tUnused\t0\n");
    put(root / "model/scene/house.lmo", "dummy model"); // Windows case mismatch
    put(root / "model/scene/Helper.lmo", "dummy helper");
    put(root / "model/scene/Unused.lmo", "dummy unused");
    writeObj(root / "map/Garner.obj");
    const auto r = top::auditClientAssets(root.string(), "garner");
    assert(r.sceneMapsScanned == 1);
    assert(r.sceneInstances == 5); // kind 1 ignored
    assert(r.sceneModelsUsed == 3);
    assert(r.sceneModelsMissingUsed == 1); // Statue, not Unused
    assert(r.sceneModelsCaseMismatchUsed == 1); // House no Linux
    assert(r.sceneUnresolvedInstances == 1); // ID 33
    assert(r.sceneCatalogNotReferenced == 1); // Unused
    assert(r.sceneInvalidSections == 0);
    const auto* house = find(r, "garner", "scene_used", "model/scene/House.lmo");
    const auto* statue = find(r, "garner", "scene_used", "model/scene/Statue.lmo");
    const auto* helper = find(r, "garner", "scene_used", "model/scene/Helper.lmo");
    assert(house && house->status == "case_mismatch" && house->detail.find("instancias=2") != std::string::npos);
    assert(statue && statue->status == "missing" && statue->detail.find("instancias=1") != std::string::npos);
    assert(helper && helper->status == "ok" && helper->detail.find("auxiliares=1") != std::string::npos);
    assert(find(r, "garner", "scene_unmapped", "sceneobjinfo_id:33"));
    assert(!find(r, "garner", "scene_used", "model/scene/Unused.lmo"));
    const fs::path json = tmp / "report.json", csv = tmp / "report.csv";
    assert(top::writeAssetAuditJson(r, json.string()) && top::writeAssetAuditCsv(r, csv.string()));
    std::ifstream in(json); const std::string report((std::istreambuf_iterator<char>(in)), {});
    assert(report.find("\"scene_models_missing_used\": 1") != std::string::npos);
    assert(report.find("\"scene_unresolved_instances\": 1") != std::string::npos);
    // A falta de .obj e normal para certos mapas: nao se inventam objetos.
    fs::remove(root / "map/Garner.obj");
    const auto empty = top::auditClientAssets(root.string(), "garner");
    assert(empty.sceneMapsScanned == 0 && empty.sceneModelsMissingUsed == 0);
    assert(empty.sceneCatalogNotReferenced == 0);
    // Dados ruins do indice devem ser sinalizados, mas sem acesso fora do vetor.
    writeObj(root / "map/Garner.obj", true);
    const auto bad = top::auditClientAssets(root.string(), "garner");
    assert(bad.sceneInvalidSections == 1);
    std::cout << "05-AH PASS: uso real de .obj, ID/case, faltas relevantes, helper, indices e JSON/CSV\n";
}
