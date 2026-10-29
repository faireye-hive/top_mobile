#include "../HudDrawList.h"
#include "NpcDialogUI.h"
#include "../PanelChrome.h"
#include <algorithm>

namespace top::ui {
void NpcDialogUI::setWindow(bool shown, const std::string& name, const std::string& text, bool choices, bool trade, bool forge) {
    visible_ = shown;
    name_ = name;
    text_ = text;
    choices_ = choices;
    trade_ = shown && trade;
    forge_ = shown && forge;
}
int NpcDialogUI::teleportHit(int x, int y, int w, int h) const {
    if (!visible_ || teleportChoices_.empty()) return -1;
    // Mantem os indices e as areas de clique da interface 04-D.
    const int left = (w - 320) / 2;
    const int top = h - 316;
    for (int i = 0; i < std::min(4, static_cast<int>(teleportChoices_.size())); ++i) {
        const int bx = left + 20 + (i % 2) * 146;
        const int by = top + 220 + (i / 2) * 36;
        if (x >= bx && x < bx + 136 && y >= by && y < by + 30) return i;
    }
    return -1;
}
int NpcDialogUI::dialogOptionHit(int x, int y, int w, int h) const {
    if (!visible_ || !tutorialMode_ || dialogOptions_.empty()) return -1;
    const auto b = modal_layout::npc(w, h);
    if (x < b.x + 18.f || x >= b.x + 302.f || y < b.y + 105.f || y >= b.y + 232.f) return -1;
    const int row = static_cast<int>((y - (b.y + 105.f)) / 25.f);
    const int index=dialogOptionPage_*5+row;
    return row >= 0 && row < 5 && index < static_cast<int>(dialogOptions_.size()) ? index : -1;
}
int NpcDialogUI::dialogOptionPageHit(int x, int y, int w, int h) const {
    if (!visible_ || !tutorialMode_ || dialogOptions_.size() <= 5) return 0;
    const auto b = modal_layout::npc(w, h);
    if (y < b.y + 235.f || y >= b.y + 260.f) return 0;
    if (x >= b.x + 18.f && x < b.x + 84.f && dialogOptionPage_ > 0) return -1;
    const int maxPage=static_cast<int>((dialogOptions_.size()-1)/5);
    if (x >= b.x + 236.f && x < b.x + 302.f && dialogOptionPage_ < maxPage) return 1;
    return 0;
}
void NpcDialogUI::changeDialogOptionPage(int delta) {
    const int maxPage=dialogOptions_.empty()?0:static_cast<int>((dialogOptions_.size()-1)/5);
    dialogOptionPage_=std::clamp(dialogOptionPage_+delta,0,maxPage);
}
int NpcDialogUI::tutorialGuideHit(int x, int y, int w, int h) const {
    if (!visible_ || tutorialMode_ || selected_ >= 0 || !tutorialHasMissions_ || dialogOptions_.empty()) return -1;
    const auto b = modal_layout::npc(w, h);
    return x >= b.x + 210.f && x < b.x + 302.f && y >= b.y + 33.f && y < b.y + 59.f ? 1 : -1;
}
int NpcDialogUI::tutorialFooterHit(int x, int y, int w, int h) const {
    if (!visible_ || !tutorialMode_) return -1;
    const auto b = modal_layout::npc(w, h);
    if (y < b.y + 261.f || y >= b.y + 291.f) return -1;
    if (tutorialHasMissions_) {
        if (x >= b.x + 18.f && x < b.x + 147.f) return 1;
        if (x >= b.x + 159.f && x < b.x + 301.f) return 0;
    } else if (x >= b.x + 105.f && x < b.x + 214.f) return 0;
    return -1;
}
int NpcDialogUI::offlineTravelHit(int x, int y, int w, int h) {
    const int left = (w - 320) / 2;
    const int top = h - 316;
    if (x < left + 18 || x >= left + 302 || y < top + 224 || y >= top + 304) return -1;
    const int column = (x - left - 18) / 71;
    return std::min(3, std::max(0, column));
}
} // namespace top::ui

