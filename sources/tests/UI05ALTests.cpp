// 05-AL: classic quest diary with multiple entries and paging is procedural.
#include "../Engine/ui/screens/QuestUI.h"
#include "../Engine/ui/HudDrawList.h"
#include <cassert>
int main() {
    top::ui::UIRenderer renderer(1280,720);
    top::ui::QuestUI::draw(renderer,24,60,{},true);
    assert(!renderer.empty());
    top::ui::HudDrawList drawList;
    top::ui::QuestUI::appendText(drawList,{
        "[Ativa] > Primeira", "[Ativa]   Segunda", "[Feita]   Terceira",
        "[Ativa]   Quarta", "[Ativa]   Quinta", "Progresso 1/3", "Pagina 1/2"
    },24,60);
    bool hasDetail=false,hasPrev=false,hasNext=false,hasPage=false;
    for(const auto& label:drawList.labels) {
        if(label.value.find("Progresso")!=std::string::npos)hasDetail=true;
        if(label.value.find("Anterior")!=std::string::npos)hasPrev=true;
        if(label.value.find("Proxima")!=std::string::npos)hasNext=true;
        if(label.value.find("Pagina")!=std::string::npos)hasPage=true;
    }
    assert(hasDetail&&hasPrev&&hasNext&&hasPage);
    return 0;
}
