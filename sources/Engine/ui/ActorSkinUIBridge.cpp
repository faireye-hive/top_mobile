// MIGRACAO 03: implementacoes de interface fora do ActorSkin.cpp.
// API publica ActorSkin mantida como adaptador temporario para Play.cpp.
#include "top/ActorSkin.h"
#include "top/Image.h"
#include "top/ModelFile.h"
#include "top/SceneWorld.h"
#include "top/MapQuestLayout.h"
#include "screens/GameHUD.h"
#include "screens/MinimapUI.h"
#include "screens/CharacterStatusUI.h"
#include "screens/QuestUI.h"
#include "HudDrawList.h"
#include "HudComposer.h"
#include "PanelChrome.h"
#include "screens/InventoryLayout.h"
#include "screens/ModalLayout.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include <utility>
namespace top {
namespace {
// SHOWRSIZE do cliente antigo. A janela do radar tem 128 px e mostra 40 m.
// A camera do minimapa olha para baixo com up (0, -1, 0): +X vai para a
// direita da tela e +Y desce. O norte desenhado no quadro fica em -Y.
constexpr float kShowR = 40.f;
constexpr int kRadar = 128;

struct ScreenBox {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
};

ScreenBox funBarBox(int screenW, int screenH) {
    return {static_cast<float>(screenW) - 215.f, static_cast<float>(screenH) - 30.f, 215.f, 30.f};
}

// Barra de atalhos de texture/ui/MUI/em-uso/skill/new4.tga. A textura so tem F1 a F8: a faixa
// de cima traz F1-F7 e metade do F8, e a ponta do F8 esta na faixa de baixo.
// Desenhada: 256 + 39 = 295 px de largura. As caixas tem 32 px e ficam a
// cada 34 px, a primeira em x=5 (medido na textura).
constexpr float kKeyBarW = 295.f;
constexpr float kKeyBarH = 44.f;
constexpr float kKeyFirst = 5.f;
constexpr float kKeyPitch = 34.f;
constexpr float kKeyCell = 32.f;
constexpr int kKeySlots = 8;

ScreenBox skillBarBox(int screenW, int screenH) {
    // floor: 295 e impar, e meio pixel deixaria a textura borrada.
    return {std::floor((static_cast<float>(screenW) - kKeyBarW) * 0.5f),
            static_cast<float>(screenH) - kKeyBarH - 6.f, kKeyBarW, kKeyBarH};
}

struct FunIcon {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
    float cropX = 0.f;
    float cropY = 0.f;
    int which = -1;
};

void funIcons(FunIcon* out, int& count) {
    // frmMainFun em main.clu. which segue togglePanel: 0 bolsa, 1 status, 2 skill, 3 missao.
    const FunIcon icons[] = {
        {7.f, 4.f, 23.f, 22.f, 0.f, 0.f, 1},
        {31.f, 4.f, 23.f, 22.f, 0.f, 22.f, 0},
        {55.f, 4.f, 23.f, 22.f, 0.f, 44.f, 2},
        {79.f, 4.f, 23.f, 22.f, 0.f, 66.f, 3},
        {103.f, 4.f, 23.f, 22.f, 0.f, 88.f, -1},
        {127.f, 4.f, 23.f, 22.f, 0.f, 110.f, -1},
        {151.f, 4.f, 23.f, 22.f, 0.f, 132.f, -1},
        {175.f, 4.f, 37.f, 22.f, 0.f, 154.f, -1},
    };
    count = 8;
    for (int i = 0; i < count; ++i) out[i] = icons[i];
}

void radarPixel(float playerX, float playerY, float worldX, float worldY, int& px, int& py) {
    const float ppm = static_cast<float>(kRadar) / kShowR;
    px = static_cast<int>(std::floor((worldX - playerX) * ppm + static_cast<float>(kRadar) * 0.5f));
    py = static_cast<int>(std::floor((worldY - playerY) * ppm + static_cast<float>(kRadar) * 0.5f));
}



// Geometria da bolsa unificada com os eventos em InventoryUI.
using ui::inventory_layout::PanelRect;
using ui::inventory_layout::invRectAt;
using ui::inventory_layout::tempRectAt;
using ui::inventory_layout::kIcon;
PanelRect stateRect(float x, float y) { return {x, y, 198.f, 394.f}; }
PanelRect skillRect(float x, float y, int screenW) {
    return {x < 0.f ? static_cast<float>(screenW - 206) : x, y, 198.f, 394.f};
}

// Funcoes binarias usadas pelo minimapa, locais a UI.
bool readFile(const std::string& path, std::vector<std::uint8_t>& out, std::size_t limit) {
    FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return false;
    if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return false; }
    long n = std::ftell(f); if (n <= 0 || static_cast<std::size_t>(n) > limit) { std::fclose(f); return false; }
    out.resize(static_cast<std::size_t>(n)); std::rewind(f);
    bool ok = std::fread(out.data(), 1, out.size(), f) == out.size(); std::fclose(f); return ok;
}
std::uint32_t ru32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}


} // namespace
void ActorSkin::setNotes(std::vector<HudNote> notes) { notes_ = std::move(notes); }
void ActorSkin::setStatusLines(std::vector<std::string> lines, int jobMark) {
    ui_.content.statLines_ = std::move(lines);
    ui_.content.jobMark_ = jobMark;
}

