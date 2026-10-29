#pragma once
#include "../UIRenderer.h"
namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
class CharacterStatusUI { public:
    static void appendText(HudDrawList& out, const UIContentState& content, const WindowState& windows);
    static void draw(UIRenderer& r,float x,float y,int mx,int my,const UITheme& t = {});
};
}
