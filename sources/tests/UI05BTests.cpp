#include "top/InventoryStackRules.h"
#include "../Engine/ui/screens/NpcDialogUI.h"
#include "../Engine/ui/HudDrawList.h"
#include "../Engine/ui/UIRenderer.h"
#include "../Engine/ui/screens/InventoryLayout.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

struct Stack { int id, count; };
int main() {
    using top::inventory_rules::insert;
    std::vector<Stack> items;
    assert(insert(items,1,2,1,24));
    assert(items.size()==2 && items[0].count==1 && items[1].count==1);
    assert(insert(items,55,120,99,24));
    assert(items.size()==4 && items[2].count==99 && items[3].count==21);
    std::vector<Stack> noSpace(24,{44,1});
    assert(!insert(noSpace,1,2,1,24) && noSpace.size()==24);
    for (const auto& item: noSpace) assert(item.id==44 && item.count==1);
    assert(top::ui::inventory_layout::formSlot(0)); // hat/helmet
    assert(top::ui::inventory_layout::formSlot(9) && top::ui::inventory_layout::formSlot(6));

    top::ui::NpcDialogUI dialog;
    constexpr int w=1280,h=720;
    dialog.setWindow(true,"Goldie","What would you like?",false,true);
    assert(dialog.visible() && dialog.hasTrade() && !dialog.hasChoices());
    top::ui::UIRenderer painter(w,h);
    dialog.draw(painter,w,h);
    assert(!painter.empty());
    top::ui::HudDrawList draw;
    dialog.appendText(draw,w,h);
    bool hasTrade=false, hasClose=false;
    for(const auto& line:draw.labels) {
        if(line.value=="Trade") hasTrade=true;
        if(line.value=="Close") hasClose=true;
    }
    assert(hasTrade && hasClose);
    dialog.setWindow(true,"Goldie","Quest and store",true,true);
    draw.labels.clear();
    dialog.appendText(draw,w,h);
    bool accept=false, reject=false;
    for(const auto& line:draw.labels) {
        if(line.value=="Aceitar") accept=true;
        if(line.value=="Recusar") reject=true;
    }
    assert(accept && reject);
    dialog.setWindow(false);
    assert(!dialog.visible() && !dialog.hasTrade());
    std::puts("05-B: item stacks, equipment layouts and NPC Trade passed");
}