// 04-E: ActorSkin mantem apenas adaptadores ABI; estado e hit-test estao no UIManager.
void ActorSkin::setNpcWindow(bool visible, const std::string& name, const std::string& text, bool choices, bool trade, bool forge) {
    ui_.npc.setWindow(visible, name, text, choices, trade, forge);
}
void ActorSkin::setNpcQuestViews(std::vector<NpcQuestView> missions,int selected,int page) {
    ui_.npc.setMissions(std::move(missions),selected,page);
}
void ActorSkin::setQuestCanAbandon(bool active) { ui_.content.questCanAbandon_=active; }
// 05-AO: localizador visual (somente ofertas elegiveis). O Play e a autoridade.
void ActorSkin::setMapQuestList(std::vector<MapQuestView> quests) {
    auto& c=ui_.content;
    c.mapQuests_=std::move(quests);
    c.mapQuestPage_=std::clamp(c.mapQuestPage_,0,
        std::max(0,(int(c.mapQuests_.size())-1)/7));
    if(c.mapQuestSelected_>=int(c.mapQuests_.size()))c.mapQuestSelected_=-1;
}
int ActorSkin::mapQuestHit(int x,int y,int sw,int sh) const {
    const auto& c=ui_.content;
    return map_quest_layout::hit(x,y,sw,sh,c.showMapQuests_,c.mapQuestPage_,int(c.mapQuests_.size()));
}
void ActorSkin::mapQuestAction(int action) {
    auto& c=ui_.content;
    if(action==0){c.showMapQuests_=!c.showMapQuests_;return;}
    if(action==1){c.showMapQuests_=false;return;}
    if(action==20){c.mapQuestPage_=std::max(0,c.mapQuestPage_-1);return;}
    if(action==21){c.mapQuestPage_=std::min(std::max(0,(int(c.mapQuests_.size())-1)/7),c.mapQuestPage_+1);return;}
    if(action>=10&&action<=16) {
        const int index=c.mapQuestPage_*7+action-10;
        if(index>=0&&index<int(c.mapQuests_.size()))c.mapQuestSelected_=index;
    }
}

int ActorSkin::questListActionAt(int x,int y) const {
    const auto& w=ui_.windows;
    if(!w.showQuest_ || x<w.questX_+8 || x>=w.questX_+227)return -1;
    const int rx=x-w.questX_,ry=y-w.questY_;
    if(ry>=50 && ry<130) return (ry-50)/16+1;
    if(ry>=134 && ry<158 && rx>=12 && rx<101)return 20;
    if(ry>=134 && ry<158 && rx>=126 && rx<221)return 21;
    return -1;
}
bool ActorSkin::questAbandonHit(int x,int y) const {
    const auto& w=ui_.windows;
    return w.showQuest_ && ui_.content.questCanAbandon_ &&
           x>=w.questX_+120 && x<w.questX_+224 && y>=w.questY_+365 && y<w.questY_+389;
}
void ActorSkin::setTeleportChoices(std::vector<std::string> choices) {
    ui_.npc.setTeleportChoices(std::move(choices));
}
void ActorSkin::setNpcDialogOptions(std::vector<std::string> options, bool tutorialMode, bool hasMissions) {
    ui_.npc.setDialogOptions(std::move(options), tutorialMode, hasMissions);
}
int ActorSkin::npcDialogOptionHit(int x, int y, int sw, int sh) const {
    return ui_.npc.dialogOptionHit(x, y, sw, sh);
}
int ActorSkin::npcDialogOptionPageHit(int x, int y, int sw, int sh) const {
    return ui_.npc.dialogOptionPageHit(x, y, sw, sh);
}
void ActorSkin::changeNpcDialogOptionPage(int delta) {
    ui_.npc.changeDialogOptionPage(delta);
}
int ActorSkin::npcTutorialGuideHit(int x, int y, int sw, int sh) const {
    return ui_.npc.tutorialGuideHit(x, y, sw, sh);
}
int ActorSkin::npcTutorialFooterHit(int x, int y, int sw, int sh) const {
    return ui_.npc.tutorialFooterHit(x, y, sw, sh);
}
int ActorSkin::teleportHit(int x, int y, int screenW, int screenH) const {
    return ui_.npc.teleportHit(x, y, screenW, screenH);
}
int ActorSkin::offlineTravelHit(int x, int y, int screenW, int screenH) const {
    return ui::NpcDialogUI::offlineTravelHit(x, y, screenW, screenH);
}
void ActorSkin::setShopWindow(bool visible, const std::string& name, std::vector<std::string> lines, int first,
                              int total, int gold, std::vector<std::string> icons, std::vector<std::string> names) {
    ui_.shop.setWindow(visible, name, std::move(lines), first, total, gold, std::move(icons), std::move(names));
}
int ActorSkin::shopWindowHit(int x,int y,int width) const { return ui_.shop.hit(x,y,width); }
bool ActorSkin::shopContains(int x, int y, int screenW) const {
    return ui_.shop.contains(x,y,screenW);
}
void ActorSkin::beginShopDrag(int x,int y,int width) { ui_.shop.beginDrag(x,y,width); }
void ActorSkin::dragShop(int x,int y,bool down,int width,int height) { ui_.shop.drag(x,y,down,width,height); }
void ActorSkin::setCharacterWindow(bool visible) { ui_.characterSelect.setVisible(visible); }
void ActorSkin::setCharacterRoster(std::vector<std::string> names, int selected) {
    ui_.characterSelect.setRoster(std::move(names), selected);
}
void ActorSkin::beginCharacterCreation() { ui_.characterSelect.beginCreate(); }
void ActorSkin::beginCharacterDeletion() { ui_.characterSelect.beginDelete(); }
void ActorSkin::chooseCharacterRace(int raceId) { ui_.characterSelect.chooseRace(raceId); }
void ActorSkin::cancelCharacterCreation() { ui_.characterSelect.cancelCreate(); }
int ActorSkin::characterCreateRace() const { return ui_.characterSelect.createRace(); }
int ActorSkin::characterCreationStage() const { return ui_.characterSelect.createStage(); }
std::string ActorSkin::characterCreateName() const { return ui_.characterSelect.createName(); }
void ActorSkin::editCharacterCreateName(const std::string& text, bool backspace) {
    ui_.characterSelect.editCreateName(text, backspace);
}
void ActorSkin::toggleSystemWindow() { ui_.toggleSystemModal(); }
void ActorSkin::closeSystemWindow() { ui_.system.close(); }
int ActorSkin::systemHit(int x, int y, int screenW, int screenH) const {
    return ui_.system.hit(x, y, screenW, screenH);
}
int ActorSkin::characterHit(int x, int y, int screenW, int screenH) const {
    return ui_.characterSelect.hit(x, y, screenW, screenH);
}

void ActorSkin::setBag(std::vector<std::string> lines) { ui_.inventory.setLines(std::move(lines)); }

void ActorSkin::toggleBoatWindow() { ui_.boat.toggle(); }
void ActorSkin::closeBoatWindow() { ui_.boat.close(); }
int ActorSkin::boatHit(int x, int y, int screenW, int screenH) const {
    return ui_.boat.hit(x, y, screenW, screenH);
}

