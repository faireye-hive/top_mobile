#include "top/ItemChestRules.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace top::item_chests {
namespace {
bool readFile(const std::string& path, std::string& text) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream data;
    data << file.rdbuf();
    text = data.str();
    return file.good() || file.eof();
}

bool integerField(const std::string& row, const std::string& field, int& value) {
    std::smatch match;
    const std::regex pattern("(?:^|[^A-Za-z0-9_])" + field + R"(\s*=\s*(\d+))");
    if (!std::regex_search(row, match, pattern)) return false;
    try { value = std::stoi(match[1].str()); return true; }
    catch (...) { return false; }
}

std::vector<int> arrayField(const std::string& row, const std::string& field) {
    std::smatch match;
    const std::regex pattern("(?:^|[^A-Za-z0-9_])" + field + R"(\s*=\s*\{([^}]*)\})");
    if (!std::regex_search(row, match, pattern)) return {};
    std::vector<int> values;
    const std::string content = match[1].str();
    const std::regex number(R"(\d+)");
    for (auto it = std::sregex_iterator(content.begin(), content.end(), number);
         it != std::sregex_iterator(); ++it) {
        try { values.push_back(std::stoi((*it)[0].str())); }
        catch (...) { return {}; }
    }
    return values;
}
} // namespace

bool DataRules::load(const std::string& itemEffectPath) {
    plans_.clear();
    std::string effects;
    if (!readFile(itemEffectPath, effects)) return false;
    const std::regex functionPattern(
        R"(function\s+(Corsairs_WeaponChest|Corsairs_WeaponChest2|Corsairs_EquipmentChest)\s*\([^\n]*\)([\s\S]*?)(?=\n\s*function\s+|\s*$))");
    const std::regex entryPattern(R"(Chest\s*\[\s*(\d+)\s*\]\s*=\s*\{(.*)\})");
    for (auto function = std::sregex_iterator(effects.begin(), effects.end(), functionPattern);
         function != std::sregex_iterator(); ++function) {
        const std::string callback = (*function)[1].str();
        const std::string body = (*function)[2].str();
        std::istringstream lines(body);
        std::string line;
        while (std::getline(lines, line)) {
            std::smatch entry;
            if (!std::regex_search(line, entry, entryPattern)) continue;
            int itemId = 0;
            try { itemId = std::stoi(entry[1].str()); }
            catch (...) { continue; }
            const std::string row = entry[2].str();
            int active = 0;
            if (itemId <= 0 || !integerField(row, "Active", active) || active != 1) continue;

            std::vector<int> itemIds = arrayField(row, "Item");
            std::vector<int> quantities = arrayField(row, "Quantity");
            std::vector<int> qualities = arrayField(row, "Quality");
            int scalarItem = 0, scalarQuantity = 0, scalarQuality = 0;
            const bool hasScalarItem = integerField(row, "Item", scalarItem);
            const bool hasScalarQuantity = integerField(row, "Quantity", scalarQuantity);
            const bool hasScalarQuality = integerField(row, "Quality", scalarQuality);
            Plan plan;
            plan.requiresShore = true;

            if (callback == "Corsairs_EquipmentChest") {
                if (itemIds.empty() || quantities.size() != itemIds.size() ||
                    qualities.size() != itemIds.size()) continue;
                plan.requiredFreeSlots = static_cast<int>(itemIds.size());
                bool valid = true;
                for (std::size_t i = 0; i < itemIds.size(); ++i) {
                    if (itemIds[i] <= 0 || quantities[i] <= 0 || qualities[i] < 0) { valid = false; break; }
                    plan.rewards.push_back({itemIds[i], quantities[i], qualities[i]});
                }
                if (!valid) continue;
            } else if (callback == "Corsairs_WeaponChest") {
                if (!hasScalarItem || !hasScalarQuantity || !hasScalarQuality ||
                    scalarItem <= 0 || scalarQuantity <= 0 || scalarQuality < 0) continue;
                plan.requiredFreeSlots = scalarQuantity + 1; // histórico: Free <= Quantity falha
                plan.rewards.push_back({scalarItem, scalarQuantity, scalarQuality});
            } else {
                if (itemIds.empty() || !hasScalarQuantity || !hasScalarQuality ||
                    scalarQuantity <= 0 || scalarQuality < 0) continue;
                plan.requiredFreeSlots = scalarQuantity + 1; // mesma condição Lua do WeaponChest
                bool valid = true;
                for (int candidate : itemIds) {
                    if (candidate <= 0) { valid = false; break; }
                    plan.randomChoices.push_back({candidate, scalarQuantity, scalarQuality});
                }
                if (!valid) continue;
            }
            plans_[itemId] = {callback, std::move(plan)};
        }
    }
    return !plans_.empty();
}

