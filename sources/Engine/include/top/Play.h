#pragma once

// Fachada de gameplay: regras de missao em PlayMissions.cpp, interacao
// com NPC em PlayNpcInteraction.cpp. Os numeros saem de characterinfo.
// O servidor nao entra nessa conta.

#include "top/ActorSkin.h"
#include "top/EffectSystem.h"
#include "top/MapFile.h"
#include "top/PathFinder.h"
#include "top/MonsterSkills.h"
#include "top/MonsterLifecycle.h"
#include "top/HistoricalStats.h"
#include "top/EquipmentStats.h"
#include "top/ForgeRules.h"
#include "top/SocketRules.h"
#include "top/ScreenPick.h"
#include "top/MissionEligibility.h"
#include "top/MissionEvents.h"
#include "top/MissionJournal.h"
#include "top/ItemRandomRewardRules.h"
#include "top/ItemAmplifierRules.h"
#include "top/ItemStatRules.h"
#include "top/ItemProductionRules.h"
#include "top/ItemGemVoucherRules.h"
#include "top/ItemChestRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace top {
struct HistoricalMission;
struct HistoricalMissionStep;

class Play {
public:
    enum class StarActionResult { Success, Ineligible, NoSpace, DataMissing, UnsupportedClass };
    struct MissionItem {
        int id = 0;
        int count = 0;
    };

    static bool selfTest();
    static bool selfTestExplicitCombat();
    static bool selfTestHistoricalSkillExecution();
    static bool selfTestPetStats();
    // Isolated screenshot scenarios may change RAM, but cannot write saves.
    void setReadOnlyPreview(bool enabled) { readOnlyPreview_=enabled; }
    bool configureStatPreview(const std::string& scenario);
    int equippedFairyItemId() const;
    float effectiveAttackInterval() const;
    int effectiveDefense() const;
    std::array<int,5> effectivePrimaryAttributes() const; // STR, CON, AGI, DEX, SPR
    std::string statDiagnostic() const;
    // profileId separates save identity from the characterinfo race/model ID.
    // Zero preserves the legacy one-save-per-race behavior.
    bool bind(const std::string& clientRoot, const ActorSkin& actors,
              int profileId = 0, const std::string& profileName = {});
    int profileId() const { return profileId_ > 0 ? profileId_ : player_.id; }
    const std::string& profileName() const { return player_.name; }
    int level() const { return player_.level; }
    std::uint64_t respawnSerial() const { return respawnSerial_; }
    // Recria somente a população visível. O estado persistente do personagem
    // é salvo antes e restaurado pelo bind, permitindo streaming por região.
    bool rebindWorld(const ActorSkin& actors);
    // PERF-01: no save/rebind on a routine 32m spawn update. New foes use
    // the same historical stat factory as the initial bind.
    bool refreshPopulation(const ActorSkin& actors);
    bool hasTransientSummons() const;

