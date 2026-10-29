#include "../Engine/ui/UIManager.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace top::ui;
int main() {
    UIManager ui;
    constexpr int w=1280, h=720;
    assert(ui.npc.teleportHit(500, 640, w, h) == -1);
    ui.npc.setWindow(true, "Senna", "Dialogo offline", true);
    ui.npc.setTeleportChoices({"Argent", "Icicle", "Shaitan", "Thundoria", "Extra"});
    assert(ui.npc.visible() && ui.npc.hasChoices() && ui.npc.name() == "Senna");
    assert(ui.npc.teleportHit(500, 640, w, h) == 0); // escolha 0
    assert(ui.npc.teleportHit(650, 640, w, h) == 1); // escolha 1
    assert(ui.npc.teleportHit(500, 680, w, h) == 2); // escolha 2
    assert(ui.npc.teleportHit(650, 680, w, h) == 3); // escolha 3
    assert(ui.npc.teleportHit(30, 680, w, h) == -1);
    assert(ui.npc.contains(640, 600, w, h));
    assert(!ui.npc.contains(640, 100, w, h));
    assert(NpcDialogUI::offlineTravelHit(510, 635, w, h) == 0);
    assert(NpcDialogUI::offlineTravelHit(580, 635, w, h) == 1);
    assert(NpcDialogUI::offlineTravelHit(650, 635, w, h) == 2);
    assert(NpcDialogUI::offlineTravelHit(750, 635, w, h) == 3);
    assert(NpcDialogUI::offlineTravelHit(50, 635, w, h) == -1);
    ui.npc.setWindow(false);
    assert(!ui.npc.visible() && ui.npc.teleportHit(500, 640, w, h) == -1);

    ui.shop.setWindow(true, "Goldie", {"27", "80"}, 12, 99, 123,
                      {"knife.png", "sword.png"}, {"Knife", "Sword"});
    assert(ui.shop.visible() && ui.shop.name() == "Goldie");
    assert(ui.shop.first() == 12 && ui.shop.total() == 99 && ui.shop.gold() == 123);
    assert(ui.shop.lines().size() == 2 && ui.shop.icons().size() == 2 && ui.shop.itemNames().at(1) == "Sword");
    assert(ui.shop.contains(950, 100, w) && !ui.shop.contains(100, 100, w));
    ui.shop.close();
    assert(!ui.shop.visible());

    assert(ui.boat.hit(500, 260, w, h) == -1);
    ui.boat.toggle();
    assert(ui.boat.visible());
    assert(ui.boat.hit(500, 260, w, h) == 1);
    assert(ui.boat.hit(680, 260, w, h) == 10);
    assert(ui.boat.hit(812, 180, w, h) == 0);
    assert(ui.boat.hit(100, 100, w, h) == -1);
    ui.boat.close();
    assert(ui.boat.hit(500, 260, w, h) == -1);

    assert(ui.system.hit(600, 275, w, h) == 0);
    ui.npc.setWindow(true, "NPC", "texto");
    ui.shop.setWindow(true, "Shop", {"5"});
    ui.toggleSystemModal();
    assert(ui.system.visible() && !ui.npc.visible() && !ui.shop.visible());
    assert(ui.system.hit(600, 275, w, h) == 1);
    assert(ui.system.hit(600, 430, w, h) == 6);
    assert(ui.system.hit(727, 215, w, h) == -1);
    assert(ui.system.hit(100, 100, w, h) == 0);
    ui.toggleSystemModal();
    assert(!ui.system.visible());

    ui.characterSelect.setRoster({"Lance", "Carsise", "Phyllis", "Ami"}, 2);
    assert(ui.characterSelect.hit(320, 290, w, h) == 0);
    ui.characterSelect.setVisible(true);
    assert(ui.characterSelect.hit(320, 290, w, h) == 1);
    assert(ui.characterSelect.hit(830, 290, w, h) == 4);
    assert(ui.characterSelect.hit(650, 100, w, h) == 0);
    ui.characterSelect.setVisible(false);
    assert(ui.characterSelect.hit(320, 290, w, h) == 0);

    // Posicionamento dos modais nao pode herdar coordenadas dos outros paineis.
    assert(modal_layout::npc(w, h).contains(640, 600));
    assert(modal_layout::shop(w).contains(950, 100));
    assert(modal_layout::boat(w,h).contains(640,360));
    assert(modal_layout::system(w,h).contains(640,360));
    assert(modal_layout::character(w,h).contains(640,360));
    std::puts("04-E: modal state and interaction tests PASS");
}
