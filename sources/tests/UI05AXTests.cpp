#include "top/ItemChestRules.h"
#include "top/InventoryStackRules.h"
#include <cassert>
#include <vector>
#include <iostream>
struct Stack { int id=0, count=0, uid=0; };
int main() {
    using namespace top::item_chests;
    const int boxes[]={215,216,217,218,219,220,221};
    const char* callbacks[]={"ItemUse_SCBoxRYBZ","ItemUse_SCBoxXMC","ItemUse_SCBoxLQJ","ItemUse_SCBoxLYJ","ItemUse_SCBoxFHG","ItemUse_SCBoxLXHZ","ItemUse_SCBoxYCJ"};
    const int rewards[]={109,111,113,115,117,119,150};
    for (int i=0;i<7;++i) {
        auto plan=planFor(boxes[i], callbacks[i], 1);
        assert(plan && plan->requiredFreeSlots==1 && plan->rewards.size()==1);
        assert(plan->rewards[0].itemId==rewards[i] && plan->rewards[0].count==1);
        assert(!planFor(boxes[i], callbacks[(i+1)%7], 1));
    }
    auto race1=planFor(192,"ItemUse_SaintCloth",1);
    auto race4=planFor(192,"ItemUse_SaintCloth",4);
    assert(race1 && race1->requiredFreeSlots==4 && race1->rewards.size()==3);
    assert(race4 && race4->requiredFreeSlots==4 && race4->rewards.size()==4);
    assert(race4->rewards[0].itemId==828 && race1->rewards[0].itemId==825);
    assert(!planFor(192,"NewbieChest",4));
    assert(!planFor(436,"NewbieChest",1)); // no auto fabricated rewards
    assert(!planFor(332,"Ticket",1)); // not a chest
    // Simulacao da insercao atomica usada pelo inventario: falha nunca altera o original.
    std::vector<Stack> bag{{215,1,51},{1,1,52}};
    auto preview=bag;
    preview.erase(preview.begin());
    assert(!top::inventory_rules::insert(preview,109,2,1,2));
    assert(bag.size()==2 && bag[0].id==215);
    std::cout<<"05-AX: 8 baus deterministicos mapeados sem Lua arbitario\n";
}
