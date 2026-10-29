#pragma once
#include "ModalLayout.h"
namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
class BoatUI {
public:
    void draw(class UIRenderer& r, int width, int height, int selectedBoat) const;
    void appendSprites(HudDrawList& out, int width, int height) const;
    void appendText(HudDrawList& out, int width, int height, int selectedBoat) const;
    void toggle() { visible_ = !visible_; }
    void close() { visible_ = false; }
    bool visible() const { return visible_; }
    // -1 fora/sem selecao; 0 fechar; 1..17 casco selecionado.
    int hit(int x, int y, int w, int h) const;
private:
    bool visible_ = false;
};
} // namespace top::ui
