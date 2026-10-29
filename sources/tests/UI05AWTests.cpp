#include "top/ItemUseRules.h"
#include "top/JobNames.h"
#include <cassert>
#include <vector>
#include <string>
#include <iostream>
int main() {
    using namespace top::item_rules;
    const std::vector<int> dual{9,6};
    assert(isEquipment(9, dual, 0,0,0));
    assert(canEquipSlot(9, dual, 9));
    assert(canEquipSlot(9, dual, 6)); // segunda mao historica
    assert(!canEquipSlot(9, dual, 2));
    assert(canEquipSlot(2, {}, 2));
    assert(!canEquipSlot(-1, {}, 6));
    assert(!isEquipment(-1, {}, 0,0,0)); // quest items are not gear
    assert(!hasUnsupportedScriptAction("0"));
    assert(!hasUnsupportedScriptAction("ItemMedKit"));
    assert(!hasUnsupportedScriptAction("InventorySlot"));
    assert(!hasUnsupportedScriptAction("AddPlayerSkill"));
    assert(hasUnsupportedScriptAction("ItemUse_SaintCloth")); // not yet executed!
    assert(hasUnsupportedScriptAction("GiveRandomItems"));
    assert(std::string(top::jobName(2)) == "Hunter");
    std::cout<<"05-AW: politica pura de uso de itens e multi-slot OK\n";
}
