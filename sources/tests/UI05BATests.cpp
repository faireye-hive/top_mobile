#include "top/ItemInstantiationRules.h"
#include "top/ItemInstancePersistence.h"

#include <cassert>
#include <string>
#include <vector>

struct PersistedStack {
    int id=0, count=0, uid=0, forgeLevel=0;
    std::array<int,3> gemIds{};
    top::equipment_stats::Bonus extra;
    int quality=0, durability=0, maxDurability=0, energy=0, maxEnergy=0;
    bool tradable=true;
    int itemValStaTenths=0;
    std::array<int,3> blueprintParts{};
    std::array<int,6> fairySkills{};
    std::array<int,5> fairyStats{};
    int fairySystemTick=0,expDisableFlag=0,altarSacrifice=0,cloakLevel=0,fuseId=0;
    int equipmentUpgradeLevel=0;
};

int main() {
    using namespace top::item_instantiation;
    const auto shop = initialize(Origin::Shop, 80, 120, true, 0);
    assert(shop.valid && shop.requiresHistoricalGenerator);
    assert(shop.defaults.maxDurability == 80 && shop.defaults.maxEnergy == 120);
    assert(shop.defaults.tradable && shop.defaults.expiration == 0);

    const auto reward = initialize(Origin::Quest, 40, 0, false, 1234);
    assert(reward.valid && reward.requiresHistoricalGenerator);
    assert(!reward.defaults.tradable && reward.defaults.expiration == 1234);

    assert(!initialize(static_cast<Origin>(9), 10, 10).valid);
    assert(!initialize(Origin::MonsterDrop, -1, 10).valid);
    assert(!initialize(Origin::PlayerCombine, 10, -1).valid);
    assert(!initialize(Origin::Quest, 10, 10, true, -1).valid);

    int noAttributes = 0, oneAttribute = 0, twoAttributes = 0;
    for (int roll = 1; roll <= 100; ++roll) {
        const auto xsbox = resolveQuality(HistoricalEvent::PlayerXsBox, roll, roll, 1);
        const auto questChest = resolveQuality(HistoricalEvent::QuestAwardScBox, roll, roll, 22);
        assert(xsbox.valid && xsbox.quality == 3);
        assert(questChest.valid && questChest.quality == 3);
        assert(xsbox.attributeCount == questChest.attributeCount);
        if (xsbox.attributeCount == 0) ++noAttributes;
        else if (xsbox.attributeCount == 1) ++oneAttribute;
        else if (xsbox.attributeCount == 2) ++twoAttributes;
        else assert(false && "regular items cap affix count at quality - 1");
    }
    assert(noAttributes == 50 && oneAttribute == 46 && twoAttributes == 4);

    // Type 46 deliberately skips CreatItemAttr's quality-based cap.
    assert(resolveQuality(HistoricalEvent::QuestAwardScBox, 100, 1, 46).attributeCount == 3);
    assert(!resolveQuality(static_cast<HistoricalEvent>(12), 1, 1, 1).valid);
    assert(!resolveQuality(HistoricalEvent::PlayerXsBox, 0, 1, 1).valid);
    assert(!resolveQuality(HistoricalEvent::PlayerXsBox, 1, 101, 1).valid);

    const auto one = resolveAffix(HistoricalEvent::PlayerXsBox, 1, 20, 3, 1, 1);
    assert(one.valid && one.hasAffix && one.groupId == 1 && one.attributeCount == 1 &&
           one.attributes[0].itemAttributeId == 26 && one.attributes[0].percent == 30);
    const auto two = resolveAffix(HistoricalEvent::QuestAwardScBox, 22, 20, 3, 2, 1);
    assert(two.valid && two.hasAffix && two.groupId == 11 && two.attributeCount == 2 &&
           two.attributes[0].itemAttributeId == 26 && two.attributes[1].itemAttributeId == 28 &&
           two.attributes[0].percent == 30 && two.attributes[1].percent == 30);
    top::equipment_stats::Bonus minimum{}, maximum{};
    maximum.str = 12;
    maximum.dex = 8;
    const auto resolved = affixBonusDelta(two, minimum, maximum);
    assert(resolved.str == 3 && resolved.dex == 2);
    std::vector<std::string> itemColumns(74, "0");
    itemColumns[52] = "0,12";
    itemColumns[54] = "0,8";
    itemColumns[39] = "8,16";
    itemColumns[40] = "12,24";
    itemColumns[62] = "100,200";
    itemColumns[73] = "100,200";
    const auto parsedMinimum = top::equipment_stats::fromColumns(itemColumns);
    const auto parsedMaximum = top::equipment_stats::maximumFromColumns(itemColumns);
    const auto parsedAffix = affixBonusDelta(two, parsedMinimum, parsedMaximum);
    assert(parsedAffix.str == 3 && parsedAffix.dex == 2);
    assert(parsedMinimum.minAttackCoefficient == 8 && parsedMinimum.maxAttackCoefficient == 12 &&
           parsedMaximum.minAttackCoefficient == 16 && parsedMaximum.maxAttackCoefficient == 24);
    const auto generated=materialize(HistoricalEvent::QuestAwardScBox,22,40,7001,
                                     parsedMinimum,parsedMaximum,10000,20000,1000,1100);
    const auto generatedAgain=materialize(HistoricalEvent::QuestAwardScBox,22,40,7001,
                                          parsedMinimum,parsedMaximum,10000,20000,1000,1100);
    assert(generated.valid && generated.quality==3 && generated.maxEnergy>=1300 && generated.maxEnergy<=1320 &&
           generated.maxDurability==10300 &&
           generated.bonus.defense>=30 && generated.bonus.resistance>=30 &&
           generatedAgain.valid && generatedAgain.quality==generated.quality &&
           generatedAgain.maxEnergy==generated.maxEnergy && generatedAgain.bonus.str==generated.bonus.str &&
           generatedAgain.bonus.agi==generated.bonus.agi && generatedAgain.bonus.dex==generated.bonus.dex &&
           !materialize(HistoricalEvent::QuestAwardScBox,41,40,7001,
                     parsedMinimum,parsedMaximum).valid);
    const auto three = resolveAffix(HistoricalEvent::QuestAwardScBox, 46, 1, 3, 3, 70);
    assert(three.valid && three.hasAffix && three.groupId == 59 && three.attributeCount == 3 &&
           three.attributes[0].itemAttributeId == 29 && three.attributes[1].itemAttributeId == 27 &&
           three.attributes[2].itemAttributeId == 30);
    const auto lowLevel = resolveAffix(HistoricalEvent::PlayerXsBox, 22, 10, 3, 2, 0);
    assert(lowLevel.valid && !lowLevel.hasAffix);
    const auto noAffix = resolveAffix(HistoricalEvent::PlayerXsBox, 22, 20, 3, 0, 0);
    assert(noAffix.valid && !noAffix.hasAffix);
    assert(!resolveAffix(HistoricalEvent::PlayerXsBox, 5, 20, 3, 1, 1).valid);
    assert(!resolveAffix(HistoricalEvent::PlayerXsBox, 22, 20, 3, 2, 81).valid);

    // Y3 conserva o estado de blueprint e aceita os registros Y1/Y2.
    PersistedStack saved;
    saved.id=123; saved.count=1; saved.uid=42; saved.forgeLevel=3;
    saved.gemIds={8,9,10}; saved.extra.str=17;
    saved.extra.minAttackCoefficient=8; saved.extra.maxAttackCoefficient=12;
    saved.quality=3; saved.durability=75; saved.maxDurability=80;
    saved.energy=110; saved.maxEnergy=120; saved.tradable=false;
    saved.itemValStaTenths=15; saved.blueprintParts={7,4,6};
    const std::string encoded=top::item_instance::encodeStacks(std::vector<PersistedStack>{saved});
    PersistedStack restored; restored.id=123; restored.count=1;
    std::vector<PersistedStack> target{restored};
    top::item_instance::decodeStacks(encoded.c_str(),target);
    assert(target[0].uid==42 && target[0].extra.str==17 && target[0].quality==3 &&
           target[0].extra.minAttackCoefficient==8 && target[0].extra.maxAttackCoefficient==12 &&
           target[0].durability==75 && target[0].maxDurability==80 && target[0].energy==110 &&
           target[0].maxEnergy==120 && !target[0].tradable);
    assert(target[0].itemValStaTenths==15 && (target[0].blueprintParts==std::array<int,3>{7,4,6}));
    // Y1 tinha 25 campos: os dados de stats/UID continuam legíveis, os campos
    // que nao existiam permanecem nos defaults sem inventar estado atual.
    const char* y1="123,1,42,3,8,9,10,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0";
    std::vector<PersistedStack> oldTarget{restored};
    top::item_instance::decodeStacks(y1,oldTarget);
    assert(oldTarget[0].uid==42 && oldTarget[0].extra.str==17 &&
           oldTarget[0].quality==0 && oldTarget[0].durability==0);
    // O formato anterior com durabilidade tinha 31 campos; novos coeficientes
    // devem permanecer zero ao ler saves feitos antes desta extensão.
    const char* previous="123,1,42,3,8,9,10,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,75,80,110,120,0";
    std::vector<PersistedStack> previousTarget{restored};
    top::item_instance::decodeStacks(previous,previousTarget);
    assert(previousTarget[0].uid==42 && previousTarget[0].extra.str==17 &&
           previousTarget[0].extra.minAttackCoefficient==0 &&
           previousTarget[0].extra.maxAttackCoefficient==0 &&
           previousTarget[0].quality==3 && previousTarget[0].durability==75 &&
           previousTarget[0].maxDurability==80 && previousTarget[0].energy==110 &&
           previousTarget[0].maxEnergy==120 && !previousTarget[0].tradable);
}