std::optional<Plan> DataRules::planFor(int itemId, const std::string& callback) const {
    const auto found = plans_.find(itemId);
    if (found == plans_.end() || found->second.first != callback) return std::nullopt;
    return found->second.second;
}

bool DataRules::supports(int itemId, const std::string& callback) const {
    const auto found = plans_.find(itemId);
    return found != plans_.end() && found->second.first == callback;
}

bool characterRestrictedBox(int id, const std::string& callback) {
    switch (id) {
        case 209: return callback == "ItemUse_SCBoxYXTZ";
        case 210: return callback == "ItemUse_SCBoxLSTZ";
        case 211: return callback == "ItemUse_SCBoxHYTZ";
        case 212: return callback == "ItemUse_SCBoxFYSTZ";
        case 213: return callback == "ItemUse_SCBoxSZZTZ";
        case 214: return callback == "ItemUse_SCBoxHHSTZ";
        default: return false;
    }
}

std::optional<Plan> planFor(int itemId, const std::string& callback, int characterType,
                            int job) {
    // ItemEffect.lua::ItemUse_minibh uses 1..10000: grand prize 1,
    // first prize 50, second prize 1600, and third prize 8349.
    if (itemId == 1019 && callback == "ItemUse_minibh") {
        Plan plan;
        plan.requiredFreeSlots = 1;
        plan.weightedRandomChoices = {
            {1,{272,1,4}}, {50,{273,1,4}}, {1600,{274,1,4}}, {8349,{275,1,4}}
        };
        return plan;
    }
    // Death's Burden A..H: literal ItemEffect.lua callback tables. A roll of
    // 4/4 grants the class piece; the other three outcomes grant the common
    // piece, with both rewards at GiveItem quality 16.
    struct DeathBurden { int id; const char* callback; int rare; int common; };
    static constexpr DeathBurden deathBurdens[] = {
        {2919,"ItemUse_DathBagA",2846,2928}, {2920,"ItemUse_DathBagB",2847,2929},
        {2921,"ItemUse_DathBagC",2848,2927}, {2922,"ItemUse_DathBagD",2849,2927},
        {2923,"ItemUse_DathBagE",2850,2929}, {2924,"ItemUse_DathBagF",2851,2931},
        {2925,"ItemUse_DathBagG",2852,2932}, {2926,"ItemUse_DathBagH",2930,2930},
    };
    for (const DeathBurden& bag : deathBurdens) {
        if (itemId != bag.id || callback != bag.callback) continue;
        Plan plan;
        plan.requiredFreeSlots = 2; // Lua rejects GetChaFreeBagGridNum(role) < 2
        plan.requiresShore = true;
        if (bag.rare == bag.common) {
            plan.rewards = {{bag.common,1,16}};
        } else {
            plan.weightedRandomChoices = {{1,{bag.rare,1,16}}, {3,{bag.common,1,16}}};
        }
        return plan;
    }

    // Fixed gem vouchers: each callback grants one known item at quality 4.
    switch (itemId) {
        case 856: if (callback == "ItemUse_lieyanBook") return Plan{1, {{878,1,4}}}; break;
        case 857: if (callback == "ItemUse_zhiyanBook") return Plan{1, {{879,1,4}}}; break;
        case 858: if (callback == "ItemUse_huoyaoBook") return Plan{1, {{880,1,4}}}; break;
        case 859: if (callback == "ItemUse_manaoBook") return Plan{1, {{881,1,4}}}; break;
        case 911: if (callback == "ItemUse_shengguangBook") return Plan{1, {{887,1,4}}}; break;
        case 914: if (callback == "ItemUse_hanyuBook") return Plan{1, {{882,1,4}}}; break;
        case 915: if (callback == "ItemUse_yuezhixinBook") return Plan{1, {{883,1,4}}}; break;
        case 916: if (callback == "ItemUse_xianlingBook") return Plan{1, {{884,1,4}}}; break;
        case 918: if (callback == "ItemUse_fenglingBook") return Plan{1, {{860,1,4}}}; break;
        case 919: if (callback == "ItemUse_yingyanBook") return Plan{1, {{861,1,4}}}; break;
        case 920: if (callback == "ItemUse_yanyuBook") return Plan{1, {{862,1,4}}}; break;
        case 921: if (callback == "ItemUse_tanyuBook") return Plan{1, {{863,1,4}}}; break;
        default: break;
    }
    // Black Dragon prize vouchers.
    if (itemId == 272 && callback == "ItemUse_tedengjiang")
        return Plan{3, {{845,1,16},{846,1,16},{847,1,16}}};
    if (itemId == 273 && callback == "ItemUse_yidengjiang")
        return Plan{1, {{853,1,4}}};
    if (itemId == 274 && callback == "ItemUse_erdengjiang") {
        Plan plan;
        plan.requiredFreeSlots = 14; // Lua checks free grids before any GiveItem.
        plan.rewards = {{456,10,4}}; // Strengthening Crystal.
        // The four source outcomes containing missing iteminfo IDs 5034..5038
        // remain literal in the table. The dispatcher rolls one group, then
        // preserves the chest transactionally if any selected item is absent.
        plan.randomRewardGroups = {
            {{5013,1,4},{5021,1,4},{5029,1,4}},
            {{5014,1,4},{5022,1,4},{5030,1,4}},
            {{5015,1,4},{5023,1,4},{5031,1,4}},
            {{5020,1,4},{5024,1,4},{5032,1,4},{5037,1,4}},
            {{5017,1,4},{5025,1,4},{5033,1,4}},
            {{5018,1,4},{5026,1,4},{5034,1,4}},
            {{5019,1,4},{5027,1,4},{5035,1,4}},
            {{5016,1,4},{5028,1,4},{5036,1,4},{5038,1,4}},
        };
        return plan;
    }
    // ItemEffect.lua::ItemUse_YZheBOX (item 460, Champion's Chest): four
    // guaranteed materials, followed by one of eight literal reward groups.
    // Several historical outcomes reference absent iteminfo rows (5034..5038);
    // the shared chest transaction keeps the chest if that selected reward
    // cannot be represented by the available historical catalog.
    if (itemId == 460 && callback == "ItemUse_YZheBOX") {
        Plan plan;
        plan.requiredFreeSlots = 8; // Lua rejects before grants when free slots < 8.
        plan.rewards = {{453,1,4},{454,1,4},{455,1,4},{456,1,4}};
        plan.randomRewardGroups = {
            {{5013,1,4},{5021,1,4},{5029,1,4}},
            {{5014,1,4},{5022,1,4},{5030,1,4}},
            {{5015,1,4},{5023,1,4},{5031,1,4}},
            {{5016,1,4},{5024,1,4},{5032,1,4},{5037,1,4}},
            {{5017,1,4},{5025,1,4},{5033,1,4}},
            {{5018,1,4},{5026,1,4},{5034,1,4}},
            {{5019,1,4},{5027,1,4},{5035,1,4}},
            {{5020,1,4},{5028,1,4},{5036,1,4},{5038,1,4}},
        };
        return plan;
    }
    if (itemId == 1036 && callback == "ItemUse_mohuan") {
        Plan plan;
        plan.requiredFreeSlots = 1;
        // ItemEffect.lua rolls 1..1000. The four ranges plus fallback have
        // exact weights 8, 9, 10, 200, and 773; all outputs use quality 4.
        plan.weightedRandomChoices = {
            {8,{1014,1,4}},
            {9,{271,1,4}},
            {10,{1012,1,4}},
            {200,{3886,1,4}},
            {773,{19525,5,4}},
        };
        return plan;
    }
    if (itemId == 2371 && callback == "ItemUse_ALDXB") {
        Plan plan;
        plan.requiredFreeSlots = 1;
        // Flatten ItemEffect.lua's 20,000 equally likely first rolls and its
        // nested uniform rolls into their exact marginal weights. Missing
        // catalog entries (the 195xx rewards) fail transactionally at grant.
        plan.weightedRandomChoices = {
            {3700,{2440,10,4}},
            {2000,{3885,1,4}},
            {1400,{3094,1,4}},
            {1200,{19540,3,4}}, {1200,{19538,3,4}},
            {1200,{19546,3,4}}, {1200,{19539,3,4}}, {1200,{19525,3,4}},
            {1000,{849,1,4}}, {1000,{680,1,4}},
            {700,{2438,5,4}}, {700,{2419,3,4}}, {700,{2386,4,4}},
            {700,{179,1,4}}, {700,{3084,1,4}}, {700,{3085,1,4}},
            {60,{863,1,4}}, {60,{860,1,4}}, {60,{861,1,4}},
            {60,{862,1,4}}, {60,{1012,1,4}},
            {1,{192,1,4}},
            {399,{2224,1,4}},
        };
        return plan;
    }
    if (itemId == 275 && callback == "ItemUse_sandengjiang") {
        Plan plan;
        plan.requiredFreeSlots = 2;
        plan.randomRewardGroups = {
            {{3094,1,4},{3096,1,4}},
            {{1094,1,4}}
        };
        return plan;
    }
    // Recompensas fixas de baus de evento com todas as saidas presentes no iteminfo historico.
    if (itemId == 1017 && callback == "ItemUse_JaNaBox")
        return Plan{1, {{1012,1,4}}};
    if (itemId == 2897 && callback == "ItemUse_SDDLB")
        return Plan{6, {{2894,1,4},{2893,10,4},{2889,1,4},
                        {2890,1,4},{2891,1,4},{2896,99,4}}};
    // Posh Christmas Box: four fixed quality-4 summon scrolls, with the
    // historical pre-open requirement of four free inventory slots.
    if (itemId == 2900 && callback == "ItemUse_GWZHQ")
        return Plan{4, {{2888,1,4},{2889,1,4},{2890,1,4},{2891,1,4}}};
    // Number Ticket: issue its fixed quality-4 IPOD Voucher. The historical
    // global Notice is represented by a local message in the offline client.
    if (itemId == 2305 && callback == "ItemUse_SZF") {
        Plan plan{1, {{2306,1,4}}};
        plan.successMessage = "Number Ticket: IPOD Voucher emitido; anuncio global historico indisponivel offline.";
        return plan;
    }
    // New Year Chest: one voucher from the exact 30,000-outcome Lua table.
    if ((itemId == 2307 && callback == "ItemUse_XNBOX") ||
        (itemId == 1011 && callback == "ItemUse_HHLP")) {
        Plan plan;
        plan.requiredFreeSlots = 1;
        plan.minimumLevel = 40;
        plan.weightedRandomChoices = {
            {300, {2240,1,4}}, {1000, {2237,1,4}}, {3000, {2239,1,4}},
            {10000, {2241,1,4}}, {3925, {2242,1,4}}, {3925, {2243,1,4}},
            {3925, {2244,1,4}}, {3925, {2245,1,4}}
        };
        return plan;
    }
    // Chaos Chest: the historical callback always grants the voucher, then
    // one of four Chaos equipment stones. Keep its original one-free-slot
    // precheck; the offline transaction additionally verifies that both
    // outputs fit before consuming the chest.
    if (itemId == 1134 && callback == "ItemUse_FightingBox") {
        Plan plan;
        plan.requiredFreeSlots = 1;
        plan.rewards = {{2610,1,4}};
        plan.randomChoices = {{1124,1,15},{1125,1,15},{1126,1,15},{1127,1,15}};
        return plan;
    }
    // Fixed Lv75 boss-set chests in ItemEffect.lua. The callbacks require four
    // free slots before consuming the chest and grant three quality-4 items.
    switch (itemId) {
        case 2836: if (callback == "ItemUse_Hadisi")
            return Plan{4, {{2817,1,4},{2818,1,4},{2819,1,4}}, {}, true}; break;
        case 2837: if (callback == "ItemUse_Anhei")
            return Plan{4, {{2820,1,4},{2821,1,4},{2822,1,4}}, {}, true}; break;
        case 2838: if (callback == "ItemUse_Diyu")
            return Plan{4, {{2823,1,4},{2824,1,4},{2825,1,4}}, {}, true}; break;
        case 2839: if (callback == "ItemUse_Xiuluo")
            return Plan{4, {{2826,1,4},{2827,1,4},{2828,1,4}}, {}, true}; break;
        case 2840: if (callback == "ItemUse_Youming")
            return Plan{4, {{2829,1,4},{2830,1,4},{2831,1,4}}, {}, true}; break;
        case 2841: if (callback == "ItemUse_Minghe")
            return Plan{4, {{2832,1,4},{2833,1,4},{2834,1,4}}, {}, true}; break;
        case 2842: if (callback == "ItemUse_Sishen") {
            Plan plan;
            plan.requiredFreeSlots = 2;
            plan.requiresShore = true;
            plan.minimumLevel = 40;
            plan.allowedJobs = {8,9,12,13,14,16};
            if (job == 9) plan.randomChoices = {{2331,1,4},{2332,1,4}};
            else if (job == 8) plan.randomChoices = {{2333,1,4},{2334,1,4}};
            else if (job == 12) plan.randomChoices = {{2337,1,4},{2338,1,4},{2339,1,4},{2340,1,4}};
            else if (job == 16) plan.randomChoices = {{2335,1,4},{2336,1,4}};
            else if (job == 13) plan.randomChoices = {{2341,1,4},{2342,1,4}};
            else if (job == 14) plan.randomChoices = {{2343,1,4},{2344,1,4}};
            return plan;
        } break;
        default: break;
    }
    // ItemEffect.lua: Chest of Kylin. O quarto item pertence somente ao tipo 4.
    if (itemId == 192 && callback == "ItemUse_SaintCloth") {
        Plan plan;
        plan.requiredFreeSlots = 4;
        if (characterType == 4) plan.rewards.push_back({828, 1});
        plan.rewards.push_back({825, 1});
        plan.rewards.push_back({826, 1});
        plan.rewards.push_back({827, 1});
        return plan;
    }
    // 05-AY: caixas Relic de armadura, com exigencia e tipo historicos.
    // A variante do tipo 4 pode adicionar um quarto equipamento.
    switch (itemId) {
        case 209: if (callback == "ItemUse_SCBoxYXTZ" && characterType == 1)
            return Plan{3, {{395,1}, {587,1}, {747,1}}}; break;
        case 210: if (callback == "ItemUse_SCBoxLSTZ" && characterType == 2)
            return Plan{4, {{397,1}, {829,1}, {603,1}}}; break;
        case 211: if (callback == "ItemUse_SCBoxHYTZ" && (characterType == 1 || characterType == 3))
            return Plan{3, {{399,1}, {589,1}, {749,1}}}; break;
        case 212: if (callback == "ItemUse_SCBoxFYSTZ") {
            if (characterType == 3) return Plan{4, {{401,1},{591,1},{751,1}}};
            if (characterType == 4) return Plan{4, {{403,1},{593,1},{753,1},{2218,1}}};
            break;
        }
        case 213: if (callback == "ItemUse_SCBoxSZZTZ") {
            if (characterType == 3) return Plan{4, {{405,1},{595,1},{755,1}}};
            if (characterType == 4) return Plan{4, {{407,1},{597,1},{757,1},{2220,1}}};
            break;
        }
        case 214: if (callback == "ItemUse_SCBoxHHSTZ") {
            if (characterType == 1 || characterType == 3)
                return Plan{4, {{409,1},{599,1},{759,1}}};
            if (characterType == 4)
                return Plan{4, {{412,1},{601,1},{761,1},{2222,1}}};
            break;
        }
        default: break;
    }
    // ItemEffect.lua: sete Relic Weapon Boxes sem sorteio, com uma GiveItem
    // por callback e exigencia historica de 1 slot livre ANTES da abertura.
    switch (itemId) {
        case 215: if (callback == "ItemUse_SCBoxRYBZ") return Plan{1, {{109, 1}}}; break;
        case 216: if (callback == "ItemUse_SCBoxXMC") return Plan{1, {{111, 1}}}; break;
        case 217: if (callback == "ItemUse_SCBoxLQJ") return Plan{1, {{113, 1}}}; break;
        case 218: if (callback == "ItemUse_SCBoxLYJ") return Plan{1, {{115, 1}}}; break;
        case 219: if (callback == "ItemUse_SCBoxFHG") return Plan{1, {{117, 1}}}; break;
        case 220: if (callback == "ItemUse_SCBoxLXHZ") return Plan{1, {{119, 1}}}; break;
        case 221: if (callback == "ItemUse_SCBoxYCJ") return Plan{1, {{150, 1}}}; break;
        default: break;
    }
    return std::nullopt;
}
} // namespace top::item_chests
