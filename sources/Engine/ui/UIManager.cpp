#include "UIManager.h"
#include <algorithm>

namespace top::ui {
namespace {
struct Box { float x,y,w,h; };
Box panelBox(const WindowState& s, UIManager::Panel p, int screenWidth) {
    switch (p) {
    case UIManager::Panel::Inventory: return {s.invX_, s.invY_, 199.f, 394.f};
    case UIManager::Panel::Status: return {s.stateX_, s.stateY_, 198.f, 394.f};
    case UIManager::Panel::Skills: return {s.skillX_ < 0.f ? float(screenWidth - 206) : s.skillX_, s.skillY_, 198.f, 394.f};
    case UIManager::Panel::Quests: return {s.questX_, s.questY_, 235.f, 394.f};
    }
    return {};
}
}
void UIManager::toggleSystemModal() {
    system.toggle();
    if (system.visible()) { npc.close(); shop.close(); }
}
void UIManager::resize(int w,int h) { width_=std::max(1,w); height_=std::max(1,h); }
int UIManager::windowHit(Panel p,int x,int y,int w) const {
    const auto& s = windows;
    const bool shown = p==Panel::Inventory ? s.showInv_ : p==Panel::Status ? s.showState_ : p==Panel::Skills ? s.showSkill_ : s.showQuest_;
    if (!shown) return -1;
    const Box b=panelBox(s,p,w);
    const float dx=float(x)-b.x, dy=float(y)-b.y;
    if(dx<0 || dy<0 || dx>=b.w || dy>=b.h) return -1;
    if (dy < 24.f && dx > b.w - 25.f) return 1;
    return dy<24.f ? 2 : 0;
}
void UIManager::beginDrag(Panel p,int x,int y,int w) {
    auto& s=windows;
    if (p==Panel::Skills && windowHit(p,x,y,w)!=2) return;
    if (p==Panel::Quests && windowHit(p,x,y,w)!=2) return;
    const bool shown=p==Panel::Inventory?s.showInv_:p==Panel::Status?s.showState_:p==Panel::Skills?s.showSkill_:s.showQuest_;
    if (!shown) return;
    if(p==Panel::Skills && s.skillX_<0.f) s.skillX_=float(w-206);
    float *px=nullptr,*py=nullptr,*gx=nullptr,*gy=nullptr; bool* dragging=nullptr;
    switch(p){
    case Panel::Inventory: px=&s.invX_;py=&s.invY_;gx=&s.invGrabX_;gy=&s.invGrabY_;dragging=&s.invDrag_;break;
    case Panel::Status: px=&s.stateX_;py=&s.stateY_;gx=&s.stateGrabX_;gy=&s.stateGrabY_;dragging=&s.stateDrag_;break;
    case Panel::Skills: px=&s.skillX_;py=&s.skillY_;gx=&s.skillGrabX_;gy=&s.skillGrabY_;dragging=&s.skillDrag_;break;
    case Panel::Quests: px=&s.questX_;py=&s.questY_;gx=&s.questGrabX_;gy=&s.questGrabY_;dragging=&s.questDrag_;break;
    }
    *dragging=true;*gx=float(x)-*px;*gy=float(y)-*py;
}
void UIManager::drag(Panel p,int x,int y,bool down,int w,int h) {
    auto& s=windows;
    float *px=nullptr,*py=nullptr,*gx=nullptr,*gy=nullptr;bool* dragging=nullptr;bool shown=false;
    switch(p){
    case Panel::Inventory:px=&s.invX_;py=&s.invY_;gx=&s.invGrabX_;gy=&s.invGrabY_;dragging=&s.invDrag_;shown=s.showInv_;break;
    case Panel::Status:px=&s.stateX_;py=&s.stateY_;gx=&s.stateGrabX_;gy=&s.stateGrabY_;dragging=&s.stateDrag_;shown=s.showState_;break;
    case Panel::Skills:px=&s.skillX_;py=&s.skillY_;gx=&s.skillGrabX_;gy=&s.skillGrabY_;dragging=&s.skillDrag_;shown=s.showSkill_;break;
    case Panel::Quests:px=&s.questX_;py=&s.questY_;gx=&s.questGrabX_;gy=&s.questGrabY_;dragging=&s.questDrag_;shown=s.showQuest_;break;
    }
    if(!down || !shown || !*dragging){*dragging=false;return;}
    const float minX=p==Panel::Skills?-158.f:p==Panel::Quests?-195.f:-150.f;
    *px=std::clamp(float(x)-*gx,minX,float(std::max(1,w)-40));
    *py=std::clamp(float(y)-*gy,0.f,float(std::max(1,h)-24));
}
void UIManager::close(Panel p){
    auto& s=windows;
    switch(p){
    case Panel::Inventory:s.showInv_=false;s.invDrag_=false;inventory.cancelItemDrag();break;
    case Panel::Status:s.showState_=false;s.stateDrag_=false;break;
    case Panel::Skills:s.showSkill_=false;s.skillDrag_=false;break;
    case Panel::Quests:s.showQuest_=false;s.questDrag_=false;break;
    }
}
void UIManager::toggle(int which) {
    auto& s=windows;
    switch(which) {
    case 0:s.showInv_=!s.showInv_;s.invDrag_=false;if(!s.showInv_)inventory.cancelItemDrag();break;
    case 1:s.showState_=!s.showState_;s.stateDrag_=false;break;
    case 2:s.showSkill_=!s.showSkill_;s.skillDrag_=false;break;
    case 3:s.showQuest_=!s.showQuest_;s.questDrag_=false;break;
    case 4:s.showTemp_=!s.showTemp_;break;
    }
}
} // namespace top::ui
