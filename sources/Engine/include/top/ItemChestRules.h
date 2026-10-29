#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace top::item_chests {
// Regras auditadas em ItemEffect.lua. Dynamic DataRules lê apenas tabelas
// literais conhecidas; o dispatcher exige ID e callback iguais ao iteminfo.
struct Grant { int itemId=0; int count=0; int quality=-1; };
struct WeightedGrant { int weight=0; Grant grant; };
struct Plan {
    int requiredFreeSlots = 0; // GetChaFreeBagGridNum ANTES de consumir
    std::vector<Grant> rewards;
    std::vector<Grant> randomChoices; // escolhe exatamente uma recompensa, uniforme
    bool requiresShore = false;
    int minimumLevel = 0;
    std::vector<int> allowedJobs;
    // Some historical prize callbacks roll one outcome containing several
    // grants (instead of one item from randomChoices).
    std::vector<std::vector<Grant>> randomRewardGroups;
    std::vector<WeightedGrant> weightedRandomChoices;
    // Optional player-facing offline message for rewards whose Lua callback
    // also used a server-wide Notice that cannot exist in a solo client.
    std::string successMessage;
};
class DataRules {
public:
    bool load(const std::string& itemEffectPath);
    std::optional<Plan> planFor(int itemId, const std::string& callback) const;
    bool supports(int itemId, const std::string& callback) const;
    std::size_t mappedItems() const { return plans_.size(); }
private:
    std::unordered_map<int, std::pair<std::string, Plan>> plans_;
};
std::optional<Plan> planFor(int itemId, const std::string& callback, int characterType,
                            int job = 0);
// True somente para armaduras Relic conhecidas, bloqueadas por tipo.
bool characterRestrictedBox(int itemId, const std::string& callback);
} // namespace top::item_chests
