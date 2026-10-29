#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace top {

// Diagnostico passivo: nao converte nem altera dados originais do cliente.
struct AssetAuditEntry {
    std::string map;         // vazio para recursos globais
    std::string category;    // terrain, scene, npc, spawns, portals, ...
    std::string expected;    // caminho relativo ao Client
    std::string actual;      // caminho real relativo ao Client (se encontrado)
    std::string status;      // ok, case_mismatch, missing, invalid, empty
    std::string importance;  // required, optional (ou diagnostic)
    std::string detail;
};

// 05-AI: impacto real por mapa analisado. Mapas sem .obj nao sao listados
// (ausencia de dados != zero modelos faltando).
struct SceneMapImpact {
    std::string map;
    std::size_t instances = 0;
    std::size_t distinctModels = 0;
    std::size_t missingModels = 0;
    std::size_t missingInstances = 0;
    std::size_t missingVisual = 0;
    std::size_t missingHelpers = 0;
    std::size_t unresolvedInstances = 0;
    std::size_t invalidSections = 0;
};

struct AssetAuditReport {
    std::string clientRoot;
    std::vector<AssetAuditEntry> entries;
    std::size_t mapCount = 0;
    std::size_t validMapCount = 0;
    std::size_t mapsWithNpcData = 0;
    std::size_t mapsWithSpawnData = 0;
    std::size_t mapsWithPortalData = 0;
    std::size_t missingRequired = 0;
    std::size_t missingOptional = 0;
    std::size_t caseMismatches = 0;
    std::size_t invalidResources = 0;
    // 05-AH: referencias que aparecem nos .obj disponiveis, nao em todo o catalogo.
    std::size_t sceneMapsScanned = 0;
    std::size_t sceneInstances = 0;
    std::size_t sceneModelsUsed = 0;
    std::size_t sceneModelsMissingUsed = 0;
    std::size_t sceneModelsCaseMismatchUsed = 0;
    std::size_t sceneUnresolvedInstances = 0;
    std::size_t sceneCatalogNotReferenced = 0;
    std::size_t sceneInvalidSections = 0;
    std::size_t sceneIgnoredByCap = 0;
    std::size_t sceneMissingInstances = 0;
    std::size_t sceneMapsWithMissingModels = 0;
    std::vector<SceneMapImpact> sceneImpact;
};

AssetAuditReport auditClientAssets(const std::string& clientRoot, const std::string& mapFilter = {});
bool writeAssetAuditJson(const AssetAuditReport& report, const std::string& output);
bool writeAssetAuditCsv(const AssetAuditReport& report, const std::string& output);
// Relatorios adicionais 05-AI: ranking por mapa e nomes exatos dos arquivos
// ausentes. Nenhum arquivo do Client e modificado.
bool writeSceneImpactCsv(const AssetAuditReport& report, const std::string& output);
bool writeMissingSceneModelsTxt(const AssetAuditReport& report, const std::string& output);
// CLI compartilhada por top_gl --audit-assets e pelo executavel headless.
int runAssetAuditCli(int argc, char** argv);

} // namespace top