void ActorSkin::setSkills(std::vector<std::string> lines) { ui_.skills.setLines(std::move(lines)); }
void ActorSkin::setShortcutIcons(std::array<std::string, 8> icons) {
    ui_.shortcutIcons = std::move(icons);
}
void ActorSkin::showShopInventory(int width) {
    ui_.windows.showInv_ = true;
    const auto shop = ui_.shop.bounds(width);
    // 6 px between the two movable panels, always inside the screen when possible.
    ui_.windows.invX_ = std::max(0.f, shop.x - 205.f);
    ui_.windows.invY_ = shop.y;
}
void ActorSkin::setSkillView(std::vector<SkillView> skills, int points, bool testMode) {
    ui_.skills.setView(std::move(skills), points, testMode);
}
void ActorSkin::setSkillDragIcon(const std::string& icon, int x, int y, bool active) {
    skillIconDragging_ = active && !icon.empty();
    skillDragIcon_ = active ? icon : std::string{};
    skillDragX_ = x;
    skillDragY_ = y;
}
void ActorSkin::toggleForgeWorkbench(int screenW,int screenH) {
    if (ui_.forge.visible()) { ui_.forge.close(); return; }
    openForgeWorkbench(screenW,screenH,7u);
}
void ActorSkin::toggleProductionWorkbench(int screenW,int screenH) {
    if (ui_.forge.visible()) { ui_.forge.close(); return; }
    openForgeWorkbench(screenW,screenH,8u);
}
void ActorSkin::openForgeWorkbench(int screenW,int screenH,unsigned allowedModes) {
    ui_.forge.open(allowedModes);
    if (ui_.forge.visible()) {
        ui_.windows.showInv_=true;
        const auto b=ui::ForgeWorkbenchUI::bounds(screenW,screenH);
        ui_.windows.invX_=std::max(2.f,b.x-206.f);
        ui_.windows.invY_=b.y;
    }
}
int ActorSkin::forgeWorkbenchHit(int x,int y,int w,int h) const {
    return ui_.forge.hit(x,y,w,h);
}
int ActorSkin::forgeWorkbenchDropSlot(int x,int y,int w,int h) const {
    return ui_.forge.dropSlot(x,y,w,h);
}
bool ActorSkin::forgeWorkbenchSelect(int inventoryHit,int targetSlot) {
    if(!ui_.forge.visible())return false;
    const ui::BagView* row=nullptr;
    if(inventoryHit>=200&&inventoryHit<300) {
        const int idx=inventoryHit-200;
        for(const auto& item:ui_.inventory.rows())if(item.index==idx){row=&item;break;}
    }else if(inventoryHit>=100&&inventoryHit<200)
        row=ui_.inventory.wornRow(inventoryHit-100);
    if(!row)return false;
    return targetSlot>=0 ? ui_.forge.select(targetSlot,*row) : ui_.forge.selectNext(*row);
}
void ActorSkin::forgeWorkbenchClearSlot(int slot){ui_.forge.clearSlot(slot);}
void ActorSkin::forgeWorkbenchMode(int mode){
    if(mode>=0&&mode<=7)ui_.forge.setMode(ui::ForgeWorkbenchUI::Mode(mode));
}
void ActorSkin::forgeWorkbenchReset(){ui_.forge.clear();}
void ActorSkin::forgeWorkbenchPreview(bool valid,int price,int chance,const std::string& text){
    ui_.forge.setPreview(valid,price,chance,text);
}
void ActorSkin::setItemConfirm(const std::string& text) { itemConfirmText_ = text; }
int ActorSkin::skillUpgradeHit(int x, int y, int screenW) const {
    return ui_.skills.upgradeHit(x, y, screenW, ui_.windows);
}
bool ActorSkin::skillTestModeHit(int x, int y, int screenW) const {
    return ui_.skills.testModeHit(x, y, screenW, ui_.windows);
}
int ActorSkin::skillPageHit(int x, int y, int screenW) const {
    return ui_.skills.pageHit(x, y, screenW, ui_.windows);
}
void ActorSkin::changeSkillPage(int delta) {
    ui_.skills.changePage(delta);
}
int ActorSkin::skillHit(int x, int y, int screenW) const {
    return ui_.skills.hit(x, y, screenW, ui_.windows);
}
bool ActorSkin::skillWheel(int x, int y, int screenW, int delta) {
    return ui_.skills.wheel(x, y, screenW, delta, ui_.windows);
}

void ActorSkin::setQuests(std::vector<std::string> lines) { ui_.content.questLines_ = std::move(lines); }

int ActorSkin::skillWindowHit(int x, int y, int screenW) const {
    return ui_.windowHit(ui::UIManager::Panel::Skills, x, y, screenW);
}
int ActorSkin::questWindowHit(int x, int y) const {
    return ui_.windowHit(ui::UIManager::Panel::Quests, x, y);
}
void ActorSkin::beginSkillDrag(int x, int y, int screenW) {
    ui_.beginDrag(ui::UIManager::Panel::Skills, x, y, screenW);
}
void ActorSkin::dragSkill(int x, int y, bool down, int w, int h) {
    ui_.drag(ui::UIManager::Panel::Skills, x, y, down, w, h);
}
void ActorSkin::beginQuestDrag(int x, int y) {
    ui_.beginDrag(ui::UIManager::Panel::Quests, x, y);
}
void ActorSkin::dragQuest(int x, int y, bool down, int w, int h) {
    ui_.drag(ui::UIManager::Panel::Quests, x, y, down, w, h);
}

void ActorSkin::setLiveLine(int index, const std::string& text) {
    if (index < 0 || index >= static_cast<int>(ui_.content.statLines_.size())) return;
    ui_.content.statLines_[static_cast<std::size_t>(index)] = text;
}

void ActorSkin::togglePanel(int which) {
    ui_.toggle(which);
}

void ActorSkin::showSkillPanel() {
    ui_.windows.showSkill_=true;
    ui_.windows.skillDrag_=false;
}

void ActorSkin::hideSkillPanel() {
    ui_.windows.showSkill_=false;
    ui_.windows.skillDrag_=false;
}

