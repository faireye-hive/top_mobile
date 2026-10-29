#include "../HudDrawList.h"
#include "GameHUD.h"
#include "../PanelChrome.h"
#include <algorithm>
#include "QuestUI.h"
#include "CharacterStatusUI.h"
#include "SkillsUI.h"
#include "InventoryUI.h"
#include "ShortcutBarUI.h"
#include "MinimapUI.h"

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





void GameHUD::drawStatus(UIRenderer& r,float x,float y,int mx,int my,const UITheme& t) { CharacterStatusUI::draw(r,x,y,mx,my,t); }
void GameHUD::drawQuests(UIRenderer& r, float x, float y, const UITheme& t) { QuestUI::draw(r,x,y,t); }
void GameHUD::drawSkills(UIRenderer& r,float x,float y,int page,const std::vector<SkillView>& skills,const UITheme& t,bool testMode) {
    SkillsUI::draw(r,x,y,page,skills,t,testMode);
}
void GameHUD::drawInventory(UIRenderer& r,float x,float y,const UITheme& t,int page,int capacity) { InventoryUI::draw(r,x,y,t,page,capacity); }
void GameHUD::drawFunctionBar(UIRenderer& r,float x,float y,int mx,int my,bool down,const UITheme& t) { ShortcutBarUI::drawFunctionBar(r,x,y,mx,my,down,t); }
void GameHUD::drawSkillBar(UIRenderer& r,int sw,int sh,const UITheme& t) { ShortcutBarUI::drawSkillBar(r,sw,sh,t); }
} // namespace top::ui

namespace top::ui {
void GameHUD::appendBaseSprites(HudDrawList& out, float radarX, float radarY, bool miniReady) {
    MinimapUI::appendSprites(out, static_cast<int>(radarX + 144.f), miniReady);
}
void GameHUD::drawBase(UIRenderer& r,float radarX,float radarY,float hp,float sp,float exp,bool miniReady) {
    r.rect(4.f,3.f,189.f,53.f,{0.07f,0.15f,0.21f,0.90f});
    r.border(4.f,3.f,189.f,53.f,2.f,chrome::edge);
    r.rect(9.f,8.f,55.f,43.f,chrome::shade);
    const Color tint[]={ {0.72f,0.17f,0.16f,0.98f},{0.22f,0.48f,0.72f,0.98f},{0.26f,0.65f,0.38f,0.98f} };
    const float frac[]={hp,sp,exp};
    for(int i=0;i<3;++i) {
        const float y=25.f+i*9.f;
        r.rect(74.f,y,111.f,8.f,{0.02f,0.06f,0.10f,0.95f});
        r.rect(74.f,y,111.f*std::clamp(frac[i],0.f,1.f),8.f,tint[i]);
    }
    MinimapUI::draw(r, static_cast<int>(radarX + 144.f), miniReady);
}
} // namespace top::ui