    // Registrar mudanca de mapa ANTES de recriar a populacao de actores.
    void setWorldMap(const std::string& mapName);
    void pick(float x, float y);
    screen_pick::Hit rayHit(const screen_pick::Ray& ray, float terrainT, const MapFile* map=nullptr) const;
    void pickFoe(int index);
    bool selectedMonster(float& x, float& y, float& radius) const;
    bool requestLoot(int index, float px, float py, float& tx, float& ty);
    bool sellBag(int index);
    bool discardBag(int index);
    int bagItemId(int index) const;
    int bagItemUid(int index) const;
    std::string bagItemName(int index) const;
    bool hasBagItem(int id) const { return bagCount(id) > 0; }
    bool increaseSkill(int id);
    bool testIncreaseSkill(int id);
    bool processNpcRecipe(const std::vector<std::pair<int,int>>& inputs, int goldCost,
                          const std::vector<std::pair<int,int>>& outputs, const std::string& label,
                          bool requireEmptySlotBefore = false, int historicalItemEvent = 0,
                          int historicalRecordToSet = 0);
    bool redeemNpcFameReward(int fameCost, int rewardItemId, int rewardCount, const std::string& label);
    int fame() const { return fame_; }
    bool applyHistoricalHairStyle(int modelItemId, const std::vector<std::pair<int,int>>& inputs,
                                  int goldCost);
    int hairStyleItemId() const { return hairStyleItemId_; }
    bool processNpcRecipeAnyInput(const std::vector<int>& inputItemIds, int goldCost,
                                  const std::vector<std::pair<int,int>>& outputs, const std::string& label,
                                  bool requireEmptySlotBefore = false);
    bool redeemNpcDiamondVoucher(int level);
    bool cleanWithNpcWasher();
    bool receiveNpcHealing(int goldCost, int hpAmount, bool restoreAll);
    bool hasActiveHistoricalMission(int missionId) const;
    bool hasHistoricalRecord(int recordId) const;
    bool hasNewcomerHealingEntitlement() const;
    bool claimArgentWelcomeGift();
    bool grantMarriamPkTest();
    void setSkillTestMode(bool enabled) { skillTestMode_=enabled; }
    bool skillTestMode() const { return skillTestMode_; }
    void gmAddExperience(int amount);
    void gmHeal();
    bool gmGiveItem(int id, int count=1);
    // 05-Z: trabalho offline usando tres indices da bolsa (equipamento, gema, refiner).
    struct WorkbenchPreview { bool valid=false; int cost=0, chance=0; std::string message; };
    WorkbenchPreview previewWorkbench(int mode,int a,int b,int c) const;
    int repairItemCost(int index) const;
    bool repairBagItem(int index);
    int gemExtractionCost(int equipmentIndex,int plierIndex) const;
    bool extractSocketGem(int equipmentIndex,int plierIndex);
    int tiChunCost(int equipmentIndex,int catalystIndex) const;
    bool upgradeEquipmentTiChun(int equipmentIndex,int catalystIndex);
    int equipmentUpgradeCost(int equipmentIndex) const;
    bool upgradeEquipmentEffectiveness(int scrollIndex,int equipmentIndex,int crystalIndex);
    int forgeCost(int bagIndex) const;
    // Perfuração histórica: equipamento + estabilizador 890 + catalisador 891.
    int socketCost(int equipmentIndex) const;
    bool socketEquipment(int equipmentIndex,int stabilizerIndex,int catalystIndex);
    bool forgeBag(int equipmentIndex,int gemIndex,int refinerIndex);
    bool combineGems(int scrollIndex,int leftIndex,int rightIndex);
    bool gmOpenSockets(int equipmentIndex,int sockets);
    // PORT-04: preview and execute the historical blueprint machine using
    // fairy, blueprint, and optional tool bag indices. The dice minigame and
    // native MakeItem stat rolls are explicitly approximate offline.
    struct ProductionPreview {
        bool valid=false;
        int productId=0;
        int fee=0;
        std::array<int,3> materialIds{};
        std::array<int,3> materialCounts{};
        int successChance=0;
        std::string message;
    };
    ProductionPreview previewProduction(int fairyIndex,int blueprintIndex,int toolIndex) const;
    bool processProduction(int fairyIndex,int blueprintIndex,int toolIndex);
    void gmAddGold(int amount);
    // NpcSdk.lua::Teleport charges 500G above level 40. The caller preflights
    // the fare, performs the map transition, then commits the charge only if it succeeds.
    bool historicalNpcTeleportFareReady() const;
    bool payHistoricalNpcTeleportFare();
    // 1 monstro, 2 loot, 0 o resto. Nao muda o alvo.
    int cursorOver(float x, float y) const;
    bool skillArmed() const;
    void clearTarget();
    bool chasePoint(float px, float py, float& outX, float& outY) const;
    bool aim(float px, float py, float& yaw) const;
    void setPlayerFacing(float yaw) { if (std::isfinite(yaw)) playerFacing_ = yaw; }
    bool lockMove() const;
    int playerPose() const;
    float moveSpeed() const {
        const float base = player_.speed > 0.f ? player_.speed : 4.8f;
        // Mspd_final adds state coefficients before applying the historical
        // 30% floor. Windwalk and numb must not multiply each other.
        const float slow = std::max(.3f,1.f+passiveBonuses().moveCoefficient-
            (playerNumbFor_>0.f?.2f+.03f*playerNumbLevel_:0.f));
        const float potion = itemPotionMspdState_.remaining > 0.f
            ? 1.f + static_cast<float>(itemPotionMspdState_.bonus) / 100.f : 1.f;
        return (base * slow + itemBodyMoveSpeedBonus_) * potion;
    }
    // 0..3 correspondem a F1..F4 / teclas 1..4.
    void castSkill(int slot = 0);
    // Disparo local do barco: não usa servidor e compartilha os efeitos do
    // combate terrestre.
    bool fireBoatCannon(float px, float py);
    void assignSkill(int skillId); // legacy API; chooses first free slot
    bool assignSkillToSlot(int skillId, int slot);
    void moveSkillSlot(int from, int to);
    int skillAtSlot(int slot) const;
    std::string skillIcon(int skillId) const;
    void setVendor(float x, float y, const std::string& name = "Vendedor", const std::vector<int>& stock = {});
    void clearVendor();
    // -2 é o X da loja. 0 ou mais é o item do catálogo.
    int shopHit(int x, int y, int screenW, int screenH) const;
    void clickShop(int index);
    void closeShop();
    void scrollShop(int rows);
    bool shopOpen() const { return shopOpen_; }
    void talkNpc(const std::string& name, const std::string& text, bool trade = false, bool forge = false);
    int dialogActionAt(int x, int y, int screenW, int screenH) const;
    void clickDialog(int action);
    void closeDialog();
    void setNpcMissionList(const std::vector<HistoricalMission>& missions);
    // Registro Lua AddMission(recordId) -> IDs aceitos via NPC, para validar
    // HasRecord/NoRecord mesmo depois de carregar um save antigo.
    void setMissionRecordIndex(std::unordered_map<int,std::vector<int>> index) {
        missionsByRecord_=std::move(index);
    }
    struct QuestNpcMarker {
        struct Mission { int id=0, minLevel=1, maxLevel=0; bool valid=false; MissionGate gate; std::string title; };
        float x=0.f,y=0.f;
        std::string npcName;
        std::vector<Mission> missions;
    };
    void setQuestNpcMarkers(std::vector<QuestNpcMarker> markers) { questNpcMarkers_=std::move(markers); }
    std::vector<MapQuestView> availableMapQuests() const;
    // Missao mundial 1 (Welcome) inicia no personagem novo e termina em Senna.
    bool deliverWelcomeToSenna(const std::string& npcName);
    // Called only after the Linux client's verified in-range NPC interaction.
    bool deliverHistoricalToNpc(const std::string& npcName, int missionId = 0);
    bool applyNpcMissionStep(const HistoricalMissionStep& step);
    bool hasWelcomeMission() const;
    bool abandonHistoricalMission();
    bool abandonHistoricalMission(int missionId);
    bool historicalMissionActive() const;
    bool selectJournalAction(int action); // 1..5 rows, 20 previous, 21 next
    std::size_t historicalActiveCount() const;
    bool npcDialogOpen() const { return npcTalkOpen_; }
    bool offerHistoricalMission(int id, const std::string& title, const std::string& objective, float targetX,
                                float targetY, int targetId, int targetCount, int rewardExp, int rewardGold,
                                int minLevel = 1, int maxLevel = 0,
                                const std::vector<MissionItem>& neededItems = {},
                                const std::vector<MissionItem>& rewardItems = {});
    bool acceptHistoricalMission(int id, const std::string& title, const std::string& objective, float targetX,
                                 float targetY, int targetId, int targetCount, int rewardExp, int rewardGold,
                                 int minLevel = 1, int maxLevel = 0,
                                 const std::vector<MissionItem>& neededItems = {},
                                 const std::vector<MissionItem>& rewardItems = {});
    void interact(float px, float py);
    // 05-AY: duplo clique no ticket prepara destino, mas NAO o consome.
    // Somente o frontend confirma depois de conseguir carregar o mapa.
    struct TicketTravel { int uid=0,itemId=0; std::string city,map; float x=0.f,y=0.f; };
    void setTicketBoatMode(bool usingBoat) { usingTicketBoat_ = usingBoat; }
    float boatItemSpeedMultiplier() const { return itemBoatSpeedFor_ > 0.f ? 1.15f : 1.f; }
    std::optional<TicketTravel> takeTicketTravelRequest();
    bool consumeTicketAfterTravel(int itemUid,int itemId);
    void reportTicketTravelFailed();
    void clickBag(int index, ActorSkin* actors = nullptr);
    // Tira o que estiver no slot de equipamento (o mesmo clique da grade).
    void clickEquip(int slot);
    // A bolsa temporária guarda uma pilha e devolve com outro clique.
    void stashBag(int index);
    void takeTemp(int index);
    bool openBank();
    void closeBank() { bankOpen_ = false; }
    bool bankOpen() const { return bankOpen_; }
    void depositBank(int bagIndex);
    void withdrawBank(int bankIndex);
    // Offline equivalents for Senna's Star of Unity script actions.
    bool hasBankItem(int id) const;
    StarActionResult grantSennaUnityChest();
    StarActionResult redeemTeamStar();
    // Codigos iguais aos de ActorSkin: origem 100/200/300, destino 100/250/350.
    void dragInventory(int source, int target);
    // 0 FOR, 1 AGI, 2 CON, 3 STA, 4 DEX. Gasta um ponto de atributo.
    void spendAttr(int which);
    // 0 Newbie, 1 Swordsman, 2 Hunter, 4 Explorer. A troca pede nivel 10.
    void setJob(int job);
    // Argent test-trainer action: immediate class switch for offline QA.
    bool testSetJob(int job);
    struct BoatBuildOption {
        int id = 0;
        std::string name;
        int requiredLevel = 0;
        int cost = 0;
        int maxHp = 0;
        int maxSp = 0;
        std::vector<int> allowedJobs;
    };
    void loadBoatBuildCatalog(const std::string& clientRoot);
    const std::vector<BoatBuildOption>& boatBuildOptions() const { return boatBuildOptions_; }
    const std::vector<int>& ownedBoatIds() const { return ownedBoatIds_; }
    bool ownsBoat(int id) const;
    bool buildHistoricalBoat(int id);
    int boatHp(int id) const;
    int boatMaxHp(int id) const;
    int boatSp(int id) const;
    int boatMaxSp(int id) const;
    int boatRepairCost(int id) const;
    int boatSupplyCost(int id) const;
    bool repairOwnedBoat(int id);
    bool refuelOwnedBoat(int id);
    void toggleSit();
    bool sitting() const { return sitting_; }
    void step(float dt, MapFile* map, float px, float py);
    void pushActors(ActorSkin& actors);
    void pushHud(ActorSkin& actors, const float* mvp, int screenW, int screenH) const;
    std::vector<EffectEvent> takeEffects();

private:
    struct ScheduledEffect {
        float delay = 0.f;
        EffectEvent event;
        int owner = -1; // player; non-negative values are foe indices
        unsigned long long actionCycle = 0;
    };
    friend class PlaySelfTest;
    struct Foe {
        int id = 0;
        std::string name;
        int level = 1;
        int hp = 1;
        int maxHp = 1;
        int minAtk = 1;
        int maxAtk = 1;
        int def = 0;
        int hit = 0;
        int dodge = 0;
        int resist = 0;  // characterinfo coluna 67: PDEF
        int crit = 0;    // characterinfo coluna 71: CRT / CRTRADIX
        int sta = 0;     // characterinfo coluna 84: Spirit/STA
        int con = 0;     // characterinfo coluna 83: Constitution/CON
        int job = 0;     // characterinfo coluna 89: Class
        int expReward = -1; // tabela: 0 significa zero XP historico; -1 = fixture sem tabela
        float x = 0.f;
        float y = 0.f;
        float homeX = 0.f;
        float homeY = 0.f;
        float yaw = 0.f;
        float range = 1.6f;
        float radius = 0.4f;
        float vision = 10.f;
        float leash = 15.f;
        float speed = 0.f;
        float cd = 1.4f;
        float cdLeft = 0.f;
        float swing = 0.f;
        int action = 7;
        unsigned long long actionCycle = 0;
        float playbackRate = 1.f;
        std::array<float, 55> actionDurations{};
        std::array<float, 55> actionImpacts{};
        float attackPending = 0.f;
        int pendingSkillId = 0; // 05-Q: skill comprometida ate o quadro de impacto
        float pendingReach = 0.f; // nao trocar alcance quando o alvo se move
        std::vector<monster_skills::Attack> attacks;
        float deadFor = 0.f;
        float numbFor = 0.f;
        int numbLevel = 0;
        float stunFor = 0.f;
        float poisonFor = 0.f;
        float poisonTick = 0.f;
        int poisonLevel = 0;
        float defenseDownFor = 0.f;
        int defenseDownLevel = 0;
        float enfeebleFor = 0.f; // STATE_SJ / STATE_JNJZ: attack -20%, no skills
        int enfeebleLevel = 0;
        std::uint64_t spawnKey = 0;
        int respawnSeconds = 0;
        float wanderX = 0.f;
        float wanderY = 0.f;
        float wanderWait = 1.f;
        // characterinfo coluna 53. 10 persegue. 1 e 5 so revidam. 2 foge. 0 fica.
        int ai = 1;
        bool provoked = false;
        bool goingHome = false;
        bool wandering = false;
        bool retreating = false; // 05-P: AI_ATK_FLEE nao acerta enquanto recua.
        bool moving = false;
        bool summoned = false;
        bool followsPlayer = false; // item pets use SetChaTarget(role) as a follow order
        float summonRemaining = 0.f;
        int actorSlot = -1;
        // 05-N: memoria breve do ultimo alvo visto e caminho A* limitado.
        float lastSeenX = 0.f, lastSeenY = 0.f;
        float lostSightFor = 3.f;
        std::vector<PathPoint> route;
        std::size_t routeIndex = 0;
        float routeCooldown = 0.f;
        float routeGoalX = 0.f, routeGoalY = 0.f;
        int routeMode = 0; // 1 perseguicao, 2 retorno, 3 fuga, 4 passeio
        struct Drop {
            int id = 0;
            int rate = 0;
        };
        std::vector<Drop> drops;
    };
    Foe createWorldFoe(const MonsterSlot& slot, const ActorSkin& actors, int ordinal);
    void ensureWorldFoeRow(int id);
    struct ItemInfo {
        int id = 0;
        std::string name;
        int price = 0;
        int level = 0;
        int slot = -1;
        int type = 0;
        bool chInstance = false; // iteminfo col. 21: gera atributos por copia
        int maxEnergy = 0; // iteminfo col. 75
        int maxEnergyRange = 0; // limite superior do par da coluna 75
        int maxDurability = 0; // iteminfo col. 76 (MAXURE); 25000 indica apparel
        int maxDurabilityRange = 0; // limite superior do par da coluna 76
        int maxSockets = 3;  // iteminfo col. 77, limite adicional por equipamento
        int maxStack = 1; // iteminfo col. 20: swords 1, consumables up to 99
        std::vector<int> equipSlots; // iteminfo col. 28: swords 9,6
        std::vector<int> requiredEquipSlots; // iteminfo col. 29: slots the item displaces/occupies
        std::string icon;
        int minAtk = 0;
        int maxAtk = 0;
        int def = 0;
        equipment_stats::Bonus stats;  // iteminfo 52..73, valores estaticos sem rolagem
        equipment_stats::Bonus statsMaximum; // segundo extremo dos ranges ItemInfo para instancias
        int healHp = 0;
        int healSp = 0;
        double healHpPercent = 0.0;
        double healSpPercent = 0.0;
        int healCooldownSeconds = 0;
        int healStateId = 0;
        int healStateLevel = 0;
        int healStateSeconds = 0;
        int fairyRationAmount = 0; // Server.Fairy.Ration; runtime recovers amount * 50 stamina
        int fairyEggTargetId = 0; // Server.Fairy.Egg item output
        int fairySkillId = 0; // Server.Fairy.Skill item mapping
        int fairySkillLevel = 0;
        int learnedSkillId = 0; // AddFairyBody/AddFunnySkill character-skill mapping
        int learnedSkillLevel = 0;
        int fairyFruitAttribute = -1; // Server.Fairy.Fruit ATTR as STR/CON/AGI/DEX/STA index
        int fairyFruitLevels = 0; // Server.Fairy.Fruit LV
        bool fairyFruitImproved = false;
        bool medkit = false;
        std::string scriptAction; // iteminfo coluna 86: nome do callback historico, nao executado como Lua
        std::vector<int> races;
        std::vector<int> jobs;
    };
    struct Stack {
        Stack() = default;
        Stack(int itemId, int amount) : id(itemId), count(amount) {}
        int id = 0;
        int count = 0;
        // 05-Y: identificador estavel por copia fisica, sem depender do slot.
        int uid = 0;
        int forgeLevel = 0; // soma dos niveis limitada ao formato Y1 (15)
        std::array<int, 3> gemIds{}; // IDs de pedras por encaixe
        std::array<int, 3> gemLevels{}; // 05-Z: niveis por encaixe
        int socketCount = 0; // 05-Z: aberto explicitamente (0..3)
        int stoneLevel = 1; // 05-Z: nivel de gema/refiner na bolsa
        equipment_stats::Bonus extra; // valores individuais reais (nao estimados)
        // Estado fisico de uma instancia, salvo em Y2. Zero indica item legado
        // ou ainda nao instanciado; nunca inferir durabilidade/energia atuais.
        int quality = 0;
        int durability = 0;
        int maxDurability = 0;
        int energy = 0;
        int maxEnergy = 0;
        // PORT-04: VAL_STA fracionario e Part2/4/6 dos blueprints gerados pelo
        // callback historico; permanecem separados do nivel de forja Y1.
        int itemValStaTenths = 0;
        std::array<int, 3> blueprintParts{};
        // Historical fairy forge-param parts 2..7: three skill ID/level pairs.
        std::array<int, 6> fairySkills{};
        // Fairy item attributes and FairyCoins tick; kept separate from gear
        // bonuses so pet growth cannot silently modify character stats.
        std::array<int, 5> fairyStats{}; // STR, CON, AGI, DEX, STA
        int fairySystemTick = 0;
        int expDisableFlag = 0; // item 9621 ITEMATTR 55; 1 disables monster EXP
        int altarSacrifice = 0; // Black Dragon Altar ITEMATTR_VAL_STA tribute selection
        int cloakLevel = 0; // Admiral Cloak ITEMATTR 55 / SetItemLv; starts at level 1
        int fuseId = 0; // ApparelConverter FuseID: visual source equipment, not its stats.
        int equipmentUpgradeLevel = 0; // VAL_LEVEL: historical OpenUpgrade level, 0..10.
        bool tradable = true;
    };
    struct FoodState {
        int level = 0;
        float remaining = 0.f;
    };
    struct ItemBoostState {
        int multiplierPercent = 100;
        float remaining = 0.f;
        int sourceItemId = 0;
    };
    struct ItemAttributeState {
        int bonus = 0;
        float remaining = 0.f;
    };
    struct ItemAllAttributeState {
        std::array<int, 5> bonuses{}; // FOR, AGI, CON, DEX, STA
        float remaining = 0.f;
    };
    struct Loot {
        int id = 0;
        float x = 0.f;
        float y = 0.f;
        Stack instance; // descartes conservam a copia exata
    };
    struct Quest {
        const char* title = "";
        int idA = 0;
        int idB = 0;
        int need = 1;
        int have = 0;
        int rewardExp = 0;
        int rewardGold = 0;
        bool claimed = false;
    };
    struct Pop {
        float x = 0.f;
        float y = 0.f;
        float life = 0.f;
        float r = 1.f;
        float g = 1.f;
        float b = 1.f;
        std::string text;
    };

