#include "MinimapUI.h"
#include "../PanelChrome.h"
#include <cstdio>

namespace top::ui {
MinimapBounds MinimapUI::bounds(int width) {
    return {static_cast<float>(width) - 144.f, 15.f, 140.f, 172.f};
}
bool MinimapUI::contains(int x, int y, int width) {
    const auto b = bounds(width);
    return static_cast<float>(x) >= b.x && static_cast<float>(x) < b.x + b.w &&
           static_cast<float>(y) >= b.y && static_cast<float>(y) < b.y + b.h;
}
void MinimapUI::draw(UIRenderer& r, int width, bool hasRadar) {
    const auto b = bounds(width);
    if (!hasRadar) r.rect(b.x + 6.f, b.y + 8.f, 128.f, 128.f,
                          {0.10f, 0.32f, 0.51f, 1.f});
    r.border(b.x, b.y, 140.f, 155.f, 3.f, chrome::edge);
    r.rect(b.x, b.y, 140.f, 7.f, chrome::header);
    r.rect(b.x, b.y + 136.f, 140.f, 19.f, chrome::header);
}
void MinimapUI::appendSprites(HudDrawList& out, int width, bool hasRadar) {
    if (!hasRadar) return;
    const auto b = bounds(width);
    out.sprites.push_back({"minimap", b.x + 6.f, b.y + 8.f,
                           128.f, 128.f, 0.f, 0.f, 1.f, 1.f, 1, 1});
}
void MinimapUI::appendText(HudDrawList& out, int width, float x, float y,
                            const char* mapLabel) {
    const auto b = bounds(width);
    out.addText(mapLabel ? mapLabel : "", b.x + 4.f, b.y + 136.f, 1.f, 1.f, 1.f);
    char buf[48];
    std::snprintf(buf, sizeof buf, "%d, %d", static_cast<int>(x), static_cast<int>(y));
    out.addText(buf, b.x + 4.f, b.y + 156.f, 1.f, 1.f, 1.f);
}
}
