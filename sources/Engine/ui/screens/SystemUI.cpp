#include "../HudDrawList.h"
#include "SystemUI.h"
#include "../PanelChrome.h"
namespace top::ui {
int SystemUI::hit(int x, int y, int w, int h) const {
    if (!visible_) return 0;
    const int left = (w - 190) / 2;
    const int top = (h - 309) / 2;
    if (x >= left + 169 && x < left + 183 && y >= top + 3 && y < top + 17) return -1;
    if (x < left + 35 || x >= left + 156) return 0;
    for (int i = 0; i < 6; ++i) {
        const int y0 = top + 61 + i * 31;
        if (y >= y0 && y < y0 + 26) return i + 1;
    }
    return 0;
}
} // namespace top::ui

namespace top::ui {

void SystemUI::draw(UIRenderer& r,int width,int height) const {
    if (!visible()) return;
    const auto b = modal_layout::system(width,height);
    chrome::panel(r,b.x,b.y,b.w,b.h);
    chrome::closeButton(r,b.x+169.f,b.y+3.f,14.f,14.f);
    for(int i=0;i<6;++i) chrome::button(r,b.x+35.f,b.y+61.f+i*31.f,121.f,26.f);
}
void SystemUI::appendSprites(HudDrawList& out, int width, int height) const {
    // Fundos da interface migrados para draw() (C++ puro).
    (void)out; (void)width; (void)height;
}
} // namespace top::ui

namespace top::ui {
void SystemUI::appendText(HudDrawList& out, int width, int height) const {
    if (!visible()) return;
    const auto b=modal_layout::system(width,height);
    out.addText("Sistema (offline)",b.x+22.f,b.y+28.f,.08f,.16f,.28f);
    const char* names[]={"Video / Camera","Audio / Volume","Jogo / Visao",
                         "Trocar personagem","Selecionar perfil","Sair"};
    for(int i=0;i<6;++i) out.addText(names[i],b.x+40.f,b.y+66.f+i*31.f,.08f,.16f,.28f);
}
} // namespace top::ui
