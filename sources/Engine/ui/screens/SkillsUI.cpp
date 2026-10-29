#include "../HudDrawList.h"
#include "InventoryLayout.h"
#include "SkillsUI.h"
#include "../UIRenderer.h"
#include "../UIManager.h"
#include <algorithm>
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

void SkillsUI::draw(UIRenderer& r,float x,float y,int page,const std::vector<SkillView>& skills,const UITheme& t,bool testMode){
    panel(r,x,y,198.f,394.f,t);
    if (testMode) {
        r.rect(x+2.f,y+2.f,194.f,23.f,{0.56f,0.79f,0.67f,0.98f});
        r.rect(x+4.f,y+27.f,190.f,1.f,{0.20f,0.47f,0.34f,0.98f});
        r.rect(x+7.f,y+335.f,184.f,1.f,{0.56f,0.79f,0.67f,0.96f});
    }
    for(int i=0;i<5;++i){
        const float yy=y+66.f+48.f*i;
        slot(r,x+15.f,yy+2.f);
        r.rect(x+53.f,yy+37.f,130.f,1.f,stripe);
        const int index=page*5+i;
        if (index>=0 && index<static_cast<int>(skills.size()) && skills[static_cast<std::size_t>(index)].upgradeable) {
            slot(r,x+165.f,yy+8.f,19.f,19.f);
            if (testMode) r.rect(x+166.f,yy+9.f,17.f,17.f,{0.62f,0.91f,0.69f,0.98f});
            r.rect(x+170.f,yy+17.f,9.f,2.f,ink);
            r.rect(x+173.5f,yy+13.5f,2.f,9.f,ink);
        }
    }
    r.rect(x+7.f,y+294.f,175.f,1.f,ink);
    slot(r,x+132.f,y+296.f,39.f,18.f);
    r.rect(x+7.f,y+335.f,184.f,1.f,stripe);
}
}

// 04-D: selecao e paginacao ficam na UI, nao no ActorSkin.
namespace top::ui {
void SkillsUI::setView(std::vector<SkillView> skills, int points, bool testMode) {
    // Entering/leaving the catalog always starts at its first page. Without
    // this, reopening test mode after browsing deep into the learned list can
    // make the catalog look empty or incomplete until the user pages back.
    if (testMode_ != testMode) page_ = 0;
    rows_ = std::move(skills);
    points_ = points;
    testMode_ = testMode;
    page_ = std::min(page_, rows_.empty() ? 0 : static_cast<int>((rows_.size() - 1) / 5));
}
int SkillsUI::hit(int x, int y, int screenW, const WindowState& windows) const {
    if (!windows.showSkill_) return -1;
    const float bx = windows.skillX_ < 0.f ? static_cast<float>(screenW - 206) : windows.skillX_;
    const float lx = static_cast<float>(x) - bx;
    const float ly = static_cast<float>(y) - windows.skillY_;
    if (lx < 7.f || lx >= 190.f || ly < 62.f || ly >= 62.f + 5.f * 48.f) return -1;
    const int row = static_cast<int>((ly - 62.f) / 48.f);
    const int index = page_ * 5 + row;
    return index >= 0 && index < static_cast<int>(rows_.size()) ? rows_[static_cast<std::size_t>(index)].id : -1;
}
int SkillsUI::upgradeHit(int x, int y, int screenW, const WindowState& windows) const {
    if (!windows.showSkill_) return -1;
    const float bx = windows.skillX_ < 0.f ? static_cast<float>(screenW - 206) : windows.skillX_;
    const float lx = static_cast<float>(x) - bx;
    const float ly = static_cast<float>(y) - windows.skillY_;
    if (lx < 165.f || lx >= 184.f || ly < 0.f) return -1;
    for (int row = 0; row < 5; ++row) {
        const float y0 = 74.f + static_cast<float>(row) * 48.f;
        if (ly < y0 || ly >= y0 + 19.f) continue;
        const int index = page_ * 5 + row;
        if (index < 0 || index >= static_cast<int>(rows_.size())) return -1;
        const SkillView& skill = rows_[static_cast<std::size_t>(index)];
        // In test mode the plus button is also the learn action: catalog rows
        // must remain actionable before they exist in the character's learned
        // skill set. Normal mode still requires the historical learn flow.
        return (skill.upgradeable && (testMode_ || skill.learned)) ? skill.id : -1;
    }
    return -1;
}
bool SkillsUI::testModeHit(int x, int y, int screenW, const WindowState& windows) const {
    if (!windows.showSkill_) return false;
    const float bx = windows.skillX_ < 0.f ? static_cast<float>(screenW - 206) : windows.skillX_;
    const float lx = static_cast<float>(x) - bx;
    const float ly = static_cast<float>(y) - windows.skillY_;
    // Keep the toggle below the title bar so the existing drag/close handling
    // continues to work unchanged.
    return lx >= 8.f && lx < 53.f && ly >= 356.f && ly < 384.f;
}
int SkillsUI::pageHit(int x, int y, int screenW, const WindowState& windows) const {
    if (!windows.showSkill_) return 0;
    const float bx = windows.skillX_ < 0.f ? static_cast<float>(screenW - 206) : windows.skillX_;
    const float lx = static_cast<float>(x) - bx;
    const float ly = static_cast<float>(y) - windows.skillY_;
    if (ly < 314.f || ly >= 338.f) return 0;
    if (lx >= 10.f && lx < 34.f) return -1;
    if (lx >= 164.f && lx < 188.f) return 1;
    return 0;
}
void SkillsUI::changePage(int delta) {
    const int maxPage = rows_.empty() ? 0 : static_cast<int>((rows_.size() - 1) / 5);
    page_ = std::clamp(page_ + delta, 0, maxPage);
}
bool SkillsUI::wheel(int x, int y, int screenW, int delta, const WindowState& windows) {
    if (!windows.showSkill_ || delta == 0) return false;
    const float bx = windows.skillX_ < 0.f ? static_cast<float>(screenW - 206) : windows.skillX_;
    if (x < bx || y < windows.skillY_ || x >= bx + 198.f || y >= windows.skillY_ + 394.f) return false;
    changePage(delta > 0 ? 1 : -1);
    return true;
}
} // namespace top::ui

