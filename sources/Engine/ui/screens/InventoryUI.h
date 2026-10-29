#pragma once
#include "../UITheme.h"
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
class UIRenderer;
struct WindowState;

// Modelo de apresentacao do inventario (nao altera a logica de itens em Play).
struct TooltipLine {
    enum class Kind { Title, Normal, Muted, Bonus, Gem, Warning };
    std::string text;
    Kind kind = Kind::Normal;
};
struct BagView {
    int index = -1;
    int id = 0;
    int count = 0;
    int equip = -1;
    bool worn = false;
    std::string icon;
    std::string name;
    int uid = 0;
    std::vector<TooltipLine> tooltip{};
};

class InventoryUI {
public:
    void drawTemp(class UIRenderer& r, const WindowState& windows, int screenW) const;
    void appendSprites(HudDrawList& out, const WindowState& windows, int screenW, int pointerX, int pointerY, bool down,
                       bool showMain = true, bool showTemporary = true, bool showDrag = true) const;
    void appendText(HudDrawList& out, const WindowState& windows, int screenW, int pointerX, int pointerY,
                    bool showMain = true, bool showTemporary = true) const;
    static void draw(UIRenderer& r, float x, float y, const UITheme& t = {}, int page = 0, int capacity = 24);
    void setView(std::vector<BagView> rows, std::unordered_map<int, int> worn,
                 std::vector<BagView> temp, int gold, int capacity = 24,
                 std::unordered_map<int, int> wornUids = {}, bool bankMode = false);
    void setLines(std::vector<std::string> lines) { lines_ = std::move(lines); }
    void pageBy(int delta, const WindowState& windows);
    bool wheelHit(int x, int y, int screenW, const WindowState& windows) const;
    int hit(int x, int y, int screenW, const WindowState& windows) const;
    int dropHit(int x, int y, int screenW, const WindowState& windows) const;
    void setItemDrag(int source, int x, int y, bool active);
    void cancelItemDrag() { itemDrag_ = false; itemDragSource_ = -1; }

    const std::vector<BagView>& rows() const { return rows_; }
    const std::vector<BagView>& tempRows() const { return temp_; }
    const std::vector<int>& looseIndices() const { return loose_; }
    const std::vector<std::string>& lines() const { return lines_; }
    const BagView* wornRow(int slot) const;
    const BagView* hoveredRow(int x, int y, int screenW, const WindowState& windows) const;
    void appendTooltip(UIRenderer& shapes, HudDrawList& out, const WindowState& windows,
                       int screenW, int screenH, int pointerX, int pointerY, float fontLine) const;
    const BagView* draggedRow() const;
    int page() const { return page_; }
    int capacity() const { return capacity_; }
    int gold() const { return gold_; }
    bool itemDragActive() const { return itemDrag_; }
    int dragX() const { return itemDragX_; }
    int dragY() const { return itemDragY_; }
private:
    std::vector<std::string> lines_;
    std::vector<BagView> rows_, temp_;
    std::unordered_map<int, int> worn_;
    std::unordered_map<int, int> wornUids_;
    std::vector<int> loose_;
    int page_ = 0;
    int capacity_ = 24;
    int gold_ = 0;
    bool bankMode_ = false;
    bool itemDrag_ = false;
    int itemDragSource_ = -1, itemDragX_ = 0, itemDragY_ = 0;
};
} // namespace top::ui
