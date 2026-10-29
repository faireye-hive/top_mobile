#include "top/ClassicCamera.h"
#include "top/InventoryConfirm.h"
#include "../Engine/ui/UIManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    using namespace top;
    using namespace top::classic_camera;
    assert(kDefaultLift > 1.5f && kMinLift > 1.f);
    const float angle = std::atan(kDefaultLift) * 180.f / 3.14159265f;
    assert(angle > 60.f && angle < 68.f);
    assert(fromSavedLift(.43f) == kDefaultLift); // upgrade old camera save
    assert(lift(0.f)==kMinLift);
    assert(lift(99.f)==kMaxLift);
    assert(distance(2.f)==kMinDistance);
    assert(distance(5.f)==5.f); // 05-X: zoom aproximado adicional
    assert(distance(999.f)==kMaxDistance);
    ui::UIManager manager;
    manager.windows.showSkill_ = true;
    ui::SkillView heal;heal.id=97;heal.name="Heal";heal.sp=5;heal.learned=true;
    ui::SkillView berserk;berserk.id=84;berserk.name="Berserk";berserk.sp=6;
    berserk.learned=true;berserk.hotkey=7;
    manager.skills.setView({heal,berserk},2);
    constexpr int screenW=1024;
    const int bx=screenW-206;
    assert(manager.skills.hit(bx+25,64+76,screenW,manager.windows)==97);
    assert(manager.skills.upgradeHit(bx+171,64+79,screenW,manager.windows)==97);
    assert(manager.skills.upgradeHit(bx+171,64+79+48,screenW,manager.windows)==84);
    assert(manager.skills.upgradeHit(bx+200,64+79,screenW,manager.windows)==-1);
    manager.windows.showSkill_=false;
    assert(manager.skills.upgradeHit(bx+171,64+79,screenW,manager.windows)==-1);
    assert(inventory_confirm::hit(1024/2-80,768/2+32,1024,768)==inventory_confirm::Click::Confirm);
    assert(inventory_confirm::hit(1024/2+80,768/2+32,1024,768)==inventory_confirm::Click::Cancel);
    assert(inventory_confirm::hit(5,5,1024,768)==inventory_confirm::Click::Cancel);
    std::puts("05-W2: camera classica, botao + e confirmacao testados");
}
