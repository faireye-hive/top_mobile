#include "../HudDrawList.h"
#include "CharacterSelectUI.h"
#include "../PanelChrome.h"
namespace top::ui {
void CharacterSelectUI::editCreateName(const std::string& text, bool backspace) {
    if (stage_ != 2) return;
    if (backspace && !name_.empty()) name_.pop_back();
    for (unsigned char ch : text) {
        const bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                             (ch >= '0' && ch <= '9') || ch == ' ' || ch == '_' || ch == '-';
        if (allowed && name_.size() < 24) name_.push_back(static_cast<char>(ch));
    }
}
int CharacterSelectUI::hit(int x, int y, int w, int h) const {
    if (!visible_) return 0;
    const auto box = modal_layout::character(w, h);
    const int left = static_cast<int>(box.x);
    const int top = static_cast<int>(box.y);
    if (stage_ == 3) {
        if (y >= top + 330 && y < top + 380) {
            if (x >= left + 170 && x < left + 350) return -5;
            if (x >= left + 370 && x < left + 550) return -6;
        }
        return 0;
    }
    if (stage_ == 2) {
        if (y >= top + 120 && y < top + 295) {
            for (int i = 0; i < 3; ++i) {
                const int x0 = left + 74 + i * 194;
                if (x >= x0 && x < x0 + 170) return 201 + i;
            }
        }
        if (y >= top + 330 && y < top + 380 && x >= left + 500 && x < left + 660) return -4;
        return 0;
    }
    if (stage_ == 1) {
        if (y >= top + 105 && y < top + 300) {
            for (int i = 0; i < 4; ++i) {
                const int x0 = left + 28 + i * 170;
                if (x >= x0 && x < x0 + 150) return 101 + i;
            }
        }
        if (y >= top + 330 && y < top + 380 && x >= left + 500 && x < left + 660) return -4;
        return 0;
    }
    if (y >= top + 105 && y < top + 300) {
        for (int i = 0; i < 4; ++i) {
            const int x0 = left + 28 + i * 170;
            if (x >= x0 && x < x0 + 150 && i < static_cast<int>(names_.size())) return i + 1;
        }
    }
    if (y >= top + 330 && y < top + 380) {
        if (x >= left + 32 && x < left + 188) return -1;
        if (x >= left + 196 && x < left + 352) return -2;
        if (x >= left + 360 && x < left + 516) return -3;
        if (x >= left + 524 && x < left + 680) return -4;
    }
    return 0;
}
} // namespace top::ui

