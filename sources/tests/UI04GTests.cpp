#include "../Engine/ui/UIManager.h"
#include "../Engine/ui/HudDrawList.h"
#include "../Engine/ui/UIRenderer.h"
#include "../Engine/ui/screens/CharacterStatusUI.h"
#include "../Engine/ui/screens/GameHUD.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace top::ui;
int main() {
    constexpr int w=1280,h=720;
    UIManager ui;
    ui.content.statLines_={"Lance","25"};
    HudDrawList d;
    // Regressao: textos de C nao podem continuar em tela depois de fechar.
    CharacterStatusUI::appendText(d,ui.content,ui.windows);
    assert(d.labels.empty());
    ui.toggle(1);
    CharacterStatusUI::appendText(d,ui.content,ui.windows);
    assert(!d.labels.empty());
    ui.close(UIManager::Panel::Status);
    d={}; CharacterStatusUI::appendText(d,ui.content,ui.windows);
    assert(d.labels.empty());
    ui.toggle(1);
    d={}; CharacterStatusUI::appendText(d,ui.content,ui.windows);
    assert(!d.labels.empty());

    ui.shop.setWindow(true,"Goldie",{"27","80"},12,80,245,
                      {"knife.png","sword.png"},{"Knife","Sword"});
    const auto old=ui.shop.bounds(w);
    assert(old.x==926.f && old.y==64.f);
    assert(ui.shop.hit(int(old.x)+24,int(old.y)+63,w)==12);
    assert(ui.shop.hit(int(old.x)+174,int(old.y)+5,w)==-2);
    assert(ui.shop.hit(int(old.x)+20,int(old.y)+6,w)==-3);
    ui.shop.beginDrag(int(old.x)+20,int(old.y)+10,w);
    assert(ui.shop.dragging());
    ui.shop.drag(400,250,true,w,h);
    const auto now=ui.shop.bounds(w);
    assert(std::abs(now.x-380.f)<.01f && std::abs(now.y-240.f)<.01f);
    assert(ui.shop.hit(404,303,w)==12);
    assert(ui.shop.hit(554,245,w)==-2);
    assert(ui.shop.hit(950,127,w)==-1);
    // Atualizacoes do jogo chegam a cada quadro; nao podem reposicionar a loja.
    ui.shop.setWindow(true,"Goldie",{"27","80"},12,80,245,
                      {"knife.png","sword.png"},{"Knife","Sword"});
    assert(ui.shop.bounds(w).x==now.x);
    d={}; ui.shop.appendSprites(d,w); ui.shop.appendText(d,w,404,303);
    assert(d.sprites.empty() && d.icons.size()==2);
    assert(std::abs(d.icons.front().x-(now.x+23.f))<.01f);
    assert(!d.labels.empty());
    UIRenderer renderer(w,h);
    ui.shop.draw(renderer,w);
    ui.npc.setWindow(true,"NPC","Dialogo offline",true);
    ui.npc.draw(renderer,w,h);
    ui.boat.toggle(); ui.boat.draw(renderer,w,h,4);
    ui.system.toggle(); ui.system.draw(renderer,w,h);
    ui.characterSelect.setVisible(true); ui.characterSelect.draw(renderer,w,h);
    ui.windows.showTemp_=true; ui.inventory.drawTemp(renderer,ui.windows,w);
    GameHUD::drawBase(renderer,1136.f,15.f,.5f,.7f,.9f,true);
    assert(!renderer.empty());
    auto batch=renderer.takeBatch();
    assert(batch.texture=="white" && !batch.verts.empty());
    ui.shop.drag(400,250,false,w,h);
    assert(!ui.shop.dragging());
    ui.shop.close();
    assert(ui.shop.hit(404,303,w)==-1);
    std::puts("04-G: status visibility, movable shop and procedural UI PASS");
}
