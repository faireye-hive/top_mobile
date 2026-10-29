// 05-AB: inspection popups must use the exact item instance, not just item ID.
#include "../Engine/ui/UIManager.h"
#include "../Engine/ui/UIRenderer.h"
#include "../Engine/ui/HudDrawList.h"
#include <cassert>
#include <iostream>
#include <algorithm>
int main(){
    using namespace top::ui;
    UIManager ui;
    ui.toggle(0);
    const float ox=ui.windows.invX_, oy=ui.windows.invY_;
    BagView swordA{0,777,1,9,true,"icon-a","Sword",0,{}};
    swordA.uid=111;
    swordA.tooltip={{"Sword +1",TooltipLine::Kind::Title},
                    {"UID 111",TooltipLine::Kind::Muted},
                    {"Strength: +5",TooltipLine::Kind::Bonus}};
    BagView swordB{1,777,1,9,true,"icon-b","Sword",0,{}};
    swordB.uid=222;
    swordB.tooltip={{"Sword +7",TooltipLine::Kind::Title},
                    {"UID 222",TooltipLine::Kind::Muted},
                    {"Strength: +9",TooltipLine::Kind::Bonus}};
    BagView potion{2,900,3,-1,false,"icon-p","Potion",0,{}};
    potion.uid=333;
    potion.tooltip={{"Potion",TooltipLine::Kind::Title},{"Quantity: 3",TooltipLine::Kind::Normal}};
    ui.inventory.setView({swordA,swordB,potion},{{9,777},{6,777}}, {}, 200, 24, {{9,111},{6,222}});
    assert(ui.inventory.wornRow(9) && ui.inventory.wornRow(9)->uid==111);
    assert(ui.inventory.wornRow(6) && ui.inventory.wornRow(6)->uid==222);
    assert(ui.inventory.hoveredRow(int(ox+44),int(oy+95),1280,ui.windows)->uid==111);
    assert(ui.inventory.hoveredRow(int(ox+130),int(oy+95),1280,ui.windows)->uid==222);
    assert(ui.inventory.hoveredRow(int(ox+26),int(oy+209),1280,ui.windows)->uid==333);
    assert(!ui.inventory.hoveredRow(0,0,1280,ui.windows));
    UIRenderer shapes(1280,720); HudDrawList overlay;
    ui.inventory.appendTooltip(shapes,overlay,ui.windows,1280,720,int(ox+130),int(oy+95),18.f);
    assert(!shapes.empty());
    assert(overlay.labels.size()==3);
    assert(overlay.labels[0].value=="Sword +7");
    assert(overlay.labels[2].value=="Strength: +9");
    // UI must not display a popup while actively dragging the item.
    ui.inventory.setItemDrag(106,400,100,true);
    UIRenderer dragging(1280,720); HudDrawList dragLabels;
    ui.inventory.appendTooltip(dragging,dragLabels,ui.windows,1280,720,int(ox+130),int(oy+95),18.f);
    assert(dragging.empty() && dragLabels.labels.empty());
    ui.inventory.cancelItemDrag();
    // When near the edge, clamp the popup back inside the screen.
    ui.windows.invX_=980.f; ui.windows.invY_=510.f;
    UIRenderer edge(1280,720); HudDrawList edgeLabels;
    ui.inventory.appendTooltip(edge,edgeLabels,ui.windows,1280,720,1110,605,18.f);
    assert(!edge.empty());
    assert(edgeLabels.labels[0].x>=6.f && edgeLabels.labels[0].x<1280.f);
    assert(edgeLabels.labels[0].y>=6.f && edgeLabels.labels[0].y<720.f);
    // With an invalid per-instance UID, never display another duplicate sword.
    ui.inventory.setView({swordA,swordB},{{9,777}}, {}, 200, 24, {{9,444}});
    assert(ui.inventory.wornRow(9)==nullptr);
    // Backward compatibility: older tests and clients still omit UIDs.
    ui.inventory.setView({swordA},{{9,777}}, {}, 200);
    assert(ui.inventory.wornRow(9)!=nullptr);
    std::cout<<"05-AB: tooltip, per-instance UID, drag and edge layout OK\n";
}
