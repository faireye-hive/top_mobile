#pragma once
#include <array>
#include "top/EquipmentStats.h"
// Regras verificaveis da origem e geração deterministica de instâncias offline.
namespace top::item_instantiation {

enum class Origin : int { Shop = 0, MonsterDrop = 1, PlayerCombine = 2, Quest = 3 };
enum class HistoricalEvent : int {
    PlayerCombine = 2, QuestAward2 = 4, PlayerXsBox = 11,
    PlayerCcfsBoxA = 12, PlayerCcfsBoxB = 13, PlayerCcfsBoxC = 14,
    PlayerCcfsBoxD = 15, PlayerCcfsBoxE = 16, PlayerCcfsBoxF = 17,
    PlayerCcfsBoxG = 18, PlayerCcfsBoxH = 19, PlayerCcfsBoxI = 20,
    QuestAwardScBox = 95
};

struct Defaults {
    int maxDurability = 0;
    int maxEnergy = 0;
    bool tradable = true;
    long expiration = 0;
};

struct Result {
    Defaults defaults;
    bool valid = false;
    bool requiresHistoricalGenerator = false;
};

struct GeneratedQuality {
    int quality = 0;
    int attributeCount = 0;
    bool valid = false;
};

struct GeneratedAffix {
    struct Attribute {
        int itemAttributeId = 0; // ITEMATTR_VAL_* do ItemAttrType.h historico
        int percent = 0;         // valor de entrada de Creat_Item, antes do range do ItemInfo
    };
    int groupId = 0;
    std::array<Attribute, 3> attributes{};
    int attributeCount = 0;
    bool valid = false;
    bool hasAffix = false;
};

struct GeneratedInstance {
    int quality = 0;
    int maxDurability = 0;
    int maxEnergy = 0;
    equipment_stats::Bonus bonus;
    bool valid = false;
};

// Mirrors only the proven C++ ItemInstance contract. It does not invent
// random attributes and never implies that an origin-specific Lua branch ran.
Result initialize(Origin origin, int maxDurability, int maxEnergy,
                  bool tradable = true, long expiration = 0);

// Reproduces SetItemQua/CreateItemAttrCount for the implemented historical
// events. Rolls are the historical Lua math.random outputs (1..100) where the
// source event actually rolls; TiChun's events 2 and 12..20 are deterministic.
GeneratedQuality resolveQuality(HistoricalEvent event, int qualityRoll,
                                int attributeCountRoll, int itemType);

// Reproduces the weighted one/two/three-stat affix choice in SetItem*Attr.
// selectionRoll is math.random(1, sum(ItemAttr_Rad[group range])).
GeneratedAffix resolveAffix(HistoricalEvent event, int itemType, int itemLevel,
                            int quality, int attributeCount, int selectionRoll);

// ItemInstance transforms each Lua percentage through ItemInfo's min/max
// range. Returns only the delta over the static minimum used by the client.
equipment_stats::Bonus affixBonusDelta(const GeneratedAffix& affix,
                                      const equipment_stats::Bonus& minimum,
                                      const equipment_stats::Bonus& maximum);

// Gera uma instância repetível a partir do UID persistente. O escopo inclui os
// tipos dos eventos portados; tipos ainda não reproduzidos falham explicitamente.
GeneratedInstance materialize(HistoricalEvent event, int itemType, int itemLevel,
                              int uid, const equipment_stats::Bonus& minimum,
                              const equipment_stats::Bonus& maximum,
                              int durabilityMinimum = 0, int durabilityMaximum = 0,
                              int energyMinimum = 0, int energyMaximum = 0);

} // namespace top::item_instantiation
