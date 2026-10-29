#include "../HudDrawList.h"
#include <algorithm>
#include "InventoryUI.h"
#include "../UIRenderer.h"
#include "../PanelChrome.h"
#include "../UIManager.h"
#include "InventoryLayout.h"
#include <cmath>
namespace top::ui {
namespace {
// Light parchment backing keeps the existing dark status/inventory text readable.
constexpr Color paper{0.87f, 0.89f, 0.84f, 0.96f};
constexpr Color stripe{0.71f, 0.79f, 0.77f, 0.96f};
constexpr Color ink{0.23f, 0.36f, 0.43f, 0.98f};
constexpr Color slotFill{0.77f, 0.82f, 0.79f, 0.96f};
void panel(UIRenderer& r, float x, float y, float w, float h, const UITheme& t) {
    r.rect(x, y, w, h, paper);
    r.border(x, y, w, h, 2.f, t.border);
    r.rect(x+2.f, y+2.f, w-4.f, 23.f, stripe);
    r.rect(x+4.f, y+27.f, w-8.f, 1.f, ink);
}
void slot(UIRenderer& r, float x, float y, float w = 32.f, float h = 32.f) {
    r.rect(x,y,w,h,slotFill);
    r.border(x,y,w,h,1.f,ink);
}
}

void InventoryUI::draw(UIRenderer& r,float x,float y,const UITheme& t,int page,int capacity){
    panel(r,x,y,199.f,394.f,t);
    // As mesmas coordenadas usadas em hit/dropHit.
    for (const auto& eq : inventory_layout::kEq)
        slot(r, x + eq.x, y + eq.y, eq.sw, eq.sh);
    r.rect(x+7.f,y+193.f,185.f,2.f,ink);
    for (float ry : inventory_layout::kGridRow)
        for (float cx : inventory_layout::kGridCol) slot(r, x + cx, y + ry);
    r.rect(x+7.f,y+355.f,185.f,1.f,ink);
    r.border(x+179.f,y+4.f,14.f,14.f,1.f,ink);
    // Scroll track and thumb: same hit areas as InventoryUI::hit.
    // At capacity 24: 6 rows, viewport 4, maximum scroll is 2.
    r.rect(x+174.f,y+193.f,15.f,151.f,{0.48f,0.57f,0.57f,0.95f});
    r.border(x+174.f,y+193.f,15.f,151.f,1.f,ink);
    r.rect(x+175.f,y+194.f,13.f,15.f,stripe);    // up
    r.rect(x+175.f,y+328.f,13.f,15.f,stripe);    // down
    r.rect(x+179.f,y+200.f,5.f,2.f,ink);
    r.rect(x+179.f,y+334.f,5.f,2.f,ink);
    const int limit=inventory_layout::bagMaxScroll(capacity);
    const float thumbH=std::max(18.f,118.f*16.f/static_cast<float>(std::max(capacity,16)));
    const float thumbY=210.f + (limit>0 ? static_cast<float>(std::clamp(page,0,limit))/limit*(117.f-thumbH) : 0.f);
    r.rect(x+176.f,y+thumbY,11.f,thumbH,{0.24f,0.52f,0.56f,1.f});
    r.border(x+176.f,y+thumbY,11.f,thumbH,1.f,ink);
}
}

