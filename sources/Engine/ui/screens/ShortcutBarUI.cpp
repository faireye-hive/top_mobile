#include "ShortcutBarUI.h"
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

void ShortcutBarUI::drawFunctionBar(UIRenderer& r,float x,float y,int mx,int my,bool down,const UITheme& t){
    r.rect(x,y,215.f,30.f,t.panel); r.border(x,y,215.f,30.f,1.f,t.border);
    for(int i=0;i<8;++i){
        const float ox=x+7.f+24.f*i;
        const float w=i==7?37.f:23.f;
        const bool hover=mx>=ox && mx<ox+w && my>=y+4.f && my<y+26.f;
        r.rect(ox,y+4.f,w,22.f,hover?(down?t.slot:stripe):slotFill);
        r.border(ox,y+4.f,w,22.f,1.f,t.slotBorder);
        // Minimal geometric markers. The original hitboxes remain in ActorSkin.
        if(i==0) { r.rect(ox+7.f,y+9.f,9.f,2.f,ink); r.rect(ox+7.f,y+14.f,9.f,2.f,ink); }
        if(i==1) { r.border(ox+7.f,y+9.f,10.f,11.f,1.f,ink); }
        if(i==2) { r.rect(ox+10.f,y+8.f,3.f,12.f,ink); r.rect(ox+7.f,y+13.f,9.f,2.f,ink); }
        if(i==3) { r.border(ox+7.f,y+9.f,10.f,11.f,1.f,ink); r.rect(ox+10.f,y+12.f,4.f,2.f,ink); }
    }
}
void ShortcutBarUI::drawSkillBar(UIRenderer& r, int sw, int sh, const UITheme& t) {
    constexpr float w=295.f, h=44.f;
    const float x=std::floor((static_cast<float>(sw)-w)*.5f);
    const float y=static_cast<float>(sh)-h-6.f;
    r.rect(x,y,w,h,t.panel); r.border(x,y,w,h,2.f,t.border);
    r.rect(x+2.f,y+2.f,w-4.f,2.f,t.shine);
    for(int i=0;i<8;++i) {
        float sx=x+5.f+static_cast<float>(i)*34.f;
        r.rect(sx,y+6.f,32.f,32.f,t.slot);
        r.border(sx,y+6.f,32.f,32.f,1.f,t.slotBorder);
    }
}
}
