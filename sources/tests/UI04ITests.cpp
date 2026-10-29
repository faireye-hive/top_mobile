#include "../Engine/ui/screens/MinimapUI.h"
#include <cassert>
#include <string>
int main() {
    using top::ui::MinimapUI;
    auto b = MinimapUI::bounds(1290);
    assert(b.x == 1146.f && b.y == 15.f && b.w == 140.f && b.h == 172.f);
    assert(MinimapUI::contains(1146, 15, 1290));
    assert(MinimapUI::contains(1285, 186, 1290));
    assert(!MinimapUI::contains(1145, 15, 1290));
    assert(!MinimapUI::contains(1286, 186, 1290));
    top::ui::HudDrawList list;
    MinimapUI::appendSprites(list, 1290, false);
    assert(list.sprites.empty());
    MinimapUI::appendSprites(list, 1290, true);
    assert(list.sprites.size() == 1 && list.sprites[0].x == 1152.f);
    MinimapUI::appendText(list, 1290, 2248.5f, 2781.5f, "Argent");
    assert(list.labels.size() == 2);
    assert(list.labels[0].value == "Argent" && list.labels[1].value == "2248, 2781");
    top::ui::UIRenderer r(1290, 744);
    MinimapUI::draw(r, 1290, false);
    assert(!r.empty());
    return 0;
}
