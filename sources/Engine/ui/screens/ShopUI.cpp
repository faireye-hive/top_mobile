#include "../HudDrawList.h"
#include "InventoryLayout.h"
#include "ShopUI.h"
#include "../UIRenderer.h"
#include "../PanelChrome.h"
#include <algorithm>
namespace top::ui {
void ShopUI::setWindow(bool shown, const std::string& name, std::vector<std::string> lines,
                       int first, int total, int gold, std::vector<std::string> icons,
                       std::vector<std::string> names) {
    visible_ = shown;
    if (!shown) dragging_ = false;
    name_ = name;
    lines_ = std::move(lines);
    first_ = first;
    total_ = total;
    gold_ = gold;
    icons_ = std::move(icons);
    names_ = std::move(names);
}
} // namespace top::ui

namespace top::ui {
void ShopUI::beginDrag(int x, int y, int width) {
    if (hit(x,y,width) != -3) return;
    const auto b = bounds(width);
    if (x_ < 0.f) x_ = b.x;
    grabX_ = float(x) - b.x;
    grabY_ = float(y) - b.y;
    dragging_ = true;
}
void ShopUI::drag(int x, int y, bool down, int width, int height) {
    if (!visible_ || !down) { dragging_ = false; return; }
    if (!dragging_) return;
    x_ = std::clamp(float(x) - grabX_, -158.f, float(std::max(width, 1) - 40));
    y_ = std::clamp(float(y) - grabY_, 0.f, float(std::max(height, 1) - 24));
}
int ShopUI::hit(int x, int y, int width) const {
    if (!contains(x,y,width)) return -1;
    const auto box=bounds(width);
    const float lx=float(x)-box.x, ly=float(y)-box.y;
    if (lx>=172.f && lx<196.f && ly>=2.f && ly<18.f) return -2;
    if (ly<24.f) return -3;
    constexpr int cols[4]={23,59,96,133};
    constexpr int rows[8]={62,98,134,170,206,242,278,314};
    for (int row=0; row<8; ++row) for(int col=0; col<4; ++col) {
        if (lx<cols[col] || lx>=cols[col]+32 || ly<rows[row] || ly>=rows[row]+32) continue;
        const int index=first_+row*4+col;
        return index < total_ ? index : -1;
    }
    return -1;
}
void ShopUI::draw(UIRenderer& r, int width) const {
    if (!visible_) return;
    const auto box=bounds(width);
    chrome::panel(r, box.x, box.y, box.w, box.h);
    chrome::closeButton(r, box.x+172.f, box.y+2.f, 24.f, 16.f);
    for(int row=0; row<8; ++row) for(int col=0; col<4; ++col)
        chrome::slot(r,box.x+float(col==0?23:col==1?59:col==2?96:133),box.y+float(62+row*36),32.f,32.f);
    // linha de saldo e rodape
    r.rect(box.x+8.f,box.y+355.f,182.f,1.f,chrome::edge);
}
namespace { constexpr int kShopColX[4] = {23,59,96,133};
constexpr int kShopRowY[8] = {62,98,134,170,206,242,278,314};
constexpr float kCell = 32.f; constexpr float kIcon = 32.f; }

void ShopUI::appendSprites(HudDrawList& out, int width) const {
    if (!visible()) return;
            const float left = bounds(width).x;
            const float top = bounds(width).y;
            for (int i = 0; i < 32 && i < static_cast<int>(icons().size()); ++i) {
                if (icons()[static_cast<std::size_t>(i)].empty()) continue;
                const int col = i % 4;
                const int row = i / 4;
                out.icons.push_back({icons()[static_cast<std::size_t>(i)],
                                     left + static_cast<float>(kShopColX[col]),
                                     top + static_cast<float>(kShopRowY[row]), kIcon, kIcon});
            }
    
}
void ShopUI::appendText(HudDrawList& out, int width, int pointerX, int pointerY) const {
    if (!visible()) return;
            const float left = bounds(width).x;
            const float top = bounds(width).y;
            std::string hover;
            const float hx = static_cast<float>(pointerX) - left;
            const float hy = static_cast<float>(pointerY) - top;
            for (int i = 0; i < 32 && i < static_cast<int>(lines().size()); ++i) {
                const int col = i % 4;
                const int row = i / 4;
                const float cx = static_cast<float>(kShopColX[col]);
                const float cy = static_cast<float>(kShopRowY[row]);
                out.addText(lines()[static_cast<std::size_t>(i)], left + cx, top + cy + 18.f, 0.05f, 0.1f, 0.16f);
                if (hx >= cx && hy >= cy && hx < cx + kCell && hy < cy + kCell) {
                    if (i < static_cast<int>(itemNames().size()) && !itemNames()[static_cast<std::size_t>(i)].empty())
                        hover = itemNames()[static_cast<std::size_t>(i)];
                    else hover = lines()[static_cast<std::size_t>(i)];
                }
            }
            if (!hover.empty()) out.addText(hover.substr(0, 22), left + 8.f, top + 6.f, 0.05f, 0.12f, 0.22f);
            else out.addText(name().substr(0, 18), left + 8.f, top + 6.f, 0.08f, 0.16f, 0.28f);
            out.addText("Gold: " + std::to_string(gold()), left + 12.f, top + 362.f, 0.08f, 0.16f, 0.28f);
    
}
} // namespace top::ui