// 04-D: interacoes da bolsa agora pertencem a InventoryUI.
// Valores de hit mantidos iguais aos consumidos pelo main.cpp / Play.cpp.
namespace top::ui {
namespace {
using namespace inventory_layout;
}
void InventoryUI::setView(std::vector<BagView> rows, std::unordered_map<int,int> worn,
                          std::vector<BagView> temp, int gold, int capacity, std::unordered_map<int, int> wornUids, bool bankMode) {
    rows_ = std::move(rows);
    worn_ = std::move(worn);
    wornUids_ = std::move(wornUids);
    temp_ = std::move(temp);
    gold_ = gold;
    bankMode_ = bankMode;
    capacity_ = (capacity>=24 && capacity<=48 && capacity%4==0) ? capacity : 24;
    loose_.clear();
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
        const BagView& row = rows_[static_cast<std::size_t>(i)];
        // Itens sem moldura (chapeu, asa, capa, joias) permanecem na grade.
        if (!row.worn || !formSlot(row.equip)) loose_.push_back(i);
    }
    page_ = std::clamp(page_, 0, bagMaxScroll(capacity_));
}
void InventoryUI::pageBy(int delta, const WindowState& windows) {
    if (!windows.showInv_ || !delta) return;
    page_ = std::clamp(page_ + (delta > 0 ? 1 : -1), 0, bagMaxScroll(capacity_));
}
bool InventoryUI::wheelHit(int x, int y, int screenW, const WindowState& windows) const {
    if (!windows.showInv_) return false;
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    const PanelRect box = invRectAt(windows.invX_, windows.invY_);
    if (fx >= box.x && fy >= box.y && fx < box.x + box.w && fy < box.y + box.h) return true;
    if (!windows.showTemp_) return false;
    const PanelRect temp = tempRectAt(windows.invX_, windows.invY_, screenW, true);
    return fx >= temp.x && fy >= temp.y && fx < temp.x + temp.w && fy < temp.y + temp.h;
}
int InventoryUI::hit(int x, int y, int screenW, const WindowState& windows) const {
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    if (windows.showTemp_) {
        const PanelRect box = tempRectAt(windows.invX_, windows.invY_, screenW, windows.showInv_);
        const float lx = fx - box.x, ly = fy - box.y;
        if (lx >= 0.f && ly >= 0.f && lx < box.w && ly < box.h) {
            if (lx >= 176.f && ly >= 2.f && lx < 194.f && ly < 18.f) return 5;
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    const float cx = kTempCol[col], cy = kTempRow[row];
                    if (lx < cx || ly < cy || lx >= cx + kCell || ly >= cy + kCell) continue;
                    const int index = row * 4 + col;
                    return index >= 0 && index < static_cast<int>(temp_.size())
                        ? 300 + temp_[static_cast<std::size_t>(index)].index : 0;
                }
            }
            return 0;
        }
    }
    if (!windows.showInv_) return -1;
    const PanelRect box = invRectAt(windows.invX_, windows.invY_);
    const float lx = fx - box.x, ly = fy - box.y;
    if (lx < 0.f || ly < 0.f || lx >= box.w || ly >= box.h) return -1;
    if (lx >= 172.f && ly >= 2.f && lx < 196.f && ly < 18.f) return 1;
    if (lx >= 170.f && ly >= 188.f && lx < 198.f && ly < 352.f) return ly < 270.f ? 3 : 4;
    for (const EqSpot& eq : kEq) {
        if (lx < eq.x || ly < eq.y || lx >= eq.x + eq.sw || ly >= eq.y + eq.sh) continue;
        return worn_.count(eq.slot) ? 100 + eq.slot : 0;
    }
    const int cell = gridCell(lx, ly);
    if (cell == -2) return 0;
    if (cell >= 0) {
        const int n = page_ * 4 + cell;
        if (n >= 0 && n < static_cast<int>(loose_.size())) {
            const int row = loose_[static_cast<std::size_t>(n)];
            if (row >= 0 && row < static_cast<int>(rows_.size()))
                return 200 + rows_[static_cast<std::size_t>(row)].index;
        }
        return 0;
    }
    if (ly < 22.f && lx < 170.f) return 2;
    return 0;
}
int InventoryUI::dropHit(int x, int y, int screenW, const WindowState& windows) const {
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    if (windows.showTemp_) {
        const PanelRect temp = tempRectAt(windows.invX_, windows.invY_, screenW, windows.showInv_);
        if (fx >= temp.x && fy >= temp.y && fx < temp.x + temp.w && fy < temp.y + temp.h) return 350;
    }
    if (!windows.showInv_) return -1;
    const PanelRect box = invRectAt(windows.invX_, windows.invY_);
    const float lx = fx - box.x, ly = fy - box.y;
    if (lx < 0.f || ly < 0.f || lx >= box.w || ly >= box.h) return -1;
    for (const EqSpot& eq : kEq) {
        if (lx >= eq.x && ly >= eq.y && lx < eq.x + eq.sw && ly < eq.y + eq.sh) return 100 + eq.slot;
    }
    if (gridCell(lx, ly) != -1) return 250;
    return 0;
}
void InventoryUI::setItemDrag(int source, int x, int y, bool active) {
    itemDrag_ = active;
    itemDragSource_ = active ? source : -1;
    itemDragX_ = x;
    itemDragY_ = y;
}
const BagView* InventoryUI::wornRow(int slot) const {
    const auto it = worn_.find(slot);
    if (it == worn_.end()) return nullptr;
    // 05-AB: two identical swords can have different gem levels and extra stats.
    const auto uid = wornUids_.find(slot);
    if (uid != wornUids_.end() && uid->second > 0) {
        for (const BagView& row : rows_)
            if (row.uid == uid->second && row.id == it->second) return &row;
        return nullptr; // do not silently display the wrong item's bonuses
    }
    // Backward compatibility for older UI clients without per-instance UIDs.
    for (const BagView& row : rows_) if (row.id == it->second && row.worn) return &row;
    for (const BagView& row : rows_) if (row.id == it->second) return &row;
    return nullptr;
}
const BagView* InventoryUI::draggedRow() const {
    if (!itemDrag_) return nullptr;
    if (itemDragSource_ >= 100 && itemDragSource_ < 200) return wornRow(itemDragSource_ - 100);
    if (itemDragSource_ >= 200 && itemDragSource_ < 300) {
        for (const BagView& row : rows_) if (row.index == itemDragSource_ - 200) return &row;
    } else if (itemDragSource_ >= 300 && itemDragSource_ < 400) {
        for (const BagView& row : temp_) if (row.index == itemDragSource_ - 300) return &row;
    }
    return nullptr;
}
} // namespace top::ui

