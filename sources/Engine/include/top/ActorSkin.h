#pragma once

#include <cstdint>

// Personagem, monstro e HUD no mesmo caminho do cliente antigo:
// Characterinfo + iteminfo -> .lab e .lgo, CharacterAction.tx -> pose,
// textura em texture/character, barra em texture/ui.

#include "top/Font.h"
#include "top/PresentationTypes.h"
#include "../../ui/UIManager.h"
#include "top/GfxDevice.h"
#include "top/Image.h"
#include "top/MapFile.h"
#include "top/FrustumCulling.h"

#include <cstdint>
#include <array>
#include <memory>
#include <future>
#include <chrono>
#include <unordered_set>
#include <string>
#include "top/QuestPresentation.h"
#include <unordered_map>
#include <vector>

namespace top {

struct ActorReport {
    int actors = 0;
    int frustumTested = 0;
    int frustumVisible = 0;
    int frustumCulled = 0;
    int parts = 0;
    int partFail = 0;
    int bones = 0;
    int triangles = 0;
    int skeletonUpdates = 0;
    int skeletonReuses = 0;
    int skinnedUniqueVertices = 0;
    int transformedUniqueVertices = 0;
    int shadows = 0;
    int untexturedTriangles = 0;
    std::vector<std::string> missingTextures;
    bool animMoves = false;
    bool weaponAttached = false;
    bool weaponMoves = false;
    bool weaponGripCorrect = false;
    int playerPoseMode = 0;
    int playerActionBase = 7;
    int monsterPoseReady = 0;    // monsters with a usable action bank
    int monsterPoseRemaps = 0;   // monsters whose Action ID differs from character ID
    int monsterPoseFallbacks = 0; // invalid Action ID bank, character ID used
    int monsterPoseMissing = 0;  // no usable pose bank found
    float weaponHeight = 0.f;
    float bodyMinZ = 0.f;
    float bodyMaxZ = 0.f;
};

struct MonsterSlot {
    int id = 0;
    float x = 0.f;
    float y = 0.f;
    bool passive = false;
    float yaw = 0.f;
    std::uint64_t spawnKey = 0;   // 05-T: identidade estavel do ponto historico
    int respawnSeconds = 0;      // CreateCha quinto argumento; 0 = fallback offline
};

struct GroundMark {
    float x = 0.f;
    float y = 0.f;
    int kind = 0;
};

struct ActorActionTiming {
    float duration = 0.f;
    float impact = 0.f;
};

struct SceneMesh;

class ActorSkin {
public:
    ActorSkin();
    ~ActorSkin();
    bool open(const std::string& clientRoot, const std::string& mapName = "garner",
              const std::vector<MonsterSlot>& monsters = {}, int playerId = 1);
    // PERF-01: retain world actor models, animation clocks and player/boat/UI.
    // A new character type is loaded only once; known types reuse existing meshes.
    bool updatePopulation(const std::vector<MonsterSlot>& monsters);
    struct PopulationUpdateStats {
        int retained = 0;
        int created = 0;
        int newTypes = 0;
        int cachedTypes = 0;
    };
    const PopulationUpdateStats& lastPopulationUpdate() const { return lastPopulationUpdate_; }

    // PERF-02: decode *historical* monster .lab/.lgo on a worker owning a
    // completely separate ActorSkin. The render thread alone owns live bodies.
    enum class PrefetchState { Ready, Pending, Fallback };
    struct PrefetchStats {
        int requested = 0;
        int loaded = 0;
        int failed = 0;
        double elapsedMs = 0.;
    };
    // Poll is nonblocking; returns true only when a worker finished.
    bool pollPopulationPrefetch();
    int beginPopulationPrefetch(const std::vector<MonsterSlot>& candidates, int maxTypes = 3);
    PrefetchState ensurePopulationPrefetched(const std::vector<MonsterSlot>& required);
    const PrefetchStats& lastPrefetchStats() const { return lastPrefetchStats_; }


