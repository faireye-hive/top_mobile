#pragma once

// Geometria compartilhada pelos eventos e pelo HUD legado (etapa 04-F
// movera tambem os sprites/textos para os paineis). Pixels logicos de tela.
namespace top::ui::modal_layout {
struct Rect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};
inline Rect npc(int w, int h) { return {(w - 320.f) * .5f, float(h) - 316.f, 320.f, 304.f}; }
inline Rect shop(int w) { return {float(w) - 354.f, 64.f, 198.f, 394.f}; }
inline Rect boat(int w, int h) { return {(w - 382.f) * .5f, (h - 374.f) * .5f, 382.f, 374.f}; }
inline Rect system(int w, int h) { return {(w - 190.f) * .5f, (h - 309.f) * .5f, 190.f, 309.f}; }
inline Rect character(int w, int h) { return {(w - 720.f) * .5f, (h - 400.f) * .5f, 720.f, 400.f}; }
} // namespace top::ui::modal_layout