bool ActorSkin::uiBlocks(int x, int y, int screenW, int screenH) const {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    auto inside = [&](PanelRect box) {
        return fx >= box.x && fy >= box.y && fx < box.x + box.w && fy < box.y + box.h;
    };
    if (fx >= 0.f && fy >= 0.f && fx < 200.f && fy < 60.f) return true;
    const ScreenBox bar = funBarBox(screenW, screenH);
    if (fx >= bar.x && fy >= bar.y && fx < bar.x + bar.w && fy < bar.y + bar.h) return true;
    const ScreenBox skills = skillBarBox(screenW, screenH);
    if (fx >= skills.x && fy >= skills.y && fx < skills.x + skills.w && fy < skills.y + skills.h) return true;
    if (ui::MinimapUI::contains(x, y, screenW)) return true;
    if (ui_.windows.showState_ && inside(stateRect(ui_.windows.stateX_, ui_.windows.stateY_))) return true;
    if (ui_.windows.showSkill_ && inside(skillRect(ui_.windows.skillX_, ui_.windows.skillY_, screenW))) return true;
    if (ui_.windows.showInv_ && inside(invRectAt(ui_.windows.invX_, ui_.windows.invY_))) return true;
    if (ui_.windows.showTemp_ && inside(tempRectAt(ui_.windows.invX_, ui_.windows.invY_, screenW, ui_.windows.showInv_))) return true;
    if (ui_.windows.showQuest_ && fx >= ui_.windows.questX_ && fx < ui_.windows.questX_ + 235.f && fy >= ui_.windows.questY_ && fy < ui_.windows.questY_ + 394.f) return true;
    if (ui_.forge.contains(x,y,screenW,screenH)) return true;
    if (ui_.shop.contains(x, y, screenW) || ui_.npc.contains(x, y, screenW, screenH)) return true;
    // Menus modais consomem cliques fora da moldura, como na 04-D.
    if (ui_.characterSelect.visible() || ui_.system.visible() || ui_.boat.visible()) return true;
    return false;
}

void ActorSkin::setPointer(int x, int y, bool down) {
    pointerX_ = x;
    pointerY_ = y;
    pointerDown_ = down;
}

void ActorSkin::setJobMark(int job) { ui_.content.jobMark_ = job; }

void ActorSkin::closeState() {
    ui_.close(ui::UIManager::Panel::Status);
}

void ActorSkin::beginStateDrag(int x, int y) {
    ui_.beginDrag(ui::UIManager::Panel::Status, x, y);
}

void ActorSkin::dragState(int x, int y, bool down, int screenW, int screenH) {
    ui_.drag(ui::UIManager::Panel::Status, x, y, down, screenW, screenH);
}

int ActorSkin::stateHit(int x, int y) const {
    if (!ui_.windows.showState_) return -1;
    const PanelRect box = stateRect(ui_.windows.stateX_, ui_.windows.stateY_);
    const float lx = static_cast<float>(x) - box.x;
    const float ly = static_cast<float>(y) - box.y;
    if (lx < 0.f || ly < 0.f || lx >= box.w || ly >= box.h) return -1;
    auto hit = [&](float bx, float by, float bw, float bh) {
        return lx >= bx && ly >= by && lx < bx + bw && ly < by + bh;
    };
    if (hit(172.f, 2.f, 24.f, 16.f)) return 1;
    if (hit(62.f, 176.f, 12.f, 14.f)) return 2;
    if (hit(168.f, 176.f, 12.f, 14.f)) return 3;
    if (hit(62.f, 192.f, 12.f, 14.f)) return 4;
    if (hit(168.f, 192.f, 12.f, 14.f)) return 5;
    if (hit(168.f, 208.f, 12.f, 14.f)) return 6;
    if (hit(114.f, 42.f, 40.f, 16.f)) return 7;
    if (hit(156.f, 42.f, 40.f, 16.f)) return 8;
    if (hit(114.f, 58.f, 40.f, 16.f)) return 9;
    if (hit(156.f, 58.f, 40.f, 16.f)) return 10;
    if (ly < 22.f && lx < 170.f) return 11;
    return 0;
}

int ActorSkin::shortcutHit(int x, int y, int screenW, int screenH) const {
    const ScreenBox bar = funBarBox(screenW, screenH);
    FunIcon icons[8];
    int count = 0;
    funIcons(icons, count);
    for (int i = 0; i < count; ++i) {
        if (icons[i].which < 0) continue;
        const float bx = bar.x + icons[i].x;
        const float by = bar.y + icons[i].y;
        if (static_cast<float>(x) >= bx && static_cast<float>(y) >= by &&
            static_cast<float>(x) < bx + icons[i].w && static_cast<float>(y) < by + icons[i].h) {
            return icons[i].which;
        }
    }

    // Barra F1-F8: caixas de 32 px a cada 34 px, a primeira em x=5. O vao entre
    // as caixas e a margem da esquerda nao acertam nenhum slot.
    const ScreenBox skills = skillBarBox(screenW, screenH);
    if (static_cast<float>(x) >= skills.x && static_cast<float>(y) >= skills.y &&
        static_cast<float>(x) < skills.x + skills.w && static_cast<float>(y) < skills.y + skills.h) {
        const float local = static_cast<float>(x) - skills.x - kKeyFirst;
        if (local >= 0.f) {
            const int slot = static_cast<int>(local / kKeyPitch);
            if (slot < kKeySlots && local - static_cast<float>(slot) * kKeyPitch < kKeyCell) {
                return 10 + slot;  // 10 = F1, ... 17 = F8
            }
        }
    }
    return -1;
}

