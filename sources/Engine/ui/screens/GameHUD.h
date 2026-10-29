#pragma once
#include "../UIRenderer.h"
#include "SkillsUI.h"
namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
class GameHUD {
public:
    static void drawBase(class UIRenderer& r,float radarX,float radarY,float hp,float sp,float exp,bool miniReady);
    static void appendBaseSprites(HudDrawList& out, float radarX, float radarY, bool miniReady);
    static void drawSkillBar(UIRenderer& r, int sw, int sh, const UITheme& theme = {});
    static void drawFunctionBar(UIRenderer& r, float x, float y, int mx, int my, bool down, const UITheme& theme = {});
    static void drawStatus(UIRenderer& r, float x, float y, int mx, int my, const UITheme& theme = {});
    static void drawSkills(UIRenderer& r, float x, float y, int page, const std::vector<SkillView>& skills,
                          const UITheme& theme = {}, bool testMode = false);
    static void drawQuests(UIRenderer& r, float x, float y, const UITheme& theme = {});
    static void drawInventory(UIRenderer& r, float x, float y, const UITheme& theme = {}, int page = 0, int capacity = 24);
};
}