    // O mesmo giro de CSceneObj / CCharacter::SetYaw. Dois pontos fixos.
    static bool selfTest();
    void setPlayer(float x, float y, float yawRad, float height, bool moving, float dt);
    void setPlayerPose(int pose);
    void setPlayerActionBase(int base); // base action in CharacterAction.tx (1..54)
    void setPlayerActionOnce(bool once);
    void setPlayerActionPlaybackRate(float rate);
    void setPlayerActionCycle(unsigned long long cycle);
    void setPlayerWeaponsDrawn(bool drawn);
    void ensurePlaced(MapFile& map);
    int monsterCount() const;
    // Add a model row for a transient historical summon. Returns its monster slot.
    int appendMonsterModel(int id, bool duplicate = false);
    int playerId() const { return playerId_; }
    // Tempos extraidos do pose 7 de CharacterAction.tx (30 quadros/s).
    // SK2.13-A: dummy transforms from the SAME .lab frame used by actor rendering.
    // D3DX row-vector order, XYZ=game X/Y/up. No terrain-height guess.
    std::unordered_map<int, std::array<float, 16>> playerDummyWorldMatrices() const;
    // Runtime dummies for one non-passive monster slot, sampled from its current .lab pose.
    std::unordered_map<int, std::array<float, 16>> monsterDummyWorldMatrices(int index) const;
    std::array<float, 3> monsterWorldRoot(int index) const; // GL world X/height/Y.
    // World matrices of the primary equipped weapon's OWN dummy IDs.
    std::unordered_map<int, std::array<float, 16>> playerItemDummyWorldMatrices() const;
    std::unordered_map<int, std::array<float, 16>> actorItemDummyWorldMatrices(int index) const;
    float playerAttackWindup() const;
    float playerAttackDuration() const;
    float playerActionDuration(int actionBase) const;
    float playerActionWindup(int actionBase) const;
    ActorActionTiming monsterActionTiming(int index, int actionBase) const;
    MonsterSlot monsterSlot(int index) const;
    // pose: 0 ocioso, 1 corre, 2 ataque, 3 morte. hp01 enche a barra da cabeca.
    void setMonster(int index, float x, float y, float yawRad, bool moving, int pose, bool hide, float hp01,
                    int action = 7, float playbackRate = 1.f, unsigned long long cycle = 0);
    void setVitals(float hp01, float sp01, float exp01);
    void setStatusLines(std::vector<std::string> lines, int jobMark);
    void setNotes(std::vector<HudNote> notes);
    void setNpcWindow(bool visible, const std::string& name = {}, const std::string& text = {},
                      bool choices = false, bool trade = false, bool forge = false);
    void setNpcQuestViews(std::vector<NpcQuestView> missions, int selected, int page);
    void setQuestCanAbandon(bool active);
    void setMapQuestList(std::vector<MapQuestView> quests);
    // 0 botão inferior, 1 fechar, 10..16 linha, 20 anterior, 21 proximo.
    int mapQuestHit(int x,int y,int sw,int sh) const;
    void mapQuestAction(int action);
    bool mapQuestPanelOpen() const {return ui_.content.showMapQuests_;}
    bool questAbandonHit(int x, int y) const;
    int questListActionAt(int x, int y) const; // 1..5 rows, 20/21 pages
    void setTeleportChoices(std::vector<std::string> choices);
    void setNpcDialogOptions(std::vector<std::string> options, bool tutorialMode, bool hasMissions);
    int npcDialogOptionHit(int x, int y, int screenW, int screenH) const;
    int npcDialogOptionPageHit(int x, int y, int screenW, int screenH) const;
    void changeNpcDialogOptionPage(int delta);
    int npcTutorialGuideHit(int x, int y, int screenW, int screenH) const;
    int npcTutorialFooterHit(int x, int y, int screenW, int screenH) const;
    int teleportHit(int x, int y, int screenW, int screenH) const;
    int offlineTravelHit(int x, int y, int screenW, int screenH) const;
    // lines são os preços curtos, icons os png, names o nome ao passar o mouse.
    void setShopWindow(bool visible, const std::string& name = {}, std::vector<std::string> lines = {},
                       int first = 0, int total = 0, int gold = 0, std::vector<std::string> icons = {},
                       std::vector<std::string> names = {});
    // 04-G: eventos de loja sempre usam a mesma geometria do desenho.
    int shopWindowHit(int x, int y, int screenW) const;
    bool shopContains(int x, int y, int screenW) const;
    void beginShopDrag(int x, int y, int screenW);
    void dragShop(int x, int y, bool down, int screenW, int screenH);
    void setCharacterWindow(bool visible);
    void setCharacterRoster(std::vector<std::string> names, int selected);
    void beginCharacterCreation();
    void beginCharacterDeletion();
    void chooseCharacterRace(int raceId);
    void cancelCharacterCreation();
    int characterCreateRace() const;
    int characterCreationStage() const;
    std::string characterCreateName() const;
    void editCharacterCreateName(const std::string& text, bool backspace);
    int characterHit(int x, int y, int screenW, int screenH) const;
    void toggleSystemWindow();
    void closeSystemWindow();
    bool systemWindow() const { return ui_.system.visible(); }
    // 1 video, 2 audio, 3 jogo, 4 trocar personagem, 5 relogar offline, 6 sair.
    int systemHit(int x, int y, int screenW, int screenH) const;
    void setLiveLine(int index, const std::string& text);
    // Alias para manter a API C++ pre-04-D (inclusive inicializadores agregados).
    using BagView = ui::BagView;
    // Pilhas da bolsa, o que está posto e a bolsa temporária do Alt+D.
    void setBagView(std::vector<BagView> rows, std::unordered_map<int, int> worn, std::vector<BagView> temp, int gold, int capacity = 24,
                    std::unordered_map<int, int> wornUids = {}, bool bankMode = false);
    void openBankPanel();
    void setBag(std::vector<std::string> lines);
    // Atualiza as cinco partes do corpo e as armas usando os IDs/slots originais
    // do iteminfo. Retorna false somente quando uma peca nova nao pode ser lida.
    bool setEquipment(const std::unordered_map<int, int>& worn);
    // Troca o corpo pelo casco definido em shipinfo/shipiteminfo.
    bool setBoatMode(bool enabled, int shipId = 0);
    bool boatMode() const { return boatMode_; }
    int boatId() const { return boatId_; }
    float boatSeaHeight() const { return boatSeaHeight_; }
    float boatMoveSpeed() const { return boatMoveSpeed_; }
    void toggleBoatWindow();
    void closeBoatWindow();
    // -1 fora, 0 fecha, 1..17 seleciona o casco historico.
    int boatHit(int x, int y, int screenW, int screenH) const;
    using SkillView = ui::SkillView;
    void setSkillView(std::vector<SkillView> skills, int points, bool testMode = false);
    void setShortcutIcons(std::array<std::string, 8> icons);
    void setSkillDragIcon(const std::string& icon, int x, int y, bool active);
    // 05-AC: bancada visual, sem mutacao de itens. Play valida/consome os materiais.
    void toggleForgeWorkbench(int screenW,int screenH);
    void toggleProductionWorkbench(int screenW,int screenH);
    void openForgeWorkbench(int screenW,int screenH,unsigned allowedModes);
    bool forgeWorkbenchOpen() const { return ui_.forge.visible(); }
    int forgeWorkbenchHit(int x,int y,int w,int h) const;
    int forgeWorkbenchDropSlot(int x,int y,int w,int h) const;
    bool forgeWorkbenchSelect(int inventoryHit,int targetSlot=-1);
    void forgeWorkbenchClearSlot(int slot);
    void forgeWorkbenchMode(int mode);
    void forgeWorkbenchReset();
    int forgeWorkbenchModeIndex() const { return int(ui_.forge.mode()); }
    std::array<int,3> forgeWorkbenchIndices() const { return ui_.forge.indices(ui_.inventory); }
    void forgeWorkbenchPreview(bool valid,int price,int chance,const std::string& text);
    void setItemConfirm(const std::string& text);
    void showShopInventory(int width);
    int skillHit(int x, int y, int screenW) const;
    int skillUpgradeHit(int x, int y, int screenW) const;
    bool skillTestModeHit(int x, int y, int screenW) const;
    int skillPageHit(int x, int y, int screenW) const;
    void changeSkillPage(int delta);
    bool skillWheel(int x, int y, int screenW, int delta);
    void setSkills(std::vector<std::string> lines);
    void setQuests(std::vector<std::string> lines);
    // -1 fora, 0 conteudo, 1 fechar, 2 arrastar pela barra superior.
    int skillWindowHit(int x, int y, int screenW) const;
    void showSkillPanel();
    void hideSkillPanel();
    int questWindowHit(int x, int y) const;
    void beginSkillDrag(int x, int y, int screenW);
    void dragSkill(int x, int y, bool down, int screenW, int screenH);
    void beginQuestDrag(int x, int y);
    void dragQuest(int x, int y, bool down, int screenW, int screenH);
    void setMarks(std::vector<GroundMark> marks);
    void addMarks(const std::vector<GroundMark>& marks);
    // -1 fora. 0 o miolo. 1 fecha. 2 arrasta. 3/4 rola a grade. 5 abre ou fecha a temporaria.
    // 100+slot tira o equipamento. 200+indice da bolsa. 300+indice da temporaria.
    int invHit(int x, int y, int screenW, int screenH) const;
    // Destinos para soltar: 100+equipamento, 250 bolsa, 350 bolsa temporaria.
    int invDropHit(int x, int y, int screenW, int screenH) const;
    void setItemDrag(int source, int x, int y, bool active);
    void beginInvDrag(int x, int y);
    void dragInv(int x, int y, bool down, int screenW, int screenH);
    void closeInv();
    void pageBag(int delta);
    bool bagWheel(int x, int y, int screenW, int screenH) const;
    bool tempOpen() const { return ui_.windows.showTemp_; }
    // 0 bolsa, 1 status, 2 skill. -1 fora dos tres botoes.
    int shortcutHit(int x, int y, int screenW, int screenH) const;
    // -1 fora. 0 no miolo. 1 fecha. 2..6 os +. 7..10 a classe. 11 a faixa para arrastar.
    int stateHit(int x, int y) const;
    void beginStateDrag(int x, int y);
    void dragState(int x, int y, bool down, int screenW, int screenH);
    void closeState();
    void setJobMark(int job);
    void setPointer(int x, int y, bool down);
    void setFrustum(const float* viewProjection, bool enabled = true);
    void build(MapFile& map, const SceneMesh* ground = nullptr);
    void buildHud(int width, int height);
    void togglePanel(int which);
    bool uiBlocks(int x, int y, int screenW, int screenH) const;
    bool minimapReady() const { return miniReady_; }
    bool minimapFallback() const { return miniFallback_; }
    bool minimapLand() const { return miniLand_; }
    void buildMinimapFallback(MapFile& map);
    int minimapTiles() const { return static_cast<int>(miniIndex_.size()); }
    const std::uint8_t* minimapPixels() const { return miniRadar_.empty() ? nullptr : miniRadar_.data(); }
    int minimapSize() const { return miniSide_; }
    bool fontReady() const { return font_.ready(); }
    const std::uint8_t* fontPixels() const { return font_.pixels(); }
    int fontWidth() const { return font_.width(); }
    int fontHeight() const { return font_.height(); }
    const std::vector<ActorBatch>& batches() const { return batches_; }
    const std::vector<ActorBatch>& hud() const { return hud_; }
    const ActorReport& report() const { return report_; }

private:
    std::string root_;
    std::string mapName_ = "garner";
    int playerId_ = 1;
    std::vector<MonsterSlot> roster_;
    ActorReport report_;
    std::vector<ActorBatch> batches_;
    std::vector<ActorBatch> hud_;
    frustum_culling::Frustum frustum_;
    bool frustumEnabled_ = false;
    ui::UIManager ui_;
    bool placed_ = false;
    float playerX_ = 0.f;
    float playerY_ = 0.f;
    float playerZ_ = 0.f;
    float playerYaw_ = 0.f;
    float playerDt_ = 0.f;
    bool playerMoving_ = false;
    int playerPose_ = 0;
    int playerActionBase_ = 7;
    bool playerActionOnce_ = false;
    float playerActionPlaybackRate_ = 1.f;
    unsigned long long playerActionCycle_ = 0;
    // Moving interrupts only the visible casting pose, not the spell's
    // independent particle lifetime or scheduled action callbacks.
    bool playerCastPoseInterrupted_ = false;
    unsigned long long playerCastPoseCycle_ = 0;
    bool playerWeaponsDrawn_ = true;
    float hp01_ = 1.f;
    float sp01_ = 1.f;
    float exp01_ = 0.f;
    std::vector<HudNote> notes_;
    std::string itemConfirmText_;
    std::string skillDragIcon_;
    int skillDragX_ = 0, skillDragY_ = 0;
    bool skillIconDragging_ = false;
    std::vector<GroundMark> marks_;
    struct MobDrive {
        bool used = false;
        float x = 0.f;
        float y = 0.f;
        float yaw = 0.f;
        bool moving = false;
        int pose = 0;
        bool hide = false;
        float hp01 = 1.f;
        int action = 7;
        float playbackRate = 1.f;
        unsigned long long cycle = 0;
    };
    std::vector<MobDrive> drives_;
    Font font_;
    std::vector<std::string> equipLines_;
    std::unordered_map<int, int> wornVisual_;
    int playerFramework_ = 0;
    int playerDefaultSkin_[5] = {};
    bool boatMode_ = false;
    int boatId_ = 1;
    float boatSeaHeight_ = 0.f;
    float boatMoveSpeed_ = 0.f;
    struct Body;
    std::vector<Body> bodies_;
    // PERF-01: bounded per-map cache of parsed historical .lab/.lgo by type.
    // shared_ptr supports Body's out-of-line definition; no GL resources here.
    std::unordered_map<int, std::shared_ptr<Body>> cachedPopulationTypes_;
    PopulationUpdateStats lastPopulationUpdate_;
    // A future from std::async joins on destruction/map change: no worker can
    // access an ActorSkin after its owner is destroyed. Worker never uses GL.
    std::future<std::unique_ptr<ActorSkin>> populationPreload_;
    std::vector<int> populationPreloadIds_;
    std::unordered_set<int> populationPreloadFailures_;
    PrefetchStats lastPrefetchStats_;
    std::chrono::steady_clock::time_point populationPreloadStart_{};
    bool hasPopulationType(int id) const;


    bool openMinimap();
    void paintMinimap();
    Image miniImageCopy(int tx, int ty);
    struct MiniSpan {
        std::uint32_t offset = 0;
        std::uint32_t length = 0;
    };
    std::vector<std::uint8_t> miniPack_;
    std::unordered_map<int, MiniSpan> miniIndex_;
    std::unordered_map<int, Image> miniCache_;
    Image miniSea_;
    std::vector<std::uint8_t> miniRadar_;
    int miniSide_ = 0;
    bool miniReady_ = false;
    bool miniFallback_ = false;
    bool miniLand_ = false;
    int pointerX_ = -1;
    int pointerY_ = -1;
    bool pointerDown_ = false;

};

}  // namespace top
