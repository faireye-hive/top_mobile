#pragma once
#include "../UIRenderer.h"
namespace top::ui {
class ShortcutBarUI { public:
    static void drawFunctionBar(UIRenderer& r,float x,float y,int mx,int my,bool down,const UITheme& t = {});
    static void drawSkillBar(UIRenderer& r,int sw,int sh,const UITheme& t = {});
};
}
