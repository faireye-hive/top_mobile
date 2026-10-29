#include "../HudDrawList.h"
#include "QuestUI.h"
namespace top::ui {
void QuestUI::draw(UIRenderer& r, float x, float y, const UITheme& theme, bool canAbandon) {
    const Color paper{0.87f, 0.89f, 0.84f, 0.96f};
    const Color header{0.71f, 0.79f, 0.77f, 0.96f};
    const Color thin{0.59f, 0.77f, 0.78f, 0.65f};
    const Color ink{0.23f, 0.36f, 0.43f, 0.98f};
    r.rect(x,y,235.f,394.f,paper);
    r.border(x,y,235.f,394.f,2.f,theme.border);
    r.rect(x+2.f,y+2.f,231.f,23.f,header);
    r.rect(x+4.f,y+27.f,227.f,1.f,ink);
    r.border(x+216.f,y+4.f,14.f,14.f,1.f,ink);
    r.rect(x+220.f,y+10.f,6.f,2.f,ink);
    r.border(x+8.f,y+37.f,219.f,115.f,1.f,thin);
    r.border(x+8.f,y+173.f,219.f,181.f,1.f,thin);
    r.rect(x+10.f,y+164.f,215.f,1.f,thin);
    r.rect(x+12.f,y+134.f,89.f,22.f,{.75f,.85f,.83f,1.f});
    r.rect(x+126.f,y+134.f,95.f,22.f,{.75f,.85f,.83f,1.f});
    r.border(x+12.f,y+134.f,89.f,22.f,1.f,ink);
    r.border(x+126.f,y+134.f,95.f,22.f,1.f,ink);
    r.rect(x+10.f,y+366.f,215.f,1.f,thin);
    if(canAbandon) {
        r.rect(x+120.f,y+365.f,104.f,24.f,{.88f,.70f,.67f,1.f});
        r.border(x+120.f,y+365.f,104.f,24.f,1.f,ink);
    }
}
}

namespace top::ui {
void QuestUI::appendText(HudDrawList& out, const std::vector<std::string>& questLines, float x, float y) {
            out.addText("Quest Log",x+20.f,y+8.f,.11f,.24f,.31f);
            out.addText("Missoes - Historicas e Locais",x+20.f,y+33.f,.11f,.25f,.29f);
            out.addText("Objetivos",x+20.f,y+165.f,.14f,.32f,.34f);
            out.addText("< Anterior",x+21.f,y+140.f,.14f,.32f,.34f);
            out.addText("Proxima >",x+137.f,y+140.f,.14f,.32f,.34f);
            float listY = y + 52.f;
            float detailY = y + 188.f;
            bool any = false;
            for (const std::string& row : questLines) {
                const bool detail = row.empty() || row[0] != '[';
                if (detail) {
                    if (detailY > y + 336.f) continue;
                    out.addText(row.substr(0, 32), x + 22.f, detailY, 0.52f, 0.16f, 0.20f);
                    detailY += 16.f;
                } else {
                    if (listY > y + 126.f) continue;
                    const bool active=row.find("[Ativa]")!=std::string::npos;
                    const bool done=row.find("[Feita]")!=std::string::npos;
                    const bool ready=row.find("[Pronta]")!=std::string::npos;
                    out.addText(row.substr(0, 32), x + 22.f, listY,
                                ready?.56f:done?.14f:active?.22f:.35f,ready?.38f:done?.46f:active?.29f:.14f,ready?.04f:.30f);
                    listY += 16.f;
                }
                any = true;
            }
            if (!any) out.addText("Nenhuma missao ativa", x + 22.f, y + 56.f, 0.3f, 0.32f, 0.35f);
    
}
}
