#pragma once
#include "ModalLayout.h"
#include "top/QuestPresentation.h"
#include <string>
#include <utility>
#include <vector>

namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
// Estado somente de apresentacao; a autoridade sobre o dialogo permanece em Play.
class NpcDialogUI {
public:
    void draw(class UIRenderer& r, int width, int height) const;
    void appendSprites(HudDrawList& out, int width, int height) const;
    void appendText(HudDrawList& out, int width, int height) const;
    void setWindow(bool shown, const std::string& name = {}, const std::string& text = {}, bool choices = false, bool trade = false, bool forge = false);
    void setTeleportChoices(std::vector<std::string> choices) { teleportChoices_ = std::move(choices); }
    void setDialogOptions(std::vector<std::string> options, bool tutorialMode, bool hasMissions) {
        if (dialogOptions_ != options) dialogOptionPage_ = 0;
        dialogOptions_ = std::move(options); tutorialMode_ = tutorialMode; tutorialHasMissions_ = hasMissions;
    }
    int dialogOptionHit(int x, int y, int w, int h) const;
    int dialogOptionPageHit(int x, int y, int w, int h) const;
    void changeDialogOptionPage(int delta);
    int tutorialGuideHit(int x, int y, int w, int h) const;
    int tutorialFooterHit(int x, int y, int w, int h) const;
    int teleportHit(int x, int y, int w, int h) const;
    // Grade compacta 1..4 de teleporte usada no main offline (4 colunas).
    // Retorna -1 fora, 0..3 dentro da area, sem validar numero de destinos.
    static int offlineTravelHit(int x, int y, int w, int h);
    void setMissions(std::vector<top::NpcQuestView> missions, int selected, int page) { missions_=std::move(missions);selected_=selected;page_=page; }
    bool hasMissions() const { return !missions_.empty(); }
    bool visible() const { return visible_; }
    void close() { visible_ = false; }
    bool contains(int x, int y, int w, int h) const { return visible_ && modal_layout::npc(w,h).contains(float(x),float(y)); }
    const std::string& name() const { return name_; }
    const std::string& text() const { return text_; }
    bool hasChoices() const { return choices_; }
    bool hasTrade() const { return trade_; }
    bool hasForge() const { return forge_; }
    const std::vector<std::string>& teleportChoices() const { return teleportChoices_; }
private:
    bool visible_ = false;
    bool choices_ = false;
    bool trade_ = false;
    bool forge_ = false;
    std::string name_;
    std::string text_;
    std::vector<std::string> teleportChoices_;
    std::vector<std::string> dialogOptions_;
    bool tutorialMode_ = false;
    bool tutorialHasMissions_ = false;
    int dialogOptionPage_ = 0;
    std::vector<top::NpcQuestView> missions_;
    int selected_=-1, page_=0;
};
} // namespace top::ui
