#include "top/ItemUseRules.h"
#include <algorithm>

namespace top::item_rules {
bool isEquipment(int legacySlot, const std::vector<int>& equipSlots, int minAtk, int maxAtk, int defense) {
    return legacySlot >= 0 || !equipSlots.empty() || minAtk > 0 || maxAtk > 0 || defense > 0;
}
bool canEquipSlot(int legacySlot, const std::vector<int>& equipSlots, int requestedSlot) {
    if (requestedSlot < 0 || requestedSlot > 30) return false;
    if (!equipSlots.empty())
        return std::find(equipSlots.begin(), equipSlots.end(), requestedSlot) != equipSlots.end();
    return legacySlot == requestedSlot;
}
bool hasUnsupportedScriptAction(const std::string& callback) {
    return !callback.empty() && callback != "0" && callback != "ItemMedKit" &&
           callback != "InventorySlot" && callback != "AddPlayerSkill";
}
} // namespace top::item_rules
