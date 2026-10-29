#include "top/ItemInstantiationRules.h"

#include <algorithm>
#include <cstdint>

namespace top::item_instantiation {
namespace {
bool tiChunEvent(HistoricalEvent event) {
    const int id=static_cast<int>(event);
    return id==2||(id>=12&&id<=20);
}
}

Result initialize(Origin origin, int maxDurability, int maxEnergy,
                  bool tradable, long expiration) {
    const int rawOrigin = static_cast<int>(origin);
    if (rawOrigin < static_cast<int>(Origin::Shop) ||
        rawOrigin > static_cast<int>(Origin::Quest) ||
        maxDurability < 0 || maxEnergy < 0 || expiration < 0)
        return {};

    Result result;
    result.valid = true;
    result.defaults = {maxDurability, maxEnergy, tradable, expiration};
    // FightAble::ItemInstance calls Creat_Item for records with chInstance.
    // That script uses origin-specific branches, so mark them for the future
    // generator instead of silently treating the default attributes as final.
    result.requiresHistoricalGenerator = true;
    return result;
}

GeneratedQuality resolveQuality(HistoricalEvent event, int qualityRoll,
                                int attributeCountRoll, int itemType) {
    const int rawEvent = static_cast<int>(event);
    if (itemType < 0 || (!tiChunEvent(event) &&
        (qualityRoll < 1 || qualityRoll > 100 || attributeCountRoll < 1 || attributeCountRoll > 100)))
        return {};
    if (rawEvent==2) return {0,0,true};
    if (rawEvent>=12&&rawEvent<=20) return {rawEvent-11,6,true};
    if (rawEvent == static_cast<int>(HistoricalEvent::QuestAward2)) {
        int quality = qualityRoll <= 1 ? 4 : qualityRoll <= 5 ? 3 :
                      qualityRoll <= 20 ? 2 : qualityRoll <= 80 ? 1 : 0;
        int count = attributeCountRoll <= 10 ? 2 : attributeCountRoll <= 30 ? 1 : 0;
        // AttrCalculate.lua clamps a multi-stat roll to quality - 1, but
        // retains at least one stat when quality is zero or one.
        if (count >= 2 && itemType != 46)
            count = std::max(1, std::min(quality - 1, count));
        return {quality, count, true};
    }
    if ((rawEvent != static_cast<int>(HistoricalEvent::PlayerXsBox) &&
         rawEvent != static_cast<int>(HistoricalEvent::QuestAwardScBox)))
        return {};

    // Item_Mission_11/95 have 100 at quality-table index 7, yielding
    // quality 3 for every valid roll in Item_Quality_Ran.
    int count = attributeCountRoll == 1 ? 3 :
                attributeCountRoll <= 4 ? 2 :
                attributeCountRoll <= 50 ? 1 : 0;
    // CreatItemAttr clamps multi-affixes to quality - 1 except type 46.
    if (count >= 2 && itemType != 46)
        count = std::max(1, std::min(2, count));
    return {3, count, true};
}

GeneratedAffix resolveAffix(HistoricalEvent event, int itemType, int itemLevel,
                            int quality, int attributeCount, int selectionRoll) {
    const int rawEvent = static_cast<int>(event);
    if (rawEvent==2) return {0,{},0,true,false};
    if(rawEvent>=12&&rawEvent<=20) {
        if(itemType<1||itemLevel<0||quality<1||quality>9||attributeCount!=6)return {};
        if(itemLevel<=10&&itemType!=46)return {0,{},0,true,false};
        // TiChun's Lua path forces the random group for these armor types.
        std::array<GeneratedAffix::Attribute,3> attrs{};
        int group=0,count=0;
        const int percent=quality*10;
        auto set=[&](int id){attrs[static_cast<std::size_t>(count++)]={id,percent};};
        if(itemType==20){group=6;set(38);}                      // VAL_MXSP
        else if(itemType==22){group=21;set(37);set(43);}        // VAL_MXHP, VAL_HREC
        else if(itemType==23){group=60;set(35);set(34);set(41);} // MXATK, MNATK, CRT
        else if(itemType==24){group=61;set(45);set(38);set(44);} // MSPD, MXSP, SREC
        return {group,attrs,count,true,count>0};
    }
    if ((rawEvent != static_cast<int>(HistoricalEvent::QuestAward2) &&
         rawEvent != static_cast<int>(HistoricalEvent::PlayerXsBox) &&
         rawEvent != static_cast<int>(HistoricalEvent::QuestAwardScBox)) ||
        itemType < 1 || itemLevel < 0 || quality < 0 || quality > 10 ||
        attributeCount < 0 || attributeCount > 3)
        return {};

    // CreatItemAttr exits before rolling for low-level items, except type 46.
    if (itemLevel <= 10 && itemType != 46) return {0, {}, 0, true, false};
    if (attributeCount == 0) return {0, {}, 0, true, false};

    const bool supportedType = (itemType >= 1 && itemType <= 4) || itemType == 7 ||
        itemType == 9 || itemType == 11 || itemType == 20 || itemType == 22 ||
        itemType == 23 || itemType == 24 || itemType == 25 || itemType == 26 ||
        itemType == 27 || itemType == 29 || itemType == 46;
    if (!supportedType || (attributeCount == 3 && itemType != 46)) return {};

    static constexpr int oneWeights[] = {10, 10, 10, 5, 10};
    static constexpr int twoWeights[] = {10, 10, 5, 10, 10, 5, 10, 5, 10, 5};
    static constexpr int threeWeights[] = {10, 5, 10, 5, 10, 5, 5, 10, 5, 5};
    const int* weights = attributeCount == 1 ? oneWeights : attributeCount == 2 ? twoWeights : threeWeights;
    const int length = attributeCount == 1 ? 5 : 10;
    const int firstGroup = attributeCount == 1 ? 1 : attributeCount == 2 ? 11 : 50;
    int total = 0;
    for (int i = 0; i < length; ++i) total += weights[i];
    if (selectionRoll < 1 || selectionRoll > total) return {};
    int groupId = 0;
    int cumulative = 0;
    for (int i = 0; i < length; ++i) {
        cumulative += weights[i];
        if (selectionRoll <= cumulative) { groupId = firstGroup + i; break; }
    }

    const int percent = std::max(10, quality * 10);
    std::array<GeneratedAffix::Attribute, 3> attributes{};
    int outputCount = 0;
    auto add = [&](int stat) {
        // ItemAttrType.h: STR=26, AGI=27, DEX=28, CON=29, STA=30.
        static constexpr int itemAttrIds[] = {0, 26, 28, 29, 27, 30};
        if (stat < 1 || stat > 5 || outputCount >= static_cast<int>(attributes.size())) return;
        attributes[static_cast<std::size_t>(outputCount++)] = {itemAttrIds[stat], percent};
    };
    switch (groupId) {
        case 1: add(1); break;
        case 2: add(2); break;
        case 3: add(3); break;
        case 4: add(4); break;
        case 5: add(5); break;
        case 11: add(1); add(2); break;
        case 12: add(1); add(3); break;
        case 13: add(1); add(4); break;
        case 14: add(1); add(5); break;
        case 15: add(2); add(3); break;
        case 16: add(2); add(4); break;
        case 17: add(2); add(5); break;
        case 18: add(3); add(4); break;
        case 19: add(3); add(5); break;
        case 20: add(4); add(5); break;
        case 50: add(1); add(2); add(3); break;
        case 51: add(1); add(2); add(4); break;
        case 52: add(1); add(2); add(5); break;
        case 53: add(1); add(3); add(4); break;
        case 54: add(1); add(3); add(5); break;
        case 55: add(1); add(4); add(5); break;
        case 56: add(2); add(3); add(4); break;
        case 57: add(2); add(3); add(5); break;
        case 58: add(2); add(4); add(5); break;
        case 59: add(3); add(4); add(5); break;
        default: return {};
    }
    return {groupId, attributes, outputCount, true, true};
}

equipment_stats::Bonus affixBonusDelta(const GeneratedAffix& affix,
                                       const equipment_stats::Bonus& minimum,
                                       const equipment_stats::Bonus& maximum) {
    equipment_stats::Bonus delta;
    if (!affix.valid || !affix.hasAffix || affix.attributeCount < 1 ||
        affix.attributeCount > static_cast<int>(affix.attributes.size()))
        return delta;
    for (int i = 0; i < affix.attributeCount; ++i) {
        const auto& attribute = affix.attributes[static_cast<std::size_t>(i)];
        if (attribute.percent < 0 || attribute.percent > 100) continue;
        const auto rangeDelta = [&](int low, int high) {
            return static_cast<int>((static_cast<long long>(high) - low) * attribute.percent / 100);
        };
        const int itemDelta = [&]() {
            switch (attribute.itemAttributeId) {
                case 26: return rangeDelta(minimum.str, maximum.str); // STR
                case 27: return rangeDelta(minimum.agi, maximum.agi); // AGI
                case 28: return rangeDelta(minimum.dex, maximum.dex); // DEX
                case 29: return rangeDelta(minimum.con, maximum.con); // CON
                case 30: return rangeDelta(minimum.sta, maximum.sta); // STA
                case 34: return rangeDelta(minimum.minAttack, maximum.minAttack); // MNATK
                case 35: return rangeDelta(minimum.maxAttack, maximum.maxAttack); // MXATK
                case 37: return rangeDelta(minimum.hp, maximum.hp); // MXHP
                case 38: return rangeDelta(minimum.sp, maximum.sp); // MXSP
                case 41: return rangeDelta(minimum.critical, maximum.critical); // CRT
                case 43: return rangeDelta(minimum.hpRegen, maximum.hpRegen); // HREC
                case 44: return rangeDelta(minimum.spRegen, maximum.spRegen); // SREC
                case 45: return rangeDelta(minimum.moveSpeed, maximum.moveSpeed); // MSPD
                default: return 0;
            }
        }();
        switch (attribute.itemAttributeId) {
            case 26: delta.str += itemDelta; break;
            case 27: delta.agi += itemDelta; break;
            case 28: delta.dex += itemDelta; break;
            case 29: delta.con += itemDelta; break;
            case 30: delta.sta += itemDelta; break;
            case 34: delta.minAttack += itemDelta; break;
            case 35: delta.maxAttack += itemDelta; break;
            case 37: delta.hp += itemDelta; break;
            case 38: delta.sp += itemDelta; break;
            case 41: delta.critical += itemDelta; break;
            case 43: delta.hpRegen += itemDelta; break;
            case 44: delta.spRegen += itemDelta; break;
            case 45: delta.moveSpeed += itemDelta; break;
            default: break;
        }
    }
    return delta;
}

GeneratedInstance materialize(HistoricalEvent event, int itemType, int itemLevel,
                              int uid, const equipment_stats::Bonus& minimum,
                              const equipment_stats::Bonus& maximum,
                              int durabilityMinimum, int durabilityMaximum,
                              int energyMinimum, int energyMaximum) {
    const bool generatedWeaponType=(itemType>=1 && itemType<=4) || itemType==7 || itemType==9;
    const bool tiChunOutput=tiChunEvent(event)&&itemType>0;
    if (uid <= 0 || uid > 1000000000 || (!tiChunOutput&&!generatedWeaponType&&itemType!=22)) return {};

    // SplitMix32: não depende do estado global de rand(), da ordem em que o
    // mapa foi carregado, nem de um servidor. O UID gravado torna o resultado
    // estável ao salvar, reiniciar e reabrir a mesma instância.
    std::uint32_t state = static_cast<std::uint32_t>(uid) ^
        (static_cast<std::uint32_t>(static_cast<int>(event)) * 0x9e3779b9u) ^
        (static_cast<std::uint32_t>(itemType) * 0x85ebca6bu);
    auto next = [&]() {
        state += 0x9e3779b9u;
        std::uint32_t value = state;
        value = (value ^ (value >> 16)) * 0x85ebca6bu;
        value = (value ^ (value >> 13)) * 0xc2b2ae35u;
        return value ^ (value >> 16);
    };
    const int qualityRoll = static_cast<int>(next() % 100u) + 1;
    const int countRoll = static_cast<int>(next() % 100u) + 1;
    const GeneratedQuality quality = resolveQuality(event, qualityRoll, countRoll, itemType);
    if (!quality.valid) return {};
    const int count = quality.attributeCount;
    const int groupCount = count == 1 ? 50 : count == 2 ? 80 : 1;
    const int selectionRoll = count > 0 ? static_cast<int>(next() % static_cast<unsigned>(groupCount)) + 1 : 1;
    const GeneratedAffix affix = resolveAffix(event, itemType, itemLevel, quality.quality,
                                              count, selectionRoll);
    if (!affix.valid) return {};

    GeneratedInstance result;
    result.quality = quality.quality;
    const auto interpolate = [](int low, int high, int input) {
        const long long value = static_cast<long long>(low) +
            (static_cast<long long>(high) - low) * input / 100;
        return static_cast<int>(std::clamp(value, 0LL, 1000000000LL));
    };
    // FightAble::ItemInstance transforms MAXURE by quality (0..100) and
    // MAXENERGY by quality*100 + selected attribute group (legacy C++ uses
    // /100 here despite the Lua comment describing a per-mille scale).
    result.maxDurability = interpolate(durabilityMinimum, durabilityMaximum,
                                       quality.quality);
    const int energyInput = itemLevel <= 10 && itemType != 46 ? 0 :
        quality.quality * 100 + (affix.hasAffix ? affix.groupId : 0);
    result.maxEnergy = interpolate(energyMinimum, energyMaximum, energyInput);
    result.bonus = affixBonusDelta(affix, minimum, maximum);
    // Creat_Item_Tattr passes quality*10 for the basic attack/defense attrs;
    // ItemInstance then interpolates each ItemInfo min/max pair by that value.
    if (itemType >= 1 && itemType <= 10) {
        result.bonus.minAttack += static_cast<int>((static_cast<long long>(maximum.minAttack) - minimum.minAttack) * quality.quality * 10 / 100);
        result.bonus.maxAttack += static_cast<int>((static_cast<long long>(maximum.maxAttack) - minimum.maxAttack) * quality.quality * 10 / 100);
    } else if (itemType == 22) {
        result.bonus.defense += static_cast<int>((static_cast<long long>(maximum.defense) - minimum.defense) * quality.quality * 10 / 100);
        result.bonus.resistance += static_cast<int>((static_cast<long long>(maximum.resistance) - minimum.resistance) * quality.quality * 10 / 100);
    }
    result.valid = true;
    return result;
}

} // namespace top::item_instantiation
