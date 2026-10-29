#include "../HudDrawList.h"
#include "../UIManager.h"
#include "InventoryLayout.h"
#include <string>
#include <vector>
#include "CharacterStatusUI.h"
#include <cmath>
namespace top::ui {
namespace {
// Light parchment backing keeps the existing dark status/inventory text readable.
constexpr Color paper{0.87f, 0.89f, 0.84f, 0.96f};
constexpr Color stripe{0.71f, 0.79f, 0.77f, 0.96f};
constexpr Color ink{0.23f, 0.36f, 0.43f, 0.98f};
constexpr Color slotFill{0.77f, 0.82f, 0.79f, 0.96f};
void panel(UIRenderer& r, float x, float y, float w, float h, const UITheme& t) {
    r.rect(x, y, w, h, paper);
    r.border(x, y, w, h, 2.f, t.border);
    r.rect(x+2.f, y+2.f, w-4.f, 23.f, stripe);
    r.rect(x+4.f, y+27.f, w-8.f, 1.f, ink);
}
void slot(UIRenderer& r, float x, float y, float w = 32.f, float h = 32.f) {
    r.rect(x,y,w,h,slotFill);
    r.border(x,y,w,h,1.f,ink);
}
}

void CharacterStatusUI::draw(UIRenderer& r,float x,float y,int mx,int my,const UITheme& t) {
    (void)mx; (void)my;
    panel(r,x,y,198.f,394.f,t);
    r.rect(x+5.f,y+71.f,188.f,2.f,stripe);
    r.rect(x+5.f,y+161.f,188.f,2.f,stripe);
    r.rect(x+5.f,y+229.f,188.f,2.f,stripe);
    r.border(x+178.f,y+4.f,14.f,14.f,1.f,ink);
    r.rect(x+182.f,y+10.f,6.f,2.f,ink);

    // Linhas finas da ficha classica: divisorias de classe, atributos e fama.
    const Color hairline{0.59f,0.77f,0.78f,0.52f};
    const float ys[]={53.f, 69.f, 93.f, 121.f, 157.f, 222.f, 242.f, 308.f, 320.f};
    for(float dy: ys) r.rect(x+8.f,y+dy,182.f,1.f,hairline);
    // pequenos risquinhos na divisao de atributos
    for(int i=0;i<4;++i){
        r.rect(x+95.f+i*5.f,y+156.f+i%2,3.f,1.f,ink);
        r.rect(x+95.f+i*5.f,y+222.f+i%2,3.f,1.f,ink);
    }
    // regua inferior de reputacao, sem textura historica
    r.border(x+7.f,y+325.f,184.f,59.f,1.f,hairline);
    const float bx[]={62.f,168.f,62.f,168.f,168.f};
    const float by[]={176.f,176.f,192.f,192.f,208.f};
    for(int i=0;i<5;++i){slot(r,x+bx[i],y+by[i],10.f,11.f);r.rect(x+bx[i]+4.f,y+by[i]+2.f,2.f,7.f,ink);}
}
}