bool ActorSkin::openMinimap() {
    miniPack_.clear();
    miniIndex_.clear();
    miniCache_.clear();
    miniRadar_.clear();
    miniSea_ = Image{};
    miniReady_ = false;
    miniFallback_ = false;
    miniLand_ = false;
    miniSide_ = 0;
    miniSea_.load(root_ + "/texture/ui/MUI/em-uso/minimapa/mapsea.bmp");
    if (!readFile(root_ + "/texture/minimap/" + mapName_ + "/" + mapName_ + ".pk", miniPack_, 32 * 1024 * 1024)) return false;
    const std::uint8_t* data = miniPack_.data();
    const std::size_t size = miniPack_.size();
    std::size_t cursor = 0;
    auto parseDir = [&](auto& self) -> bool {
        if (cursor >= size) return false;
        const int nameLen = data[cursor++];
        if (cursor + static_cast<std::size_t>(nameLen) + 8 > size) return false;
        cursor += static_cast<std::size_t>(nameLen);
        const std::uint32_t files = ru32(data + cursor);
        cursor += 4;
        const std::uint32_t dirs = ru32(data + cursor);
        cursor += 4;
        if (files > 20000 || dirs > 32) return false;
        for (std::uint32_t n = 0; n < files; ++n) {
            if (cursor >= size) return false;
            const int len = data[cursor++];
            if (cursor + static_cast<std::size_t>(len) + 8 > size) return false;
            const std::string name(reinterpret_cast<const char*>(data + cursor), static_cast<std::size_t>(len));
            cursor += static_cast<std::size_t>(len);
            const std::uint32_t offset = ru32(data + cursor);
            cursor += 4;
            const std::uint32_t length = ru32(data + cursor);
            cursor += 4;
            int tx = -1;
            int ty = -1;
            if (std::sscanf(name.c_str(), "sm_%d_%d.bmp", &tx, &ty) != 2) continue;
            if (tx < 0 || ty < 0 || tx > 255 || ty > 255) continue;
            if (length == 0 || static_cast<std::uint64_t>(offset) + length > size) continue;
            MiniSpan span;
            span.offset = offset;
            span.length = length;
            miniIndex_[tx + (ty << 8)] = span;
        }
        for (std::uint32_t n = 0; n < dirs; ++n) {
            if (!self(self)) return false;
        }
        return true;
    };
    if (!parseDir(parseDir) || miniIndex_.size() < 1000) {
        miniIndex_.clear();
        miniPack_.clear();
        return false;
    }
    const MiniSpan probe = miniIndex_.begin()->second;
    Image image;
    if (!image.decode(miniPack_.data() + probe.offset, probe.length) || image.width != 64 || image.height != 64) {
        miniIndex_.clear();
        miniPack_.clear();
        return false;
    }
    miniReady_ = true;
    return true;
}

Image ActorSkin::miniImageCopy(int tx, int ty) {
    const int key = tx + (ty << 8);
    const auto cached = miniCache_.find(key);
    if (cached != miniCache_.end()) return cached->second;
    if (miniCache_.size() >= 16) miniCache_.clear();
    Image image;
    const auto it = miniIndex_.find(key);
    if (it != miniIndex_.end()) {
        const MiniSpan span = it->second;
        const std::size_t end = static_cast<std::size_t>(span.offset) + span.length;
        if (span.length > 0 && end <= miniPack_.size()) image.decode(miniPack_.data() + span.offset, span.length);
    }
    if (image.width <= 0 && miniSea_.width > 0) image = miniSea_;
    miniCache_.emplace(key, image);
    return image;
}

void ActorSkin::paintMinimap() {
    miniLand_ = false;
    miniSide_ = 0;
    miniRadar_.clear();
    if (!miniReady_) return;
    constexpr int side = kRadar;
    miniRadar_.assign(static_cast<std::size_t>(side * side * 4), 255);
    const float metersPerPixel = kShowR / static_cast<float>(side);
    const int tx0 = static_cast<int>(std::floor((playerX_ - kShowR * 0.5f) / kShowR)) - 1;
    const int tx1 = static_cast<int>(std::floor((playerX_ + kShowR * 0.5f) / kShowR)) + 1;
    const int ty0 = static_cast<int>(std::floor((playerY_ - kShowR * 0.5f) / kShowR)) - 1;
    const int ty1 = static_cast<int>(std::floor((playerY_ + kShowR * 0.5f) / kShowR)) + 1;
    struct Slot {
        int tx = 0;
        int ty = 0;
        Image image;
    };
    std::vector<Slot> slots;
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            Slot slot;
            slot.tx = tx;
            slot.ty = ty;
            slot.image = miniImageCopy(tx, ty);
            slots.push_back(std::move(slot));
        }
    }
    int gmin = 255;
    int gmax = 0;
    for (int py = 0; py < side; ++py) {
        for (int px = 0; px < side; ++px) {
            const float worldX = playerX_ + (static_cast<float>(px) + 0.5f - static_cast<float>(side) * 0.5f) * metersPerPixel;
            const float worldY = playerY_ + (static_cast<float>(py) + 0.5f - static_cast<float>(side) * 0.5f) * metersPerPixel;
            const int tx = static_cast<int>(std::floor(worldX / kShowR));
            const int ty = static_cast<int>(std::floor(worldY / kShowR));
            const Image* image = nullptr;
            for (const Slot& slot : slots) {
                if (slot.tx == tx && slot.ty == ty && slot.image.width > 0) {
                    image = &slot.image;
                    break;
                }
            }
            std::uint8_t* dst = miniRadar_.data() + static_cast<std::size_t>((py * side + px) * 4);
            if (!image) {
                dst[0] = 30;
                dst[1] = 80;
                dst[2] = 140;
                dst[3] = 255;
            } else {
                const float fx = (worldX - static_cast<float>(tx) * kShowR) / kShowR;
                const float fy = (worldY - static_cast<float>(ty) * kShowR) / kShowR;
                int sx = static_cast<int>(fx * static_cast<float>(image->width));
                int sy = static_cast<int>(fy * static_cast<float>(image->height));
                if (sx < 0) sx = 0;
                if (sy < 0) sy = 0;
                if (sx >= image->width) sx = image->width - 1;
                if (sy >= image->height) sy = image->height - 1;
                const std::uint8_t* src = image->rgba.data() + static_cast<std::size_t>((sy * image->width + sx) * 4);
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
            }
            if (dst[1] < gmin) gmin = dst[1];
            if (dst[1] > gmax) gmax = dst[1];
        }
    }
    miniLand_ = (gmax - gmin) > 12;
    auto plot = [&](int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        if (x < 0 || y < 0 || x >= side || y >= side) return;
        std::uint8_t* dst = miniRadar_.data() + static_cast<std::size_t>((y * side + x) * 4);
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = 255;
    };
    for (const MobDrive& mob : drives_) {
        if (!mob.used || mob.hide) continue;
        int mx = 0;
        int my = 0;
        radarPixel(playerX_, playerY_, mob.x, mob.y, mx, my);
        for (int oy = -1; oy <= 1; ++oy) {
            for (int ox = -1; ox <= 1; ++ox) plot(mx + ox, my + oy, 210, 40, 36);
        }
    }
    // O yaw do corpo e atan2(dx, dy). No radar, esse par e (direita, baixo).
    const float faceX = std::sin(playerYaw_);
    const float faceY = std::cos(playerYaw_);
    const int cx = side / 2;
    const int cy = side / 2;
    for (int i = -2; i <= 6; ++i) {
        const int x = cx + static_cast<int>(std::lround(faceX * static_cast<float>(i)));
        const int y = cy + static_cast<int>(std::lround(faceY * static_cast<float>(i)));
        plot(x, y, 255, 220, 40);
        plot(x + 1, y, 255, 220, 40);
    }
    miniSide_ = side;
}

