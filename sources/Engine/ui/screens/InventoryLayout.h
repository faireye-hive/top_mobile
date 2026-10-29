#pragma once

#include <algorithm>

// Geometria historica de Inventary_my / bolsa temporaria.
// Compartilhada pelo hit-test e pelo desenho para nao deslocar os slots.
namespace top::ui::inventory_layout {
struct PanelRect { float x, y, w, h; };
inline PanelRect invRectAt(float x, float y) { return {x, y, 199.f, 394.f}; }
inline PanelRect tempRectAt(float invX, float invY, int screenW, bool invOpen) {
    float x = invOpen ? invX + 206.f : invX;
    if (x + 198.f > static_cast<float>(screenW) - 2.f)
        x = invOpen ? invX - 206.f : static_cast<float>(std::max(screenW, 200) - 200);
    if (x < 0.f) x = 0.f;
    return {x, invY, 198.f, 232.f};
}
struct EqSpot { int slot; float x, y, sw, sh; };
inline constexpr EqSpot kEq[] = {
    {5, 19.f, 29.f, 30.f, 30.f}, {3, 148.f, 29.f, 30.f, 30.f},
    {0, 82.f, 44.f, 30.f, 30.f}, {9, 40.f, 85.f, 30.f, 30.f},
    {2, 82.f, 85.f, 30.f, 30.f}, {6, 125.f, 85.f, 30.f, 30.f},
    {4, 82.f, 127.f, 30.f, 30.f}, {7, 18.f, 142.f, 30.f, 30.f},
    {8, 147.f, 143.f, 30.f, 30.f}
};
inline constexpr float kGridCol[4] = {24.f, 60.f, 96.f, 132.f};
inline constexpr float kGridRow[4] = {208.f, 244.f, 280.f, 316.f};
inline constexpr float kTempCol[4] = {24.f, 63.f, 102.f, 141.f};
inline constexpr float kTempRow[4] = {58.f, 93.f, 128.f, 163.f};
inline constexpr float kCell = 32.f;
inline constexpr float kIcon = 32.f;
inline bool formSlot(int slot) {
    for (const EqSpot& eq : kEq) if (eq.slot == slot) return true;
    return false;
}
// 16 visible cells, 4 columns: scroll by complete rows. 24/48 slots are
// logical capacity, not the number of currently occupied items.
inline int bagMaxScroll(int capacity) {
    return std::max(0, (std::clamp(capacity, 24, 48) + 3) / 4 - 4);
}
// -1 fora da grade, -2 no espaco entre as celulas, >=0 indice visual.
inline int gridCell(float lx, float ly) {
    if (lx < kGridCol[0] || ly < kGridRow[0] ||
        lx >= kGridCol[3] + kCell || ly >= kGridRow[3] + kCell) return -1;
    int col = -1, row = -1;
    for (int i = 0; i < 4; ++i) {
        if (lx >= kGridCol[i] && lx < kGridCol[i] + kCell) col = i;
        if (ly >= kGridRow[i] && ly < kGridRow[i] + kCell) row = i;
    }
    return col < 0 || row < 0 ? -2 : row * 4 + col;
}
} // namespace top::ui::inventory_layout
