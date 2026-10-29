#include "top/ItemNewbieChestRules.h"
#include <cassert>
#include <iostream>
using namespace top::item_newbie_chests;
static int count(const Plan& p, int id) {
    for (const auto& r : p.rewards) if (r.itemId==id) return r.quantity;
    return 0;
}
static int event(const Plan& p, int id) {
    for (const auto& r : p.rewards) if (r.itemId==id) return r.itemEvent;
    return 0;
}
int main() {
    // NewbieChest() original + tabela iteminfo.txt, IDs 436..443.
    assert(!isHistoricalNewbieChest(435,"NewbieChest"));
    assert(!isHistoricalNewbieChest(436,"Ticket"));
    assert(!evaluate(436,"Ticket",50,0,1,false));
    for(int id=436;id<=442;++id) {
        const int level=5*(id-435);
        auto low=evaluate(id,"NewbieChest",level-1,1,1,false);
        assert(low.failure==Failure::Level);
        assert(evaluate(id,"NewbieChest",level,1,1,true).failure==Failure::Boat);
        if(id>=437) assert(evaluate(id,"NewbieChest",level,0,1,false).failure==Failure::Job);
        auto valid=evaluate(id,"NewbieChest",level,1,1,false);
        assert(valid && valid.plan.minimumFreeSlots>=3 && valid.plan.gold==2500*(id-435));
    }
    auto start=evaluate(436,"NewbieChest",5,0,1,false);
    assert(start && start.plan.minimumFreeSlots==4 && start.plan.rewards.size()==5 &&
           count(start.plan,437)==1 && count(start.plan,4308)==1 && count(start.plan,6050)==1);
    auto wing=evaluate(437,"NewbieChest",10,1,1,false);
    assert(wing && wing.plan.minimumFreeSlots==9 && wing.plan.gold==5000 &&
           count(wing.plan,10)==1 && count(wing.plan,296)==1 &&
           count(wing.plan,438)==1 && count(wing.plan,4602)==10 &&
           count(wing.plan,4603)==10 && count(wing.plan,4604)==10);
    auto fish=evaluate(437,"NewbieChest",10,4,4,false);
    assert(fish && count(fish.plan,80)==1 && count(fish.plan,351)==1 && count(fish.plan,2202)==1);
    auto cleric=evaluate(437,"NewbieChest",10,5,3,false);
    assert(cleric && count(cleric.plan,104)==1 && count(cleric.plan,372)==1);
    assert(evaluate(437,"NewbieChest",10,5,2,false).failure==Failure::CharacterType);
    auto flower=evaluate(438,"NewbieChest",15,2,2,false);
    assert(flower && count(flower.plan,26)==1 && count(flower.plan,306)==1 &&
           count(flower.plan,439)==1 && count(flower.plan,3351)==1 && count(flower.plan,446)==1);
    auto smith=evaluate(440,"NewbieChest",25,1,1,false);
    assert(smith && count(smith.plan,3)==1 && count(smith.plan,293)==1 && count(smith.plan,262)==1);
    auto guide=evaluate(442,"NewbieChest",35,1,1,false);
    assert(guide && guide.plan.gold==17500 && guide.plan.minimumFreeSlots==3 &&
           guide.plan.rewards.size()==2 && count(guide.plan,443)==1 && count(guide.plan,451)==1);
    assert(isHistoricalNewbieChest(443,"NewbieChest"));
    assert(evaluate(443,"NewbieChest",39,8,1,false).failure==Failure::Level);
    assert(evaluate(443,"NewbieChest",40,8,1,true).failure==Failure::Boat);
    assert(evaluate(443,"NewbieChest",40,8,1,false).failure==Failure::None);
    auto champion=evaluate(443,"NewbieChest",40,8,1,false);
    assert(champion && champion.plan.gold==200000 && champion.plan.minimumFreeSlots==4 &&
           count(champion.plan,300)==1 && count(champion.plan,15)==1 && count(champion.plan,301)==1 &&
           count(champion.plan,20)==1 && count(champion.plan,450)==1 && count(champion.plan,855)==49 &&
           event(champion.plan,300)==95 && event(champion.plan,20)==11);
    auto clericSecond=evaluate(443,"NewbieChest",40,13,3,false);
    auto clericSecondAmi=evaluate(443,"NewbieChest",40,13,4,false);
    assert(clericSecond && count(clericSecond.plan,370)==1 && count(clericSecond.plan,378)==1 &&
           clericSecondAmi && count(clericSecondAmi.plan,392)==1 && count(clericSecondAmi.plan,388)==1);
    for(int job:{9,12,14,16}) assert(evaluate(443,"NewbieChest",40,job,3,false));
    assert(evaluate(443,"NewbieChest",40,5,1,false).failure==Failure::Job);
    assert(evaluate(436,"NewbieChest",5,0,1,false));
    std::cout << "05-AZ: oito Newbie Chests e eventos de instancia validados\n";
}
