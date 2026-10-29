#include "../HudDrawList.h"
#include "BoatUI.h"
#include "../PanelChrome.h"
namespace top::ui {
int BoatUI::hit(int x, int y, int w, int h) const {
    if (!visible_) return -1;
    const int left = (w - 382) / 2;
    const int top = (h - 374) / 2;
    if (x >= left + 360 && x < left + 374 && y >= top + 3 && y < top + 17) return 0;
    for (int id = 1; id <= 17; ++id) {
        const int col = (id - 1) / 9;
        const int row = (id - 1) % 9;
        const int x0 = left + 34 + col * 174;
        const int y0 = top + 76 + row * 27;
        if (x >= x0 && x < x0 + 158 && y >= y0 && y < y0 + 23) return id;
    }
    return -1;
}
} // namespace top::ui

namespace top::ui {

void BoatUI::draw(UIRenderer& r,int width,int height,int selectedBoat) const {
    if (!visible()) return;
    const auto b = modal_layout::boat(width,height);
    chrome::panel(r,b.x,b.y,b.w,b.h);
    (void)selectedBoat;
    chrome::closeButton(r,b.x+360.f,b.y+3.f,14.f,14.f);
    for(int id=1;id<=17;++id) {
        const int col=(id-1)/9, row=(id-1)%9;
        chrome::button(r,b.x+34.f+col*174.f,b.y+76.f+row*27.f,158.f,23.f,id==selectedBoat);
    }
}
void BoatUI::appendSprites(HudDrawList& out, int width, int height) const {
    // Fundos da interface migrados para draw() (C++ puro).
    (void)out; (void)width; (void)height;
}
void BoatUI::appendText(HudDrawList& out, int width, int height, int selectedBoat) const {
    if (!visible()) return;
            static const char* names[] = {"", "Guppy", "Transporter I", "Flying Fish", "Turtle", "Windseeker",
                                          "Swordfish", "Low level 7", "Boat 8", "Boat 9", "Boat 10",
                                          "Phantom Galley", "Silver Dolphin", "Great White Shark", "Torrent Galley",
                                          "Goddess", "Eagle", "Ark"};
            const float left = ui::modal_layout::boat(width, height).x;
            const float top = ui::modal_layout::boat(width, height).y;
            out.addText("Estaleiro offline - escolha o casco", left + 70.f, top + 24.f, .06f, .22f, .36f);
            out.addText("N abre/fecha | B embarca na agua", left + 78.f, top + 48.f, .12f, .25f, .36f);
            for (int id = 1; id <= 17; ++id) {
                const int col = (id - 1) / 9;
                const int row = (id - 1) % 9;
                const float x = left + 40.f + static_cast<float>(col * 174);
                const float y = top + 81.f + static_cast<float>(row * 27);
                const bool selected = id == selectedBoat;
                out.addText(std::string(selected ? "> " : "  ") + names[id], x, y,
                     selected ? .05f : .15f, selected ? .38f : .22f, selected ? .16f : .30f);
            }
    
}
} // namespace top::ui