void ActorSkin::buildMinimapFallback(MapFile& map) {
    constexpr int side = kRadar;
    miniRadar_.assign(static_cast<std::size_t>(side * side * 4), 255);
    const float sx = std::max(1.f, static_cast<float>(map.width() - 1));
    const float sy = std::max(1.f, static_cast<float>(map.height() - 1));
    int land = 0;
    for (int py = 0; py < side; ++py) {
        for (int px = 0; px < side; ++px) {
            const int mx = std::min(map.width() - 1, static_cast<int>((px + .5f) * sx / side));
            const int my = std::min(map.height() - 1, static_cast<int>((py + .5f) * sy / side));
            const Tile tile = map.at(mx, my);
            std::uint8_t* dst = miniRadar_.data() + static_cast<std::size_t>((py * side + px) * 4);
            if (!tile.valid || tile.blocked) {
                dst[0] = 24; dst[1] = 70; dst[2] = 125;
            } else {
                const float h = std::max(0.f, std::min(1.f, map.heightAt(static_cast<float>(mx), static_cast<float>(my)) / 8.f));
                dst[0] = static_cast<std::uint8_t>(34 + h * 70.f);
                dst[1] = static_cast<std::uint8_t>(92 + h * 105.f);
                dst[2] = static_cast<std::uint8_t>(48 + h * 46.f);
                ++land;
            }
            dst[3] = 255;
        }
    }
    miniReady_ = true;
    miniFallback_ = true;
    miniLand_ = land > side * side / 20;
    miniSide_ = side;
    miniIndex_.clear();
    miniPack_.clear();
    // Mantém o marcador do personagem mesmo sem o pacote histórico: o radar
    // continua utilizável em mapas novos/compactados.
    const int cx = side / 2;
    const int cy = side / 2;
    for (int i = -2; i <= 6; ++i) {
        const int px = std::max(0, std::min(side - 1, cx + i));
        const std::size_t offset = static_cast<std::size_t>((cy * side + px) * 4);
        miniRadar_[offset] = 255;
        miniRadar_[offset + 1] = 220;
        miniRadar_[offset + 2] = 40;
        miniRadar_[offset + 3] = 255;
    }
}

void ActorSkin::setBagView(std::vector<BagView> rows, std::unordered_map<int, int> worn,
                           std::vector<BagView> temp, int gold, int capacity, std::unordered_map<int, int> wornUids, bool bankMode) {
    ui_.inventory.setView(std::move(rows), std::move(worn), std::move(temp), gold, capacity, std::move(wornUids), bankMode);
}
void ActorSkin::openBankPanel() {
    ui_.windows.showInv_ = true;
    ui_.windows.showTemp_ = true;
}
void ActorSkin::closeInv() {
    ui_.close(ui::UIManager::Panel::Inventory);
}
void ActorSkin::beginInvDrag(int x, int y) {
    ui_.beginDrag(ui::UIManager::Panel::Inventory, x, y);
}
void ActorSkin::dragInv(int x, int y, bool down, int screenW, int screenH) {
    ui_.drag(ui::UIManager::Panel::Inventory, x, y, down, screenW, screenH);
}
void ActorSkin::pageBag(int delta) { ui_.inventory.pageBy(delta, ui_.windows); }
bool ActorSkin::bagWheel(int x, int y, int screenW, int screenH) const {
    (void)screenH;
    return ui_.inventory.wheelHit(x, y, screenW, ui_.windows);
}
int ActorSkin::invHit(int x, int y, int screenW, int screenH) const {
    (void)screenH;
    return ui_.inventory.hit(x, y, screenW, ui_.windows);
}
int ActorSkin::invDropHit(int x, int y, int screenW, int screenH) const {
    (void)screenH;
    return ui_.inventory.dropHit(x, y, screenW, ui_.windows);
}
void ActorSkin::setItemDrag(int source, int x, int y, bool active) {
    ui_.inventory.setItemDrag(source, x, y, active);
}

