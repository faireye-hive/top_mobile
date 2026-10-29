#include "../Engine/ui/UIManager.h"
#include "../Engine/ui/HudDrawList.h"
#include "../Engine/ui/screens/GameHUD.h"
#include "../Engine/ui/screens/CharacterStatusUI.h"
#include "../Engine/ui/screens/QuestUI.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace top::ui;
namespace {
bool hasSprite(const HudDrawList& d, const std::string& part) {
    return std::any_of(d.sprites.begin(), d.sprites.end(), [&](const HudSprite& s) {
        return std::string(s.file).find(part) != std::string::npos;
    });
}
bool hasText(const HudDrawList& d, const std::string& part) {
    return std::any_of(d.labels.begin(), d.labels.end(), [&](const HudLabel& s) {
        return s.kind == HudLabel::Kind::Text && s.value.find(part) != std::string::npos;
    });
}
std::size_t badgeCount(const HudDrawList& d) {
    return std::count_if(d.labels.begin(), d.labels.end(), [](const HudLabel& s) {
        return s.kind == HudLabel::Kind::CountBadge;
    });
}
}
int main() {
    constexpr int w = 1280, h = 720;
    UIManager ui;
    HudDrawList d;
    GameHUD::appendBaseSprites(d, 1136.f, 15.f, true);
    assert(d.sprites.size() == 1);
    assert(!hasSprite(d, "new3.tga") && hasSprite(d, "minimap"));
    const float radarX = d.sprites[0].x;
    assert(std::abs(radarX - 1142.f) < 0.01f);
    d = {};
    ui.shop.appendSprites(d,w); ui.shop.appendText(d,w, 950,150);
    ui.npc.appendSprites(d,w,h); ui.npc.appendText(d,w,h);
    ui.boat.appendSprites(d,w,h); ui.boat.appendText(d,w,h,0);
    ui.system.appendSprites(d,w,h);
    ui.characterSelect.appendSprites(d,w,h); ui.characterSelect.appendText(d,w,h);
    assert(d.sprites.empty() && d.icons.empty() && d.labels.empty());

    ui.shop.setWindow(true,"Senna", {"100", "250"}, 0,2,350,
                      {"knife.png","sword.png"}, {"Faca", "Espada"});
    d = {};
    ui.shop.appendSprites(d,w); ui.shop.appendText(d,w, w-354+24, 64+63);
    assert(d.sprites.empty() && d.icons.size() == 2);
    assert(!hasSprite(d, "Store_Others.png") && hasText(d,"Faca"));
    assert(d.icons[0].file == "knife.png");
    assert(d.icons[0].x == static_cast<float>(w-354+23));

    ui.npc.setWindow(true,"NPC A", "Uma conversa do modo offline.", true);
    d = {};
    ui.npc.appendSprites(d,w,h); ui.npc.appendText(d,w,h);
    assert(d.sprites.empty() && !hasSprite(d,"NPC_TALK.png"));
    assert(hasText(d, "NPC A") && hasText(d, "Aceitar") && hasText(d, "Recusar"));
    ui.npc.close();
    d = {};
    ui.npc.appendSprites(d,w,h); ui.npc.appendText(d,w,h);
    assert(d.sprites.empty() && d.labels.empty());

    ui.boat.toggle();
    d = {};
    ui.boat.appendSprites(d,w,h); ui.boat.appendText(d,w,h,7);
    assert(d.sprites.empty() && hasText(d,"Low level 7"));
    assert(hasText(d,"> Low level 7"));

    ui.system.toggle();
    d = {};
    ui.system.appendSprites(d,w,h);
    ui.system.appendText(d,w,h);
    assert(d.sprites.empty() && hasText(d,"Video / Camera"));

    ui.characterSelect.setRoster({"Lance", "Carsise", "Phyllis", "Ami"}, 2);
    ui.characterSelect.setVisible(true);
    d = {};
    ui.characterSelect.appendSprites(d,w,h); ui.characterSelect.appendText(d,w,h);
    assert(d.sprites.empty() && hasText(d,"Phyllis") && hasText(d,"Escolha seu personagem"));

    ui.windows.showInv_=true;
    ui.inventory.setView({{0,1001,7,-1,false,"items/knife.png","Faca"}}, {}, {}, 125400);
    d = {};
    ui.inventory.appendSprites(d,ui.windows,w,0,0,false);
    ui.inventory.appendText(d,ui.windows,w,0,0);
    assert(!d.icons.empty() && d.icons.front().file == "items/knife.png");
    assert(hasText(d,"125,400") && badgeCount(d)==1);
    ui.windows.showInv_ = false;
    ui.windows.showTemp_ = true;
    d = {};
    ui.inventory.appendSprites(d,ui.windows,w,0,0,false);
    assert(!hasSprite(d,"tempbag.png"));

    ui.windows.showSkill_=true;
    SkillView slash;
    slash.id=13;slash.name="Slash";slash.icon="icons/slash.png";
    slash.sp=9;slash.learned=true;slash.hotkey=2;
    ui.skills.setView({slash},4);
    d = {};
    ui.skills.appendSprites(d,ui.windows,w);
    ui.skills.appendText(d,ui.windows,w,16.f);
    assert(d.icons.size() == 1 && hasText(d,"Slash") && hasText(d,"Skill Pts"));
    assert(std::abs(d.icons[0].x - (w - 206.f + 15.f)) < 0.1f);

    d = {};
    QuestUI::appendText(d,{"[701] Saudar", "NPC perto do ferreiro"},10.f,64.f);
    assert(hasText(d,"[701]") && hasText(d,"NPC perto"));
    ui.windows.showState_=true;
    ui.content.statLines_={"Lance", "20", "80/100", "15/23", "5", "5", "5", "5", "5", "6",
                           "Newbie", "0/25", "2", "1", "7", "7", "2", "8", "8", "1.25/s",
                           "15", "0", "2", "1"};
    d = {};
    CharacterStatusUI::appendText(d,ui.content,ui.windows);
    assert(hasText(d,"Character") && hasText(d,"Lance") && hasText(d,"LUK") &&
           hasText(d,"P.Res") && hasText(d,"CRT") && hasText(d,"SP REC"));
    std::puts("04-F: draw-command tests PASS");
}
