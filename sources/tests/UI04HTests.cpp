#include "../Engine/ui/HudComposer.h"
#include "../Engine/ui/UIManager.h"
#include "../Engine/ui/screens/CharacterStatusUI.h"
#include "../Engine/ui/screens/InventoryUI.h"
#include "../Engine/ui/screens/SkillsUI.h"
#include "../Engine/ui/screens/ShopUI.h"
#include "../Engine/ui/screens/InventoryLayout.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace top;
using namespace top::ui;
namespace {
std::size_t vertexCount(const std::vector<ActorBatch>& b, const char* tex) {
    std::size_t n=0;
    for (const auto& part:b) if (part.texture==tex) n+=part.verts.size();
    return n;
}
void drawStatus(HudComposer& compositor, UIManager& ui, int sw, int sh) {
    if (!ui.windows.showState_) return;
    UIRenderer geom(sw,sh);
    HudDrawList list;
    CharacterStatusUI::draw(geom,ui.windows.stateX_,ui.windows.stateY_,0,0);
    CharacterStatusUI::appendText(list,ui.content,ui.windows);
    compositor.layer(geom,list,{ui.windows.stateX_,ui.windows.stateY_,198.f,394.f});
}
void drawSkills(HudComposer& compositor, UIManager& ui, int sw, int sh) {
    if (!ui.windows.showSkill_) return;
    UIRenderer geom(sw,sh);
    HudDrawList list;
    const float x=ui.windows.skillX_<0.f?sw-206.f:ui.windows.skillX_;
    SkillsUI::draw(geom,x,ui.windows.skillY_,ui.skills.page(),ui.skills.rows(),UITheme{});
    ui.skills.appendSprites(list,ui.windows,sw);
    ui.skills.appendText(list,ui.windows,sw,16.f);
    compositor.layer(geom,list,{x,ui.windows.skillY_,198.f,394.f});
}
} // namespace
int main() {
    constexpr int sw=1280,sh=720;
    Font font;
    const bool haveFont=font.open();
    UIManager ui;
    ui.content.statLines_={"Lance", "12"};
    std::vector<ActorBatch> result;
    HudComposer painter(result,font,"Client",sw,sh);
    // 04-J.1: o compositor deve COPIAR a raiz dos assets. Guardar uma
    // referencia ao argumento (principalmente uma string temporaria como
    // "Client") gerava use-after-scope em layer() e podia lançar bad_alloc.
    {
        std::string originalRoot = "expected_asset_root";
        std::vector<ActorBatch> probeBatches;
        HudComposer probe(probeBatches, font, originalRoot, sw, sh);
        originalRoot.assign(4096, 'X'); // altera a string depois do construtor
        HudDrawList probeDraw;
        probeDraw.icons.push_back({"icons/example.png", 20.f, 20.f, 28.f, 28.f});
        UIRenderer probeShapes(sw, sh);
        probe.layer(probeShapes, probeDraw, {0.f, 0.f, float(sw), float(sh)});
        assert(probeBatches.size() == 1);
        assert(probeBatches[0].texture == "expected_asset_root/icons/example.png");
    }
    {
        // Usa explicitamente uma temporaria, como o cliente/teste original.
        std::vector<ActorBatch> probeBatches;
        HudComposer probe(probeBatches, font, std::string("Client"), sw, sh);
        HudDrawList probeDraw;
        probeDraw.icons.push_back({"items/example.png", 20.f, 20.f, 28.f, 28.f});
        UIRenderer probeShapes(sw, sh);
        probe.layer(probeShapes, probeDraw, {0.f, 0.f, float(sw), float(sh)});
        assert(probeBatches.size() == 1);
        assert(probeBatches[0].texture == "Client/items/example.png");
    }
    // Status fechado no primeiro frame: nao deve emitir NENHUMA geometria.
    drawStatus(painter,ui,sw,sh);
    assert(result.empty());
    ui.toggle(1);
    drawStatus(painter,ui,sw,sh);
    assert(!result.empty() && vertexCount(result,"white")>0);
    if (haveFont) assert(vertexCount(result,"font")>0);
    // Teste de regressao: fechar e reconstruir o HUD, nao reutilizar batches.
    ui.close(UIManager::Panel::Status);
    result.clear();
    drawStatus(painter,ui,sw,sh);
    assert(result.empty());
    // O texto de uma janela deve ficar ANTES do fundo da janela seguinte.
    ui.toggle(1);
    ui.windows.skillX_=120.f; // propositalmente sobreposta ao Status
    ui.windows.showSkill_=true;
    ui.skills.setView({{13,"Slash", "items/icon.png",9,true,false,0}},4);
    drawStatus(painter,ui,sw,sh);
    drawSkills(painter,ui,sw,sh);
    assert(!result.empty());
    std::size_t lastFont=0, lastWhite=0;
    for (std::size_t i=0;i<result.size();++i) {
        if (result[i].texture=="font") lastFont=i;
        if (result[i].texture=="white") lastWhite=i;
    }
    if (haveFont) {
        // O status foi desenhado, depois o fundo branco das skills, depois
        // os textos das skills. O white da skill nao pode estar no inicio.
        std::size_t firstFont=result.size();
        for (std::size_t i=0;i<result.size();++i) if(result[i].texture=="font") {firstFont=i;break;}
        assert(firstFont<lastWhite);
        assert(lastFont>lastWhite);
    }
    // Mesmo com string longa, os vertices de fonte da janela estao clipados.
    result.clear();
    HudDrawList overflow;
    overflow.addText("Linha muito longa que sairia para fora da ficha do personagem",
                     190.f,100.f,.1f,.2f,.3f);
    UIRenderer empty(sw,sh);
    painter.layer(empty,overflow,{100.f,90.f,110.f,25.f});
    for (const auto& batch:result) if(batch.texture=="font")
        for(const auto& v:batch.verts) {
            const float x=(v.x+1.f)*sw/2.f;
            assert(x>=99.9f && x<=210.6f);
        }
    // Icones da bolsa temporaria usam a posicao real quando a bolsa abre.
    ui.windows.showInv_=true;ui.windows.showTemp_=true;
    ui.inventory.setView({}, {}, {{0,7,1,-1,false,"items/apple.png","Apple"}},0);
    HudDrawList main, temp;
    ui.inventory.appendSprites(main,ui.windows,sw,0,0,false,true,false,false);
    ui.inventory.appendSprites(temp,ui.windows,sw,0,0,false,false,true,false);
    assert(main.icons.empty() && temp.icons.size()==1);
    const auto box=inventory_layout::tempRectAt(ui.windows.invX_,ui.windows.invY_,sw,true);
    assert(std::abs(temp.icons[0].x-(box.x+inventory_layout::kTempCol[0]))<.001f);
    std::puts("04-H: hidden status, frame rebuild, modal stacking, clipping and temporary bag PASS");
}
