#include "top/ItemTicketRules.h"
#include "top/ItemChestRules.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace top;
int main() {
    using item_tickets::destinationFor;
    // Conferidos em variable.lua + birth_conf.lua do GameServer historico.
    auto argent=destinationFor(4602,"Ticket");
    auto thundoria=destinationFor(3048,"Ticket");
    auto thundoria2=destinationFor(3828,"Ticket");
    auto spring=destinationFor(332,"Ticket");
    assert(argent && argent->map==std::string("garner") && argent->x==2232 && argent->y==2787);
    assert(thundoria && thundoria2 && thundoria->x==thundoria2->x && thundoria->y==1592);
    assert(spring && spring->map==std::string("darkblue") && spring->x==3323);
    assert(!destinationFor(3141,"Ticket")); // Historico tem nome de destino vazio.
    assert(!destinationFor(4602,"ItemUse_SCBoxYXTZ"));
    assert(!destinationFor(445,"Ticket"));
    item_tickets::Conditions c{10,51,100,50,100,false,"garner"};
    assert(item_tickets::check(c)==item_tickets::Rejection::None);
    c.sp=49; assert(item_tickets::check(c)==item_tickets::Rejection::Health);
    c.sp=50;c.hp=49; assert(item_tickets::check(c)==item_tickets::Rejection::Health);
    c.hp=50;c.boat=true;assert(item_tickets::check(c)==item_tickets::Rejection::Boat);
    c.boat=false;c.currentMap="garner2";assert(item_tickets::check(c)==item_tickets::Rejection::ProhibitedMap);
    c.currentMap="prisonisland";assert(item_tickets::check(c)==item_tickets::Rejection::ProhibitedMap);
    // Seis baus com recompensa por tipo, incluindo condicoes historicas.
    struct Case {int id,type,slots,first,count;const char* callback;};
    const Case cases[]={
        {209,1,3,395,3,"ItemUse_SCBoxYXTZ"},{210,2,4,397,3,"ItemUse_SCBoxLSTZ"},
        {211,3,3,399,3,"ItemUse_SCBoxHYTZ"},{212,3,4,401,3,"ItemUse_SCBoxFYSTZ"},
        {212,4,4,403,4,"ItemUse_SCBoxFYSTZ"},{213,3,4,405,3,"ItemUse_SCBoxSZZTZ"},
        {213,4,4,407,4,"ItemUse_SCBoxSZZTZ"},{214,1,4,409,3,"ItemUse_SCBoxHHSTZ"},
        {214,4,4,412,4,"ItemUse_SCBoxHHSTZ"},
    };
    for(const auto& c:cases){
        const auto plan=item_chests::planFor(c.id,c.callback,c.type);
        assert(plan && plan->requiredFreeSlots==c.slots &&
               plan->rewards.size()==static_cast<size_t>(c.count) &&
               plan->rewards.front().itemId==c.first);
    }
    assert(item_chests::characterRestrictedBox(209,"ItemUse_SCBoxYXTZ"));
    assert(!item_chests::characterRestrictedBox(209,"Ticket"));
    assert(!item_chests::planFor(209,"ItemUse_SCBoxYXTZ",2));
    assert(!item_chests::planFor(210,"ItemUse_SCBoxLSTZ",4));
    assert(!item_chests::planFor(214,"ItemUse_SCBoxHHSTZ",2));
    assert(!item_chests::planFor(209,"Ticket",1));
    auto ext=item_chests::planFor(214,"ItemUse_SCBoxHHSTZ",4);
    assert(ext && ext->rewards.back().itemId==2222);
    std::cout<<"05-AY: tickets historicos e 6 baus Relic por personagem OK\n";
}