namespace top::ui {
void SkillsUI::appendSprites(HudDrawList& out, const WindowState& windows, int screenW) const {
    if (!windows.showSkill_) return;
            const auto box = inventory_layout::PanelRect{windows.skillX_ < 0.f ? static_cast<float>(screenW - 206) : windows.skillX_, windows.skillY_, 198.f, 394.f};
            for (int i = 0; i < 5 && page() * 5 + i < static_cast<int>(rows().size()); ++i) {
                const SkillView& skill = rows()[static_cast<std::size_t>(page() * 5 + i)];
                if (!skill.icon.empty())
                    out.icons.push_back({skill.icon, box.x + 15.f, box.y + 68.f + static_cast<float>(i) * 48.f, 32.f, 32.f});
            }
    
}
void SkillsUI::appendText(HudDrawList& out, const WindowState& windows, int screenW, float lineHeight) const {
    if (!windows.showSkill_) return;
            const auto box = inventory_layout::PanelRect{windows.skillX_ < 0.f ? static_cast<float>(screenW - 206) : windows.skillX_, windows.skillY_, 198.f, 394.f};
            out.addText(testMode_ ? "CATALOGO TESTE (" + std::to_string(rows().size()) + ")" : "Skill",
                        box.x + 10.f, box.y + 4.f, 1.f, 0.96f, 0.8f);
            out.addText(testMode_ ? "TESTE ON" : "TESTE OFF", box.x + 138.f, box.y + 7.f,
                        testMode_ ? 0.05f : 0.12f, testMode_ ? 0.42f : 0.12f, 0.12f);
            for (int i = 0; i < 5 && page() * 5 + i < static_cast<int>(rows().size()); ++i) {
                const SkillView& skill = rows()[static_cast<std::size_t>(page() * 5 + i)];
                const float y = box.y + 70.f + static_cast<float>(i) * 48.f;
                const std::string key = skill.hotkey >= 0 ? "F" + std::to_string(skill.hotkey + 1) : "--";
                const std::string label = testMode_
                    ? "#" + std::to_string(skill.id) + " " + skill.name.substr(0, 10)
                    : skill.name.substr(0, 12);
                out.addText(key + " " + label, box.x + 53.f, y, 0.08f, 0.08f, 0.12f);
                std::string state;
                if (testMode_ && skill.level == 0) {
                    state = "Nao aprendida | + aprende";
                } else {
                    state = skill.professional
                        ? "Profissional Lv" + std::to_string(skill.level)
                        : "Lv" + std::to_string(skill.level) + " SP " + std::to_string(skill.sp);
                    if (!skill.learned && !testMode_) state += "  livro necessario";
                    else if (skill.cooldown) state += "  recarregando";
                }
                out.addText(state, box.x + 53.f, y + 16.f, skill.learned ? 0.12f : 0.45f, 0.12f, 0.12f);
            }
            out.addText(testMode_ ? "+ aprende/sobe gratis" : "Clique TESTE ou Alt+K", box.x + 10.f, box.y + 284.f, 0.16f, 0.23f, 0.24f);
            out.addText(testMode_ ? "TESTE: desligar" : "TESTE: ativar", box.x + 10.f, box.y + 362.f,
                        testMode_ ? 0.65f : 0.12f, testMode_ ? 0.08f : 0.34f, 0.18f);
            out.addText(testMode_ ? "Modo" : "Skill Pts", box.x + 69.f, box.y + 299.f, 0.12f, 0.12f, 0.12f);
            out.addText(testMode_ ? "Livre" : std::to_string(points()), box.x + 143.f, box.y + 299.f, 0.12f, 0.12f, 0.12f);
            const int pageCount = std::max(1, static_cast<int>((rows().size() + 4) / 5));
            out.addText(page() > 0 ? "<" : " ", box.x + 17.f, box.y + 318.f, 0.12f, 0.12f, 0.12f);
            out.addText("Pagina " + std::to_string(page() + 1) + "/" + std::to_string(pageCount),
                        box.x + 52.f, box.y + 318.f, 0.12f, 0.12f, 0.12f);
            out.addText(page() + 1 < pageCount ? ">" : " ", box.x + 172.f, box.y + 318.f, 0.12f, 0.12f, 0.12f);
            float effectY = box.y + 340.f;
            for (std::size_t i = 4; i < lines().size(); ++i) {
                out.addText(lines()[i], box.x + 55.f, effectY, 0.08f, 0.08f, 0.12f);
                effectY += lineHeight;
            }
    
}
}