namespace top::ui {

void NpcDialogUI::draw(UIRenderer& r,int width,int height) const {
    if (!visible()) return;
    const auto b = modal_layout::npc(width,height);
    chrome::panel(r,b.x,b.y,b.w,b.h);
    if (tutorialMode_) {
        r.border(b.x+14.f,b.y+63.f,b.w-28.f,172.f,1.f,chrome::muted);
        for (int i=0;i<5;++i) {
            const int index=dialogOptionPage_*5+i;
            if(index>=static_cast<int>(dialogOptions_.size()))break;
            const float y=b.y+105.f+float(i*25);
            r.rect(b.x+18.f,y,284.f,22.f,chrome::shade);
            r.border(b.x+18.f,y,284.f,22.f,1.f,chrome::muted);
        }
        if(dialogOptions_.size()>5) {
            if(dialogOptionPage_>0)chrome::button(r,b.x+18.f,b.y+235.f,66.f,23.f);
            if(dialogOptionPage_<static_cast<int>((dialogOptions_.size()-1)/5))
                chrome::button(r,b.x+236.f,b.y+235.f,66.f,23.f);
        }
        if (tutorialHasMissions_) {
            chrome::button(r,b.x+18.f,b.y+261.f,129.f,29.f);
            chrome::button(r,b.x+159.f,b.y+261.f,142.f,29.f);
        } else {
            // Dialogos historicos de NPCs podem coexistir com a loja. O
            // retorno antecipado deste painel escondia o botao Trade.
            if (hasTrade()) chrome::button(r,b.x+18.f,b.y+261.f,82.f,29.f);
            chrome::button(r,b.x+105.f,b.y+261.f,109.f,29.f);
        }
        return;
    }
    if (hasMissions()) {
        r.border(b.x+14.f,b.y+63.f,b.w-28.f,172.f,1.f,chrome::muted);
        if (selected_ < 0) {
            for(int i=0;i<top::npc_quest_layout::kRows;++i) {
                int index=page_*top::npc_quest_layout::kRows+i;
                if(index>=int(missions_.size())) break;
                const auto& m=missions_[static_cast<std::size_t>(index)];
                const float ry=b.y+float(top::npc_quest_layout::kFirstRowY+i*top::npc_quest_layout::kRowPitch);
                r.rect(b.x+18.f,ry,283.f,25.f,(m.status==top::NpcQuestView::Available||m.status==top::NpcQuestView::Ready)?chrome::selected:chrome::shade);
                r.border(b.x+18.f,ry,283.f,25.f,1.f,chrome::muted);
            }
            if(hasTrade())chrome::button(r,b.x+18.f,b.y+233.f,82.f,25.f);
            if(hasForge())chrome::button(r,b.x+112.f,b.y+233.f,82.f,25.f);
            chrome::button(r,b.x+18.f,b.y+261.f,77.f,29.f,page_>0);
            chrome::button(r,b.x+105.f,b.y+261.f,77.f,29.f,(page_+1)*top::npc_quest_layout::kRows<int(missions_.size()));
            chrome::button(r,b.x+202.f,b.y+261.f,99.f,29.f);
            if (tutorialHasMissions_ && !dialogOptions_.empty()) chrome::button(r,b.x+210.f,b.y+33.f,92.f,24.f);
        } else {
            r.rect(b.x+18.f,b.y+94.f,283.f,20.f,chrome::shade);
            chrome::button(r,b.x+18.f,b.y+261.f,129.f,29.f,
                           selected_<int(missions_.size()) && (missions_[static_cast<std::size_t>(selected_)].status==top::NpcQuestView::Available ||
                            missions_[static_cast<std::size_t>(selected_)].status==top::NpcQuestView::Ready));
            chrome::button(r,b.x+159.f,b.y+261.f,142.f,29.f);
        }
        return;
    }
    r.border(b.x+16.f,b.y+64.f,b.w-32.f,171.f,1.f,chrome::muted);
    if (hasTrade()) chrome::button(r,b.x+18.f,b.y+261.f,82.f,29.f);
    if (hasForge()) {
        if (hasChoices()) chrome::button(r,b.x+18.f,b.y+232.f,82.f,27.f);
        else chrome::button(r,b.x+(hasTrade()?108.f:18.f),b.y+261.f,82.f,29.f);
    }
    if (hasChoices()) {
        chrome::button(r,b.x+108.f,b.y+261.f,82.f,29.f);
        chrome::button(r,b.x+202.f,b.y+261.f,82.f,29.f);
    } else if (!teleportChoices_.empty()) {
        for (int i = 0; i < std::min(4, static_cast<int>(teleportChoices_.size())); ++i) {
            const float bx = b.x + 20.f + (i % 2) * 146.f;
            const float by = b.y + 220.f + (i / 2) * 36.f;
            chrome::button(r, bx, by, 136.f, 30.f, true);
        }
    } else if (hasTrade() && hasForge()) {
        chrome::button(r,b.x+202.f,b.y+261.f,100.f,29.f);
    } else if (hasTrade() || hasForge()) {
        chrome::button(r,b.x+146.f,b.y+261.f,138.f,29.f);
    } else chrome::button(r,b.x+140.f,b.y+260.f,144.f,30.f);
}
void NpcDialogUI::appendSprites(HudDrawList& out, int width, int height) const {
    // Fundos da interface migrados para draw() (C++ puro).
    (void)out; (void)width; (void)height;
}
void NpcDialogUI::appendText(HudDrawList& out, int width, int height) const {
    if (!visible()) return;
            const float left = ui::modal_layout::npc(width, height).x;
            const float top = ui::modal_layout::npc(width, height).y;
            if (tutorialMode_) {
                out.addText(name().substr(0,32),left+24.f,top+38.f,.08f,.18f,.32f);
                std::size_t textAt=0;
                for(int line=0;line<2 && textAt<text().size();++line) {
                    std::size_t end=std::min(textAt+43,text().size());
                    if(end<text().size()) {
                        const std::size_t space=text().rfind(' ',end);
                        if(space!=std::string::npos && space>textAt)end=space;
                    }
                    out.addText(text().substr(textAt,end-textAt),left+22.f,top+70.f+float(line*17),.08f,.12f,.2f);
                    textAt=end;
                    while(textAt<text().size() && text()[textAt]==' ')++textAt;
                }
                for(int i=0;i<5;++i) {
                    const int index=dialogOptionPage_*5+i;
                    if(index>=static_cast<int>(dialogOptions_.size()))break;
                    std::string label=dialogOptions_[static_cast<std::size_t>(index)];
                    if(label.size()>42)label.resize(42);
                    out.addText(label,left+25.f,top+110.f+float(i*25),.08f,.20f,.28f);
                }
                if(dialogOptions_.size()>5) {
                    const int count=static_cast<int>((dialogOptions_.size()+4)/5);
                    out.addText(dialogOptionPage_>0?"< Anterior":" ",left+28.f,top+240.f,.12f,.24f,.28f);
                    out.addText("Pagina "+std::to_string(dialogOptionPage_+1)+"/"+std::to_string(count),
                                left+130.f,top+240.f,.12f,.24f,.28f);
                    out.addText(dialogOptionPage_+1<count?"Proxima >":" ",left+246.f,top+240.f,.12f,.24f,.28f);
                }
                if(tutorialHasMissions_) {
                    out.addText("Missoes",left+54.f,top+268.f,.12f,.24f,.28f);
                    out.addText("Fechar",left+208.f,top+268.f,.12f,.24f,.28f);
                } else {
                    if (hasTrade()) out.addText("Trade",left+36.f,top+268.f,.07f,.25f,.16f);
                    out.addText("Fechar",left+140.f,top+268.f,.12f,.24f,.28f);
                }
                return;
            }
            if(hasMissions()) {
                out.addText(name().substr(0,32),left+24.f,top+38.f,.08f,.18f,.32f);
                if(selected_<0) {
                    out.addText("Missoes oferecidas",left+22.f,top+69.f,.13f,.29f,.32f);
                    if(tutorialHasMissions_ && !dialogOptions_.empty())out.addText("Guia",left+238.f,top+39.f,.08f,.20f,.30f);
                    for(int i=0;i<top::npc_quest_layout::kRows;++i) {
                        const int index=page_*top::npc_quest_layout::kRows+i;
                        if(index>=int(missions_.size())) break;
                        const auto& m=missions_[static_cast<std::size_t>(index)];
                        const char* tag=m.status==top::NpcQuestView::Ready?"?":m.status==top::NpcQuestView::Available?"!":m.status==top::NpcQuestView::Active?"?":m.status==top::NpcQuestView::Complete?"*":"-";
                        out.addText(std::string(tag)+" "+m.title.substr(0,31),left+26.f,top+110.f+float(i*29),
                                    m.status==top::NpcQuestView::Ready?.66f:m.status==top::NpcQuestView::Available?.04f:.36f,
                                    m.status==top::NpcQuestView::Ready?.48f:m.status==top::NpcQuestView::Available?.37f:.35f,.16f);
                    }
                    if(hasTrade())out.addText("Trade",left+32.f,top+238.f,.12f,.24f,.28f);
                    if(hasForge())out.addText("Forge",left+126.f,top+238.f,.12f,.24f,.28f);
                    out.addText("< Anterior",left+24.f,top+268.f,.12f,.24f,.28f);
                    out.addText("Proxima >",left+109.f,top+268.f,.12f,.24f,.28f);
                    out.addText("Fechar",left+223.f,top+268.f,.12f,.24f,.28f);
                } else if(selected_<int(missions_.size())) {
                    const auto& m=missions_[static_cast<std::size_t>(selected_)];
                    out.addText(m.title.substr(0,34),left+22.f,top+69.f,.12f,.23f,.30f);
                    std::string status=m.status==top::NpcQuestView::Ready?"Pronta para entregar":m.status==top::NpcQuestView::Available?"Disponivel":m.status==top::NpcQuestView::Active?"Em andamento":m.status==top::NpcQuestView::Complete?"Concluida":"Indisponivel";
                    out.addText(status,left+24.f,top+98.f,.12f,.32f,.15f);
                    // Quebra simples sem sobreposicao: conteudo textual fornecido pelo Play.
                    std::string descr=m.description.empty()?m.objective:m.description;
                    size_t pos=0;int lines=0;
                    while(pos<descr.size() && lines<3) {
                        size_t end=std::min(pos+40,descr.size());
                        if(end<descr.size()) { const size_t space=descr.rfind(' ',end); if(space!=std::string::npos&&space>pos)end=space; }
                        if(end<=pos)break;
                        out.addText(descr.substr(pos,end-pos),left+22.f,top+126.f+lines*18.f,.16f,.18f,.19f);
                        pos=end;while(pos<descr.size()&&descr[pos]==' ')++pos;++lines;
                    }
                    out.addText("Objetivo: "+m.objective.substr(0,31),left+22.f,top+189.f,.25f,.14f,.15f);
                    if(!m.requirements.empty())out.addText(m.requirements.front().substr(0,38),left+22.f,top+208.f,.35f,.13f,.14f);
                    out.addText("EXP "+std::to_string(m.rewardExp)+"   Ouro "+std::to_string(m.rewardGold),left+22.f,top+235.f,.12f,.27f,.18f);
                    out.addText(m.status==top::NpcQuestView::Ready?"Completar":m.status==top::NpcQuestView::Available?"Aceitar":m.status==top::NpcQuestView::Active?"Pendente":"Bloqueada",left+44.f,top+268.f,.1f,.31f,.2f);
                    out.addText("Voltar",left+198.f,top+268.f,.12f,.24f,.28f);
                }
                return;
            }
            out.addText(name(), left + 70.f, top + 46.f, 0.08f, 0.18f, 0.32f);
            std::size_t at = 0;
            int row = 0;
            while (at < text().size() && row < (!teleportChoices_.empty() ? 6 : (hasForge() && hasChoices() ? 7 : 8))) {
                std::size_t end = std::min(at + 40, text().size());
                if (end < text().size()) {
                    const std::size_t space = text().rfind(' ', end);
                    if (space != std::string::npos && space > at) end = space;
                }
                out.addText(text().substr(at, end - at), left + 28.f, top + 74.f + static_cast<float>(row * 22),
                     0.08f, 0.12f, 0.2f);
                at = end;
                while (at < text().size() && text()[at] == ' ') ++at;
                ++row;
            }
            if (hasTrade()) out.addText("Trade", left + 36.f, top + 268.f, 0.07f, 0.25f, 0.16f);
            if (hasForge()) out.addText("Forge",left+(hasChoices()?34.f:hasTrade()?120.f:32.f),
                 top+(hasChoices()?237.f:268.f),.07f,.25f,.16f);
            if (hasChoices()) {
                out.addText("Aceitar", left + 118.f, top + 268.f, 0.05f, 0.32f, 0.12f);
                out.addText("Recusar", left + 210.f, top + 268.f, 0.42f, 0.1f, 0.08f);
            } else if (!teleportChoices_.empty()) {
                out.addText("Escolha o destino (clique):", left + 28.f, top + 207.f, 0.04f, 0.24f, 0.34f);
                for (int i = 0; i < std::min(4, static_cast<int>(teleportChoices_.size())); ++i) {
                    const float bx = left + 27.f + (i % 2) * 146.f;
                    const float by = top + 228.f + (i / 2) * 36.f;
                    std::string label = teleportChoices_[static_cast<std::size_t>(i)];
                    if (label.size() > 17) label.resize(17);
                    out.addText(label, bx, by, 0.04f, 0.24f, 0.34f);
                }
            } else {
                out.addText(hasTrade() || hasForge() ? "Close" : "Clique para fechar",
                   left + (hasTrade() && hasForge()?225.f:162.f), top + 268.f, 0.12f, 0.28f, 0.4f);
            }
    
}
} // namespace top::ui
