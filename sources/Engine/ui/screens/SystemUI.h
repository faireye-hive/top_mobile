#pragma once
#include "ModalLayout.h"
namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
class SystemUI {
public:
    void draw(class UIRenderer& r, int width, int height) const;
    void appendSprites(HudDrawList& out, int width, int height) const;
    void appendText(HudDrawList& out, int width, int height) const;
    void toggle() { visible_ = !visible_; }
    void close() { visible_ = false; }
    bool visible() const { return visible_; }
    // 0: fora dos botoes; -1 fecha; 1..6: acoes historicas.
    int hit(int x, int y, int w, int h) const;
private:
    bool visible_ = false;
};
} // namespace top::ui