    static int damageOf(int minA, int maxA, int level, int def, int roll, int resist = 0);
    float attackReach(const Foe& target) const;
    // Commit a targeted skill only after a monster is explicitly chosen.
    void executeSkill(int slot);
    void landPlayerAttack(float px, float py);
    void makeQuests();
    void onKill(const Foe& foe);
    void resolve(float dt, float px, float py, MapFile* map = nullptr);
    void moveFoes(float dt, MapFile* map, float px, float py);
    bool save(const std::string& path);
    void grantItemExperience(int amount);
    std::string savePath(const std::string& root = {}) const;
    void load(const std::string& path);
    void loadItems(const std::string& path, const std::vector<int>& ids);
    bool ensureApparelInfo(int apparelId, int sourceEquipmentId);
    void loadHeals(const std::string& path);
    void loadFairyRations(const std::string& path);
    void loadFairyEggs(const std::string& path);
    void loadFairySkills(const std::string& path);
    void loadFairyTrainingBooks(const std::string& path);
    void loadFairyFruits(const std::string& path);
    void updateFairyAutoFeed();
    void updateFairySystem(float dt);
    void loadSkillBooks(const std::string& path);
    void resolveNamedSkillBooks();
    void loadLifeSkillBooks(const std::string& path);
    void loadSkillRules(const std::string& path);
    equipment_stats::Bonus gearBonuses() const;
    void recomputeGear();
    void applyAttrs(bool fillGain);
    bool addBag(int id, int count);
    static bool addStack(std::vector<Stack>& stacks, int id, int count, int limit, int slots);
    int equippedSlotAtBagIndex(int index) const;
    bool equipBagToSlot(int index, int preferredSlot); // -1: automatic; >=0: drag destination
    int bagCount(int id) const;
    struct HistoricalProgress;
    bool missionItemsReady() const;
    bool missionItemsReady(const HistoricalProgress& mission) const;
    bool missionRewardsFit() const;
    bool missionRewardsFit(const HistoricalProgress& mission) const;
    bool historicalObjectivesReady(const HistoricalProgress& mission) const;
    void onHistoricalEvent(MissionEventKind kind, int id, int amount=1);
    MissionJournalView missionJournalView() const;
    std::string encodeAdditionalMissions() const;
    void decodeAdditionalMissions(const char* data);
    HistoricalProgress* findHistoricalMission(int id);
    const HistoricalProgress* findHistoricalMission(int id) const;
    void removeBag(int id, int count);
    bool hasBag(int id) const;
    bool isWorn(int id) const;
    void dropLoot(const Foe& foe);
    void gather(float px, float py);
    void collectRequested(float px, float py);
    int roll(int span);
    bool summonHistoricalMonster(ActorSkin* actors);
    bool summonAriesGuardian(ActorSkin* actors);
    bool summonGoddessGuardian(ActorSkin* actors);
    bool summonMedicalReportCreature(ActorSkin* actors);
    bool createHistoricalSummon(int id, int respawnSeconds, int lifetimeSeconds,
                                ActorSkin* actors, bool followsPlayer = false,
                                float spawnOffsetX = 0.f, float spawnOffsetY = 0.f);
    void say(const std::string& text);
    void completeHistoricalMission();
    void completeHistoricalMission(HistoricalProgress& mission);

