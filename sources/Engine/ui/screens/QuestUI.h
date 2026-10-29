#include <string>
#include <vector>
#pragma once
#include "../UIRenderer.h"
namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
class QuestUI {
public:
    static void appendText(HudDrawList& out, const std::vector<std::string>& questLines, float x, float y);
    static void draw(UIRenderer& r, float x, float y, const UITheme& theme = {}, bool canAbandon = false);
};
}
