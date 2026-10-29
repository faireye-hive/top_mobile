#pragma once
#include <string>
#include <vector>

namespace top::item_rules {
// Politica de itens independente do gameplay, NPC, UI e OpenGL.
// O callback original em iteminfo e somente metadado; nunca executar Lua do acervo.
bool isEquipment(int legacySlot, const std::vector<int>& equipSlots, int minAtk, int maxAtk, int defense);
bool canEquipSlot(int legacySlot, const std::vector<int>& equipSlots, int requestedSlot);
bool hasUnsupportedScriptAction(const std::string& callback);
} // namespace top::item_rules
