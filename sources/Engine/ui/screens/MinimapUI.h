#pragma once
#include "../HudDrawList.h"
#include "../UIRenderer.h"

namespace top::ui {
// 04-I: geometry, drawing and input area of the radar widget.
// The historical .pk decoder and live radar pixels stay in the map subsystem.
struct MinimapBounds { float x, y, w, h; };
class MinimapUI {
public:
    static MinimapBounds bounds(int screenWidth);
    static bool contains(int x, int y, int screenWidth);
    static void draw(UIRenderer& r, int screenWidth, bool hasRadar);
    static void appendSprites(HudDrawList& out, int screenWidth, bool hasRadar);
    static void appendText(HudDrawList& out, int screenWidth,
                           float playerX, float playerY, const char* mapLabel);
};
}