void ActorSkin::buildHud(int width, int height) {
    hud_.clear();
    if (width < 1 || height < 1) return;
    paintMinimap();
    // 04-H: todos os desenhos sao compostos por janela: fundo -> icones ->
    // texto. Nao existe mais batch de textos global capaz de vazar para
    // janelas fechadas ou atravessar uma janela aberta por cima.
    ui::HudComposer compose(hud_, font_, root_, width, height);
    auto panel = [&](auto&& drawShapes, auto&& append, ui::HudClip bounds) {
        ui::UIRenderer shapes(width, height);
        ui::HudDrawList commands;
        drawShapes(shapes);
        append(commands);
        compose.layer(shapes, commands, bounds);
    };
    const float sw=static_cast<float>(width), sh=static_cast<float>(height);
    const ui::HudClip screen{0.f,0.f,sw,sh};
    const auto form=ui::MinimapUI::bounds(width);
    const ScreenBox bar=funBarBox(width,height);
    panel([&](ui::UIRenderer& r) {
        ui::GameHUD::drawBase(r,form.x,form.y,hp01_,sp01_,exp01_,miniReady_ && miniSide_>0);
        ui::GameHUD::drawSkillBar(r,width,height);
        ui::GameHUD::drawFunctionBar(r,bar.x,bar.y,pointerX_,pointerY_,pointerDown_);
    },[&](ui::HudDrawList& d) {
        ui::GameHUD::appendBaseSprites(d,form.x,form.y,miniReady_ && miniSide_>0);
        const ScreenBox shortcuts = skillBarBox(width, height);
        for (int i=0;i<8;++i) {
            const std::string& icon = ui_.shortcutIcons[static_cast<std::size_t>(i)];
            const float px = shortcuts.x + kKeyFirst + i*kKeyPitch;
            if (!icon.empty()) d.icons.push_back({icon,px,shortcuts.y+6.f,32.f,32.f});
            d.addText("F"+std::to_string(i+1),px+2.f,shortcuts.y+28.f,0.9f,0.9f,0.8f);
        }
    },screen);
    // Novo atalho de missoes disponiveis, no canto inferior esquerdo.
    // Nao sobrepoe a barra de skills central nem os atalhos antigos.
    panel([&](ui::UIRenderer& r) {
        r.rect(12.f,sh-41.f,136.f,31.f,{.72f,.87f,.76f,1.f});
        r.border(12.f,sh-41.f,136.f,31.f,1.f,{.18f,.44f,.30f,1.f});
    },[&](ui::HudDrawList& d){
        d.addText("! Missoes do mapa",20.f,sh-32.f,.07f,.28f,.15f);
    },screen);
    // Painel de descoberta independente do diario de missoes ativas.
    if(ui_.content.showMapQuests_) {
        const auto& c=ui_.content;
        const float left=12.f,top=std::max(8.f,sh-411.f);
        panel([&](ui::UIRenderer& r){
            r.rect(left,top,390.f,354.f,{.94f,.95f,.91f,.98f});
            r.border(left,top,390.f,354.f,2.f,{.20f,.37f,.35f,1.f});
            r.rect(left+2.f,top+2.f,386.f,35.f,{.67f,.81f,.75f,1.f});
            r.border(left+362.f,top+7.f,20.f,21.f,1.f,{.23f,.40f,.40f,1.f});
            for(int row=0;row<7;++row){
                const int i=c.mapQuestPage_*7+row;
                if(i>=int(c.mapQuests_.size()))break;
                const float yy=top+68.f+row*32.f;
                r.rect(left+16.f,yy,357.f,29.f,i==c.mapQuestSelected_?
                    ui::Color{.73f,.87f,.74f,1.f}:ui::Color{.85f,.91f,.86f,1.f});
                r.border(left+16.f,yy,357.f,29.f,1.f,{.66f,.77f,.69f,1.f});
            }
            r.border(left+18.f,top+315.f,132.f,29.f,1.f,{.23f,.40f,.40f,1.f});
            r.border(left+215.f,top+315.f,158.f,29.f,1.f,{.23f,.40f,.40f,1.f});
        },[&](ui::HudDrawList& d){
            d.addText("Missoes disponiveis neste mapa",left+16.f,top+13.f,.08f,.25f,.24f);
            d.addText("X",left+369.f,top+13.f,.28f,.18f,.18f);
            d.addText("Somente as que voce pode aceitar agora",left+18.f,top+46.f,.24f,.40f,.37f);
            for(int row=0;row<7;++row){
                const int i=c.mapQuestPage_*7+row;
                if(i>=int(c.mapQuests_.size()))break;
                const auto& q=c.mapQuests_[std::size_t(i)];
                const float yy=top+72.f+row*32.f;
                d.addText("! "+q.title.substr(0,28),left+24.f,yy,.07f,.32f,.17f);
                char label[160];
                std::snprintf(label,sizeof(label),"%s (%.0f,%.0f)  %.0fm",
                    q.npcName.substr(0,21).c_str(),q.x,q.y,q.distance);
                d.addText(label,left+24.f,yy+14.f,.26f,.35f,.39f);
            }
            if(c.mapQuests_.empty())d.addText("Nenhuma missao disponivel no seu nivel.",left+24.f,top+112.f,.30f,.40f,.40f);
            d.addText("< Anterior",left+28.f,top+322.f,.12f,.27f,.30f);
            d.addText("Proxima >",left+237.f,top+322.f,.12f,.27f,.30f);
            d.addText(std::to_string(c.mapQuestPage_+1)+"/"+
                std::to_string(std::max(1,(int(c.mapQuests_.size())+6)/7)),left+175.f,top+325.f,.12f,.27f,.30f);
        },screen);
    }
    // HUD do mundo fica embaixo dos modais, nunca sobre o texto das janelas.
    compose.text("I bolsa, Alt+D temporaria, C status, Insert senta.",220.f,8.f,1.f,1.f,1.f);
    ui::HudDrawList radarText;
    ui::MinimapUI::appendText(radarText, width, playerX_, playerY_, "Argent");
    ui::UIRenderer radarShapes(width, height);
    compose.layer(radarShapes, radarText, screen);
    compose.notes(notes_);

    const auto& w=ui_.windows;
    if (w.showState_) panel([&](ui::UIRenderer& r) {
        ui::GameHUD::drawStatus(r,w.stateX_,w.stateY_,pointerX_,pointerY_);
    },[&](ui::HudDrawList& d) {
        ui::CharacterStatusUI::appendText(d,ui_.content,w);
    },{w.stateX_,w.stateY_,198.f,394.f});

    if (w.showSkill_) {
        const PanelRect b=skillRect(w.skillX_,w.skillY_,width);
        panel([&](ui::UIRenderer& r) {
            ui::GameHUD::drawSkills(r,b.x,b.y,ui_.skills.page(),ui_.skills.rows(),{},ui_.skills.testMode());
        },[&](ui::HudDrawList& d) {
            ui_.skills.appendSprites(d,w,width);
            ui_.skills.appendText(d,w,width,font_.line());
        },{b.x,b.y,b.w,b.h});
    }
    if (w.showInv_) {
        const PanelRect b=invRectAt(w.invX_,w.invY_);
        panel([&](ui::UIRenderer& r) {
            ui::GameHUD::drawInventory(r,w.invX_,w.invY_,{},ui_.inventory.page(),ui_.inventory.capacity());
        },[&](ui::HudDrawList& d) {
            ui_.inventory.appendSprites(d,w,width,pointerX_,pointerY_,pointerDown_,true,false,false);
            ui_.inventory.appendText(d,w,width,pointerX_,pointerY_,true,false);
        },{b.x,b.y,b.w,b.h});
    }
    if (w.showTemp_) {
        const PanelRect b=tempRectAt(w.invX_,w.invY_,width,w.showInv_);
        panel([&](ui::UIRenderer& r) {
            ui_.inventory.drawTemp(r,w,width);
        },[&](ui::HudDrawList& d) {
            ui_.inventory.appendSprites(d,w,width,pointerX_,pointerY_,pointerDown_,false,true,false);
            ui_.inventory.appendText(d,w,width,pointerX_,pointerY_,false,true);
        },{b.x,b.y,b.w,b.h});
    }
    if (w.showQuest_) panel([&](ui::UIRenderer& r) {
        ui::QuestUI::draw(r,w.questX_,w.questY_,{},ui_.content.questCanAbandon_);
    },[&](ui::HudDrawList& d) {
        ui::QuestUI::appendText(d,ui_.content.questLines_,w.questX_,w.questY_);
        if(ui_.content.questCanAbandon_)d.addText("Abandonar",w.questX_+139.f,w.questY_+372.f,.4f,.14f,.16f);
    },{w.questX_,w.questY_,235.f,394.f});

    if (ui_.shop.visible()) {
        auto b=ui_.shop.bounds(width);
        panel([&](ui::UIRenderer& r) { ui_.shop.draw(r,width); },
              [&](ui::HudDrawList& d) { ui_.shop.appendSprites(d,width); ui_.shop.appendText(d,width,pointerX_,pointerY_); },
              {b.x,b.y,b.w,b.h});
    }
    if (ui_.forge.visible()) {
        const auto b=ui::ForgeWorkbenchUI::bounds(width,height);
        ui::UIRenderer forgeShapes(width,height);
        ui::HudDrawList forgeLabels;
        ui_.forge.draw(forgeShapes,forgeLabels,width,height,ui_.inventory);
        compose.layer(forgeShapes,forgeLabels,{b.x,b.y,b.w,b.h});
    }
    if (ui_.npc.visible()) {
        auto b=ui::modal_layout::npc(width,height);
        panel([&](ui::UIRenderer& r) { ui_.npc.draw(r,width,height); },
              [&](ui::HudDrawList& d) { ui_.npc.appendSprites(d,width,height); ui_.npc.appendText(d,width,height); },
              {b.x,b.y,b.w,b.h});
    }
    if (ui_.boat.visible()) panel([&](ui::UIRenderer& r) {
        ui_.boat.draw(r,width,height,boatId_);
    },[&](ui::HudDrawList& d) {
        ui_.boat.appendSprites(d,width,height);ui_.boat.appendText(d,width,height,boatId_);
    },screen);
    if (ui_.characterSelect.visible()) panel([&](ui::UIRenderer& r) {
        ui_.characterSelect.draw(r,width,height);
    },[&](ui::HudDrawList& d) {
        ui_.characterSelect.appendSprites(d,width,height);ui_.characterSelect.appendText(d,width,height);
    },screen);
    if (ui_.system.visible()) panel([&](ui::UIRenderer& r) {
        ui_.system.draw(r,width,height);
    },[&](ui::HudDrawList& d) {
        ui_.system.appendSprites(d,width,height);ui_.system.appendText(d,width,height);
    },screen);
    // Overlay above windows, with full-screen clipping instead of bag clipping.
    // Render only while inspecting an item, never while dragging or a modal is up.
    if (itemConfirmText_.empty() && !ui_.inventory.itemDragActive() &&
        !ui_.system.visible() && !ui_.characterSelect.visible() && !ui_.npc.visible()) {
        if (ui_.inventory.hoveredRow(pointerX_,pointerY_,width,w)) {
            ui::UIRenderer tooltipShapes(width,height);
            ui::HudDrawList tooltipLabels;
            ui_.inventory.appendTooltip(tooltipShapes,tooltipLabels,w,width,height,
                                        pointerX_,pointerY_,font_.line());
            compose.layer(tooltipShapes,tooltipLabels,screen);
        }
    }
    // Confirmation is modal and always above the shop and inventory.
    if (!itemConfirmText_.empty()) {
        const float x = (sw - 332.f)*.5f, y = (sh - 126.f)*.5f;
        panel([&](ui::UIRenderer& r) {
            r.rect(x,y,332.f,126.f,{0.88f,0.89f,0.85f,1.f});
            r.border(x,y,332.f,126.f,2.f,{0.20f,0.29f,0.31f,1.f});
            r.rect(x+18.f,y+84.f,138.f,30.f,{0.58f,0.82f,0.66f,1.f});
            r.rect(x+176.f,y+84.f,138.f,30.f,{0.86f,0.67f,0.65f,1.f});
            r.border(x+18.f,y+84.f,138.f,30.f,1.f,{0.20f,0.30f,0.30f,1.f});
            r.border(x+176.f,y+84.f,138.f,30.f,1.f,{0.20f,0.30f,0.30f,1.f});
        },[&](ui::HudDrawList& d){
            d.addText(itemConfirmText_.substr(0,47),x+16.f,y+26.f,.08f,.14f,.16f);
            d.addText("Confirmar",x+48.f,y+90.f,.08f,.14f,.16f);
            d.addText("Cancelar",x+208.f,y+90.f,.08f,.14f,.16f);
        },screen);
    }
    if (skillIconDragging_) panel([](ui::UIRenderer&) {},[&](ui::HudDrawList& d){
        d.icons.push_back({skillDragIcon_,float(skillDragX_-16),float(skillDragY_-16),32.f,32.f});
    },screen);
    // O item arrastado deve ficar acima de todos os outros paineis.
    if (ui_.inventory.itemDragActive()) panel([](ui::UIRenderer&) {},
        [&](ui::HudDrawList& d) {ui_.inventory.appendSprites(d,w,width,pointerX_,pointerY_,pointerDown_,false,false,true);},screen);
}

} // namespace top