    void snapshotWorldMonsters();
    bool usingTicketBoat_ = false;
    std::optional<TicketTravel> pendingTicketTravel_;
    int pendingAltarMaterialUid_ = 0; // two-click item-on-item use (Black Dragon Altar)
    int pendingCloakUpgradeUid_ = 0; // two-click item-on-item use (Admiral Cloak Upgrade)
    int pendingApparelConverterUid_ = 0; // two-click item-on-item use (historical apparel conversion)
    std::string worldMap_;
    double worldClock_ = 0.;
    monster_lifecycle::Registry worldMonsters_;
    std::unordered_map<int, std::vector<std::string>> summonMonsterRows_;
    std::string root_;
    bool readOnlyPreview_ = false;
    int profileId_ = 0;
    std::string profileName_;
    struct SkillDef {
        int id = 0;
        std::string name;
        int sp = 0;
        float cooldown = 0.f;
    };
    struct SkillRule {
        enum class DamageFormula { Generic, StateOnly, PhysicalLinear, PhysicalPlus, AttackRaise, Backstab, IllusionSlash, MightyStrike, PrimalRage, LightningBolt, ConchRay, Enfeeble, MonsterPhysical, MonsterPhysicalFloor20, MonsterPhysicalDouble, MonsterPhysicalPlus800, MonsterFireDistance, MonsterAttackSpeed };
        int id = 0;
        std::string name;
        std::string icon;
        std::vector<int> jobs;
        int level = 0;
        int prerequisite = 0;
        int prerequisiteLevel = 0;
        int points = 0;
        int type = 1;
        int helpful = 0;
        int target = 4;
        int radius = 0;
        int distance = 0;
        int sp = 5;
        float cooldown = 6.f;
        int spBase = 5;
        float spPerLevel = 0.f;
        int cooldownBaseMs = 6000;
        int cooldownPerLevelMs = 0;
        // CWaitAttackState scales cyclic actions with fire speed. Its
        // non-cyclic branch overrides velocity to 30 authored frames/s.
        int animationTimeBaseMs = 0;
        int animationTimePerLevelMs = 0;
        bool cyclicAnimation = false; // skillinfo column 68: chPlayTime == 1
        bool spFormulaMissing = false;
        bool cooldownFormulaMissing = false;
        std::string effectFormula;
        std::string passiveFormula; // skillinfo column 30 (Use), not skill type
        DamageFormula damageFormula = DamageFormula::Generic;
        double damageScaleBase = 1.0;
        double damageScalePerLevel = 0.0;
        std::string positiveFormula;
        std::string negativeFormula;
        std::string description;
        std::vector<int> playActions; // skillinfo.txt coluna 47, poses 1..54
        float animationDuration = 0.f; // quadro final da acao historica, em segundos
        float impactOffset = 0.f;      // quadro-chave de impacto da acao
        std::vector<float> actionDurations;
        std::vector<float> actionImpacts;
        std::vector<int> itemEffectKeyFrames; // skillinfo Item/ItemExtra channels, columns 54/55
        int actionKeyFrame = -3; // skillinfo sActionKeyFrame: -1 begin, -2 end, >=0 keyed frame
        int harmMode = 0; // column 45: 1=action callback, 2=flight arrival
        int hitKeyFrame = -3; // column 64: CHitRepresent action callback
        bool hasHitVisual = false; // target/ground/water channels, columns 63/65/66
        int flightActionKeyFrame = -3; // skillinfo coluna 56: quadro do efeito de voo
        int flightEffectId = 0;      // coluna 59: Path of Flight Effect
        float flightSpeed = 0.f;    // coluna 60: velocidade do projetil em cm/s -> m/s
    };
    struct LifeSkillBookRule {
        int itemId = 0;
        int skillId = 0;
        int addLevel = 0;
        int expectedLevel = 0;
        int prerequisiteSkill = 0;
        int prerequisiteLevel = 0;
        int rewardItem = 0;
    };
    SkillDef skills_[8]{};
    int nextSkillSlot_ = 0;
    bool hotbarLoaded_ = false;
    std::unordered_map<int, int> skillBooks_;
    std::unordered_set<int> inferredSkillBooks_;
    std::unordered_map<int, LifeSkillBookRule> lifeSkillBooks_;
    std::unordered_set<int> lifeSkillIds_;
    std::unordered_map<int, SkillRule> skillRules_;
    struct PassiveBonuses {
        int hp=0,sp=0,hit=0,dodge=0,hpRegen=0,spRegen=0,resistance=0;
        float moveCoefficient=0.f;
    };
    PassiveBonuses passiveBonuses() const;
    std::unordered_map<int,int> appliedSkillAttributes_;
    std::vector<BoatBuildOption> boatBuildOptions_;
    std::vector<int> ownedBoatIds_;
    std::unordered_map<int, int> ownedBoatHp_;
    std::unordered_map<int, int> ownedBoatSp_;
    std::unordered_set<int> historicalRecords_;
    std::unordered_map<int, int> learnedSkills_;
    std::unordered_map<int, int> lifeSkills_;
    int lifeSkillPoints_ = 0;
    Foe player_;
    std::vector<Foe> foes_;
    Quest quests_[3]{};
    int target_ = -1;
    int requestedLoot_ = -1;
    int exp_ = 0;
    int expNext_ = 25;
    int fame_ = 0; // ATTR_FAME; versioned FM1 optional save field
    int fairyCoinChestDay_ = 0;
    int fairyCoinChestOpened_ = 0;
    historical_stats::Experience experience_;
    int gold_ = 0;
    int hairStyleItemId_ = 0; // cosmetic model selected through the historical salon table
    int baseMin_ = 0;
    int baseMax_ = 0;
    int baseDef_ = 0;
    int baseResist_ = 0;
    int baseLuck_ = 5;
    float baseMoveSpeed_ = 4.8f;
    int sheetHp_ = 0;
    float restoredSp_ = -1.f;  // saves anteriores a 05-V nao possuíam SP atual
    int sheetSp_ = 0;
    bool sheetReady_ = false;
    bool saveFailureReported_ = false;
    bool saveRecoveredFromBackup_ = false;
    std::string saveRecoveryPath_;
    int job_ = 0;
    int ap_ = 0;
    int tp_ = 0;
    int bstr_ = 5;
    int bagi_ = 5;
    int bdex_ = 5;
    int bcon_ = 5;
    int bsta_ = 5;
    int attrMin_ = 0;
    int attrMax_ = 0;
    int attrDef_ = 0;
    int hit_ = 0;
    int flee_ = 0;
    int hrecGain_ = 1;
    int srecGain_ = 1;
    bool sitting_ = false;
    float playerNumbFor_ = 0.f;
    int playerNumbLevel_ = 0;
    float playerStunFor_ = 0.f;
    float playerPoisonFor_ = 0.f;
    float playerPoisonTick_ = 0.f;
    int playerPoisonLevel_ = 0;
    float playerFacing_ = 0.f; // radianos, sincronizado com o ator OpenGL antes do tique
    float vendorX_ = 0.f;
    float vendorY_ = 0.f;
    bool vendorOn_ = false;
    std::string vendorName_ = "Vendedor";
    std::vector<int> vendorStock_;
    bool shopOpen_ = false;
    int shopOffset_ = 0;
    std::string npcTalkName_;
    std::string npcTalkText_;
    float npcTalkFor_ = 0.f;
    bool npcTalkOpen_ = false;
    bool npcOffersTrade_ = false;
    bool npcOffersForge_ = false;
    struct MissionOffer {
        int id = 0;
        std::string title;
        std::string objective;
        float targetX = -1.f;
        float targetY = -1.f;
        int targetId = 0;
        int targetCount = 0;
        int rewardExp = 0;
        int rewardGold = 0;
        int minLevel = 1;
        int maxLevel = 0;
        MissionGate gate;
        bool turnInRequired = false;
        std::vector<MissionItem> neededItems;
        std::vector<MissionItem> rewardItems;
        bool active = false;
    } offer_;
    std::vector<QuestNpcMarker> questNpcMarkers_;
    std::vector<MissionOffer> npcMissionChoices_;
    std::vector<std::string> npcMissionDescriptions_;
    int npcMissionSelected_=-1;
    int npcMissionPage_=0;
    bool missionAvailable(const MissionOffer& m) const;
    // 05-AS: mission-local offer or explicit hand-in from this NPC.
    bool missionReadyAtNpc(int id, const std::string& npcName) const;
    // Offline catch-up: past-level quests remain eligible, minimum level stays enforced.
    static constexpr bool kAllowPastLevelQuests = true;
    std::string missionLockReason(int id,int minLevel,int maxLevel,const MissionGate& gate) const;
    std::unordered_map<int,MissionGate> npcMissionGates_;
    std::unordered_map<int,bool> npcMissionTurnIns_;
    std::unordered_map<int,HistoricalMission> npcMissionFlagRules_;
    std::unordered_map<int,std::vector<int>> missionsByRecord_;
    struct HistoricalProgress {
        int id = 0;
        std::string title;
        std::string objective;
        float targetX = -1.f;
        float targetY = -1.f;
        int rewardExp = 0;
        int rewardGold = 0;
        int targetId = 0;
        int targetCount = 0;
        int have = 0;
        std::vector<MissionItem> neededItems;
        std::vector<MissionItem> rewardItems;
        bool active = false;
        bool complete = false;
        bool turnInRequired = false;
        std::string turnInNpc;
        // 05-AQ: mission-scoped flags, independent from global completed records.
        std::vector<int> flags, requiredFlags, forbiddenFlags;
        // 05-AU: independent event counters, one per proven Lua trigger.
        std::vector<MissionEventTrigger> triggers;
        std::vector<int> triggerCounts;
    } historical_;
    std::vector<HistoricalProgress> additionalMissions_;
    mutable int journalSelectedId_ = 0;
    mutable int journalPage_ = 0;
    static constexpr std::size_t kMaxActiveMissions = 8;
    static constexpr std::size_t kMaxSavedExtraMissions = 128;
    float hereX_ = 0.f;
    float hereY_ = 0.f;
    bool hereSafeZone_ = false;
    float deadFor_ = 0.f;
    std::uint64_t respawnSerial_ = 0;
    float swing_ = 0.f;
    float swingPlaybackRate_ = 1.f;
    float attackWindup_ = 7.f / 30.f;
    float attackDuration_ = .7f;
    std::array<float, 55> actionDurations_{};
    std::array<float, 55> actionImpacts_{};
    int actionTimingMode_ = -1;
    void syncActionTiming(const ActorSkin& actors);
    float pendingAttack_ = 0.f;
    float boatCannonCd_ = 0.f;
    int pendingTarget_ = -1;
    int pendingSkill_ = -1;
    unsigned attackSequence_ = 0;
    int attackActionBase_ = 7;
    int castingActionBase_ = 0;
    float castingFor_ = 0.f;
    float castingPlaybackRate_ = 1.f;
    bool swingIsSkill_ = false;
    bool playerFightArea_ = true;
    float skillCd_[8]{};
    bool skillTestMode_ = false;
    int armedSkill_ = -1; // committed offensive skill waiting for hit timeline
    int requestedSkill_ = -1; // hotkey pressed; waiting for enemy/range; no SP spent
    // A target selection is not an attack command. Click issues one basic
    // strike, or castSkill arms an offensive skill until a foe is selected.
    bool pendingBasicStrike_ = false;
    float berserkFor_ = 0.f;
    float berserkAttackSpeedBonus_ = 0.f;
    float hardenFor_ = 0.f;
    int hardenDefense_ = 0;
    float itemHardenFor_ = 0.f;
    int itemHardenDefense_ = 0;
    float itemEtherCloverFor_ = 0.f;
    int itemEtherCloverDodge_ = 0;
    float itemBoatSpeedFor_ = 0.f;
    int itemBodyStateId_ = 0; // 1 Weightless, 2 Battle Array, 3 Full Body Armor
    float itemBodyStateFor_ = 0.f;
    float itemBlessedPotionFor_ = 0.f;
    float itemBodyMoveSpeedBonus_ = 0.f;
    int itemBodyAttackBonus_ = 0;
    int itemBodyDefenseBonus_ = 0;
    float willOfSteelFor_ = 0.f;
    int willOfSteelDefense_ = 0;
    float genericBuffFor_ = 0.f;
    int genericBuffAttack_ = 0;
    int genericBuffDefense_ = 0;
    float regen_ = 0.f;
    unsigned seed_ = 1;
    std::vector<std::string> log_;
    std::vector<Pop> pops_;
    std::vector<ScheduledEffect> scheduledEffects_;
    std::unordered_map<int, ItemInfo> items_;
    int fairyAutoFeedSlot_ = 2; // Lua item slot 3 (zero-based inventory index)
    int fairyAutoFeedNormalId_ = 2312;
    int fairyAutoFeedGreatId_ = 6842;
    double fairyAutoFeedThreshold_ = 0.5;
    float fairySystemTimer_ = 0.f;
    int fairySystemUid_ = 0;
    int fairyMaximumLevel_ = 42;
    int fairyNormalFruitMaximum_ = 42;
    int fairyImprovedFruitMaximum_ = 62;
    int fairyExperienceRate_ = 1;
    std::unordered_map<int, float> itemCooldownRemaining_;
    // Os estados de comidas 165..167 são separados dos buffs temporários de
    // skills para que o porte dos consumíveis não altere efeitos já prontos.
    std::array<FoodState, 3> foodStates_{};
    ItemAttributeState itemStrengthState_{};
    ItemAttributeState itemConstitutionState_{};
    ItemAttributeState itemAgilityState_{};
    ItemAttributeState itemAccuracyState_{};
    ItemAttributeState itemSpiritState_{};
    ItemAttributeState itemPotionStrengthState_{};
    ItemAttributeState itemPotionAgilityState_{};
    ItemAttributeState itemPotionConstitutionState_{};
    ItemAttributeState itemPotionAccuracyState_{};
    ItemAttributeState itemPotionSpiritState_{};
    ItemAttributeState itemPotionMspdState_{};
    ItemAttributeState itemPotionAttackState_{};
    ItemAttributeState itemPotionAspdState_{};
    ItemAttributeState itemPotionHitState_{};
    ItemAttributeState itemPotionMaxHpState_{};
    ItemAttributeState itemKaraVictoryState_{};
    ItemAttributeState itemLotteryDefenseState_{};
    ItemAttributeState itemLanternDefenseState_{};
    ItemAttributeState itemFairyMarchState_{};
    ItemAttributeState itemMaliciousHornState_{};
    ItemAllAttributeState itemInnocenceState_{};
    float itemForgeBoostFor_ = 0.f;
    int itemForgeBoostLevel_ = 0;
    float itemCombineBoostFor_ = 0.f;
    int itemCombineBoostLevel_ = 0;
    ItemBoostState experienceBoost_{};
    ItemBoostState dropRateBoost_{};
    ItemBoostState fairyGrowthBoost_{};
    item_random_rewards::Rules itemRandomRewards_;
    item_stat_rules::Rules itemStatRules_;
    item_production::Rules itemProductionRules_;
    item_gem_vouchers::Rules itemGemVoucherRules_;
    item_chests::DataRules historicalChestRules_;
    std::vector<Stack> bag_;
    int bagCapacity_ = 24; // initial 24; historical items 3088..3093 expand by four
    std::vector<Stack> temp_;
    std::vector<Stack> bank_;
    bool bankOpen_ = false;
    std::vector<Loot> loots_;
    std::unordered_map<int, int> worn_;
    socket_rules::Config socketConfig_;
    forge_rules::Catalog gemCatalog_; // regras literalizadas de variable.lua
    std::unordered_map<int, int> wornUid_; // slot de equipamento -> UID no inventario
    int nextItemUid_ = 1;
    void assignItemUids();
    bool collectInstance(const Stack& instance);
    std::vector<EffectEvent> effectEvents_;
};

}  // namespace top
