#pragma once

// CCharacter::_UpdateHeight (DX9 Client/src/Character.cpp):
// chTerritory == 1 => sSeaHeight / 100 (water actor / boat / sea monster).
// Other territory types use the scene's ground sample. The offline port
// currently lacks network nHeightOff, so no extra Z offset is injected.
namespace top::historical_character_height {
inline float baseHeight(int territory, int seaHeightCentimeters, float groundHeight) {
    return territory == 1 ? static_cast<float>(seaHeightCentimeters) / 100.f : groundHeight;
}
}