namespace top::ui {
namespace {
std::string goldWithComma(int gold) {
    const std::string digits=std::to_string(std::max(0,gold));
    std::string out; for (int i=0;i<int(digits.size());++i) { if (i>0 && (int(digits.size())-i)%3==0) out.push_back(','); out.push_back(digits[std::size_t(i)]); }
    return out;
}
}
void InventoryUI::drawTemp(UIRenderer& r, const WindowState& windows, int screenW) const {
    if (!windows.showTemp_) return;
    const auto b=inventory_layout::tempRectAt(windows.invX_,windows.invY_,screenW,windows.showInv_);
    chrome::panel(r,b.x,b.y,198.f,232.f);
    chrome::closeButton(r,b.x+176.f,b.y+2.f,18.f,16.f);
    for (float ry:inventory_layout::kTempRow)
        for (float cx:inventory_layout::kTempCol) chrome::slot(r,b.x+cx,b.y+ry);
}
void InventoryUI::appendSprites(HudDrawList& out, const WindowState& windows, int screenW, int pointerX, int pointerY, bool down, bool showMain, bool showTemporary, bool showDrag) const {
    using namespace inventory_layout;
    if (windows.showInv_ && showMain) {
            const PanelRect box = invRectAt(windows.invX_, windows.invY_);
            for (const EqSpot& eq : kEq) {
                const BagView* row = wornRow(eq.slot);
                if (row && !row->icon.empty()) {
                    const float pad = (eq.sw - kIcon) * 0.5f;
                    out.icons.push_back({row->icon, box.x + eq.x + pad, box.y + eq.y + (eq.sh - kIcon) * 0.5f, kIcon, kIcon});
                }
            }
            for (int cell = 0; cell < 16; ++cell) {
                const int n = page() * 4 + cell;
                if (n < 0 || n >= static_cast<int>(looseIndices().size())) continue;
                const int rowIndex = looseIndices()[static_cast<std::size_t>(n)];
                if (rowIndex < 0 || rowIndex >= static_cast<int>(rows().size())) continue;
                const BagView& row = rows()[static_cast<std::size_t>(rowIndex)];
                if (row.icon.empty()) continue;
                const int col = cell % 4;
                const int line = cell / 4;
                out.icons.push_back({row.icon, box.x + kGridCol[col], box.y + kGridRow[line], kIcon, kIcon});
            }
    
    }
    if (windows.showTemp_ && showTemporary) {
            const PanelRect box = tempRectAt(windows.invX_, windows.invY_, screenW, windows.showInv_);
            out.addText(bankMode_ ? "Banco" : "Bolsa temporaria", box.x + 8.f, box.y + 6.f, 0.12f, 0.28f, 0.32f);
            for (int i = 0; i < static_cast<int>(tempRows().size()) && i < 16; ++i) {
                const BagView& row = tempRows()[static_cast<std::size_t>(i)];
                if (row.icon.empty()) continue;
                const int col = i % 4;
                const int line = i / 4;
                out.icons.push_back({row.icon, box.x + kTempCol[col], box.y + kTempRow[line], kIcon, kIcon});
            }
            (void)pointerX; (void)pointerY; (void)down;
    
    }
    if (showDrag && itemDragActive()) {
            const BagView* dragged = draggedRow();
            if (dragged && !dragged->icon.empty())
                out.icons.push_back({dragged->icon, static_cast<float>(dragX() - 16),
                                     static_cast<float>(dragY() - 16), 32.f, 32.f});
    
    }
}
void InventoryUI::appendText(HudDrawList& out, const WindowState& windows, int screenW, int pointerX, int pointerY, bool showMain, bool showTemporary) const {
    using namespace inventory_layout;
    if (windows.showInv_ && showMain) {
            const PanelRect box = invRectAt(windows.invX_, windows.invY_);
            out.addText(goldWithComma(gold()), box.x + 56.f, box.y + 362.f, 0.08f, 0.14f, 0.28f);
            out.addText(std::to_string(rows().size())+"/"+std::to_string(capacity()),
                        box.x+110.f,box.y+381.f,0.08f,0.14f,0.28f);
            std::string hover;
            const float hx = static_cast<float>(pointerX) - box.x;
            const float hy = static_cast<float>(pointerY) - box.y;
            for (const EqSpot& eq : kEq) {
                const BagView* row = wornRow(eq.slot);
                if (!row) continue;
                const float pad = (eq.sw - kIcon) * 0.5f;
                out.addBadge(row->count, box.x + eq.x + pad, box.y + eq.y + (eq.sh - kIcon) * 0.5f);
                if (hx >= eq.x && hy >= eq.y && hx < eq.x + eq.sw && hy < eq.y + eq.sh) hover = row->name;
            }
            for (int cell = 0; cell < 16; ++cell) {
                const int n = page() * 4 + cell;
                if (n < 0 || n >= static_cast<int>(looseIndices().size())) continue;
                const int rowIndex = looseIndices()[static_cast<std::size_t>(n)];
                if (rowIndex < 0 || rowIndex >= static_cast<int>(rows().size())) continue;
                const BagView& row = rows()[static_cast<std::size_t>(rowIndex)];
                const int col = cell % 4;
                const int gridRow = cell / 4;
                const float cx = kGridCol[col];
                const float cy = kGridRow[gridRow];
                out.addBadge(row.count, box.x + cx, box.y + cy);
                if (hx >= cx && hy >= cy && hx < cx + kCell && hy < cy + kCell) hover = row.name;
            }
            if (!hover.empty()) out.addText(hover.substr(0, 22), box.x + 12.f, box.y + 6.f, 0.05f, 0.1f, 0.2f);
    
    }
    if (windows.showTemp_ && showTemporary) {
            const PanelRect box = tempRectAt(windows.invX_, windows.invY_, screenW, windows.showInv_);
            const float hx = static_cast<float>(pointerX) - box.x;
            const float hy = static_cast<float>(pointerY) - box.y;
            std::string hover;
            for (int i = 0; i < static_cast<int>(tempRows().size()) && i < 16; ++i) {
                const BagView& row = tempRows()[static_cast<std::size_t>(i)];
                const int col = i % 4;
                const int gridRow = i / 4;
                const float cx = kTempCol[col];
                const float cy = kTempRow[gridRow];
                out.addBadge(row.count, box.x + cx, box.y + cy);
                if (hx >= cx && hy >= cy && hx < cx + kCell && hy < cy + kCell) hover = row.name;
            }
            if (!hover.empty()) out.addText(hover.substr(0, 18), box.x + 8.f, box.y + 6.f, 0.05f, 0.1f, 0.2f);
    
    }
}

// Reuses the same hit testing as clicking/dragging: no parallel pixel geometry.
const BagView* InventoryUI::hoveredRow(int x, int y, int screenW, const WindowState& windows) const {
    const int target = hit(x, y, screenW, windows);
    if (target >= 100 && target < 200) return wornRow(target - 100);
    if (target >= 200 && target < 300) {
        for (const BagView& row : rows_) if (row.index == target - 200) return &row;
    }
    if (target >= 300 && target < 400) {
        for (const BagView& row : temp_) if (row.index == target - 300) return &row;
    }
    return nullptr;
}

void InventoryUI::appendTooltip(UIRenderer& shapes, HudDrawList& out, const WindowState& windows,
                                int screenW, int screenH, int pointerX, int pointerY, float fontLine) const {
    if (itemDrag_ || screenW < 160 || screenH < 120) return;
    const BagView* row = hoveredRow(pointerX, pointerY, screenW, windows);
    if (!row) return;
    const float w = std::min(338.f, static_cast<float>(screenW - 12));
    const float lineH = std::max(15.f, std::min(21.f, fontLine + 2.f));
    const int maxLines = std::max(1, static_cast<int>((screenH - 25.f) / lineH) - 2);
    const auto& lines = row->tooltip;
    const int count = std::min(maxLines, static_cast<int>(lines.empty() ? 1 : lines.size()));
    if (count <= 0) return;
    const float h = 18.f + static_cast<float>(count) * lineH + 10.f;
    float x = static_cast<float>(pointerX) + 19.f;
    if (x + w > screenW - 6.f) x = static_cast<float>(pointerX) - w - 19.f;
    x = std::clamp(x, 6.f, std::max(6.f, screenW - w - 6.f));
    float y = static_cast<float>(pointerY) + 12.f;
    if (y + h > screenH - 6.f) y = static_cast<float>(pointerY) - h - 12.f;
    y = std::clamp(y, 6.f, std::max(6.f, screenH - h - 6.f));
    shapes.rect(x, y, w, h, {0.065f,0.081f,0.093f,0.97f});
    shapes.border(x, y, w, h, 2.f, {0.64f,0.57f,0.38f,1.f});
    shapes.rect(x + 3.f, y + 3.f, w - 6.f, 1.f, {0.30f,0.32f,0.34f,1.f});
    for (int i = 0; i < count; ++i) {
        const TooltipLine line = (lines.empty() ? TooltipLine{row->name,TooltipLine::Kind::Title} :
            (i == count-1 && count < static_cast<int>(lines.size()) ?
             TooltipLine{"... more attributes",TooltipLine::Kind::Muted} : lines[static_cast<std::size_t>(i)]));
        float r = 0.87f, g = 0.90f, b = 0.91f;
        switch(line.kind) {
        case TooltipLine::Kind::Title: r=1.f;g=0.79f;b=0.35f;break;
        case TooltipLine::Kind::Muted: r=0.63f;g=0.70f;b=0.73f;break;
        case TooltipLine::Kind::Bonus: r=0.45f;g=0.87f;b=0.98f;break;
        case TooltipLine::Kind::Gem: r=0.56f;g=0.98f;b=0.62f;break;
        case TooltipLine::Kind::Warning: r=1.f;g=0.54f;b=0.48f;break;
        case TooltipLine::Kind::Normal: break;
        }
        // Older font atlas uses one-byte glyphs; all generated lines are ASCII.
        const std::string shown = line.text.size() > 40 ? line.text.substr(0, 37) + "..." : line.text;
        out.addText(shown, x+11.f, y+10.f+static_cast<float>(i)*lineH, r, g, b);
    }
}
} // namespace top::ui
