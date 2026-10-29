// 05-AC: bancada de forja; hitboxes, UID estavel e cancelamento seguro.
#include "../Engine/ui/UIManager.h"
#include "../Engine/ui/UIRenderer.h"
#include "../Engine/ui/HudDrawList.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace top::ui;
    UIManager ui;
    ui.inventory.setView({
        BagView{0,777,1,9,false,"icon-e","Sword",101,{}},
        BagView{1,879,1,-1,false,"icon-a","Furious Gem",201,{}},
        BagView{2,885,1,-1,false,"icon-b","Refiner",301,{}}}, {},{},200000,24);
    auto& bench=ui.forge;
    bench.toggle();
    assert(bench.visible());
    const auto b=ForgeWorkbenchUI::bounds(1280,720);
    assert(bench.hit(int(b.x+13),int(b.y+45),1280,720)==2);
    assert(bench.hit(int(b.x+123),int(b.y+45),1280,720)==3);
    assert(bench.hit(int(b.x+239),int(b.y+45),1280,720)==4);
    assert(bench.hit(int(b.x+20),int(b.y+106),1280,720)==10);
    assert(bench.hit(int(b.x+120),int(b.y+106),1280,720)==11);
    assert(bench.dropSlot(int(b.x+226),int(b.y+112),1280,720)==2);
    assert(bench.hit(int(b.x+32),int(b.y+260),1280,720)==5);
    assert(bench.hit(10,10,1280,720)==-1);
    assert(bench.selectNext(ui.inventory.rows()[0]));
    assert(bench.selectNext(ui.inventory.rows()[1]));
    assert(bench.selectNext(ui.inventory.rows()[2]));
    assert((bench.indices(ui.inventory)==std::array<int,3>({0,1,2})));
    assert(!bench.select(1,ui.inventory.rows()[0])); // no same UID in two slots
    // Two same-ID swords remain independent when bag indices change.
    const BagView second{3,777,1,9,false,"icon-x","Sword +3",102,{}};
    ui.inventory.setView({second,ui.inventory.rows()[1],ui.inventory.rows()[0],ui.inventory.rows()[2]}, {},{},100000,24);
    assert(bench.index(0,ui.inventory)==0); // old item index still zero because of same UID
    // Re-index properly after moving the real instance to a different bag slot.
    auto bag=ui.inventory.rows();
    bag[0].index=0; bag[2].index=6;
    ui.inventory.setView(bag,{}, {},100000,24);
    assert(bench.index(0,ui.inventory)==6);
    assert(bench.index(1,ui.inventory)==1);
    bench.setPreview(true,100000,6500,"Pronto para revisar operacao");
    assert(bench.validPreview());
    UIRenderer shapes(1280,720); HudDrawList labels;
    bench.draw(shapes,labels,1280,720,ui.inventory);
    assert(!shapes.empty());
    bool costShown=false, nameShown=false;
    for(const auto& label:labels.labels) {
        if(label.value.find("100000")!=std::string::npos)costShown=true;
        if(label.value.find("Sword")!=std::string::npos)nameShown=true;
    }
    assert(costShown&&nameShown);
    bench.clearSlot(0);
    assert(bench.index(0,ui.inventory)==-1);
    bench.setMode(ForgeWorkbenchUI::Mode::Socket);
    assert(bench.mode()==ForgeWorkbenchUI::Mode::Socket);
    assert((bench.indices(ui.inventory)==std::array<int,3>({-1,-1,-1})));
    bench.close();assert(!bench.visible());
    bench.open(8u);
    assert(!bench.modeAllowed(0)&&!bench.modeAllowed(1)&&!bench.modeAllowed(2)&&bench.modeAllowed(3));
    assert(bench.mode()==ForgeWorkbenchUI::Mode::Production);
    assert(bench.hit(int(b.x+269),int(b.y+45),1280,720)==0); // painel de producao nao altera as abas da forja
    assert(bench.hit(int(b.x+32),int(b.y+260),1280,720)==5);
    bench.setPreview(true,1,90,"Produto: Teste\nMateriais: 501x2 502x1 503x1\nMoedas fada: 1");
    UIRenderer productionShapes(1280,720);HudDrawList productionLabels;
    bench.draw(productionShapes,productionLabels,1280,720,ui.inventory);
    bool productionMaterialsShown=false,productionTitleShown=false;
    for(const auto& label:productionLabels.labels) {
        if(label.value.find("501x2 502x1 503x1")!=std::string::npos)productionMaterialsShown=true;
        if(label.value.find("Substance Generator")!=std::string::npos)productionTitleShown=true;
    }
    assert(!productionShapes.empty()&&productionMaterialsShown&&productionTitleShown);
    bench.close();
    const auto small=ForgeWorkbenchUI::bounds(640,480);
    bench.toggle();
    assert(bench.contains(int(small.x+25),int(small.y+50),640,480));
    std::cout<<"05-AC: bancada, UID, hitboxes e renderizacao OK\n";
}
