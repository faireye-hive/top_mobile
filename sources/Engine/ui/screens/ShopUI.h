#pragma once
#include "ModalLayout.h"
#include <string>
#include <utility>
#include <vector>

namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
class ShopUI {
public:
    void appendSprites(HudDrawList& out, int width) const;
    void draw(class UIRenderer& r, int width) const;
    void appendText(HudDrawList& out, int width, int pointerX, int pointerY) const;
    void setWindow(bool shown, const std::string& name = {}, std::vector<std::string> lines = {},
                   int first = 0, int total = 0, int gold = 0,
                   std::vector<std::string> icons = {}, std::vector<std::string> names = {});
    void close() { visible_ = false; dragging_ = false; }
    bool visible() const { return visible_; }
    modal_layout::Rect bounds(int width) const {
        return {x_ < 0.f ? modal_layout::shop(width).x : x_, y_, 198.f, 394.f};
    }
    // -3 = barra de titulo, -2 = fechar, -1 = nada, >=0 = indice real do item.
    int hit(int x, int y, int width) const;
    bool contains(int x, int y, int width) const {
        return visible_ && bounds(width).contains(float(x), float(y));
    }
    void beginDrag(int x, int y, int width);
    void drag(int x, int y, bool down, int width, int height);
    bool dragging() const { return dragging_; }
    const std::string& name() const { return name_; }
    const std::vector<std::string>& lines() const { return lines_; }
    const std::vector<std::string>& icons() const { return icons_; }
    const std::vector<std::string>& itemNames() const { return names_; }
    int first() const { return first_; }
    int total() const { return total_; }
    int gold() const { return gold_; }
private:
    bool visible_ = false;
    std::string name_;
    std::vector<std::string> lines_, icons_, names_;
    int first_ = 0, total_ = 0, gold_ = 0;
    float x_ = -1.f, y_ = 64.f, grabX_ = 0.f, grabY_ = 0.f;
    bool dragging_ = false;
};
} // namespace top::ui