namespace top::ui {

void CharacterSelectUI::draw(UIRenderer& r,int width,int height) const {
    if (!visible()) return;
    const auto b = modal_layout::character(width,height);
    chrome::panel(r,b.x,b.y,b.w,b.h);
    if (stage_ == 3) {
        r.rect(b.x+90.f,b.y+110.f,540.f,190.f,chrome::shade);
        r.border(b.x+90.f,b.y+110.f,540.f,190.f,1.f,chrome::muted);
        chrome::button(r,b.x+170.f,b.y+330.f,180.f,34.f);
        chrome::button(r,b.x+370.f,b.y+330.f,180.f,34.f);
        return;
    }
    if (stage_ == 2) {
        r.rect(b.x+74.f,b.y+62.f,572.f,38.f,chrome::shade);
        r.border(b.x+74.f,b.y+62.f,572.f,38.f,1.f,chrome::muted);
        for (int i = 0; i < 3; ++i) {
            const float x = b.x + 74.f + 194.f * i;
            r.rect(x, b.y + 120.f, 170.f, 170.f, chrome::shade);
            r.border(x, b.y + 120.f, 170.f, 170.f, 1.f, chrome::muted);
            chrome::button(r, x + 12.f, b.y + 245.f, 146.f, 28.f);
        }
        chrome::button(r, b.x + 500.f, b.y + 330.f, 160.f, 34.f);
        return;
    }
    for(int i=0;i<4;++i) {
        const float cx=b.x+28.f+170.f*i;
        r.rect(cx,b.y+105.f,150.f,196.f,chrome::shade);
        r.border(cx,b.y+105.f,150.f,196.f,1.f,chrome::muted);
        if (stage_ == 1 || i < static_cast<int>(names_.size())) {
            r.rect(cx+63.f,b.y+132.f,24.f,29.f,chrome::muted);
            r.rect(cx+40.f,b.y+162.f,70.f,57.f,chrome::muted);
            if (stage_ == 0) chrome::button(r,cx+11.f,b.y+265.f,128.f,27.f);
        }
        if (stage_ == 0 && i == selected_) r.border(cx+2.f,b.y+107.f,146.f,192.f,2.f,chrome::selected);
    }
    if (stage_ == 0) for (int i=0;i<4;++i)
        chrome::button(r,b.x+32.f+164.f*i,b.y+330.f,156.f,34.f);
    else chrome::button(r,b.x+500.f,b.y+330.f,160.f,34.f);
}
void CharacterSelectUI::appendSprites(HudDrawList& out, int width, int height) const {
    // Fundos da interface migrados para draw() (C++ puro).
    (void)out; (void)width; (void)height;
}
void CharacterSelectUI::appendText(HudDrawList& out, int width, int height) const {
    if (!visible()) return;
            const float left = ui::modal_layout::character(width, height).x;
            const float top = ui::modal_layout::character(width, height).y;
            out.addText(stage_ == 2 ? "Escolha o local de nascimento" :
                        stage_ == 1 ? "Escolha a raca" : "Escolha seu personagem", left + 238.f, top + 26.f,
                        0.06f, 0.22f, 0.36f);
            const char* names[] = {"Lance", "Carsise", "Phyllis", "Ami"};
            const char* classes[] = {"Sword / Hunter / Explorer", "Sword", "Hunter / Explorer / Herbalist",
                                     "Explorer / Herbalist"};
            if (stage_ == 3) {
                out.addText("Excluir este personagem? O arquivo de save sera mantido.", left + 132.f, top + 190.f,
                            0.08f, 0.2f, 0.34f);
                out.addText("Excluir perfil", left + 215.f, top + 340.f, 0.07f, 0.22f, 0.36f);
                out.addText("Cancelar", left + 425.f, top + 340.f, 0.07f, 0.22f, 0.36f);
            } else if (stage_ == 2) {
                const char* cities[] = {"Argent City", "Shaitan City", "Icicle Castle"};
                out.addText("Nome (ate 24 caracteres): " + (name_.empty() ? std::string("digite aqui") : name_),
                            left + 90.f, top + 75.f, 0.07f, 0.22f, 0.36f);
                for (int i = 0; i < 3; ++i) {
                    const float x = left + 74.f + 194.f * i;
                    out.addText(cities[i], x + 20.f, top + 190.f, 0.08f, 0.2f, 0.34f);
                    out.addText("Selecionar nascimento", x + 10.f, top + 252.f, 0.12f, 0.38f, 0.18f);
                }
                out.addText("Voltar", left + 548.f, top + 340.f, 0.07f, 0.22f, 0.36f);
            } else if (stage_ == 1) {
                for (int i = 0; i < 4; ++i) {
                    const float x = left + 34.f + static_cast<float>(i * 170);
                    out.addText(names[i], x + 38.f, top + 78.f, 0.08f, 0.2f, 0.34f);
                    out.addText(classes[i], x, top + 244.f, 0.12f, 0.22f, 0.3f);
                }
                out.addText("Voltar", left + 548.f, top + 340.f, 0.07f, 0.22f, 0.36f);
            } else {
                for (int i = 0; i < 4; ++i) {
                    const float x = left + 34.f + static_cast<float>(i * 170);
                    if (i < static_cast<int>(names_.size())) out.addText(names_[i], x + 20.f, top + 78.f, 0.08f, 0.2f, 0.34f);
                    else out.addText("Vazio", x + 47.f, top + 78.f, 0.08f, 0.45f, 0.49f);
                }
                const char* actions[] = {"Criar", "Excluir", "Entrar", "Sair"};
                for (int i = 0; i < 4; ++i)
                    out.addText(actions[i], left + 80.f + 164.f * i, top + 340.f, 0.07f, 0.22f, 0.36f);
            }
    
}
} // namespace top::ui
