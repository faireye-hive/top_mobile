#include "../Engine/ui/UIManager.h"
#include <cassert>
#include <cstdio>
#include <vector>
int main() {
    top::ui::UIManager ui;
    ui.toggle(0);
    ui.inventory.setView({{0, 42, 1, 5, true, {}, "Sword"}, {1, 77, 2, -1, false, {}, "Potion"}},
                         {{5, 42}}, {}, 120);
    assert(ui.inventory.hit(214 + 24, 64 + 32, 1280, ui.windows) == 105);
    assert(ui.inventory.hit(214 + 27, 64 + 213, 1280, ui.windows) == 201);
    assert(ui.inventory.dropHit(214 + 27, 64 + 213, 1280, ui.windows) == 250);
    // A selecao do inventario deve usar indices originais e nao indices filtrados.
    assert(!ui.inventory.wheelHit(0, 0, 1280, ui.windows));
    ui.inventory.setItemDrag(201, 600, 400, true);
    assert(ui.inventory.draggedRow() && ui.inventory.draggedRow()->id == 77);
    ui.toggle(4);
    ui.inventory.setView({{0, 42, 1, 5, true, {}, "Sword"}, {1, 77, 2, -1, false, {}, "Potion"}},
                         {{5, 42}}, {{13, 91, 1, -1, false, {}, "Temporary"}}, 150);
    assert(ui.inventory.hit(214 + 206 + 26, 64 + 60, 1280, ui.windows) == 313);
    assert(ui.inventory.wheelHit(450, 120, 1280, ui.windows));
    assert(ui.inventory.dropHit(450, 120, 1280, ui.windows) == 350);
    ui.inventory.setItemDrag(313, 600, 400, true);
    assert(ui.inventory.draggedRow() && ui.inventory.draggedRow()->id == 91);
    ui.close(top::ui::UIManager::Panel::Inventory);
    assert(!ui.inventory.itemDragActive());
    ui.toggle(2);
    ui.skills.setView({{41}, {42}, {43}, {44}, {45}, {46}}, 2);
    assert(ui.skills.hit(1094, 134, 1280, ui.windows) == 41);
    assert(ui.skills.wheel(1094, 134, 1280, 1, ui.windows));
    assert(ui.skills.hit(1094, 134, 1280, ui.windows) == 46);
    ui.skills.setView({{47}}, 1);
    assert(ui.skills.page() == 0 && ui.skills.points() == 1);
    assert(!ui.skills.wheel(0, 0, 1280, 1, ui.windows));
    ui.toggle(2);
    assert(ui.skills.hit(1094, 134, 1280, ui.windows) == -1);
    // A paginacao deve manter o contrato de quatro colunas e quatro linhas.
    ui.toggle(0);
    std::vector<top::ui::BagView> many;
    for (int i=0; i<24; ++i) many.push_back({i, i + 100, 1, -1, false, {}, "Generic"});
    ui.inventory.setView(many, {}, {}, 999);
    ui.inventory.pageBy(1, ui.windows);
    assert(ui.inventory.page() == 1);
    assert(ui.inventory.hit(214 + 27, 64 + 213, 1280, ui.windows) == 204);
    ui.inventory.pageBy(1, ui.windows);
    ui.inventory.pageBy(1, ui.windows);
    assert(ui.inventory.page() == 2); // 24 itens = 6 linhas, apenas 2 paginas extras
    ui.inventory.setView({{0, 100, 1, -1, false, {}, "Few"}}, {}, {}, 0);
    assert(ui.inventory.page() == 2); // 24 slots remain scrollable even when empty.
    ui.inventory.pageBy(-10, ui.windows);
    assert(ui.inventory.page() == 1); // wheel steps one row, not delta magnitude.
    std::puts("04-D: controller tests PASS");
}