namespace top::ui {
void CharacterStatusUI::appendText(HudDrawList& out, const UIContentState& content, const WindowState& windows) {
    if (!windows.showState_) return;  // 04-G: textos nao podem sobreviver ao fechamento da ficha.
    const auto fieldAt = [](const std::vector<std::string>& rows, int index) -> const std::string& {
        static const std::string empty;
        if (index < 0 || index >= static_cast<int>(rows.size())) return empty;
        return rows[static_cast<std::size_t>(index)];
    };
            // A imagem antiga continha rotulos fixos: agora desenhados com fonte dinamica.
            const auto box = inventory_layout::PanelRect{windows.stateX_, windows.stateY_, 198.f, 394.f};
            const bool underQuest = windows.showQuest_ && box.x < windows.questX_ + 235.f && box.x + box.w > windows.questX_ && box.y < windows.questY_ + 394.f && box.y + 80.f > windows.questY_;
            if (!underQuest) {
            const float pr = 0.08f;
            const float pg = 0.16f;
            const float pb = 0.32f;
            out.addText("Character", box.x + 12.f, box.y + 6.f, 0.05f, 0.12f, 0.22f);
            out.addText("Class:", box.x + 12.f, box.y + 39.f, pr, pg, pb);
            out.addText("Guild:", box.x + 12.f, box.y + 55.f, pr, pg, pb);
            out.addText("EXP", box.x + 106.f, box.y + 80.f, pr, pg, pb);
            out.addText("Skill Pts", box.x + 12.f, box.y + 136.f, pr, pg, pb);
            out.addText("Stat Pts", box.x + 105.f, box.y + 136.f, pr, pg, pb);
            out.addText("Base Attributes", box.x + 12.f, box.y + 158.f, pr, pg, pb);
            out.addText("Attributes", box.x + 12.f, box.y + 230.f, pr, pg, pb);
            out.addText("Fame", box.x + 12.f, box.y + 329.f, pr, pg, pb);
            out.addText("Reputation:", box.x + 12.f, box.y + 355.f, pr, pg, pb);
            out.addText("Lv", box.x + 12.f, box.y + 80.f, pr, pg, pb);
            out.addText("HP", box.x + 12.f, box.y + 108.f, pr, pg, pb);
            out.addText("SP", box.x + 92.f, box.y + 108.f, pr, pg, pb);
            out.addText("STR", box.x + 12.f, box.y + 176.f, pr, pg, pb);
            out.addText("CON", box.x + 92.f, box.y + 176.f, pr, pg, pb);
            out.addText("AGI", box.x + 12.f, box.y + 192.f, pr, pg, pb);
            out.addText("SPR", box.x + 92.f, box.y + 192.f, pr, pg, pb);
            out.addText("ACC", box.x + 12.f, box.y + 208.f, pr, pg, pb);
            out.addText("LUK", box.x + 92.f, box.y + 208.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 0), box.x + 12.f, box.y + 24.f, 0.05f, 0.12f, 0.22f);
            out.addText(fieldAt(content.statLines_, 10), box.x + 48.f, box.y + 46.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 1), box.x + 36.f, box.y + 80.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 11), box.x + 118.f, box.y + 80.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 2), box.x + 36.f, box.y + 108.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 3), box.x + 112.f, box.y + 108.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 13), box.x + 70.f, box.y + 140.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 12), box.x + 140.f, box.y + 140.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 4), box.x + 40.f, box.y + 176.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 5), box.x + 122.f, box.y + 176.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 7), box.x + 40.f, box.y + 192.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 8), box.x + 122.f, box.y + 192.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 6), box.x + 40.f, box.y + 208.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 9), box.x + 122.f, box.y + 208.f, pr, pg, pb);
            out.addText("Min Att", box.x + 12.f, box.y + 248.f, pr, pg, pb);
            out.addText("Max Att", box.x + 96.f, box.y + 248.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 14), box.x + 62.f, box.y + 248.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 15), box.x + 150.f, box.y + 248.f, pr, pg, pb);
            out.addText("DEF", box.x + 12.f, box.y + 264.f, pr, pg, pb);
            out.addText("P.Res", box.x + 96.f, box.y + 264.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 16), box.x + 62.f, box.y + 264.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 21), box.x + 150.f, box.y + 264.f, pr, pg, pb);
            out.addText("HIT", box.x + 12.f, box.y + 280.f, pr, pg, pb);
            out.addText("Dodge", box.x + 96.f, box.y + 280.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 17), box.x + 62.f, box.y + 280.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 18), box.x + 150.f, box.y + 280.f, pr, pg, pb);
            out.addText("CRT", box.x + 12.f, box.y + 296.f, pr, pg, pb);
            out.addText("HP REC", box.x + 96.f, box.y + 296.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 20), box.x + 62.f, box.y + 296.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 22), box.x + 150.f, box.y + 296.f, pr, pg, pb);
            out.addText("SP REC", box.x + 12.f, box.y + 312.f, pr, pg, pb);
            out.addText("ASPD", box.x + 96.f, box.y + 312.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 23), box.x + 62.f, box.y + 312.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_, 19), box.x + 150.f, box.y + 312.f, pr, pg, pb);
            out.addText("MSPD", box.x+12.f, box.y+342.f, pr, pg, pb);
            out.addText(fieldAt(content.statLines_,24), box.x+62.f, box.y+342.f, pr, pg, pb);
            auto choice = [&](const char* text, float x, float y, int job) {
                const bool on = content.jobMark_ == job;
                out.addText(text, box.x + x, box.y + y, on ? 0.05f : 0.2f, on ? 0.28f : 0.28f, on ? 0.55f : 0.4f);
            };
            choice("Sword", 116.f, 44.f, 1);
            choice("Hunt", 158.f, 44.f, 2);
            choice("Expl", 116.f, 60.f, 4);
            choice("Herb", 158.f, 60.f, 5);
            }
    
}
}
