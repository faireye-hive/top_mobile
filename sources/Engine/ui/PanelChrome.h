#pragma once
#include "UIRenderer.h"

// 04-G: janelas e botoes feitos integralmente por geometria 2D.
// Coordenadas em pixels logicos; sem arquivos da texture/ui/MUI.
namespace top::ui::chrome {
constexpr Color paper{0.91f, 0.92f, 0.88f, 0.98f};
constexpr Color header{0.68f, 0.79f, 0.79f, 0.98f};
constexpr Color edge{0.23f, 0.37f, 0.43f, 0.98f};
constexpr Color muted{0.62f, 0.73f, 0.73f, 0.95f};
constexpr Color shade{0.82f, 0.86f, 0.82f, 0.96f};
constexpr Color slotFill{0.78f, 0.83f, 0.81f, 0.98f};
constexpr Color selected{0.65f, 0.83f, 0.76f, 0.98f};
inline void panel(UIRenderer& r,float x,float y,float w,float h) {
    r.rect(x+3.f,y+4.f,w,h,{0.02f,0.06f,0.08f,0.35f});
    r.rect(x,y,w,h,paper);
    r.border(x,y,w,h,2.f,edge);
    r.rect(x+2.f,y+2.f,w-4.f,24.f,header);
    r.rect(x+4.f,y+28.f,w-8.f,1.f,muted);
}
inline void closeButton(UIRenderer& r,float x,float y,float w=18.f,float h=16.f) {
    r.rect(x,y,w,h,shade); r.border(x,y,w,h,1.f,edge);
    const float cx=x+w*.5f, cy=y+h*.5f;
    r.rect(cx-4.f,cy-1.f,8.f,2.f,edge);
}
inline void slot(UIRenderer& r,float x,float y,float w=32.f,float h=32.f) {
    r.rect(x,y,w,h,slotFill); r.border(x,y,w,h,1.f,muted);
}
inline void button(UIRenderer& r,float x,float y,float w,float h,bool active=false) {
    r.rect(x,y,w,h,active?selected:shade);r.border(x,y,w,h,1.f,edge);
}
} // namespace top::ui::chrome
