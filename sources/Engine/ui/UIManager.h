#pragma once
#include "screens/InventoryUI.h"
#include "screens/ForgeWorkbenchUI.h"
#include "screens/SkillsUI.h"
#include "screens/NpcDialogUI.h"
#include "screens/ShopUI.h"
#include "screens/BoatUI.h"
#include "screens/SystemUI.h"
#include "screens/CharacterSelectUI.h"
#include <string>
#include <vector>
#include <array>
namespace top::ui {
// Estado visual das janelas. O dono passa a ser UIManager nesta migracao.
// 04-D: posicionamento continua aqui; dados/interacoes sao dos paineis.
struct WindowState {
    bool showInv_ = false;
    bool showTemp_ = false;
    bool showState_ = false;
    bool showSkill_ = false;
    bool showQuest_ = false;
    float skillX_ = -1.f; // -1 = posicao inicial: encostada na direita
    float skillY_ = 64.f;
    float questX_ = 10.f;
    float questY_ = 64.f;
    bool skillDrag_ = false;
    bool questDrag_ = false;
    float skillGrabX_ = 0.f, skillGrabY_ = 0.f;
    float questGrabX_ = 0.f, questGrabY_ = 0.f;
    float invX_ = 214.f;
    float invY_ = 64.f;
    bool invDrag_ = false;
    float invGrabX_ = 0.f;
    float invGrabY_ = 0.f;
    float stateX_ = 8.f;
    float stateY_ = 64.f;
    bool stateDrag_ = false;
    float stateGrabX_ = 0.f;
    float stateGrabY_ = 0.f;
 };

// Dados somente de apresentacao; nao devem alterar as regras de jogo.
struct UIContentState {
    std::vector<std::string> statLines_;
    std::vector<std::string> questLines_;
    int jobMark_ = 0;
    bool questCanAbandon_ = false;
    std::vector<top::MapQuestView> mapQuests_;
    bool showMapQuests_ = false;
    int mapQuestPage_ = 0;
    int mapQuestSelected_ = -1;
};

class UIManager {
public:
    enum class Panel { Inventory, Status, Skills, Quests };
    // Return -1 outside, 0 content, 1 close button, 2 title bar.
    int windowHit(Panel panel, int x, int y, int screenWidth = 1) const;
    void beginDrag(Panel panel, int x, int y, int screenWidth = 1);
    void drag(Panel panel, int x, int y, bool down, int screenWidth, int screenHeight);
    void close(Panel panel);
    void toggle(int which);
    void resize(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    WindowState windows;
    UIContentState content;
    std::array<std::string, 8> shortcutIcons{};
    InventoryUI inventory;
    ForgeWorkbenchUI forge;
    SkillsUI skills;
    NpcDialogUI npc;
    ShopUI shop;
    BoatUI boat;
    SystemUI system;
    CharacterSelectUI characterSelect;
    // Ao abrir o menu de sistema, fecha dialogo e loja como no cliente 04-D.
    void toggleSystemModal();
private:
    int width_ = 1, height_ = 1;
};
} // namespace top::ui
