#include "top/InventoryCapacity.h"
#include "../Engine/ui/UIManager.h"
#include "../Engine/ui/UIRenderer.h"
#include "../Engine/ui/HudDrawList.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace top;
int main() {
    using namespace inventory_rules;
    assert(upgradeTarget(3088)==28 && upgradeTarget(3093)==48);
    assert(!canUpgrade(3093,24) && canUpgrade(3088,24) && canUpgrade(3093,44));
    assert(validatedCapacity(60)==24 && validatedCapacity(32)==32);
    ui::UIManager manager;
    manager.toggle(0);
    // Empty inventory must still expose all 24 slots via scrolling.
    manager.inventory.setView({}, {}, {}, 0, 24);
    assert(manager.inventory.capacity()==24);
    manager.inventory.pageBy(100, manager.windows);
    assert(manager.inventory.page()==1);
    manager.inventory.pageBy(100, manager.windows);
    assert(manager.inventory.page()==2);
    manager.inventory.pageBy(100, manager.windows);
    assert(manager.inventory.page()==2);
    assert(manager.inventory.hit(214+27, 64+213, 1280, manager.windows)==0);
    // 48 slots: scroll by four positions up to offset 32.
    std::vector<ui::BagView> many;
    for(int i=0;i<48;i++) many.push_back({i,100+i,1,-1,false,"icon.png","Item"});
    manager.inventory.setView(many,{}, {},0,48);
    for(int i=0;i<30;i++) manager.inventory.pageBy(1, manager.windows);
    assert(manager.inventory.page()==8);
    assert(manager.inventory.hit(214+27,64+213,1280,manager.windows)==232);
    ui::HudDrawList list;
    manager.inventory.appendSprites(list,manager.windows,1280,0,0,false,true,false,false);
    assert(list.icons.size()==16);
    assert(list.icons[0].x==214+24 && list.icons[0].y==64+208);
    ui::UIRenderer renderer(1280,720);
    ui::InventoryUI::draw(renderer,214,64,{},manager.inventory.page(),manager.inventory.capacity());
    assert(!renderer.empty());
    manager.inventory.setView({}, {}, {}, 0, 24);
    assert(manager.inventory.page()==2); // clamp to last logical row, not item count
    manager.inventory.pageBy(-1,manager.windows);
    assert(manager.inventory.page()==1);
    std::puts("05-C: capacity/scroll/graphics PASS");
}
