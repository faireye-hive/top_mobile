#include "top/Play.h"
#include "top/PlayCombatRules.h"
#include "top/JobNames.h"
#include "top/MissionCatalog.h"
#include "top/SavePaths.h"
#include "top/ProgressionPoints.h"
#include "top/NpcSystem.h"
#include "top/InventoryStackRules.h"
#include "top/InventoryCapacity.h"
#include "top/ItemInstancePersistence.h"
#include "top/ForgeInstancePersistence.h"
#include "top/ActionPose.h"
#include "top/MonsterAI.h"
#include "top/PathFinder.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <limits>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <tuple>

namespace top {
using namespace play_combat;

class PlaySelfTest {
public:
    using Foe = Play::Foe;
    using ItemInfo = Play::ItemInfo;
    using Stack = Play::Stack;
    using Quest = Play::Quest;
    using SkillRule = Play::SkillRule;
    using MissionItem = Play::MissionItem;
    using QuestNpcMarker = Play::QuestNpcMarker;
    using StarActionResult = Play::StarActionResult;

private:
    static int damageOf(int minAttack, int maxAttack, int level, int defense, int roll, int resistance = 0) {
        return Play::damageOf(minAttack, maxAttack, level, defense, roll, resistance);
    }

public:
    static bool run() {
    // A save that cannot create its adjacent temporary file must leave the
    // previous character data untouched. Use the filesystem's component
    // limit so the target is valid while the atomic-save suffix is not.
    char saveTestDirTemplate[] = "/tmp/top-save-atomic-XXXXXX";
    char* saveTestDirPath = ::mkdtemp(saveTestDirTemplate);
    if (!saveTestDirPath) {
        std::fprintf(stderr, "save: nao criou pasta isolada do teste atomico\n");
        return false;
    }
    const std::string saveTestDir(saveTestDirPath);
    const long maxComponent = ::pathconf(saveTestDir.c_str(), _PC_NAME_MAX);
    if (maxComponent <= 16) {
        ::rmdir(saveTestDir.c_str());
        std::fprintf(stderr, "save: limite de nome de arquivo indisponivel no teste\n");
        return false;
    }
    const std::string saveTestPath = saveTestDir + "/" +
                                     std::string(static_cast<std::size_t>(maxComponent - 1), 's');
    FILE* oldSaveFile = std::fopen(saveTestPath.c_str(), "wb");
    if (!oldSaveFile) {
        ::rmdir(saveTestDir.c_str());
        std::fprintf(stderr, "save: nao criou save anterior do teste atomico\n");
        return false;
    }
    std::fputs("previous-save-must-survive\n", oldSaveFile);
    const bool oldSaveWritten = std::fclose(oldSaveFile) == 0;
    Play failedSave;
    const bool unexpectedSaveSuccess = failedSave.save(saveTestPath);
    FILE* preservedSave = std::fopen(saveTestPath.c_str(), "rb");
    char preservedLine[64] = {};
    const bool preserved = preservedSave && std::fgets(preservedLine, sizeof preservedLine, preservedSave) &&
                           std::string(preservedLine) == "previous-save-must-survive\n";
    if (preservedSave) std::fclose(preservedSave);
    std::remove(saveTestPath.c_str());
    ::rmdir(saveTestDir.c_str());
    if (!oldSaveWritten || unexpectedSaveSuccess || !preserved) {
        std::fprintf(stderr, "save atomico: falha de escrita alterou o save anterior\n");
        return false;
    }
    char recoveryDirTemplate[] = "/tmp/top-save-recovery-XXXXXX";
    char* recoveryDirPath = ::mkdtemp(recoveryDirTemplate);
    if (!recoveryDirPath) {
        std::fprintf(stderr, "save: nao criou pasta isolada de recuperacao\n");
        return false;
    }
    const std::string recoveryPath = std::string(recoveryDirPath) + "/character.sav";
    Play backupWriter, recoveredCharacter, recoveredAgain;
    backupWriter.gold_ = 111;
    const bool firstRecoverySave = backupWriter.save(recoveryPath);
    backupWriter.gold_ = 222;
    const bool rotatedRecoverySave = backupWriter.save(recoveryPath);
    FILE* corruptActiveSave = std::fopen(recoveryPath.c_str(), "r+b");
    bool corrupted = false;
    if (corruptActiveSave && std::fseek(corruptActiveSave, -2, SEEK_END) == 0) {
        const int original = std::fgetc(corruptActiveSave);
        if (original != EOF && std::fseek(corruptActiveSave, -1, SEEK_CUR) == 0)
            corrupted = std::fputc(original == '0' ? '1' : '0', corruptActiveSave) != EOF;
    }
    if (corruptActiveSave) std::fclose(corruptActiveSave);
    recoveredCharacter.load(recoveryPath);
    const bool restoredFromBackup = firstRecoverySave && rotatedRecoverySave && corrupted &&
                                    recoveredCharacter.gold_ == 111 &&
                                    recoveredCharacter.saveRecoveredFromBackup_;
    recoveredCharacter.gold_ = 333;
    const bool savedAfterRecovery = recoveredCharacter.save(recoveryPath);
    corruptActiveSave = std::fopen(recoveryPath.c_str(), "wb");
    const bool corruptedAgain = corruptActiveSave &&
                                std::fputs("second-broken-primary\n", corruptActiveSave) >= 0;
    if (corruptActiveSave) std::fclose(corruptActiveSave);
    recoveredAgain.load(recoveryPath);
    const bool knownGoodBackupPreserved = savedAfterRecovery && corruptedAgain &&
                                          recoveredAgain.gold_ == 111 &&
                                          recoveredAgain.saveRecoveredFromBackup_;
    std::remove(recoveryPath.c_str());
    std::remove((recoveryPath + ".bak").c_str());
    ::rmdir(recoveryDirPath);
    if (!restoredFromBackup || !knownGoodBackupPreserved) {
        std::fprintf(stderr, "save: CRC1/recuperacao perdeu o backup valido apos corrupcao\n");
        return false;
    }
    char backupFailureDirTemplate[] = "/tmp/top-save-backup-failure-XXXXXX";
    char* backupFailureDirPath = ::mkdtemp(backupFailureDirTemplate);
    if (!backupFailureDirPath) {
        std::fprintf(stderr, "save: nao criou pasta isolada da falha de backup\n");
        return false;
    }
    const std::string backupFailurePath = std::string(backupFailureDirPath) + "/character.sav";
    Play backupFailureWriter, backupFailureCheck;
    backupFailureWriter.gold_ = 444;
    const bool initialBeforeBackupFailure = backupFailureWriter.save(backupFailurePath);
    const bool blockedBackupName = ::mkdir((backupFailurePath + ".bak").c_str(), 0700) == 0;
    backupFailureWriter.gold_ = 555;
    const bool incorrectlyOverwroteWithoutBackup = backupFailureWriter.save(backupFailurePath);
    backupFailureCheck.load(backupFailurePath);
    const bool primarySurvivedBackupFailure = initialBeforeBackupFailure && blockedBackupName &&
                                              !incorrectlyOverwroteWithoutBackup &&
                                              backupFailureCheck.gold_ == 444;
    ::rmdir((backupFailurePath + ".bak").c_str());
    std::remove(backupFailurePath.c_str());
    ::rmdir(backupFailureDirPath);
    if (!primarySurvivedBackupFailure) {
        std::fprintf(stderr, "save: falha ao rotacionar backup substituiu o save principal\n");
        return false;
    }
    char profileTestDirTemplate[] = "/tmp/top-save-profiles-XXXXXX";
    char* profileTestDirPath = ::mkdtemp(profileTestDirTemplate);
    if (!profileTestDirPath) {
        std::fprintf(stderr, "save: nao criou pasta de teste de perfis offline\n");
        return false;
    }
    const std::string profileTestDir(profileTestDirPath);
    Play firstSameRace, secondSameRace, restoredFirst, restoredSecond;
    firstSameRace.root_ = secondSameRace.root_ = restoredFirst.root_ = restoredSecond.root_ = profileTestDir;
    firstSameRace.player_.id = secondSameRace.player_.id = 3; // ambos Phyllis
    firstSameRace.profileId_ = restoredFirst.profileId_ = 101;
    secondSameRace.profileId_ = restoredSecond.profileId_ = 102;
    firstSameRace.gold_ = 111;
    secondSameRace.gold_ = 222;
    const std::string firstProfileSave = firstSameRace.savePath();
    const std::string secondProfileSave = secondSameRace.savePath();
    const bool profilesSaved = firstProfileSave != secondProfileSave &&
                               firstSameRace.save(firstProfileSave) && secondSameRace.save(secondProfileSave);
    restoredFirst.load(firstProfileSave);
    restoredSecond.load(secondProfileSave);
    const bool profilesSeparated = profilesSaved && restoredFirst.gold_ == 111 && restoredSecond.gold_ == 222;
    std::remove(firstProfileSave.c_str());
    std::remove(secondProfileSave.c_str());
    const std::string profileTestUserDir = profileTestDir + "/user";
    ::rmdir(profileTestUserDir.c_str());
    ::rmdir(profileTestDir.c_str());
    if (!profilesSeparated) {
        std::fprintf(stderr, "save: perfis da mesma raca compartilharam/alteraram o mesmo progresso\n");
        return false;
    }

    Play skillData;
    Play coefficientData;
    coefficientData.loadItems("Client/scripts/table/iteminfo.txt", {6281});
    const auto coefficientItem = coefficientData.items_.find(6281);
    if (coefficientItem == coefficientData.items_.end() ||
        coefficientItem->second.stats.minAttackCoefficient != 8 ||
        applyAttackCoefficient(1000, 8) != 1008 || applyAttackCoefficient(1000, -1000) != 0) {
        std::fprintf(stderr, "iteminfo: coeficientes de ataque min/max nao foram lidos/aplicados\n");
        return false;
    }
    skillData.loadSkillBooks("Client/scripts/offline/calculate/variable.lua");
    skillData.loadSkillBooks("../Client/scripts/offline/calculate/variable.lua");
    skillData.loadSkillBooks("../../Client/scripts/offline/calculate/variable.lua");
    skillData.loadLifeSkillBooks("Client/scripts/offline/script/calculate/variable.lua");
    skillData.loadLifeSkillBooks("../Client/scripts/offline/script/calculate/variable.lua");
    skillData.loadLifeSkillBooks("../../Client/scripts/offline/script/calculate/variable.lua");
    skillData.loadSkillRules("Client/scripts/table/skillinfo.txt");
    skillData.loadSkillRules("../Client/scripts/table/skillinfo.txt");
    skillData.loadSkillRules("../../Client/scripts/table/skillinfo.txt");
    skillData.loadItems("Client/scripts/table/iteminfo.txt",{});
    skillData.resolveNamedSkillBooks();
    const int unmappedBooks[]={3169,3171,3181,3182,3183,3184,3189,3199,3200,3201,3213,3214,3221};
    if(skillData.skillBooks_.size()<85) {
        std::fprintf(stderr,"skill book mapping: fallback historico nao cobriu os livros nomeados\n");return false;
    }
    for(int itemId:unmappedBooks) {
        if(!skillData.skillBooks_.count(itemId) || !skillData.inferredSkillBooks_.count(itemId)) {
            std::fprintf(stderr,"skill book mapping: item %d sem match exato unico\n",itemId);return false;
        }
    }
    skillData.job_=2;
    skillData.player_.level=1;
    skillData.tp_=5;
    skillData.bag_={Stack{3189,1}}; // Eagle's Eye: item/skill names match uniquely.
    skillData.clickBag(0);
    Play inferredBookWrongJob;
    inferredBookWrongJob.items_=skillData.items_;
    inferredBookWrongJob.skillBooks_=skillData.skillBooks_;
    inferredBookWrongJob.inferredSkillBooks_=skillData.inferredSkillBooks_;
    inferredBookWrongJob.skillRules_=skillData.skillRules_;
    inferredBookWrongJob.job_=5;
    inferredBookWrongJob.player_.level=30;
    inferredBookWrongJob.tp_=5;
    inferredBookWrongJob.bag_={Stack{3189,1}};
    inferredBookWrongJob.clickBag(0);
    if(skillData.learnedSkills_.count(89)!=1 || skillData.tp_!=4 || skillData.bagCount(3189)!=0 ||
       inferredBookWrongJob.learnedSkills_.count(89)!=0 || inferredBookWrongJob.tp_!=5 ||
       inferredBookWrongJob.bagCount(3189)!=1) {
        std::fprintf(stderr,"skill book mapping: fluxo aproximado nao respeitou classe/consumo\n");return false;
    }
    skillData.setSkillTestMode(true);
    if(!skillData.testIncreaseSkill(97) || skillData.learnedSkills_[97]!=1) {
        std::fprintf(stderr,"skill test: + gratuito nao aprendeu skill do catalogo\n");
        return false;
    }
    if(!skillData.testIncreaseSkill(338) || skillData.lifeSkills_[338]!=1 || skillData.learnedSkills_.count(338) ||
       skillData.bagCount(1068)!=1 || skillData.bag_.back().id!=1068 || skillData.bag_.back().extra.str!=1 ||
       skillData.bag_.back().maxEnergy!=10000 || skillData.bag_.back().energy!=1) {
        std::fprintf(stderr,"skill test: primeira habilidade profissional nao concedeu a ferramenta historica correta\n");
        return false;
    }
    skillData.setSkillTestMode(false);
    if(skillData.testIncreaseSkill(97)) {
        std::fprintf(stderr,"skill test: + gratuito funcionou fora do modo de QA\n");
        return false;
    }
    char skillSaveBlockTemplate[]="/tmp/top-skill-save-block-XXXXXX";
    const int skillSaveBlockFd=::mkstemp(skillSaveBlockTemplate);
    if(skillSaveBlockFd<0) {
        std::fprintf(stderr,"skill test: nao criou bloqueio de save\n");
        return false;
    }
    ::close(skillSaveBlockFd);
    Play skillSaveFailure;
    skillSaveFailure.root_=skillSaveBlockTemplate; // an ordinary file cannot contain the character save directory
    skillSaveFailure.items_=skillData.items_;
    skillSaveFailure.skillRules_=skillData.skillRules_;
    skillSaveFailure.lifeSkillBooks_=skillData.lifeSkillBooks_;
    skillSaveFailure.lifeSkillIds_=skillData.lifeSkillIds_;
    skillSaveFailure.bagCapacity_=skillData.bagCapacity_;
    skillSaveFailure.player_.id=1;
    skillSaveFailure.setSkillTestMode(true);
    const bool regularSkillSaveRejected=!skillSaveFailure.testIncreaseSkill(97) &&
        skillSaveFailure.learnedSkills_.count(97)==0;
    const bool lifeSkillSaveRejected=!skillSaveFailure.testIncreaseSkill(338) &&
        skillSaveFailure.lifeSkills_.count(338)==0 && skillSaveFailure.bagCount(1068)==0;
    ::unlink(skillSaveBlockTemplate);
    if(!regularSkillSaveRejected || !lifeSkillSaveRejected) {
        std::fprintf(stderr,"skill test: falha de save nao reverteu nivel/ferramenta inicial\n");
        return false;
    }
    Play trainerProbe;
    if(!trainerProbe.testSetJob(9) || trainerProbe.job_!=9 ||
       !trainerProbe.testSetJob(18) || trainerProbe.job_!=18) {
        std::fprintf(stderr,"class test trainer: mudanca imediata de profissao falhou\n");
        return false;
    }
    char donnaDirTemplate[]="/tmp/top-donna-wine-XXXXXX";
    char* donnaDir=::mkdtemp(donnaDirTemplate);
    if(!donnaDir) {
        std::fprintf(stderr,"Donna wine: nao criou pasta temporaria de save\n");
        return false;
    }
    Play donnaWine;
    donnaWine.root_=donnaDir;
    donnaWine.profileId_=19001;
    donnaWine.loadItems("Client/scripts/table/iteminfo.txt",{});
    donnaWine.gold_=99999;
    const bool donnaRejectsLowGold=!donnaWine.processNpcRecipe({},100000,{{4072,1}},"Donna - Eight Treasures Wine",true) &&
        donnaWine.gold_==99999 && donnaWine.bagCount(4072)==0;
    donnaWine.gold_=100000;
    donnaWine.bagCapacity_=1;
    donnaWine.bag_={Stack{1,1}};
    const bool donnaRejectsFullBag=!donnaWine.processNpcRecipe({},100000,{{4072,1}},"Donna - Eight Treasures Wine",true) &&
        donnaWine.gold_==100000 && donnaWine.bagCount(1)==1 && donnaWine.bagCount(4072)==0;
    donnaWine.bag_.clear();
    donnaWine.gold_=100000;
    const bool donnaPurchase=donnaWine.processNpcRecipe({},100000,{{4072,1}},"Donna - Eight Treasures Wine",true) &&
        donnaWine.gold_==0 && donnaWine.bagCount(4072)==1;
    Play donnaReloaded;
    donnaReloaded.root_=donnaDir;
    donnaReloaded.profileId_=19001;
    donnaReloaded.loadItems("Client/scripts/table/iteminfo.txt",{});
    donnaReloaded.load(donnaWine.savePath());
    const bool donnaSaved=donnaReloaded.gold_==0 && donnaReloaded.bagCount(4072)==1;
    const std::string donnaSave=donnaWine.savePath();
    std::remove(donnaSave.c_str());std::remove((donnaSave+".bak").c_str());
    const std::string donnaUser=donnaDir+std::string("/user");
    ::rmdir(donnaUser.c_str());::rmdir(donnaDir);
    if(!donnaRejectsLowGold || !donnaRejectsFullBag || !donnaPurchase || !donnaSaved) {
        std::fprintf(stderr,"Donna wine: compra, preservacao de recursos ou save falhou\n");
        return false;
    }
    Play williamGate;
    williamGate.historical_.id=248;
    williamGate.historical_.active=true;
    const bool williamActiveMissionGate=williamGate.hasActiveHistoricalMission(248);
    williamGate.historical_.active=false;
    williamGate.historicalRecords_.insert(249);
    const bool williamCompletedRecordGate=!williamGate.hasActiveHistoricalMission(248) &&
        williamGate.hasHistoricalRecord(249) && !williamGate.hasHistoricalRecord(250);
    char williamDirTemplate[]="/tmp/top-william-token-XXXXXX";
    char* williamDir=::mkdtemp(williamDirTemplate);
    if(!williamDir) {
        std::fprintf(stderr,"William token: nao criou pasta temporaria de save\n");
        return false;
    }
    Play williamSale;
    williamSale.root_=williamDir;
    williamSale.profileId_=19003;
    williamSale.loadItems("Client/scripts/table/iteminfo.txt",{});
    williamSale.gold_=19999;
    const bool williamRejectsLowGold=!williamSale.processNpcRecipe({},20000,{{4082,1}},
        "William - Ancient Bounty Token",true) && williamSale.gold_==19999 && williamSale.bagCount(4082)==0;
    williamSale.gold_=20000;
    williamSale.bagCapacity_=1;
    williamSale.bag_={Stack{1,1}};
    const bool williamRejectsFullBag=!williamSale.processNpcRecipe({},20000,{{4082,1}},
        "William - Ancient Bounty Token",true) && williamSale.gold_==20000 &&
        williamSale.bagCount(1)==1 && williamSale.bagCount(4082)==0;
    williamSale.bag_.clear();
    const bool williamPurchase=williamSale.processNpcRecipe({},20000,{{4082,1}},
        "William - Ancient Bounty Token",true) && williamSale.gold_==0 && williamSale.bagCount(4082)==1;
    Play williamReloaded;
    williamReloaded.root_=williamDir;
    williamReloaded.profileId_=19003;
    williamReloaded.loadItems("Client/scripts/table/iteminfo.txt",{});
    williamReloaded.load(williamSale.savePath());
    const bool williamSaved=williamReloaded.gold_==0 && williamReloaded.bagCount(4082)==1;
    const std::string williamSave=williamSale.savePath();
    std::remove(williamSave.c_str());std::remove((williamSave+".bak").c_str());
    const std::string williamUser=williamDir+std::string("/user");
    ::rmdir(williamUser.c_str());::rmdir(williamDir);
    if(!williamActiveMissionGate || !williamCompletedRecordGate || !williamRejectsLowGold ||
       !williamRejectsFullBag || !williamPurchase || !williamSaved) {
        std::fprintf(stderr,"William token: gate historico, cobranca, inventario ou save falhou\n");
        return false;
    }
    char furlingtonDirTemplate[]="/tmp/top-furlington-refine-XXXXXX";
    char* furlingtonDir=::mkdtemp(furlingtonDirTemplate);
    if(!furlingtonDir) {
        std::fprintf(stderr,"Furlington: nao criou pasta temporaria de save\n");
        return false;
    }
    Play furlington;
    furlington.root_=furlingtonDir;
    furlington.profileId_=19006;
    furlington.loadItems("Client/scripts/table/iteminfo.txt",{1784,1785});
    furlington.bagCapacity_=1;
    furlington.bag_={Stack{1784,10}};
    furlington.gold_=199;
    const bool furlingtonRejectsLowGold=!furlington.processNpcRecipe({{1784,10}},200,{{1785,1}},
        "Furlington - Clarion Fragment",false) && furlington.gold_==199 &&
        furlington.bagCount(1784)==10 && furlington.bagCount(1785)==0;
    furlington.gold_=200;
    const bool furlingtonExchange=furlington.processNpcRecipe({{1784,10}},200,{{1785,1}},
        "Furlington - Clarion Fragment",false) && furlington.gold_==0 &&
        furlington.bagCount(1784)==0 && furlington.bagCount(1785)==1;
    Play furlingtonReloaded;
    furlingtonReloaded.loadItems("Client/scripts/table/iteminfo.txt",{1784,1785});
    furlingtonReloaded.load(furlington.savePath());
    const bool furlingtonSaved=furlingtonReloaded.gold_==0 && furlingtonReloaded.bagCount(1784)==0 &&
        furlingtonReloaded.bagCount(1785)==1;
    const std::string furlingtonSave=furlington.savePath();
    std::remove(furlingtonSave.c_str());std::remove((furlingtonSave+".bak").c_str());
    const std::string furlingtonUser=furlingtonDir+std::string("/user");
    ::rmdir(furlingtonUser.c_str());::rmdir(furlingtonDir);
    char furlingtonSaveBlockTemplate[]="/tmp/top-furlington-save-block-XXXXXX";
    const int furlingtonSaveBlockFd=::mkstemp(furlingtonSaveBlockTemplate);
    if(furlingtonSaveBlockFd<0) {
        std::fprintf(stderr,"Furlington: nao criou bloqueio de save\n");
        return false;
    }
    ::close(furlingtonSaveBlockFd);
    Play furlingtonSaveFailure;
    furlingtonSaveFailure.root_=furlingtonSaveBlockTemplate;
    furlingtonSaveFailure.profileId_=19007;
    furlingtonSaveFailure.loadItems("Client/scripts/table/iteminfo.txt",{1784,1785});
    furlingtonSaveFailure.bag_={Stack{1784,10}};
    furlingtonSaveFailure.gold_=200;
    const bool furlingtonSaveRollback=!furlingtonSaveFailure.processNpcRecipe({{1784,10}},200,{{1785,1}},
        "Furlington - Clarion Fragment",false) && furlingtonSaveFailure.gold_==200 &&
        furlingtonSaveFailure.bagCount(1784)==10 && furlingtonSaveFailure.bagCount(1785)==0;
    ::unlink(furlingtonSaveBlockTemplate);
    if(!furlingtonRejectsLowGold || !furlingtonExchange || !furlingtonSaved || !furlingtonSaveRollback) {
        std::fprintf(stderr,"Furlington: materiais, ouro, espaço na bolsa ou save falhou\n");
        return false;
    }
    char fairySaveBlockTemplate[]="/tmp/top-fairy-save-block-XXXXXX";
    const int fairySaveBlockFd=::mkstemp(fairySaveBlockTemplate);
    if(fairySaveBlockFd<0) {
        std::fprintf(stderr,"fairy fruit: nao criou bloqueio de save\n");
        return false;
    }
    ::close(fairySaveBlockFd);
    Play fairySaveFailure;
    fairySaveFailure.root_=fairySaveBlockTemplate;
    fairySaveFailure.profileId_=19004;
    fairySaveFailure.loadItems("Client/scripts/table/iteminfo.txt",{183,222});
    fairySaveFailure.loadFairyFruits("Client/scripts/offline/script/calculate/variable.lua");
    const bool fairyFruitRuleLoaded=fairySaveFailure.items_.at(222).scriptAction=="FairyFruit" &&
        fairySaveFailure.items_.at(222).fairyFruitAttribute==0 &&
        fairySaveFailure.items_.at(222).fairyFruitLevels==1;
    Stack testFairy{183,1};
    testFairy.fairyStats={{1,1,1,1,1}};
    testFairy.maxEnergy=1200;
    testFairy.energy=1200;
    testFairy.maxDurability=20000;
    testFairy.durability=20000;
    fairySaveFailure.bag_={testFairy,Stack{222,1}};
    fairySaveFailure.assignItemUids();
    fairySaveFailure.worn_[16]=183;
    fairySaveFailure.wornUid_[16]=fairySaveFailure.bag_[0].uid;
    fairySaveFailure.seed_=13579;
    const unsigned fairySeedBefore=fairySaveFailure.seed_;
    fairySaveFailure.clickBag(1);
    ::unlink(fairySaveBlockTemplate);
    const bool fairySaveRollback=fairySaveFailure.bagCount(222)==1 &&
        fairySaveFailure.bag_[0].fairyStats==testFairy.fairyStats &&
        fairySaveFailure.bag_[0].energy==testFairy.energy &&
        fairySaveFailure.bag_[0].maxEnergy==testFairy.maxEnergy &&
        fairySaveFailure.seed_==fairySeedBefore;
    if(!fairyFruitRuleLoaded || !fairySaveRollback) {
        std::fprintf(stderr,"fairy fruit: falha de save consumiu fruta ou alterou crescimento\n");
        return false;
    }
    char hairDirTemplate[]="/tmp/top-hair-style-XXXXXX";
    char* hairDir=::mkdtemp(hairDirTemplate);
    if(!hairDir) {
        std::fprintf(stderr,"Cartel hair: nao criou pasta temporaria de save\n");
        return false;
    }
    Play hairTest;
    hairTest.root_=hairDir;
    hairTest.profileId_=19002;
    hairTest.loadItems("Client/scripts/table/iteminfo.txt",{});
    hairTest.gold_=100000;
    hairTest.bag_={Stack{1807,1},Stack{1808,1},Stack{1797,5},Stack{1806,1}};
    const bool hairApplied=hairTest.applyHistoricalHairStyle(2009,{{1807,1},{1808,1},{1797,5},{1806,1}},100000) &&
        hairTest.hairStyleItemId_==2009 && hairTest.gold_==0 && hairTest.bag_.empty();
    Play hairReloaded;
    hairReloaded.root_=hairDir;
    hairReloaded.profileId_=19002;
    hairReloaded.loadItems("Client/scripts/table/iteminfo.txt",{});
    hairReloaded.load(hairTest.savePath());
    const bool hairSaved=hairReloaded.hairStyleItemId_==2009 && hairReloaded.gold_==0 &&
        hairReloaded.bagCount(1807)==0 && hairReloaded.bagCount(1808)==0 &&
        hairReloaded.bagCount(1797)==0 && hairReloaded.bagCount(1806)==0;
    const std::string hairSave=hairTest.savePath();
    std::remove(hairSave.c_str());std::remove((hairSave+".bak").c_str());
    const std::string hairUser=hairDir+std::string("/user");
    ::rmdir(hairUser.c_str());::rmdir(hairDir);
    if(!hairApplied || !hairSaved) {
        std::fprintf(stderr,"Cartel hair: requisitos, cobranca ou persistencia do modelo falhou\n");
        return false;
    }
    char apparelDirTemplate[]="/tmp/top-apparel-converter-XXXXXX";
    char* apparelDir=::mkdtemp(apparelDirTemplate);
    if(!apparelDir) {
        std::fprintf(stderr,"apparel converter: nao criou pasta temporaria de save\n");
        return false;
    }
    const std::string apparelRoot=apparelDir;
    const std::string apparelUser=apparelRoot+"/user";
    if(::mkdir(apparelUser.c_str(),0700)!=0) {
        ::rmdir(apparelRoot.c_str());
        std::fprintf(stderr,"apparel converter: nao criou pasta de usuario\n");
        return false;
    }
    Play apparelTest;
    apparelTest.root_=apparelRoot;
    apparelTest.loadItems("Client/scripts/table/iteminfo.txt",{});
    apparelTest.bag_={Stack{9602,1},Stack{1,1}}; // historical converter + Short Sword
    apparelTest.assignItemUids();
    apparelTest.clickBag(0);
    apparelTest.clickBag(1);
    const auto madeApparel=std::find_if(apparelTest.bag_.begin(),apparelTest.bag_.end(),
        [](const Stack& stack){return stack.id==5291;});
    if(apparelTest.bag_.size()!=1 || apparelTest.bagCount(9602)!=0 || apparelTest.bagCount(1)!=0 ||
       madeApparel==apparelTest.bag_.end() || madeApparel->fuseId!=1 ||
       madeApparel->maxDurability!=25000 || madeApparel->durability!=25000 ||
       !apparelTest.save(apparelTest.savePath())) {
        std::remove(apparelTest.savePath().c_str());
        std::remove((apparelTest.savePath()+".bak").c_str());
        ::rmdir(apparelUser.c_str());::rmdir(apparelRoot.c_str());
        std::fprintf(stderr,"apparel converter: conversao historica ou FuseID nao foi persistido\n");
        return false;
    }
    Play apparelReloaded;
    apparelReloaded.loadItems("Client/scripts/table/iteminfo.txt",{});
    apparelReloaded.load(apparelTest.savePath());
    const auto savedApparel=std::find_if(apparelReloaded.bag_.begin(),apparelReloaded.bag_.end(),
        [](const Stack& stack){return stack.id==5291;});
    const bool apparelRoundTrip=savedApparel!=apparelReloaded.bag_.end() &&
        savedApparel->fuseId==1 && apparelReloaded.ensureApparelInfo(savedApparel->id,savedApparel->fuseId) &&
        apparelReloaded.items_.at(5291).maxDurability==25000 &&
        apparelReloaded.items_.at(5291).equipSlots==apparelReloaded.items_.at(1).equipSlots;
    Play apparelNoSpace;
    apparelNoSpace.items_=apparelTest.items_;
    apparelNoSpace.bagCapacity_=2;
    apparelNoSpace.bag_={Stack{9602,2},Stack{1,2}};
    apparelNoSpace.assignItemUids();
    apparelNoSpace.clickBag(0);
    apparelNoSpace.clickBag(1);
    const bool apparelFailurePreserved=apparelNoSpace.bag_.size()==2 &&
        apparelNoSpace.bagCount(9602)==2 && apparelNoSpace.bagCount(1)==2 &&
        apparelNoSpace.bagCount(5291)==0;
    std::remove(apparelTest.savePath().c_str());
    std::remove((apparelTest.savePath()+".bak").c_str());
    ::rmdir(apparelUser.c_str());::rmdir(apparelRoot.c_str());
    if(!apparelRoundTrip || !apparelFailurePreserved) {
        std::fprintf(stderr,"apparel converter: save/reload ou falha sem perda nao foi preservado\n");
        return false;
    }
    char randomBoxDirTemplate[]="/tmp/top-05bc-random-box-XXXXXX";
    char* randomBoxDir=::mkdtemp(randomBoxDirTemplate);
    if(!randomBoxDir) {
        std::fprintf(stderr,"05-BC: nao criou pasta temporaria de save\n");
        return false;
    }
    const std::string randomBoxRoot=randomBoxDir;
    const std::string randomBoxUser=randomBoxRoot+"/user";
    if(::mkdir(randomBoxUser.c_str(),0700)!=0) {
        ::rmdir(randomBoxRoot.c_str());
        std::fprintf(stderr,"05-BC: nao criou pasta de save temporaria\n");
        return false;
    }
    Play randomBox;
    randomBox.root_=randomBoxRoot;
    randomBox.profileId_=9056;
    randomBox.loadItems("Client/scripts/table/iteminfo.txt",{});
    const bool randomRulesLoaded=randomBox.itemRandomRewards_.load(
        "Client/scripts/offline/script/calculate/ItemEffect.lua",
        "Client/scripts/offline/calculate/variable.lua") &&
        randomBox.itemRandomRewards_.supports(88) &&
        randomBox.itemRandomRewards_.supports(3302);
    randomBox.bagCapacity_=2;
    randomBox.bag_.push_back(Stack{3905,1});
    randomBox.seed_=6752; // roll(10564)==0: historical first reward is item 3909.
    randomBox.clickBag(0);
    const std::string randomBoxSave=randomBox.savePath();
    Play randomBoxReloaded;
    randomBoxReloaded.load(randomBoxSave);
    const bool randomBoxSaved=randomBoxReloaded.bag_.size()==1 && randomBoxReloaded.bag_[0].id==3909 &&
        randomBoxReloaded.bag_[0].quality==5;
    Play blackMarketBox;
    blackMarketBox.root_=randomBoxRoot;
    blackMarketBox.profileId_=9061;
    blackMarketBox.loadItems("Client/scripts/table/iteminfo.txt",{});
    blackMarketBox.itemRandomRewards_.load("Client/scripts/offline/script/calculate/ItemEffect.lua",
                                           "Client/scripts/offline/calculate/variable.lua");
    blackMarketBox.bagCapacity_=2;
    blackMarketBox.bag_={Stack{88,1}};
    blackMarketBox.seed_=0;
    blackMarketBox.clickBag(0);
    const bool blackMarketReward=blackMarketBox.bag_.size()==1 &&
        blackMarketBox.bag_[0].id==1902 && blackMarketBox.bag_[0].quality==24;
    Play blackMarketNoSpace;
    blackMarketNoSpace.loadItems("Client/scripts/table/iteminfo.txt",{});
    blackMarketNoSpace.itemRandomRewards_.load("Client/scripts/offline/script/calculate/ItemEffect.lua",
                                                "Client/scripts/offline/calculate/variable.lua");
    blackMarketNoSpace.bagCapacity_=1;
    blackMarketNoSpace.bag_={Stack{88,1}};
    blackMarketNoSpace.clickBag(0);
    const bool blackMarketPreserved=blackMarketNoSpace.bag_.size()==1 &&
        blackMarketNoSpace.bag_[0].id==88;
    if(!randomRulesLoaded || !blackMarketReward || !blackMarketPreserved) {
        std::fprintf(stderr,"PORT-BLACKMARKET: tabela, recompensa ou preservacao falhou\n");
        return false;
    }
    Play corsairsEquipmentChest;
    corsairsEquipmentChest.loadItems("Client/scripts/table/iteminfo.txt",{});
    const bool corsairsChestRulesLoaded=corsairsEquipmentChest.historicalChestRules_.load(
        "Client/scripts/offline/script/calculate/ItemEffect.lua") &&
        corsairsEquipmentChest.historicalChestRules_.mappedItems()==53 &&
        corsairsEquipmentChest.historicalChestRules_.supports(617,"Corsairs_EquipmentChest") &&
        corsairsEquipmentChest.historicalChestRules_.supports(322,"Corsairs_WeaponChest") &&
        corsairsEquipmentChest.historicalChestRules_.supports(6350,"Corsairs_WeaponChest2");
    corsairsEquipmentChest.bagCapacity_=4;
    corsairsEquipmentChest.bag_={Stack{617,1}};
    corsairsEquipmentChest.clickBag(0);
    std::unordered_set<int> corsairsEquipmentIds;
    for(const Stack& value:corsairsEquipmentChest.bag_)
        if(value.quality==22)corsairsEquipmentIds.insert(value.id);
    const bool corsairsEquipmentReward=corsairsEquipmentIds==std::unordered_set<int>{299,475,651};
    Play corsairsWeaponChest;
    corsairsWeaponChest.loadItems("Client/scripts/table/iteminfo.txt",{});
    corsairsWeaponChest.historicalChestRules_.load("Client/scripts/offline/script/calculate/ItemEffect.lua");
    corsairsWeaponChest.bagCapacity_=3;
    corsairsWeaponChest.bag_={Stack{6350,1}};
    corsairsWeaponChest.seed_=0;
    corsairsWeaponChest.clickBag(0);
    const bool corsairsWeaponChoice=corsairsWeaponChest.bag_.size()==1 &&
        corsairsWeaponChest.bag_[0].id==4 && corsairsWeaponChest.bag_[0].quality==22;
    Play corsairsWeaponNoSpace;
    corsairsWeaponNoSpace.loadItems("Client/scripts/table/iteminfo.txt",{});
    corsairsWeaponNoSpace.historicalChestRules_.load("Client/scripts/offline/script/calculate/ItemEffect.lua");
    corsairsWeaponNoSpace.bagCapacity_=2;
    corsairsWeaponNoSpace.bag_={Stack{322,1}};
    corsairsWeaponNoSpace.clickBag(0);
    const bool corsairsWeaponPreserved=corsairsWeaponNoSpace.bag_.size()==1 &&
        corsairsWeaponNoSpace.bag_[0].id==322;
    Play corsairsWeaponAtSea;
    corsairsWeaponAtSea.loadItems("Client/scripts/table/iteminfo.txt",{});
    corsairsWeaponAtSea.historicalChestRules_.load("Client/scripts/offline/script/calculate/ItemEffect.lua");
    corsairsWeaponAtSea.bag_={Stack{617,1}};
    corsairsWeaponAtSea.usingTicketBoat_=true;
    corsairsWeaponAtSea.clickBag(0);
    const bool corsairsAtSeaPreserved=corsairsWeaponAtSea.bag_.size()==1 &&
        corsairsWeaponAtSea.bag_[0].id==617;
    if(!corsairsChestRulesLoaded || !corsairsEquipmentReward || !corsairsWeaponChoice ||
       !corsairsWeaponPreserved || !corsairsAtSeaPreserved) {
        std::fprintf(stderr,"PORT-CORSAIRS-CHESTS: tabela, conteudo ou protecao falhou\n");
        return false;
    }
    Play birthdayCake;
    birthdayCake.root_=randomBoxRoot;
    birthdayCake.profileId_=9070;
    birthdayCake.loadItems("Client/scripts/table/iteminfo.txt",{3240,3338});
    birthdayCake.ap_=10;
    birthdayCake.bag_={Stack{3240,1}};
    birthdayCake.clickBag(0);
    Play birthdayCakeReloaded;
    birthdayCakeReloaded.loadItems("Client/scripts/table/iteminfo.txt",{3240,3338});
    birthdayCakeReloaded.load(birthdayCake.savePath());
    const bool birthdayCakeSaved=birthdayCakeReloaded.ap_==13 &&
        birthdayCakeReloaded.bagCount(3240)==0;
    Play birthdayCakeBoat;
    birthdayCakeBoat.loadItems("Client/scripts/table/iteminfo.txt",{3240,3338});
    birthdayCakeBoat.ap_=10;
    birthdayCakeBoat.bag_={Stack{3338,1}};
    birthdayCakeBoat.usingTicketBoat_=true;
    birthdayCakeBoat.clickBag(0);
    const bool birthdayCakeBoatPreserved=birthdayCakeBoat.ap_==10 &&
        birthdayCakeBoat.bagCount(3338)==1;
    Play birthdayCakeCap;
    birthdayCakeCap.loadItems("Client/scripts/table/iteminfo.txt",{3240,3338});
    birthdayCakeCap.ap_=1000000000;
    birthdayCakeCap.bag_={Stack{3338,1}};
    birthdayCakeCap.clickBag(0);
    const bool birthdayCakeCapPreserved=birthdayCakeCap.ap_==1000000000 &&
        birthdayCakeCap.bagCount(3338)==1;
    if(!birthdayCakeSaved || !birthdayCakeBoatPreserved || !birthdayCakeCapPreserved) {
        std::fprintf(stderr,"PORT-BIRTH-CAKE: incremento, save ou recusas falharam\n");return false;
    }
    Play goldNote;
    goldNote.root_=randomBoxRoot;
    goldNote.profileId_=9071;
    goldNote.loadItems("Client/scripts/table/iteminfo.txt",{854});
    goldNote.gold_=123;
    goldNote.bag_={Stack{854,1}};
    goldNote.clickBag(0);
    Play goldNoteReloaded;
    goldNoteReloaded.loadItems("Client/scripts/table/iteminfo.txt",{854});
    goldNoteReloaded.load(goldNote.savePath());
    Play goldNoteCap;
    goldNoteCap.loadItems("Client/scripts/table/iteminfo.txt",{854});
    goldNoteCap.gold_=999900000;
    goldNoteCap.bag_={Stack{854,1}};
    goldNoteCap.clickBag(0);
    if(goldNoteReloaded.gold_!=1000123 || goldNoteReloaded.bagCount(854)!=0 ||
       goldNoteCap.gold_!=999900000 || goldNoteCap.bagCount(854)!=1) {
        std::fprintf(stderr,"PORT-GOLD-NOTE: ouro, save ou recusa no limite falharam\n");return false;
    }
    Play islandTeleporterFare;
    islandTeleporterFare.root_=randomBoxRoot;
    islandTeleporterFare.profileId_=9073;
    islandTeleporterFare.player_.level=40;
    const bool freeIslandTravel=islandTeleporterFare.historicalNpcTeleportFareReady() &&
        islandTeleporterFare.payHistoricalNpcTeleportFare() && islandTeleporterFare.gold_==0;
    islandTeleporterFare.player_.level=41;
    islandTeleporterFare.gold_=499;
    const bool rejectedIslandFare=!islandTeleporterFare.historicalNpcTeleportFareReady() &&
        !islandTeleporterFare.payHistoricalNpcTeleportFare() && islandTeleporterFare.gold_==499;
    islandTeleporterFare.gold_=500;
    const bool chargedIslandFare=islandTeleporterFare.historicalNpcTeleportFareReady() &&
        islandTeleporterFare.payHistoricalNpcTeleportFare() && islandTeleporterFare.gold_==0;
    Play islandTeleporterReloaded;
    islandTeleporterReloaded.load(islandTeleporterFare.savePath());
    const bool savedIslandFare=islandTeleporterReloaded.gold_==0;
    std::remove(islandTeleporterFare.savePath().c_str());
    std::remove((islandTeleporterFare.savePath()+".bak").c_str());
    if(!freeIslandTravel || !rejectedIslandFare || !chargedIslandFare || !savedIslandFare) {
        std::fprintf(stderr,"PORT-GARNER-TELEPORT: limite de nivel, tarifa ou save falhou\n");return false;
    }
    Play happyBook;
    happyBook.root_=randomBoxRoot;
    happyBook.profileId_=9072;
    happyBook.loadItems("Client/scripts/table/iteminfo.txt",{853,2989});
    happyBook.experience_.load("Client/scripts/offline/calculate/variable.lua");
    happyBook.player_.level=1;
    happyBook.expNext_=happyBook.experience_.needed(1);
    happyBook.bag_={Stack{853,1}};
    happyBook.clickBag(0);
    Play happyBookReloaded;
    happyBookReloaded.loadItems("Client/scripts/table/iteminfo.txt",{853,2989});
    happyBookReloaded.experience_.load("Client/scripts/offline/calculate/variable.lua");
    happyBookReloaded.load(happyBook.savePath());
    Play happyBookBoat;
    happyBookBoat.loadItems("Client/scripts/table/iteminfo.txt",{853,2989});
    happyBookBoat.experience_.load("Client/scripts/offline/calculate/variable.lua");
    happyBookBoat.player_.level=30;
    happyBookBoat.expNext_=happyBookBoat.experience_.needed(30);
    happyBookBoat.bag_={Stack{2989,1}};
    happyBookBoat.usingTicketBoat_=true;
    happyBookBoat.clickBag(0);
    Play happyBookTooHigh;
    happyBookTooHigh.loadItems("Client/scripts/table/iteminfo.txt",{853,2989});
    happyBookTooHigh.experience_.load("Client/scripts/offline/calculate/variable.lua");
    happyBookTooHigh.player_.level=86;
    happyBookTooHigh.expNext_=happyBookTooHigh.experience_.needed(86);
    happyBookTooHigh.bag_={Stack{2989,1}};
    happyBookTooHigh.clickBag(0);
    if(happyBookReloaded.player_.level!=6 || happyBookReloaded.exp_!=0 ||
       happyBookReloaded.bagCount(853)!=0 || happyBookBoat.player_.level!=30 ||
       happyBookBoat.bagCount(2989)!=1 || happyBookTooHigh.player_.level!=86 ||
       happyBookTooHigh.bagCount(2989)!=1) {
        std::fprintf(stderr,"PORT-HAPPY-BOOK: EXP, save ou recusas falharam\n");return false;
    }
    Play aprilFruitExp;
    aprilFruitExp.root_=randomBoxRoot;
    aprilFruitExp.profileId_=9074;
    aprilFruitExp.loadItems("Client/scripts/table/iteminfo.txt",{2953,992});
    aprilFruitExp.experience_.load("Client/scripts/offline/calculate/variable.lua");
    aprilFruitExp.player_.level=30;
    aprilFruitExp.expNext_=aprilFruitExp.experience_.needed(30);
    const int aprilIncrement=static_cast<int>(std::floor(aprilFruitExp.expNext_*.03));
    aprilFruitExp.exp_=0;
    aprilFruitExp.bag_={Stack{2953,1}};
    aprilFruitExp.clickBag(0);
    Play aprilFruitReloaded;
    aprilFruitReloaded.loadItems("Client/scripts/table/iteminfo.txt",{2953,992});
    aprilFruitReloaded.experience_.load("Client/scripts/offline/calculate/variable.lua");
    aprilFruitReloaded.load(aprilFruitExp.savePath());
    const bool aprilFruitLevelSaved=aprilFruitReloaded.player_.level==30 &&
        aprilFruitReloaded.exp_==aprilIncrement &&
        aprilFruitReloaded.bagCount(2953)==0;
    Play fastFoodExp;
    fastFoodExp.root_=randomBoxRoot;
    fastFoodExp.profileId_=9076;
    fastFoodExp.loadItems("Client/scripts/table/iteminfo.txt",{992});
    fastFoodExp.experience_.load("Client/scripts/offline/calculate/variable.lua");
    fastFoodExp.player_.level=86;
    fastFoodExp.expNext_=fastFoodExp.experience_.needed(86);
    const int fastFoodIncrement=static_cast<int>(std::floor(fastFoodExp.expNext_*.01));
    fastFoodExp.exp_=fastFoodExp.expNext_-fastFoodIncrement+1;
    fastFoodExp.bag_={Stack{992,1}};
    fastFoodExp.clickBag(0);
    Play fastFoodReloaded;
    fastFoodReloaded.loadItems("Client/scripts/table/iteminfo.txt",{992});
    fastFoodReloaded.experience_.load("Client/scripts/offline/calculate/variable.lua");
    fastFoodReloaded.load(fastFoodExp.savePath());
    const bool fastFoodLevelUp=fastFoodExp.player_.level==87 && fastFoodExp.exp_==1 &&
        fastFoodExp.bagCount(992)==0 && fastFoodReloaded.player_.level==87 &&
        fastFoodReloaded.exp_==1 && fastFoodReloaded.bagCount(992)==0;
    Play fastFoodLowLevel;
    fastFoodLowLevel.loadItems("Client/scripts/table/iteminfo.txt",{992});
    fastFoodLowLevel.experience_.load("Client/scripts/offline/calculate/variable.lua");
    fastFoodLowLevel.player_.level=85;
    fastFoodLowLevel.expNext_=fastFoodLowLevel.experience_.needed(85);
    fastFoodLowLevel.bag_={Stack{992,1}};
    fastFoodLowLevel.clickBag(0);
    Play fastFoodAtSea;
    fastFoodAtSea.loadItems("Client/scripts/table/iteminfo.txt",{992});
    fastFoodAtSea.experience_.load("Client/scripts/offline/calculate/variable.lua");
    fastFoodAtSea.player_.level=86;
    fastFoodAtSea.expNext_=fastFoodAtSea.experience_.needed(86);
    fastFoodAtSea.bag_={Stack{992,1}};
    fastFoodAtSea.usingTicketBoat_=true;
    fastFoodAtSea.clickBag(0);
    const bool fastFoodGates=fastFoodLowLevel.player_.level==85 && fastFoodLowLevel.bagCount(992)==1 &&
        fastFoodAtSea.player_.level==86 && fastFoodAtSea.bagCount(992)==1;
    char expSaveBlockTemplate[]="/tmp/top-april-exp-save-block-XXXXXX";
    const int expSaveBlockFd=::mkstemp(expSaveBlockTemplate);
    if(expSaveBlockFd<0) {
        std::fprintf(stderr,"ItemUse_Foolish: nao criou bloqueio de save\n");return false;
    }
    ::close(expSaveBlockFd);
    Play aprilFruitSaveFailure;
    aprilFruitSaveFailure.root_=expSaveBlockTemplate;
    aprilFruitSaveFailure.profileId_=9075;
    aprilFruitSaveFailure.loadItems("Client/scripts/table/iteminfo.txt",{2953});
    aprilFruitSaveFailure.experience_.load("Client/scripts/offline/calculate/variable.lua");
    aprilFruitSaveFailure.player_.level=30;
    aprilFruitSaveFailure.expNext_=aprilFruitSaveFailure.experience_.needed(30);
    aprilFruitSaveFailure.exp_=0;
    aprilFruitSaveFailure.bag_={Stack{2953,1}};
    const int oldAprilLevel=aprilFruitSaveFailure.player_.level;
    const int oldAprilExp=aprilFruitSaveFailure.exp_;
    aprilFruitSaveFailure.clickBag(0);
    ::unlink(expSaveBlockTemplate);
    const bool aprilFruitSaveRollback=aprilFruitSaveFailure.player_.level==oldAprilLevel &&
        aprilFruitSaveFailure.exp_==oldAprilExp && aprilFruitSaveFailure.bagCount(2953)==1;
    const std::string aprilFruitSave=aprilFruitExp.savePath();
    std::remove(aprilFruitSave.c_str());std::remove((aprilFruitSave+".bak").c_str());
    const std::string fastFoodSave=fastFoodExp.savePath();
    std::remove(fastFoodSave.c_str());std::remove((fastFoodSave+".bak").c_str());
    if(!aprilFruitLevelSaved || !fastFoodLevelUp || !fastFoodGates || !aprilFruitSaveRollback) {
        std::fprintf(stderr,"ItemUse_Foolish/CZKC: formula, level up, save/reload ou rollback falhou (%d,%d,%d); result levels %d/%d exp %d/%d\n",
            aprilFruitLevelSaved?1:0,fastFoodLevelUp?1:0,aprilFruitSaveRollback?1:0,
            aprilFruitReloaded.player_.level,fastFoodExp.player_.level,
            aprilFruitReloaded.exp_,fastFoodExp.exp_);
        return false;
    }
    Play amplifiers;
    amplifiers.loadItems("Client/scripts/table/iteminfo.txt",{1,48,56,578,898,993,998,1006,1128,3039,
        3094,3095,3096,3097,3844,3845,3880,3881,3882});
    bool amplifierRulesValidated=true;
    for(const item_amplifier::Rule& rule:item_amplifier::rules) {
        Play probe;
        probe.loadItems("Client/scripts/table/iteminfo.txt",{rule.itemId});
        probe.player_.level=rule.minimumLevel>0?rule.minimumLevel:30;
        probe.bag_={Stack{rule.itemId,1}};
        probe.clickBag(0);
        if(rule.effect==item_amplifier::Effect::FairyGrowth) {
            amplifierRulesValidated=amplifierRulesValidated && probe.bagCount(rule.itemId)==0 &&
                probe.fairyGrowthBoost_.multiplierPercent==rule.multiplierPercent &&
                probe.fairyGrowthBoost_.remaining==static_cast<float>(rule.durationSeconds);
        } else {
            const Play::ItemBoostState& state=rule.effect==item_amplifier::Effect::Experience?
                probe.experienceBoost_:probe.dropRateBoost_;
            amplifierRulesValidated=amplifierRulesValidated && probe.bagCount(rule.itemId)==0 &&
                state.multiplierPercent==rule.multiplierPercent &&
                state.remaining==static_cast<float>(rule.durationSeconds);
        }
    }
    amplifiers.player_.level=30;
    amplifiers.expNext_=10000;
    amplifiers.bag_={Stack{898,1}};
    amplifiers.clickBag(0);
    Foe amplifierFoe;
    amplifierFoe.id=99999;
    amplifierFoe.name="Amplifier verification target";
    amplifierFoe.level=10;
    amplifierFoe.expReward=100;
    amplifiers.onKill(amplifierFoe);
    amplifiers.bag_={Stack{1006,1}};
    amplifiers.clickBag(0);
    Foe boostedLootFoe;
    boostedLootFoe.id=99998;
    boostedLootFoe.level=10;
    boostedLootFoe.drops.push_back({1,9000});
    amplifiers.dropLoot(boostedLootFoe);
    amplifiers.bag_={Stack{998,1}};
    amplifiers.player_.level=30;
    amplifiers.clickBag(0);
    const bool rejectedAmplifierPreserved=amplifiers.bagCount(998)==1;
    amplifiers.bag_={Stack{578,1}};
    amplifiers.clickBag(0);
    const bool fairyGrowthAmplifierConsumed=amplifiers.bagCount(578)==0 &&
        amplifiers.fairyGrowthBoost_.multiplierPercent==200 && amplifiers.fairyGrowthBoost_.remaining==900.f;
    amplifiers.experienceBoost_.remaining=0.05f;
    amplifiers.step(0.05f,nullptr,0.f,0.f);
    if(amplifiers.exp_!=200 || amplifiers.experienceBoost_.multiplierPercent!=100 ||
       amplifiers.bagCount(898)!=0 || amplifiers.dropRateBoost_.multiplierPercent!=200 ||
       amplifiers.bagCount(1006)!=0 || amplifiers.loots_.size()!=1 || !amplifierRulesValidated || !rejectedAmplifierPreserved ||
       !fairyGrowthAmplifierConsumed || amplifiers.dropRateBoost_.remaining<=0.f) {
        std::fprintf(stderr,"PORT-AMPLIFIER: EXP/drop, validade ou recusas falharam\n");return false;
    }
    Play gemVoucher;
    gemVoucher.root_=randomBoxRoot;
    gemVoucher.profileId_=9060;
    gemVoucher.loadItems("Client/scripts/table/iteminfo.txt",{});
    const bool gemVoucherRulesLoaded=gemVoucher.itemGemVoucherRules_.load(
        "Client/scripts/offline/script/calculate/ItemEffect.lua");
    gemVoucher.bagCapacity_=2;
    gemVoucher.bag_={Stack{9612,1}};
    gemVoucher.usingTicketBoat_=true;
    gemVoucher.clickBag(0);
    const bool gemVoucherBoatPreserved=gemVoucher.bag_.size()==1 && gemVoucher.bag_[0].id==9612;
    gemVoucher.usingTicketBoat_=false;
    gemVoucher.seed_=1;
    gemVoucher.clickBag(0);
    const std::string gemVoucherSave=gemVoucher.savePath();
    Play gemVoucherReloaded;
    gemVoucherReloaded.load(gemVoucherSave);
    const int gemVoucherReward=gemVoucherReloaded.bag_.size()==1 ? gemVoucherReloaded.bag_[0].id : 0;
    const bool gemVoucherCommitted=gemVoucherRulesLoaded && gemVoucherBoatPreserved &&
        (gemVoucherReward==6832 || gemVoucherReward==6833 || gemVoucherReward==6834) &&
        gemVoucherReloaded.bag_[0].quality==4 && gemVoucherReloaded.bagCount(9612)==0;
    Play fullGemVoucher;
    fullGemVoucher.loadItems("Client/scripts/table/iteminfo.txt",{});
    fullGemVoucher.itemGemVoucherRules_.load("Client/scripts/offline/script/calculate/ItemEffect.lua");
    fullGemVoucher.bagCapacity_=1;
    fullGemVoucher.bag_={Stack{9612,1}};
    fullGemVoucher.clickBag(0);
    const bool fullGemVoucherPreserved=fullGemVoucher.bag_.size()==1 &&
        fullGemVoucher.bag_[0].id==9612 && fullGemVoucher.bag_[0].count==1;
    Play fullBagBox;
    fullBagBox.loadItems("Client/scripts/table/iteminfo.txt",{});
    fullBagBox.itemRandomRewards_.load("Client/scripts/offline/script/calculate/ItemEffect.lua",
                                       "Client/scripts/offline/calculate/variable.lua");
    fullBagBox.bagCapacity_=1;
    fullBagBox.bag_.push_back(Stack{3905,1});
    fullBagBox.clickBag(0);
    const bool fullBagPreserved=fullBagBox.bag_.size()==1 &&
        fullBagBox.bag_[0].id==3905 && fullBagBox.bag_[0].count==1;
    Play blueprint;
    blueprint.root_=randomBoxRoot;
    blueprint.profileId_=9058;
    blueprint.loadItems("Client/scripts/table/iteminfo.txt",{});
    const bool productionRulesLoaded=blueprint.itemProductionRules_.load(
        "Client/scripts/offline/script/calculate/variable.lua");
    blueprint.bagCapacity_=2;
    blueprint.bag_.push_back(Stack{1000,1});
    blueprint.seed_=8291;
    blueprint.clickBag(0);
    const std::string blueprintSave=blueprint.savePath();
    Play blueprintReloaded;
    blueprintReloaded.load(blueprintSave);
    const bool blueprintCreated=blueprint.bag_.size()==1 &&
        (blueprint.bag_[0].id==2300 || blueprint.bag_[0].id==2301 || blueprint.bag_[0].id==2302) &&
        blueprint.bag_[0].extra.str>0 && blueprint.bag_[0].extra.con>0 &&
        blueprint.bag_[0].extra.dex>0 && blueprint.bag_[0].extra.agi>0 &&
        blueprint.bag_[0].blueprintParts[0]>=3 && blueprint.bag_[0].blueprintParts[1]>=2 &&
        blueprint.bag_[0].blueprintParts[2]>0 && blueprint.bag_[0].itemValStaTenths>=10;
    const bool blueprintPersisted=blueprintCreated && blueprintReloaded.bag_.size()==1 &&
        blueprintReloaded.bag_[0].id==blueprint.bag_[0].id &&
        blueprintReloaded.bag_[0].extra.agi==blueprint.bag_[0].extra.agi &&
        blueprintReloaded.bag_[0].extra.str==blueprint.bag_[0].extra.str &&
        blueprintReloaded.bag_[0].itemValStaTenths==blueprint.bag_[0].itemValStaTenths &&
        blueprintReloaded.bag_[0].blueprintParts==blueprint.bag_[0].blueprintParts;
    Play production;
    production.root_=randomBoxRoot;
    production.profileId_=9059;
    production.bagCapacity_=24;
    auto productionItem=[&](int id,int type,int maxStack=1,const char* name="") {
        ItemInfo info;info.id=id;info.type=type;info.maxStack=maxStack;info.name=name;
        production.items_[id]=std::move(info);
    };
    productionItem(7000,59,1,"Fairy");
    productionItem(2300,69,1,"Manufacturing Blueprint");
    productionItem(1068,0,1,"Manufacturing Tool");
    productionItem(855,0,99,"Fairy Coin");
    productionItem(501,0,99,"Material A");
    productionItem(502,0,99,"Material B");
    productionItem(503,0,99,"Material C");
    productionItem(9000,1,99,"Production Test Item");
    production.lifeSkills_[338]=10;
    Stack productionFairy{7000,1};productionFairy.uid=10001;
    productionFairy.durability=100;productionFairy.maxDurability=100;
    productionFairy.fairySkills={13,1,0,0,0,0};
    Stack productionPaper{2300,1};productionPaper.uid=10002;productionPaper.maxDurability=1;
    productionPaper.durability=1;productionPaper.maxEnergy=100;productionPaper.itemValStaTenths=20;
    productionPaper.extra.str=501;productionPaper.extra.con=502;productionPaper.extra.dex=503;
    productionPaper.extra.agi=9000;productionPaper.blueprintParts={1,1,1};
    Stack productionTool{1068,1};productionTool.uid=10003;
    productionTool.extra.str=1;productionTool.durability=100;productionTool.maxDurability=100;
    productionTool.maxEnergy=10000;
    production.bag_={productionFairy,productionPaper,productionTool,Stack{855,1},
                     Stack{501,1},Stack{502,1},Stack{503,1}};
    production.seed_=1; // star check 9 follows the historical guaranteed-success branch.
    const auto productionPreview=production.previewProduction(0,1,2);
    const bool productionPreviewValid=productionPreview.valid && productionPreview.productId==9000 &&
        productionPreview.fee==1 && productionPreview.materialIds==std::array<int,3>{501,502,503};
    const bool productionSucceeded=productionPreviewValid && production.processProduction(0,1,2);
    const std::string productionSave=production.savePath();
    Play productionReloaded;
    productionReloaded.items_=production.items_;
    productionReloaded.load(productionSave);
    bool productionPersisted=productionSucceeded && productionReloaded.bag_.size()==4;
    if(productionPersisted) {
        bool outputFound=false,usedBlueprintFound=false,fairyFound=false,usedToolFound=false;
        for(const Stack& stack:productionReloaded.bag_) {
            if(stack.id==9000 && stack.count==3) outputFound=true;
            if(stack.id==2300 && stack.itemValStaTenths==10) usedBlueprintFound=true;
            if(stack.id==7000 && stack.durability==100) fairyFound=true;
            if(stack.id==1068 && stack.durability==50 && stack.energy==1) usedToolFound=true;
        }
        productionPersisted=outputFound&&usedBlueprintFound&&fairyFound&&usedToolFound &&
            productionReloaded.bagCount(855)==0 && productionReloaded.bagCount(501)==0 &&
            productionReloaded.bagCount(502)==0 && productionReloaded.bagCount(503)==0;
    }
    Play productionRejected;
    productionRejected.items_=production.items_;
    productionRejected.bagCapacity_=24;
    productionRejected.bag_={productionFairy,productionPaper,productionTool,Stack{855,1},
                             Stack{501,1},Stack{502,1}};
    const bool productionFailurePreserved=!productionRejected.previewProduction(0,1,2).valid &&
        productionRejected.bagCount(855)==1 && productionRejected.bagCount(501)==1 &&
        productionRejected.bagCount(502)==1 && productionRejected.bagCount(503)==0 &&
        !productionRejected.processProduction(0,1,2) && productionRejected.bag_.size()==6 &&
        productionRejected.bagCount(855)==1 && productionRejected.bagCount(501)==1 &&
        productionRejected.bagCount(502)==1;
    std::remove(productionSave.c_str());
    std::remove((productionSave+".bak").c_str());
    Play statBook;
    statBook.root_=randomBoxRoot;
    statBook.profileId_=9057;
    statBook.loadItems("Client/scripts/table/iteminfo.txt",{899});
    const bool statRulesLoaded=statBook.itemStatRules_.load("Client/scripts/offline/calculate/variable.lua");
    statBook.sheetReady_=true;
    statBook.bstr_=10;
    statBook.ap_=0;
    statBook.player_.level=1;
    statBook.player_.maxHp=100;
    statBook.player_.hp=100;
    statBook.player_.homeX=100.f;
    statBook.player_.homeY=100.f;
    statBook.bagCapacity_=2;
    statBook.bag_.push_back(Stack{899,1});
    statBook.clickBag(0);
    const std::string statBookSave=statBook.savePath();
    Play statBookReloaded;
    statBookReloaded.load(statBookSave);
    const bool statBookSaved=statBook.bstr_==9 && statBook.ap_==1 && statBook.bag_.empty() &&
        statBookReloaded.bstr_==9 && statBookReloaded.ap_==1 && statBookReloaded.bag_.empty();
    Play statBookBoundary;
    statBookBoundary.loadItems("Client/scripts/table/iteminfo.txt",{899});
    statBookBoundary.itemStatRules_.load("Client/scripts/offline/calculate/variable.lua");
    statBookBoundary.bstr_=5;
    statBookBoundary.bagCapacity_=2;
    statBookBoundary.bag_.push_back(Stack{899,1});
    statBookBoundary.clickBag(0);
    const bool statBoundaryPreserved=statBookBoundary.bstr_==5 && statBookBoundary.ap_==0 &&
        statBookBoundary.bag_.size()==1 && statBookBoundary.bag_[0].id==899;
    Play statBookBoat;
    statBookBoat.loadItems("Client/scripts/table/iteminfo.txt",{899});
    statBookBoat.itemStatRules_.load("Client/scripts/offline/calculate/variable.lua");
    statBookBoat.bstr_=10;
    statBookBoat.bagCapacity_=2;
    statBookBoat.bag_.push_back(Stack{899,1});
    statBookBoat.setTicketBoatMode(true);
    statBookBoat.clickBag(0);
    const bool statBoatPreserved=statBookBoat.bstr_==10 && statBookBoat.ap_==0 &&
        statBookBoat.bag_.size()==1 && statBookBoat.bag_[0].id==899;
    std::remove(statBookSave.c_str());
    std::remove((statBookSave+".bak").c_str());
    std::remove(randomBoxSave.c_str());
    std::remove((randomBoxSave+".bak").c_str());
    std::remove(gemVoucherSave.c_str());
    std::remove((gemVoucherSave+".bak").c_str());
    std::remove(blueprintSave.c_str());
    std::remove((blueprintSave+".bak").c_str());
    ::rmdir(randomBoxUser.c_str());
    ::rmdir(randomBoxRoot.c_str());
    if(!randomRulesLoaded || randomBox.bag_.size()!=1 || randomBox.bag_[0].id!=3909 ||
       !randomBoxSaved || !fullBagPreserved || !gemVoucherRulesLoaded || !gemVoucherCommitted ||
       !gemVoucherBoatPreserved || !fullGemVoucherPreserved || !productionRulesLoaded || !blueprintCreated ||
       !blueprintPersisted || !productionPreviewValid || !productionSucceeded ||
       !productionPersisted || !productionFailurePreserved || !statRulesLoaded || !statBookSaved ||
       !statBoundaryPreserved || !statBoatPreserved) {
        std::fprintf(stderr,"05-BC/BE/BF/BD: random=%d saved=%d full=%d voucherRules=%d voucher=%d voucherBoat=%d voucherFull=%d rules=%d blueprint=%d persisted=%d prodPreview=%d prod=%d prodSave=%d prodReject=%d stats=%d statSave=%d boundary=%d boat=%d\n",
            randomRulesLoaded&&randomBox.bag_.size()==1&&randomBox.bag_[0].id==3909,
            randomBoxSaved,fullBagPreserved,gemVoucherRulesLoaded,gemVoucherCommitted,
            gemVoucherBoatPreserved,fullGemVoucherPreserved,productionRulesLoaded,blueprintCreated,
            blueprintPersisted,productionPreviewValid,productionSucceeded,productionPersisted,
            productionFailurePreserved,statRulesLoaded,statBookSaved,statBoundaryPreserved,
            statBoatPreserved);
        return false;
    }
    const auto healData = skillData.skillRules_.find(97);
    const auto willOfSteelData = skillData.skillRules_.find(63);
    const auto berserkData = skillData.skillRules_.find(84);
    const auto hardenData = skillData.skillRules_.find(225);
    const auto eagleData = skillData.skillRules_.find(89);
    const auto dragonFlightData = skillData.skillRules_.find(277);
    const auto fairyBodyData = skillData.skillRules_.find(280);
    const auto phantomSlashData = skillData.skillRules_.find(47);
    const auto illusionSlashData = skillData.skillRules_.find(81);
    const auto mightyStrikeData = skillData.skillRules_.find(82);
    const auto primalRageData = skillData.skillRules_.find(83);
    const auto doubleShotData = skillData.skillRules_.find(90);
    const auto frozenArrowData = skillData.skillRules_.find(93);
    const auto howlData = skillData.skillRules_.find(107);
    const auto meteorShowerData = skillData.skillRules_.find(112);
    const auto bareHandFormula = skillData.skillRules_.find(25);
    const auto meleeFormula = skillData.skillRules_.find(28);
    const auto rangedFormula = skillData.skillRules_.find(30);
    const auto staffFormula = skillData.skillRules_.find(36);
    const auto backstabData = skillData.skillRules_.find(86);
    const auto hunterStrikeData = skillData.skillRules_.find(91);
    const auto penetratingBulletData = skillData.skillRules_.find(115);
    const auto infernoBlastData = skillData.skillRules_.find(61);
    const auto crippleData = skillData.skillRules_.find(94);
    const auto barbaricCrushData = skillData.skillRules_.find(108);
    const auto dispersionBulletData = skillData.skillRules_.find(114);
    const auto directionalBackstabData = skillData.skillRules_.find(85);
    const auto lairAttackData = skillData.skillRules_.find(236);
    const auto colossusSmashData = skillData.skillRules_.find(238);
    const auto tempestBladeData = skillData.skillRules_.find(240);
    const auto lightningBoltData = skillData.skillRules_.find(214);
    const auto conchRayData = skillData.skillRules_.find(216);
    const auto poisonDartData = skillData.skillRules_.find(87);
    const auto enfeebleData = skillData.skillRules_.find(95);
    const auto monsterPoisonDartData = skillData.skillRules_.find(291);
    const auto blackHyzData = skillData.skillRules_.find(283);
    const auto hqgjData = skillData.skillRules_.find(288);
    const auto wllkData = skillData.skillRules_.find(299);
    const auto wlfdData = skillData.skillRules_.find(310);
    const auto wlsjData = skillData.skillRules_.find(305);
    const auto hyzHxData = skillData.skillRules_.find(325);
    const auto cannonShotData = skillData.skillRules_.find(27);
    const auto bombardmentData = skillData.skillRules_.find(233);
    const auto breakArmorData = skillData.skillRules_.find(222);
    const auto bareHandData = skillData.skillRules_.find(25);
    const auto bowData = skillData.skillRules_.find(30);
    // 05-F: apenas habilidades com projétil histórico recebem Flight.
    const auto arrowData = skillData.skillRules_.find(5);  // Fiery Arrow
    const auto slashData = skillData.skillRules_.find(2);  // Searing Slash
    if (arrowData == skillData.skillRules_.end() || arrowData->second.flightEffectId != 1012 ||
        slashData == skillData.skillRules_.end() || slashData->second.flightEffectId != 0) {
        std::fprintf(stderr, "skillinfo: IDs de trajeto visual 05-F nao correspondem a tabela historica\n");
        return false;
    }
    // Coluna "Play Action" de skillinfo.txt: Bare Hand usa 7,8,9.
    if (bareHandData != skillData.skillRules_.end() &&
        bareHandData->second.playActions != std::vector<int>({7, 8, 9})) {
        std::fprintf(stderr, "skillinfo: Play Action nao foi lida como 7,8,9\n");
        return false;
    }
    if (skillData.skillRules_.size() < 400 || skillData.skillBooks_[3206] != 97 ||
        skillData.lifeSkillBooks_.size() != 46 || skillData.lifeSkillBooks_[2679].skillId != 338 ||
        skillData.lifeSkillBooks_[2679].rewardItem != 1068 || skillData.lifeSkillBooks_[3226].skillId != 200 ||
        poisonDartData == skillData.skillRules_.end() ||
        enfeebleData == skillData.skillRules_.end() ||
        enfeebleData->second.effectFormula != "Skill_Sj_End" ||
        enfeebleData->second.damageFormula != SkillRule::DamageFormula::Enfeeble ||
        enfeebleData->second.spBase != 26 || enfeebleData->second.spPerLevel != 1.0 ||
        enfeebleData->second.cooldownBaseMs != 15000 ||
        monsterPoisonDartData == skillData.skillRules_.end() ||
        healData == skillData.skillRules_.end() ||
        std::find(healData->second.jobs.begin(), healData->second.jobs.end(), 5) == healData->second.jobs.end() ||
        hardenData == skillData.skillRules_.end() || hardenData->second.prerequisite != 97 ||
        hardenData->second.prerequisiteLevel != 3 || healData->second.icon != "texture/icon/s0097.png" ||
        healData->second.spBase != 27 || healData->second.spPerLevel != 2 ||
        healData->second.cooldownBaseMs != 7000 || healData->second.cooldownPerLevelMs != -300 ||
        eagleData == skillData.skillRules_.end() || eagleData->second.spBase != 10 ||
        eagleData->second.cooldownBaseMs != 2000 || dragonFlightData == skillData.skillRules_.end() ||
        dragonFlightData->second.spBase != 20 || std::fabs(dragonFlightData->second.spPerLevel + .5f) > .001f ||
        bareHandData == skillData.skillRules_.end() || bareHandData->second.distance != 100 ||
        bowData == skillData.skillRules_.end() || bowData->second.distance != 900) {
        std::fprintf(stderr, "skill: tabelas historicas de livros/requisitos nao abriram\n");
        return false;
    }
    if (fairyBodyData == skillData.skillRules_.end() || fairyBodyData->second.spBase != 20 ||
        fairyBodyData->second.spFormulaMissing || phantomSlashData == skillData.skillRules_.end() ||
        !phantomSlashData->second.spFormulaMissing) {
        std::fprintf(stderr, "skill: formula SP alternativa ou ausencia historica nao foi classificada\n");
        return false;
    }
    if(crippleData==skillData.skillRules_.end() ||
       crippleData->second.actionKeyFrame!=0 ||
       crippleData->second.itemEffectKeyFrames!=std::vector<int>({-1,0})) {
        std::fprintf(stderr,"skill 94: canal Item/-1 nao foi lido separadamente do callback Action/0\n");
        return false;
    }
    if (illusionSlashData == skillData.skillRules_.end() ||
        illusionSlashData->second.damageFormula != SkillRule::DamageFormula::IllusionSlash ||
        illusionSlashData->second.distance != 800 ||
        illusionSlashData->second.playActions.size() != 1 ||
        illusionSlashData->second.playActions.front() != 14 ||
        illusionSlashData->second.animationTimeBaseMs != 5000 ||
        illusionSlashData->second.cyclicAnimation ||
        illusionSlashData->second.actionKeyFrame != 0 ||
        illusionSlashData->second.flightActionKeyFrame != 0 ||
        mightyStrikeData == skillData.skillRules_.end() ||
        mightyStrikeData->second.damageFormula != SkillRule::DamageFormula::MightyStrike ||
        primalRageData == skillData.skillRules_.end() ||
        primalRageData->second.damageFormula != SkillRule::DamageFormula::PrimalRage ||
        illusionSlashDamage(10, 1, 69.f) != 16 || illusionSlashDamage(10, 2, 140.f) != 34 ||
        illusionSlashDamage(10, 5, 350.f) != 60 || mightyStrikeDamage(100, 1, 20, 0) != 105 ||
        mightyStrikeDamage(100, 2, 20, 0) != 110 || primalRageDamage(100, 1) != 351 ||
        primalRageDamage(100, 3) != 353) {
        std::fprintf(stderr, "skills 81-83: formulas nao correspondem aos callbacks Lua historicos\n");
        return false;
    }
    Play delayedIllusion;
    delayedIllusion.skillRules_[81] = illusionSlashData->second;
    // Authored 24-frame action, with its callback nine frames into the clip.
    // The historical cooldown is 5s; it must not stretch this 0.8s movement.
    delayedIllusion.skillRules_[81].animationDuration = .8f;
    delayedIllusion.skillRules_[81].impactOffset = .3f;
    delayedIllusion.skillRules_[81].actionDurations = {.8f};
    delayedIllusion.skillRules_[81].actionImpacts = {.3f};
    delayedIllusion.skills_[0].id = 81;
    delayedIllusion.skills_[0].name = "Illusion Slash";
    delayedIllusion.learnedSkills_[81] = 1;
    delayedIllusion.player_.homeY = 200.f;
    delayedIllusion.player_.cd = .7f;
    delayedIllusion.player_.radius = .4f;
    Foe illusionTarget;
    illusionTarget.x = 20.f;
    illusionTarget.radius = .4f;
    illusionTarget.hp = illusionTarget.maxHp = 100;
    delayedIllusion.foes_.push_back(illusionTarget);
    // 05-BI: select a skill first; selecting a foe later must launch it.
    // Neither selection nor a failed-range attempt should consume resources.
    const float initialSp = delayedIllusion.player_.homeY;
    delayedIllusion.castSkill(0);
    if (delayedIllusion.player_.homeY != initialSp ||
        delayedIllusion.requestedSkill_ != 0 || delayedIllusion.pendingBasicStrike_) {
        std::fprintf(stderr, "skill 81: nao armou para alvo posterior\n");
        return false;
    }
    delayedIllusion.pickFoe(0); // 20 m: out of the historical 8 m
    if (delayedIllusion.player_.homeY != initialSp ||
        delayedIllusion.requestedSkill_ != 0 || delayedIllusion.skillCd_[0] > 0.f) {
        std::fprintf(stderr, "skill 81: tentativa fora de alcance gastou SP/CD\n");
        return false;
    }
    float waitX = -1.f, waitY = -1.f;
    delayedIllusion.chasePoint(0.f, 0.f, waitX, waitY);
    if (waitX < 10.f || waitX >= 20.f || waitY != 0.f) {
        std::fprintf(stderr, "skill 81: nao aproximou ate o alcance historico\n");
        return false;
    }
    if (!delayedIllusion.takeEffects().empty()) {
        std::fprintf(stderr, "skill 81: conjuracao foi emitida antes de comecar o ataque\n");
        return false;
    }
    delayedIllusion.resolve(0.f, 0.f, 0.f, nullptr);
    if (!delayedIllusion.takeEffects().empty()) {
        std::fprintf(stderr, "skill 81: eventos apareceram com o alvo fora dos 8 m\n");
        return false;
    }
    delayedIllusion.foes_[0].x = 8.5f;
    delayedIllusion.pickFoe(0); // skill-first -> click-target
    delayedIllusion.resolve(0.f, 0.f, 0.f, nullptr);
    const float initialSkillSwing = delayedIllusion.swing_;
    if (!delayedIllusion.swingIsSkill_ || delayedIllusion.attackActionBase_ != 14 ||
        std::fabs(initialSkillSwing - .8f) > .01f ||
        std::fabs(delayedIllusion.swingPlaybackRate_ - 1.f) > .001f ||
        std::fabs(delayedIllusion.skillCd_[0] - 5.f) > .001f) {
        std::fprintf(stderr, "skill 81: cooldown de 5s alterou a pose nativa de 0.8s\n");
        return false;
    }
    if (!delayedIllusion.takeEffects().empty()) {
        std::fprintf(stderr, "skill 81: efeito foi disparado antes do quadro-chave da pose\n");
        return false;
    }
    const float expectedLaunchDelay = .3f;
    if (expectedLaunchDelay < .02f || expectedLaunchDelay >= delayedIllusion.swing_) {
        std::fprintf(stderr, "skill 81: tempo do quadro-chave nao foi extraido da pose 14\n");
        return false;
    }
    delayedIllusion.resolve(expectedLaunchDelay - .02f, 0.f, 0.f, nullptr);
    if (!delayedIllusion.takeEffects().empty() || delayedIllusion.foes_[0].hp != 100) {
        std::fprintf(stderr, "skill 81: efeito ou dano ocorreu antes do quadro-chave\n");
        return false;
    }
    delayedIllusion.resolve(.03f, 0.f, 0.f, nullptr);
    const auto illusionEvents = delayedIllusion.takeEffects();
    const bool castAtAttack = std::any_of(illusionEvents.begin(), illusionEvents.end(), [](const EffectEvent& event) {
        return event.kind == EffectEvent::CastAction && event.variant == 81;
    });
    const bool flightAtAttack = std::any_of(illusionEvents.begin(), illusionEvents.end(), [](const EffectEvent& event) {
        return event.kind == EffectEvent::Flight && event.variant == 81 &&
               std::fabs(event.duration - 8.5f / 16.f) < .01f;
    });
    if (!castAtAttack || !flightAtAttack) {
        std::fprintf(stderr, "skill 81: eventos Cast/Flight nao acompanharam o quadro-chave\n");
        return false;
    }
    const int illusionHpBeforeArrival = delayedIllusion.foes_[0].hp;
    delayedIllusion.resolve(8.5f / 16.f - .02f, 0.f, 0.f, nullptr);
    if (delayedIllusion.foes_[0].hp != illusionHpBeforeArrival) {
        std::fprintf(stderr, "skill 81: dano ocorreu antes do projétil alcançar o alvo\n");
        return false;
    }
    delayedIllusion.resolve(.03f, 0.f, 0.f, nullptr);
    const auto arrivedEffects = delayedIllusion.takeEffects();
    if (std::none_of(arrivedEffects.begin(), arrivedEffects.end(), [](const EffectEvent& ev) {
            return ev.kind == EffectEvent::SkillHit && ev.variant == 81;
        }) || delayedIllusion.pendingBasicStrike_ ||
        delayedIllusion.armedSkill_ >= 0 || delayedIllusion.target_ != 0) {
        std::fprintf(stderr, "skill 81: impacto 195 nao emitido ou auto-chase ativo\n");
        return false;
    }
    if (delayedIllusion.foes_[0].hp >= illusionHpBeforeArrival) {
        std::fprintf(stderr, "skill 81: dano não ocorreu após o tempo de voo\n");
        return false;
    }
    delayedIllusion.resolve(delayedIllusion.swing_ + .02f, 0.f, 0.f, nullptr);
    if (delayedIllusion.swing_ > .02f || delayedIllusion.swingIsSkill_) {
        std::fprintf(stderr, "skill 81: pose nao chegou ao fim no tempo historico\n");
        return false;
    }
    delayedIllusion.resolve(.02f, 0.f, 0.f, nullptr);
    float idleX = -1.f, idleY = -1.f;
    const bool autoChasing = delayedIllusion.chasePoint(0.f, 0.f, idleX, idleY);
    if (autoChasing) {
        std::fprintf(stderr, "skill 81: auto-chase voltou depois do disparo\n");
        return false;
    }
    if (delayedIllusion.swing_ != 0.f || delayedIllusion.swingIsSkill_) {
        std::fprintf(stderr, "skill 81: pose one-shot nao encerrou corretamente\n");
        return false;
    }
    // The chosen attack variant supplies both duration and impact; ASPD
    // changes the playback speed by the same factor as the damage callback.
    Play fastAttacks;
    fastAttacks.player_.cd = .3f;
    fastAttacks.player_.range = 2.f;
    fastAttacks.actionDurations_[7] = 1.2f;
    fastAttacks.actionImpacts_[7] = .8f;
    fastAttacks.actionDurations_[8] = .6f;
    fastAttacks.actionImpacts_[8] = .1f;
    Foe durableTarget;
    durableTarget.x = 1.f;
    durableTarget.hp = durableTarget.maxHp = 10000;
    durableTarget.cdLeft = 100.f;
    fastAttacks.foes_.push_back(durableTarget);
    fastAttacks.target_ = 0;
    fastAttacks.pendingBasicStrike_ = true;
    fastAttacks.resolve(0.f, 0.f, 0.f);
    if (fastAttacks.attackActionBase_ != 7 || std::fabs(fastAttacks.swing_ - .3f) > .001f ||
        std::fabs(fastAttacks.swingPlaybackRate_ - 4.f) > .001f ||
        std::fabs(fastAttacks.pendingAttack_ - .2f) > .001f) {
        std::fprintf(stderr, "animacao: ataque rapido nao sincronizou pose e impacto\n");
        return false;
    }
    fastAttacks.resolve(.19f, 0.f, 0.f);
    if (fastAttacks.foes_[0].hp != 10000) return false;
    fastAttacks.resolve(.02f, 0.f, 0.f);
    if (fastAttacks.foes_[0].hp >= 10000) return false;
    fastAttacks.pickFoe(0); // explicit second click, never automatic
    fastAttacks.resolve(.10f, 0.f, 0.f);
    if (fastAttacks.attackActionBase_ != 8 || std::fabs(fastAttacks.swingPlaybackRate_ - 2.f) > .001f ||
        std::fabs(fastAttacks.pendingAttack_ - .05f) > .001f) {
        std::fprintf(stderr, "animacao: segundo golpe reutilizou tempos da primeira variante\n");
        return false;
    }
    // A keyframe at the first frame is valid, not a missing-timing sentinel.
    Play firstFrameSkill;
    firstFrameSkill.skillRules_[81] = delayedIllusion.skillRules_[81];
    firstFrameSkill.skillRules_[81].actionImpacts = {0.f};
    firstFrameSkill.skillRules_[81].flightEffectId = 0;
    firstFrameSkill.skills_[0] = {81, "Illusion Slash", 0, 5.f};
    firstFrameSkill.learnedSkills_[81] = 1;
    firstFrameSkill.player_.homeY = 100.f;
    firstFrameSkill.foes_.push_back(durableTarget);
    firstFrameSkill.target_ = 0;
    firstFrameSkill.castSkill(0);
    firstFrameSkill.pickFoe(0);
    firstFrameSkill.resolve(0.f, 0.f, 0.f);
    // Keep this before the unrelated legacy keyframe assertion below so the
    // ration regression executes even while that existing check is failing.
    Play fairyRation;
    fairyRation.loadItems("Client/scripts/table/iteminfo.txt",{227,231});
    fairyRation.loadFairyRations("Client/scripts/offline/script/calculate/variable.lua");
    if(!fairyRation.addBag(231,1) || !fairyRation.addBag(227,1))return false;
    const int fairyUid=fairyRation.bag_[0].uid;
    fairyRation.bag_[0].durability=100;fairyRation.bag_[0].maxDurability=5000;
    fairyRation.worn_[16]=231;fairyRation.wornUid_[16]=fairyUid;
    fairyRation.clickBag(1);
    const bool fairyRationApplied=fairyRation.bag_[0].durability==2600 &&
        fairyRation.bagCount(227)==0 && fairyRation.wornUid_[16]==fairyUid;
    const char* fairyRationSave="/tmp/top-fairy-ration-save.txt";
    fairyRation.save(fairyRationSave);
    Play fairyRationReloaded;
    fairyRationReloaded.loadItems("Client/scripts/table/iteminfo.txt",{227,231});
    fairyRationReloaded.loadFairyRations("Client/scripts/offline/script/calculate/variable.lua");
    fairyRationReloaded.load(fairyRationSave);
    const bool fairyRationPersisted=fairyRationReloaded.bag_.size()==1 &&
        fairyRationReloaded.bag_[0].id==231 && fairyRationReloaded.bag_[0].durability==2600 &&
        fairyRationReloaded.wornUid_[16]==fairyUid;
    std::remove(fairyRationSave);
    Play fairyRationRejected;
    fairyRationRejected.loadItems("Client/scripts/table/iteminfo.txt",{227});
    fairyRationRejected.loadFairyRations("Client/scripts/offline/script/calculate/variable.lua");
    if(!fairyRationRejected.addBag(227,1))return false;
    fairyRationRejected.clickBag(0);
    const bool fairyRationNeedsPet=fairyRationRejected.bagCount(227)==1;
    Play fairyRationFull;
    fairyRationFull.loadItems("Client/scripts/table/iteminfo.txt",{227,231});
    fairyRationFull.loadFairyRations("Client/scripts/offline/script/calculate/variable.lua");
    if(!fairyRationFull.addBag(231,1) || !fairyRationFull.addBag(227,1))return false;
    const int fullFairyUid=fairyRationFull.bag_[0].uid;
    fairyRationFull.bag_[0].durability=5000;fairyRationFull.bag_[0].maxDurability=5000;
    fairyRationFull.worn_[16]=231;fairyRationFull.wornUid_[16]=fullFairyUid;
    fairyRationFull.clickBag(1);
    const bool fairyRationFullPreserved=fairyRationFull.bagCount(227)==1 &&
        fairyRationFull.bag_[0].durability==5000;
    if(!fairyRationApplied || !fairyRationPersisted || !fairyRationNeedsPet || !fairyRationFullPreserved) {
        std::fprintf(stderr,"PORT-FAIRY-RATION: apply=%d save=%d needs_pet=%d full=%d\n",
            fairyRationApplied,fairyRationPersisted,fairyRationNeedsPet,fairyRationFullPreserved);return false;
    }
    Play fairyEgg;
    fairyEgg.loadItems("Client/scripts/table/iteminfo.txt",{183,422,227});
    fairyEgg.loadFairyEggs("Client/scripts/offline/script/calculate/variable.lua");
    fairyEgg.loadFairyRations("Client/scripts/offline/script/calculate/variable.lua");
    if(!fairyEgg.addBag(422,1))return false;
    fairyEgg.clickBag(0);
    if(fairyEgg.bagCount(422)!=0 || fairyEgg.bagCount(183)!=1 ||
       fairyEgg.bag_.front().quality!=4 || fairyEgg.bag_.front().maxDurability!=5000 ||
       fairyEgg.bag_.front().maxEnergy!=240) {
        std::fprintf(stderr,"PORT-FAIRY-EGG: egg/output/quality/stat limits mismatch\n");return false;
    }
    if(!fairyEgg.equipBagToSlot(0,16) || !fairyEgg.addBag(227,1))return false;
    const int hatchedFairyUid=fairyEgg.wornUid_[16];
    const auto hatchedFairy=std::find_if(fairyEgg.bag_.begin(),fairyEgg.bag_.end(),[&](const Stack& value) {
        return value.uid==hatchedFairyUid;
    });
    if(hatchedFairy==fairyEgg.bag_.end())return false;
    hatchedFairy->durability=100;
    const int rationIndex=static_cast<int>(fairyEgg.bag_.size())-1;
    fairyEgg.clickBag(rationIndex);
    const auto fedFairy=std::find_if(fairyEgg.bag_.begin(),fairyEgg.bag_.end(),[&](const Stack& value) {
        return value.uid==hatchedFairyUid;
    });
    if(fedFairy==fairyEgg.bag_.end() || fedFairy->durability!=2600 || fairyEgg.bagCount(227)!=0) {
        std::fprintf(stderr,"PORT-FAIRY-EGG: hatched fairy could not equip/feed\n");return false;
    }
    Play fairyEggNoSpace;
    fairyEggNoSpace.loadItems("Client/scripts/table/iteminfo.txt",{183,422,227});
    fairyEggNoSpace.loadFairyEggs("Client/scripts/offline/script/calculate/variable.lua");
    fairyEggNoSpace.bagCapacity_=2;
    if(!fairyEggNoSpace.addBag(422,1) || !fairyEggNoSpace.addBag(227,1))return false;
    fairyEggNoSpace.clickBag(0);
    if(fairyEggNoSpace.bagCount(422)!=1 || fairyEggNoSpace.bagCount(183)!=0) {
        std::fprintf(stderr,"PORT-FAIRY-EGG: insufficient-space failure consumed egg\n");return false;
    }
    Play fairyAutoFeed;
    fairyAutoFeed.loadItems("Client/scripts/table/iteminfo.txt",{183,422,2312,6842});
    fairyAutoFeed.loadFairyRations("Client/scripts/offline/script/calculate/variable.lua");
    if(!fairyAutoFeed.addBag(183,1) || !fairyAutoFeed.addBag(422,1) ||
       !fairyAutoFeed.addBag(2312,2))return false;
    const int autoFairyUid=fairyAutoFeed.bag_[0].uid;
    fairyAutoFeed.bag_[0].durability=2500;fairyAutoFeed.bag_[0].maxDurability=5000;
    fairyAutoFeed.worn_[16]=183;fairyAutoFeed.wornUid_[16]=autoFairyUid;
    fairyAutoFeed.step(.01f,nullptr,0.f,0.f);
    const bool fairyAutoFeedTriggered=fairyAutoFeed.bag_[0].durability==5000 &&
        fairyAutoFeed.bagCount(2312)==1;
    fairyAutoFeed.bag_[0].durability=2501;
    fairyAutoFeed.step(.01f,nullptr,0.f,0.f);
    const bool fairyAutoFeedAboveThreshold=fairyAutoFeed.bag_[0].durability==2501 &&
        fairyAutoFeed.bagCount(2312)==1;
    if(!fairyAutoFeedTriggered || !fairyAutoFeedAboveThreshold) {
        std::fprintf(stderr,"PORT-FAIRY-AUTOFEED: threshold/consumption mismatch\n");return false;
    }
    Play fairySkillBook;
    fairySkillBook.root_=randomBoxRoot;
    fairySkillBook.profileId_=9062;
    fairySkillBook.loadItems("Client/scripts/table/iteminfo.txt",{183,243,244,245,246,249,252});
    fairySkillBook.loadFairySkills("Client/scripts/offline/script/calculate/variable.lua");
    if(!fairySkillBook.addBag(183,1) || !fairySkillBook.addBag(243,1) ||
       !fairySkillBook.addBag(244,1) || !fairySkillBook.addBag(245,1) ||
       !fairySkillBook.addBag(246,1) || !fairySkillBook.addBag(249,1) ||
       !fairySkillBook.addBag(252,1) || !fairySkillBook.equipBagToSlot(0,16))return false;
    auto useFairyBook=[&](Play& owner,int itemId) {
        const auto found=std::find_if(owner.bag_.begin(),owner.bag_.end(),[&](const Stack& value) {
            return value.id==itemId;
        });
        if(found==owner.bag_.end())return false;
        owner.clickBag(static_cast<int>(std::distance(owner.bag_.begin(),found)));
        return true;
    };
    useFairyBook(fairySkillBook,244); // Nivel 2 sem nivel 1: preserva.
    const bool fairySkillPrerequisitePreserved=fairySkillBook.bagCount(244)==1;
    useFairyBook(fairySkillBook,243);
    useFairyBook(fairySkillBook,244);
    useFairyBook(fairySkillBook,245);
    const auto learnedFairy=std::find_if(fairySkillBook.bag_.begin(),fairySkillBook.bag_.end(),[&](const Stack& value) {
        return value.id==183;
    });
    const bool fairySkillSequence=learnedFairy!=fairySkillBook.bag_.end() &&
        learnedFairy->fairySkills[0]==1 && learnedFairy->fairySkills[1]==3;
    Play fairySkillReloaded;
    fairySkillReloaded.loadItems("Client/scripts/table/iteminfo.txt",{183,243,244,245,246,249,252});
    fairySkillReloaded.load(fairySkillBook.savePath());
    const auto loadedFairySkill=std::find_if(fairySkillReloaded.bag_.begin(),fairySkillReloaded.bag_.end(),
        [&](const Stack& value) { return value.id==183; });
    const bool fairySkillPersisted=loadedFairySkill!=fairySkillReloaded.bag_.end() &&
        loadedFairySkill->fairySkills[0]==1 && loadedFairySkill->fairySkills[1]==3;
    useFairyBook(fairySkillBook,246);
    useFairyBook(fairySkillBook,249);
    useFairyBook(fairySkillBook,252); // Quarto grupo: sem quarto espaco, preserva.
    const bool fairySkillSlotsPreserved=fairySkillBook.bagCount(252)==1;
    if(!fairySkillPrerequisitePreserved || !fairySkillSequence || !fairySkillPersisted ||
       !fairySkillSlotsPreserved) {
        std::fprintf(stderr,"PORT-FAIRY-SKILLS: prerequisito, progressao, save ou limite falhou\n");
        return false;
    }
    const auto firstFrameEvents = firstFrameSkill.takeEffects();
    if (firstFrameSkill.pendingAttack_ != 0.f || firstFrameEvents.size() != 1 ||
        firstFrameEvents.front().kind != EffectEvent::Cast ||
        !firstFrameEvents.front().sourceIsPlayer) {
        std::fprintf(stderr, "animacao: keyframe no inicio foi substituido por atraso generico\n");
        return false;
    }
    const bool itemKeyframeCallbacksOK=[&]() {
        if(::setenv("TOP_HISTORICAL_ITEM_EFFECT_KEYFRAME","1",1)!=0)return false;
        Play itemKeyframes;
        itemKeyframes.skillRules_[94]=crippleData->second;
        itemKeyframes.skillRules_[94].playActions={7};
        itemKeyframes.skillRules_[94].actionDurations={.8f};
        itemKeyframes.skillRules_[94].actionImpacts={.2f};
        itemKeyframes.skillRules_[94].flightEffectId=0;
        itemKeyframes.skills_[0]={94,"Cripple",0,5.f};
        itemKeyframes.learnedSkills_[94]=1;
        itemKeyframes.player_.homeY=100.f;
        itemKeyframes.player_.range=8.f;
        Foe target;target.x=1.f;target.hp=target.maxHp=10000;target.cdLeft=100.f;
        itemKeyframes.foes_.push_back(target);
        itemKeyframes.castSkill(0);
        itemKeyframes.pickFoe(0);
        itemKeyframes.resolve(0.f,0.f,0.f,nullptr);
        const auto beginEvents=itemKeyframes.takeEffects();
        const bool began=std::count_if(beginEvents.begin(),beginEvents.end(),[](const EffectEvent& e) {
            return e.kind==EffectEvent::CastBegin&&e.variant==94;
        })==1;
        itemKeyframes.resolve(.19f,0.f,0.f,nullptr);
        const auto beforeAction=itemKeyframes.takeEffects();
        const bool held=std::none_of(beforeAction.begin(),beforeAction.end(),[](const EffectEvent& e) {
            return e.kind==EffectEvent::CastAction&&e.variant==94;
        });
        itemKeyframes.resolve(.02f,0.f,0.f,nullptr);
        const auto actionEvents=itemKeyframes.takeEffects();
        const bool action=std::count_if(actionEvents.begin(),actionEvents.end(),[](const EffectEvent& e) {
            return e.kind==EffectEvent::CastAction&&e.variant==94;
        })==1;
        SkillVisualCatalog visuals;
        const bool loaded=visuals.load("Client/scripts/table/skillinfo.txt") ||
            visuals.load("../Client/scripts/table/skillinfo.txt") ||
            visuals.load("../../Client/scripts/table/skillinfo.txt");
        const SkillVisual* visual=loaded?visuals.find(94):nullptr;
        if(!visual)return false;
        const auto beginChannels=visual->castChannels(-1);
        const auto actionChannels=visual->castChannels(0);
        return began&&held&&action&&beginChannels.size()==1&&
            beginChannels.front().kind==SkillVisualChannelKind::Item&&
            beginChannels.front().effectId==256&&actionChannels.size()==1&&
            actionChannels.front().kind==SkillVisualChannelKind::ItemExtra&&
            actionChannels.front().effectId==142;
    }();
    ::unsetenv("TOP_HISTORICAL_ITEM_EFFECT_KEYFRAME");
    if(!itemKeyframeCallbacksOK) {
        std::fprintf(stderr,"skill 94: Item/-1 nao precedeu Action/0 no modo historico opt-in\n");
        return false;
    }
    Play repeatedBuff;
    repeatedBuff.skills_[0] = {84, "Berserk", 0, 1.f};
    repeatedBuff.skillRules_[84] = berserkData->second;
    repeatedBuff.skillRules_[84].animationDuration = .6f;
    repeatedBuff.skillRules_[84].actionImpacts = {.2f};
    repeatedBuff.learnedSkills_[84] = 1;
    repeatedBuff.player_.homeY = 100.f;
    repeatedBuff.foes_.push_back(durableTarget);
    repeatedBuff.target_ = 0;
    repeatedBuff.armedSkill_ = 1; // an old armed attack must not swallow this buff
    repeatedBuff.castSkill(0);
    repeatedBuff.resolve(.1f, 0.f, 0.f);
    const unsigned firstBuffCycle = repeatedBuff.attackSequence_;
    if (repeatedBuff.playerPose() != 5 || repeatedBuff.pendingTarget_ >= 0 ||
        !repeatedBuff.takeEffects().empty() || repeatedBuff.armedSkill_ != -1) return false;
    repeatedBuff.skillCd_[0] = 0.f;
    repeatedBuff.castSkill(0);
    repeatedBuff.resolve(.11f, 0.f, 0.f);
    if (repeatedBuff.attackSequence_ == firstBuffCycle || !repeatedBuff.takeEffects().empty()) {
        std::fprintf(stderr, "animacao: buff repetido nao reiniciou ou emitiu callback antigo\n");
        return false;
    }
    repeatedBuff.resolve(.1f, 0.f, 0.f);
    const auto buffEvents = repeatedBuff.takeEffects();
    if (buffEvents.size() != 1 || buffEvents.front().kind != EffectEvent::Cast ||
        !buffEvents.front().sourceIsPlayer) return false;

    // SK2.11: Berserk declares action 12, Cast 117 at key 0, and ground
    // result 236 at ACTION_END_HIT=-2. The result must survive as a separate
    // callback, while cancellation/recasting must suppress the older one.
    if(berserkData->second.harmMode!=1 || berserkData->second.hitKeyFrame!=-2 ||
       !berserkData->second.hasHitVisual || berserkData->second.playActions.front()!=12)return false;
    repeatedBuff.resolve(.4f,0.f,0.f);
    auto countBuffHit=[](const std::vector<EffectEvent>& events) {
        return std::count_if(events.begin(),events.end(),[](const EffectEvent& e) {
            return e.kind==EffectEvent::SkillHit && e.variant==84;
        });
    };
    if(countBuffHit(repeatedBuff.takeEffects())!=1)return false;
    repeatedBuff.resolve(.05f,0.f,0.f);
    if(countBuffHit(repeatedBuff.takeEffects())!=0)return false;

    // Monster skill 138 uses its own action bank, never the player's timing.
    Play timedMonster;
    timedMonster.player_.hp = timedMonster.player_.maxHp = 1000;
    timedMonster.player_.cdLeft = 100.f;
    Foe posedMonster;
    posedMonster.x = posedMonster.homeX = 5.f;
    posedMonster.hp = posedMonster.maxHp = 1000;
    posedMonster.ai = 10;
    posedMonster.leash = 15.f;
    posedMonster.cd = 1.f;
    posedMonster.attacks.push_back({138, 100, 9.f, 40.f});
    posedMonster.actionDurations[12] = 1.2f;
    posedMonster.actionImpacts[12] = .6f;
    timedMonster.foes_.push_back(posedMonster);
    SkillRule timedShot;
    timedShot.id = 138;
    timedShot.playActions = {12};
    timedShot.cyclicAnimation = true;
    timedShot.actionKeyFrame = 0;
    timedShot.flightActionKeyFrame = 0;
    timedShot.flightEffectId = 1036;
    timedMonster.skillRules_[138] = timedShot;
    timedMonster.resolve(0.f, 0.f, 0.f);
    if (timedMonster.foes_[0].action != 12 || timedMonster.foes_[0].actionCycle != 1 ||
        std::fabs(timedMonster.foes_[0].swing - 1.f) > .001f ||
        std::fabs(timedMonster.foes_[0].playbackRate - 1.2f) > .001f ||
        std::fabs(timedMonster.foes_[0].attackPending - .625f) > .001f ||
        !timedMonster.takeEffects().empty()) {
        std::fprintf(stderr, "animacao: monstro nao usou pose/ASPD/keyframe de seu modelo\n");
        return false;
    }
    timedMonster.resolve(.49f, 0.f, 0.f);
    if (!timedMonster.takeEffects().empty() || timedMonster.player_.hp != 1000) return false;
    timedMonster.resolve(.02f, 0.f, 0.f);
    const auto shotEvents = timedMonster.takeEffects();
    if (shotEvents.size() != 2 || timedMonster.player_.hp != 1000) return false;
    timedMonster.resolve(.1f, 0.f, 0.f);
    if (timedMonster.player_.hp != 1000) return false;
    timedMonster.resolve(.02f, 0.f, 0.f);
    if (timedMonster.player_.hp >= 1000) return false;
    timedMonster.takeEffects();
    // Interrupting an action must discard effects waiting for its keyframe.
    timedMonster.foes_[0].swing = timedMonster.foes_[0].cdLeft = 0.f;
    timedMonster.resolve(0.f, 0.f, 0.f);
    timedMonster.foes_[0].stunFor = 1.f;
    timedMonster.foes_[0].attackPending = 0.f;
    timedMonster.resolve(.51f, 0.f, 0.f);
    if (!timedMonster.takeEffects().empty()) {
        std::fprintf(stderr, "animacao: monstro atordoado emitiu efeito da acao cancelada\n");
        return false;
    }
    if (doubleShotData == skillData.skillRules_.end() ||
        doubleShotData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        frozenArrowData == skillData.skillRules_.end() ||
        frozenArrowData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        howlData == skillData.skillRules_.end() ||
        howlData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        meteorShowerData == skillData.skillRules_.end() ||
        meteorShowerData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        bareHandFormula == skillData.skillRules_.end() ||
        bareHandFormula->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        meleeFormula == skillData.skillRules_.end() ||
        meleeFormula->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        rangedFormula == skillData.skillRules_.end() ||
        rangedFormula->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        staffFormula == skillData.skillRules_.end() ||
        staffFormula->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        backstabData == skillData.skillRules_.end() ||
        backstabData->second.effectFormula != "Skill_Guz_End" ||
        backstabData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        backstabData->second.damageScaleBase != 1.0 ||
        backstabData->second.damageScalePerLevel != 0.1 ||
        hunterStrikeData == skillData.skillRules_.end() ||
        hunterStrikeData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        penetratingBulletData == skillData.skillRules_.end() ||
        penetratingBulletData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        infernoBlastData == skillData.skillRules_.end() ||
        infernoBlastData->second.damageFormula != SkillRule::DamageFormula::AttackRaise ||
        crippleData == skillData.skillRules_.end() ||
        crippleData->second.damageFormula != SkillRule::DamageFormula::PhysicalPlus ||
        barbaricCrushData == skillData.skillRules_.end() ||
        barbaricCrushData->second.damageFormula != SkillRule::DamageFormula::AttackRaise ||
        dispersionBulletData == skillData.skillRules_.end() ||
        dispersionBulletData->second.damageFormula != SkillRule::DamageFormula::AttackRaise ||
        directionalBackstabData == skillData.skillRules_.end() ||
        directionalBackstabData->second.damageFormula != SkillRule::DamageFormula::Backstab ||
        physicalSkillDamage(100, 1, 1.5f, .15f) != 165 ||
        physicalSkillDamage(100, 5, 1.5f, .15f) != 225 ||
        physicalSkillDamage(100, 1, 1.2f, 0.f) != 120 ||
        physicalSkillDamage(100, 5, .5f, .1f) != 100 ||
        physicalSkillDamage(100, 5, 1.f, .05f) != 125 ||
        physicalSkillDamage(100, 5, 1.f, .1f) != 150 ||
        physicalSkillDamage(100, 5, 2.f, .2f) != 300 ||
        physicalSkillDamage(100, 5, 1.f, .2f) != 200 ||
        physicalPlusDamage(100, 1, 1.f, .05f) != 6 ||
        physicalPlusDamage(100, 5, 1.f, .05f) != 26 ||
        scaledAttackDamage(100, 1.35f, 20, 0) != 115 ||
        scaledAttackDamage(100, 2.f, 20, 0) != 180 ||
        backstabDamage(100, 1, 80, 20, 0, 0.f) != 24 ||
        backstabDamage(100, 1, 80, 20, 0, 90.f) != 85 ||
        backstabDamage(100, 3, 80, 20, 0, 181.f) != 72) {
        auto diagnostic = [&](const char* name, int id) {
            const auto found = skillData.skillRules_.find(id);
            std::fprintf(stderr, " %s=%s/%d", name,
                         found == skillData.skillRules_.end() ? "missing" : found->second.effectFormula.c_str(),
                         found == skillData.skillRules_.end() ? -1 : static_cast<int>(found->second.damageFormula));
        };
        std::fprintf(stderr, "skills fisicas: coeficientes Lua incorretos");
        diagnostic("85",85); diagnostic("61",61); diagnostic("94",94); diagnostic("108",108); diagnostic("114",114);
        std::fprintf(stderr, " values=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                     physicalSkillDamage(100,1,1.5f,.15f), physicalSkillDamage(100,5,1.5f,.15f),
                     physicalSkillDamage(100,1,1.2f,0.f), physicalSkillDamage(100,5,.5f,.1f),
                     physicalSkillDamage(100,5,1.f,.05f), physicalSkillDamage(100,5,1.f,.1f),
                     physicalSkillDamage(100,5,2.f,.2f), physicalSkillDamage(100,5,1.f,.2f),
                     physicalPlusDamage(100,1,1.f,.05f), physicalPlusDamage(100,5,1.f,.05f),
                     scaledAttackDamage(100,1.35f,20,0), scaledAttackDamage(100,2.f,20,0),
                     backstabDamage(100,1,80,20,0,0.f), backstabDamage(100,1,80,20,0,90.f),
                     backstabDamage(100,3,80,20,0,181.f));
        return false;
    }
    Play shadowSlash;
    shadowSlash.skillRules_[86] = backstabData->second;
    shadowSlash.skills_[0] = {86, "Shadow Slash", 1, 1.f};
    shadowSlash.learnedSkills_[86] = 5;
    shadowSlash.player_.level = 10;
    shadowSlash.player_.minAtk = shadowSlash.player_.maxAtk = 10;
    shadowSlash.player_.hit = 50;
    shadowSlash.player_.crit = 0;
    Foe slashTarget;
    slashTarget.id = 71;
    slashTarget.level = 10;
    slashTarget.hp = slashTarget.maxHp = 10000;
    slashTarget.def = 0;
    slashTarget.resist = 0;
    slashTarget.x = 1.f;
    slashTarget.attackPending = .5f;
    slashTarget.pendingSkillId = 214;
    slashTarget.pendingReach = 2.f;
    shadowSlash.foes_.push_back(slashTarget);
    shadowSlash.pendingTarget_ = 0;
    shadowSlash.pendingSkill_ = 0;
    shadowSlash.landPlayerAttack(0.f, 0.f);
    if (shadowSlash.foes_[0].hp != 9985 || shadowSlash.foes_[0].stunFor != 10.f ||
        shadowSlash.foes_[0].attackPending != 0.f || shadowSlash.foes_[0].pendingSkillId != 0 ||
        shadowSlash.foes_[0].pendingReach != 0.f) {
        std::fprintf(stderr, "Skill_Guz_End: Shadow Slash precisa causar dano e interromper por 10 s\n");
        return false;
    }
    Play poisonDart;
    poisonDart.skillRules_[87] = poisonDartData->second;
    poisonDart.skills_[0] = {87, "Poison Dart", 1, 1.f};
    poisonDart.learnedSkills_[87] = 1;
    poisonDart.player_.level = 10;
    poisonDart.player_.hp = poisonDart.player_.maxHp = 10000;
    poisonDart.player_.minAtk = poisonDart.player_.maxAtk = 10;
    poisonDart.player_.hit = 50;
    Foe poisonTarget;
    poisonTarget.id = 71;
    poisonTarget.level = 10;
    poisonTarget.hp = poisonTarget.maxHp = 10000;
    poisonTarget.x = 1.f;
    poisonDart.foes_.push_back(poisonTarget);
    poisonDart.pendingTarget_ = 0;
    poisonDart.pendingSkill_ = 0;
    poisonDart.landPlayerAttack(0.f, 0.f);
    const int afterPoisonApplication = poisonDart.foes_[0].hp;
    if (poisonDart.foes_[0].poisonLevel != 1 || poisonDart.foes_[0].poisonFor != 9.f ||
        poisonDart.foes_[0].poisonTick != 1.f) {
        std::fprintf(stderr, "Poison Dart: STATE_ZD nao aplicou nivel/duracao/intervalo historicos\n");
        return false;
    }
    for (int i = 0; i < 21; ++i) poisonDart.step(.05f, nullptr, 0.f, 0.f);
    if (poisonDart.foes_[0].hp != afterPoisonApplication - 12 ||
        poisonDart.foes_[0].poisonLevel != 1 || poisonDart.foes_[0].poisonFor < 7.9f) {
        std::fprintf(stderr, "Poison Dart: dano periodico incorreto (hp %d -> %d, nivel %d, duracao %.3f, tick %.3f)\n",
                     afterPoisonApplication, poisonDart.foes_[0].hp, poisonDart.foes_[0].poisonLevel,
                     poisonDart.foes_[0].poisonFor, poisonDart.foes_[0].poisonTick);
        return false;
    }
    Play breakArmor;
    breakArmor.skillRules_[222] = breakArmorData->second;
    breakArmor.skills_[0] = {222, "Break Armor", 1, 1.f};
    breakArmor.learnedSkills_[222] = 3;
    breakArmor.player_.level = 10;
    breakArmor.player_.minAtk = breakArmor.player_.maxAtk = 10;
    breakArmor.player_.hit = 1000;
    breakArmor.player_.crit = 0;
    Foe armorTarget;
    armorTarget.id = 71;
    armorTarget.level = 10;
    armorTarget.hp = armorTarget.maxHp = 10000;
    armorTarget.def = 40;
    armorTarget.x = 1.f;
    breakArmor.foes_.push_back(armorTarget);
    breakArmor.target_ = -1;
    breakArmor.pendingTarget_ = 0;
    breakArmor.pendingSkill_ = 0;
    breakArmor.landPlayerAttack(0.f, 0.f);
    if (breakArmor.foes_[0].hp != 10000 || breakArmor.foes_[0].defenseDownLevel != 3 ||
        std::fabs(breakArmor.foes_[0].defenseDownFor - 15.f) > .01f) {
        std::fprintf(stderr, "Break Armor: deveria aplicar somente DEF -12 por 15 s (HP %d, nivel %d, duracao %.2f)\n",
                     breakArmor.foes_[0].hp, breakArmor.foes_[0].defenseDownLevel,
                     breakArmor.foes_[0].defenseDownFor);
        return false;
    }
    Play enfeeble;
    enfeeble.skillRules_[95] = enfeebleData->second;
    enfeeble.skills_[0] = {95, "Enfeeble", 1, 1.f};
    enfeeble.learnedSkills_[95] = 3;
    enfeeble.player_.level = 10;
    enfeeble.player_.hp = enfeeble.player_.maxHp = 100000;
    enfeeble.player_.minAtk = enfeeble.player_.maxAtk = 10;
    enfeeble.player_.hit = 50;
    enfeeble.player_.crit = 0;
    Foe enfeebleTarget;
    enfeebleTarget.id = 71;
    enfeebleTarget.level = 10;
    enfeebleTarget.hp = enfeebleTarget.maxHp = 10000;
    enfeebleTarget.minAtk = enfeebleTarget.maxAtk = 100;
    enfeebleTarget.x = 1.f;
    enfeebleTarget.ai = 10;
    enfeebleTarget.provoked = true;
    enfeebleTarget.attacks.push_back({214, 100, 5.f, 0.f});
    enfeeble.foes_.push_back(enfeebleTarget);
    enfeeble.pendingTarget_ = 0;
    enfeeble.pendingSkill_ = 0;
    enfeeble.landPlayerAttack(0.f, 0.f);
    const int enfeebleDamage = 10000 - enfeeble.foes_[0].hp;
    const int reducedAttack = static_cast<int>(std::floor(enfeeble.foes_[0].minAtk * .8f));
    if (enfeebleDamage != 130 || enfeeble.foes_[0].enfeebleFor != 6.f ||
        enfeeble.foes_[0].enfeebleLevel != 3 || reducedAttack != 80) {
        std::fprintf(stderr, "Enfeeble: dano fixo, penalidade de 20%% ou duracao historica incorretos\n");
        return false;
    }
    enfeeble.step(.05f, nullptr, 0.f, 0.f);
    if (std::fabs(enfeeble.foes_[0].enfeebleFor - 5.95f) > .001f ||
        enfeeble.foes_[0].pendingSkillId != 0 || enfeeble.foes_[0].attackPending <= 0.f ||
        damageOf(80, 80, 10, 0, 48, 0) >= damageOf(100, 100, 10, 0, 48, 0)) {
        std::fprintf(stderr, "Enfeeble: temporizador, silencio de skills ou reducao de ataque incorretos\n");
        return false;
    }
    for (int i = 0; i < 119; ++i) enfeeble.step(.05f, nullptr, 0.f, 0.f);
    if (enfeeble.foes_[0].enfeebleFor != 0.f || enfeeble.foes_[0].enfeebleLevel != 0) {
        std::fprintf(stderr, "Enfeeble: estado nao removeu a penalidade ao expirar\n");
        return false;
    }
    enfeeble.foes_[0].cdLeft = 0.f;
    enfeeble.foes_[0].attackPending = 0.f;
    enfeeble.foes_[0].pendingSkillId = 0;
    enfeeble.step(.05f, nullptr, 0.f, 0.f);
    if (enfeeble.foes_[0].pendingSkillId != 214) {
        std::fprintf(stderr, "Enfeeble: monstro nao voltou a selecionar skills apos a expiracao\n");
        return false;
    }
    if (lairAttackData == skillData.skillRules_.end() ||
        lairAttackData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        lairAttackData->second.damageScaleBase != 3.0 ||
        colossusSmashData == skillData.skillRules_.end() ||
        colossusSmashData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        colossusSmashData->second.damageScaleBase != 20.0 ||
        tempestBladeData == skillData.skillRules_.end() ||
        tempestBladeData->second.damageFormula != SkillRule::DamageFormula::PhysicalLinear ||
        tempestBladeData->second.damageScaleBase != 2.0 ||
        physicalSkillDamage(100, 1, 3.0, 0.0) != 300 ||
        physicalSkillDamage(100, 1, 20.0, 0.0) != 2000 ||
        physicalSkillDamage(100, 1, 1.5, 0.0) != 150) {
        std::fprintf(stderr, "skills de monstros: multiplicadores Atk_Dmg nao correspondem ao Lua historico\n");
        return false;
    }
    if (willOfSteelData == skillData.skillRules_.end() ||
        willOfSteelData->second.effectFormula != "Skill_Gtyz_End" ||
        berserkData == skillData.skillRules_.end() ||
        berserkData->second.effectFormula != "Skill_Kb_End" ||
        std::fabs(berserkIntervalMultiplier(1) - 1.f / 1.215f) > .0001f ||
        std::fabs(berserkIntervalMultiplier(5) - 1.f / 1.275f) > .0001f ||
        hardenData == skillData.skillRules_.end() ||
        hardenData->second.effectFormula != "Skill_Shpf_End" ||
        playerDefenseWithStates(20, 0, 0, 9) != 29 ||
        playerDefenseWithStates(20, 18, 4, 9) != 51 ||
        lightningBoltData == skillData.skillRules_.end() ||
        lightningBoltData->second.effectFormula != "Skill_Lj_End" ||
        lightningBoltData->second.damageFormula != SkillRule::DamageFormula::LightningBolt ||
        conchRayData == skillData.skillRules_.end() ||
        conchRayData->second.effectFormula != "Skill_Bkcj_End" ||
        conchRayData->second.damageFormula != SkillRule::DamageFormula::ConchRay ||
        lightningBoltDamage(1, 5, 10) != 150 || lightningBoltDamage(5, 30, 40) != 430 ||
        healSkillAmount(1, 5) != 135 || healSkillAmount(5, 12) != 384 ||
        conchRayDamage(1, 5) != 286 || conchRayDamage(5, 30) != 583 ||
        !isGarner2UnnormalMonster(728) || !isGarner2UnnormalMonster(1) || isGarner2UnnormalMonster(678) ||
        magicAtkDamage(0, 0, 20, 14, 20, 10, 20, 20, 0) != 37 ||
        garner2SkillMagicDamage(214, 4, 0, 0, 20, 14, 20, 10, 20, 20, 0) != 74 ||
        garner2SkillMagicDamage(216, 3, 0, 0, 20, 14, 20, 10, 20, 20, 0) != 637) {
        std::fprintf(stderr, "skills magicas: Lightning Bolt/Conch Ray nao correspondem aos callbacks Lua historicos\n");
        return false;
    }
    if (blackHyzData == skillData.skillRules_.end() || blackHyzData->second.effectFormula != "Skill_BlackHyz_End" ||
        blackHyzData->second.damageFormula != SkillRule::DamageFormula::MonsterPhysicalDouble ||
        hqgjData == skillData.skillRules_.end() || hqgjData->second.effectFormula != "Skill_hqgj_End" ||
        hqgjData->second.damageFormula != SkillRule::DamageFormula::MonsterPhysicalFloor20 ||
        hqgjData->second.damageScaleBase != 1.5 ||
        wllkData == skillData.skillRules_.end() || wllkData->second.effectFormula != "Skill_wllk_End" ||
        wlfdData == skillData.skillRules_.end() || wlfdData->second.effectFormula != "Skill_wlfd_End" ||
        wlsjData == skillData.skillRules_.end() || wlsjData->second.effectFormula != "Skill_wlsj_End" ||
        hyzHxData == skillData.skillRules_.end() || hyzHxData->second.effectFormula != "Skill_HyzHX_End" ||
        hyzHxData->second.damageFormula != SkillRule::DamageFormula::MonsterAttackSpeed ||
        cannonShotData == skillData.skillRules_.end() ||
        cannonShotData->second.damageFormula != SkillRule::DamageFormula::MonsterFireDistance ||
        bombardmentData == skillData.skillRules_.end() ||
        bombardmentData->second.damageFormula != SkillRule::DamageFormula::MonsterFireDistance ||
        breakArmorData == skillData.skillRules_.end() ||
        breakArmorData->second.effectFormula != "Skill_Pj_End" ||
        breakArmorData->second.damageFormula != SkillRule::DamageFormula::StateOnly ||
        monsterPhysicalCallbackDamage(100, 2.0, 20, 0) != 180 ||
        monsterPhysicalCallbackDamage(100, 1.0, 90, 0) != 20 ||
        monsterFireDamageWithDistance(100, 20, 0, 30, 0.f) != 80 ||
        monsterFireDamageWithDistance(100, 20, 0, 30, 2.f) != 64 ||
        monsterFireDamageWithDistance(100, 20, 0, 30, 9.f) != 7 ||
        monsterFireDamageWithDistance(100, 20, 0, 30, 10.f) != 0 ||
        monsterAttackSpeedCallbackDamage(100, 69) != 250 ||
        monsterAttackSpeedCallbackDamage(100, 140) != 500 ||
        monsterAttackSpeedCallbackDamage(100, 210) != 750) {
        std::fprintf(stderr, "skills de monstros: callbacks Phy_Dmg nao correspondem ao Lua historico (cannon=%d/%d bombard=%d/%d fire=%d,%d,%d,%d)\n",
            cannonShotData == skillData.skillRules_.end() ? -1 : static_cast<int>(cannonShotData->second.damageFormula),
            cannonShotData == skillData.skillRules_.end() ? -1 : cannonShotData->second.id,
            bombardmentData == skillData.skillRules_.end() ? -1 : static_cast<int>(bombardmentData->second.damageFormula),
            bombardmentData == skillData.skillRules_.end() ? -1 : bombardmentData->second.id,
            monsterFireDamageWithDistance(100, 20, 0, 30, 0.f),
            monsterFireDamageWithDistance(100, 20, 0, 30, 2.f),
            monsterFireDamageWithDistance(100, 20, 0, 30, 9.f),
            monsterFireDamageWithDistance(100, 20, 0, 30, 10.f));
        return false;
    }
    Play monsterSkillDamageTest;
    monsterSkillDamageTest.skillRules_ = skillData.skillRules_;
    monsterSkillDamageTest.player_.level = 30;
    monsterSkillDamageTest.player_.hp = monsterSkillDamageTest.player_.maxHp = 5000;
    Foe smash;
    smash.id = 678; // Snowman Warlord usa Colossus Smash (238) em characterinfo.
    smash.level = 30;
    smash.minAtk = smash.maxAtk = 100;
    smash.hp = smash.maxHp = 1000;
    smash.ai = 10;
    smash.x = .1f;
    smash.homeX = smash.x;
    smash.leash = 10.f;
    smash.cd = 1.f;
    smash.cdLeft = 3.f;
    smash.attackPending = .01f;
    smash.pendingSkillId = 238;
    smash.pendingReach = 2.f;
    monsterSkillDamageTest.foes_.push_back(smash);
    monsterSkillDamageTest.resolve(.02f, .4f, 0.f, nullptr);
    if (monsterSkillDamageTest.player_.hp != 3000) {
        std::fprintf(stderr, "skills de monstros: Colossus Smash deveria causar 20x Atk_Dmg (HP=%d)\n",
                     monsterSkillDamageTest.player_.hp);
        return false;
    }
    Play cannonSkillDamageTest;
    cannonSkillDamageTest.skillRules_ = skillData.skillRules_;
    cannonSkillDamageTest.player_.level = 30;
    cannonSkillDamageTest.player_.def = 20;
    cannonSkillDamageTest.player_.resist = 0;
    cannonSkillDamageTest.player_.hp = cannonSkillDamageTest.player_.maxHp = 5000;
    Foe cannon;
    cannon.id = 27;
    cannon.level = 30;
    cannon.minAtk = cannon.maxAtk = 100;
    cannon.hp = cannon.maxHp = 1000;
    cannon.ai = 10;
    cannon.x = .1f;
    cannon.homeX = cannon.x;
    cannon.leash = 10.f;
    cannon.cd = 1.f;
    cannon.cdLeft = 3.f;
    cannon.attackPending = .01f;
    cannon.pendingSkillId = 27;
    cannon.pendingReach = 2.f;
    cannonSkillDamageTest.foes_.push_back(cannon);
    cannonSkillDamageTest.resolve(.02f, .4f, 0.f, nullptr);
    if (cannonSkillDamageTest.player_.hp != 4923) {
        std::fprintf(stderr, "Skill_Paodan_End: Fire_Dmg e perda de distancia deveriam causar 77 (HP=%d)\n",
                     cannonSkillDamageTest.player_.hp);
        return false;
    }
    Play hotbar;
    hotbar.skillRules_[97] = healData->second;
    hotbar.learnedSkills_[97] = 1;
    if (hotbar.skillAtSlot(0) != 0 || !hotbar.assignSkillToSlot(97,7) || hotbar.skillAtSlot(7) != 97) {
        std::fprintf(stderr,"05-W2: skill nao foi arrastada para F8\n"); return false;
    }
    hotbar.moveSkillSlot(7,2);
    if (hotbar.skillAtSlot(7) != 0 || hotbar.skillAtSlot(2) != 97) {
        std::fprintf(stderr,"05-W2: reposicionamento manual da skill falhou\n"); return false;
    }
    hotbar.moveSkillSlot(2,7);
    const char* hotbarSave = "/tmp/top-skillbar-save.txt";
    hotbar.save(hotbarSave);
    Play restoredBar;
    restoredBar.skillRules_[97] = healData->second;
    restoredBar.load(hotbarSave);
    std::remove(hotbarSave);
    if (!restoredBar.hotbarLoaded_ || restoredBar.skills_[0].id != 0 ||
        restoredBar.skills_[7].id != 97 || restoredBar.skills_[7].name != "Heal") {
        std::fprintf(stderr, "skill: barra F1-F8 manual nao persistiu no save offline\n");
        return false;
    }
    if (damageOf(4, 5, 1, 12, 0) != 1) {
        std::fprintf(stderr, "combate: dano minimo contra def 12\n");
        return false;
    }
    if (damageOf(4, 5, 1, 12, 1) != 1) {
        std::fprintf(stderr, "combate: dano maximo contra def 12\n");
        return false;
    }
    if (damageOf(0, 0, 2, 80, 0) < 1) {
        std::fprintf(stderr, "combate: ataque zero ainda fere\n");
        return false;
    }
    if (damageOf(30, 30, 7, 0, 0) != 30) {
        std::fprintf(stderr, "combate: o teto antigo de dano ainda esta ativo\n");
        return false;
    }
    if (rawDamage(30, 30, 7, 0, 0) != 30) {
        std::fprintf(stderr, "combate: o golpe do jogador nao deve ter teto\n");
        return false;
    }
    // 05-U: a vida real dos monstros de nivel alto nao pode ser truncada.
    const auto boss = historical_stats::monsterFromColumns(51, 16760, 700, 548, 823, 163, 184, 200);
    if (boss.hp != 16760 || boss.minAttack != 548 || boss.maxAttack != 823 ||
        historical_stats::physicalDamage(548, 163) != 385) {
        std::fprintf(stderr, "05-U: atributos de chefes foram reduzidos\n");
        return false;
    }
    Play levelUp;
    levelUp.sheetReady_ = true;
    levelUp.sheetHp_ = 40; levelUp.sheetSp_ = 18;
    levelUp.baseMin_ = 4; levelUp.baseMax_ = 5; levelUp.baseDef_ = 3;
    levelUp.player_.level = 1; levelUp.player_.maxHp = 40; levelUp.player_.hp = 40;
    levelUp.player_.homeX = 18; levelUp.player_.homeY = 18;
    levelUp.applyAttrs(false);
    const int levelOneHp = levelUp.player_.maxHp;
    levelUp.player_.level = 2;
    levelUp.applyAttrs(true);
    if (levelUp.player_.maxHp <= levelOneHp || levelUp.player_.hp <= 40) {
        std::fprintf(stderr, "05-U: HP nao cresce ao subir de nivel\n");
        return false;
    }
    Play play;
    play.player_.level = 1;
    play.player_.hp = 40;
    play.player_.maxHp = 40;
    play.player_.minAtk = 50;
    play.player_.maxAtk = 50;
    play.player_.def = 0;
    play.player_.cd = 0.1f;
    play.player_.cdLeft = 0.f;
    play.player_.range = 1.6f;
    play.player_.homeX = 18.f;
    play.player_.homeY = 18.f;
    play.makeQuests();
    Foe foe;
    foe.id = 71;
    foe.name = "Sleepy Snail";
    foe.level = 2;
    foe.hp = 1;
    foe.maxHp = 1;
    foe.x = 1.f;
    foe.y = 0.f;
    foe.homeX = 1.f;
    foe.homeY = 0.f;
    foe.range = 1.6f;
    foe.cd = 9.f;
    foe.cdLeft = 9.f;
    play.foes_.push_back(foe);
    ItemInfo shell;
    shell.id = 4319;
    shell.name = "Flowery Snail Shell";
    shell.price = 4;
    shell.maxStack = 99;
    play.items_[4319] = shell;
    play.foes_[0].drops.push_back({4319, 10000});
    play.target_ = 0;
    play.pendingBasicStrike_ = true;
    const int hpBefore = play.foes_[0].hp;
    play.resolve(0.05f, 0.f, 0.f);
    if (play.attackActionBase_ != 7 || play.playerPose() != 2) {
        std::fprintf(stderr, "05-E: primeiro ataque deveria usar pose 7\n");
        return false;
    }
    if (play.foes_[0].hp != hpBefore || play.pendingTarget_ != 0) {
        std::fprintf(stderr, "combate: dano aconteceu antes do quadro-chave\n");
        return false;
    }
    play.resolve(play.pendingAttack_ + .01f, 0.f, 0.f);
    if (play.foes_[0].hp != 0 || play.foes_[0].hp >= hpBefore || play.quests_[0].have != 1) {
        std::fprintf(stderr, "combate: o golpe nao derrubou o caracol\n");
        return false;
    }
    play.foes_[0].x = 9.f;
    play.foes_[0].hp = 5;
    play.foes_[0].deadFor = 0.f;
    play.player_.cdLeft = 0.f;
    play.swing_ = 0.f;
    play.pendingBasicStrike_ = true; // explicit click while out of range
    play.resolve(0.05f, 0.f, 0.f);
    if (play.foes_[0].hp != 5) {
        std::fprintf(stderr, "combate: golpe fora do alcance\n");
        return false;
    }
    play.foes_[0].x = 1.f;
    play.foes_[0].hp = 1;
    play.foes_[0].deadFor = 0.f;
    play.player_.cdLeft = 0.f;
    play.swing_ = 0.f;
    play.pendingBasicStrike_ = true;
    play.resolve(0.05f, 0.f, 0.f);
    if (play.attackActionBase_ != 8) {
        std::fprintf(stderr, "05-E: segundo ataque deveria usar pose 8\n");
        return false;
    }
    play.resolve(play.pendingAttack_ + .01f, 0.f, 0.f);
    play.foes_[0].hp = 1;
    play.foes_[0].deadFor = 0.f;
    play.player_.cdLeft = 0.f;
    play.swing_ = 0.f;
    play.pendingBasicStrike_ = true;
    play.resolve(0.05f, 0.f, 0.f);
    if (play.attackActionBase_ != 9) {
        std::fprintf(stderr, "05-E: terceiro ataque deveria usar pose 9\n");
        return false;
    }
    play.resolve(play.pendingAttack_ + .01f, 0.f, 0.f);
    if (!play.quests_[0].claimed || play.quests_[0].have != 3) {
        std::fprintf(stderr, "combate: missao do caracol nao fechou\n");
        return false;
    }
    if (play.gold_ < 15 || play.exp_ <= 0) {
        std::fprintf(stderr, "combate: recompensa vazia\n");
        return false;
    }
    if (play.ap_ != 1 || play.player_.maxHp != 40 || play.player_.minAtk != 50) {
        std::fprintf(stderr, "combate: o nivel nao deu so o ponto (%d hp %d atk %d)\n", play.ap_, play.player_.maxHp,
                     play.player_.minAtk);
        return false;
    }
    if (play.loots_.size() != 3 || play.loots_[0].id != 4319) {
        std::fprintf(stderr, "combate: o caracol nao largou o loot\n");
        return false;
    }
    play.gather(1.f, 0.f);
    if (play.bag_.size() != 1 || play.bag_[0].id != 4319 || play.bag_[0].count != 3) {
        std::fprintf(stderr, "combate: a bolsa nao recebeu o loot\n");
        return false;
    }
    const int goldBefore = play.gold_;
    play.shopOpen_=true;
    play.sellBag(0);
    if (play.gold_ != goldBefore + 2 || play.bag_[0].count != 2) {
        std::fprintf(stderr, "combate: a venda nao entrou no ouro\n");
        return false;
    }

    Play retaliation;
    retaliation.player_.hp = retaliation.player_.maxHp = 40;
    retaliation.player_.def = 0;
    Foe attacker;
    attacker.hp = attacker.maxHp = 30;
    attacker.x = 1.f;
    attacker.range = 1.6f;
    attacker.minAtk = attacker.maxAtk = 6;
    attacker.cd = 1.f;
    attacker.ai = 10;
    attacker.vision = 20.f;
    retaliation.foes_.push_back(attacker);
    retaliation.resolve(.05f, 0.f, 0.f);
    if (retaliation.player_.hp != 40 || retaliation.foes_[0].attackPending <= 0.f) {
        std::fprintf(stderr, "combate: monstro acertou antes da animacao\n");
        return false;
    }
    retaliation.resolve(.16f, 0.f, 0.f);
    if (retaliation.player_.hp >= 40 || retaliation.foes_[0].attackPending > 0.f) {
        std::fprintf(stderr, "combate: quadro de impacto do monstro nao aplicou dano\n");
        return false;
    }

    // 05-Q: uma criatura com skill de 9m deve iniciar o voo, aguardar
    // seu quadro de impacto e cancelar o dano se o alvo escapar.
    Play rangedMonster;
    rangedMonster.player_.hp = rangedMonster.player_.maxHp = 50;
    rangedMonster.player_.radius = .4f;
    rangedMonster.player_.def = 0;
    rangedMonster.player_.cdLeft = 100.f; // jogador nao revida neste teste
    Foe shadow;
    shadow.id = 201;
    shadow.name = "Shadow Hunter";
    shadow.ai = 10;
    shadow.hp = shadow.maxHp = 30;
    shadow.x = shadow.homeX = 5.f;
    shadow.y = shadow.homeY = 0.f;
    shadow.radius = .4f;
    shadow.range = 1.f; // alcance do golpe comum, inferior a skill 138
    shadow.cd = 1.f;
    shadow.vision = 10.f;
    shadow.leash = 15.f;
    shadow.minAtk = shadow.maxAtk = 6;
    shadow.attacks.push_back({138, 100, 9.f, 40.f});
    rangedMonster.foes_.push_back(shadow);
    SkillRule shadowSkill;
    shadowSkill.id = 138;
    shadowSkill.flightEffectId = 1036;
    rangedMonster.skillRules_[138] = shadowSkill;
    rangedMonster.resolve(.05f, 0.f, 0.f);
    const Foe& pending = rangedMonster.foes_[0];
    if (pending.pendingSkillId != 138 || pending.attackPending < .15f ||
        rangedMonster.player_.hp != 50) {
        std::fprintf(stderr, "05-Q: monstro nao iniciou skill de longa distancia\n");
        return false;
    }
    bool cast = false, flight = false;
    for (const auto& ev : rangedMonster.takeEffects()) {
        cast |= ev.kind == EffectEvent::Cast && ev.variant == 138;
        flight |= ev.kind == EffectEvent::Flight && ev.variant == 138 && ev.duration >= .15f;
    }
    if (!cast || !flight) {
        std::fprintf(stderr, "05-Q: faltaram eventos Cast/Flight historicos\n");
        return false;
    }
    rangedMonster.resolve(.08f, 0.f, 0.f);
    if (rangedMonster.player_.hp != 50) return false;
    rangedMonster.resolve(.15f, 0.f, 0.f);
    if (rangedMonster.player_.hp >= 50 || rangedMonster.foes_[0].pendingSkillId != 0) {
        std::fprintf(stderr, "05-Q: impacto de skill fora da janela esperada\n");
        return false;
    }
    bool hitVisual = false;
    for (const auto& ev : rangedMonster.takeEffects())
        hitVisual |= ev.kind == EffectEvent::SkillHit && ev.variant == 138;
    if (!hitVisual) {
        std::fprintf(stderr, "05-Q: faltou visual do impacto da skill 138\n");
        return false;
    }
    rangedMonster.foes_[0].cdLeft = 0.f;
    rangedMonster.foes_[0].swing = 0.f;
    rangedMonster.resolve(.05f, 0.f, 0.f); // novo tiro
    const int hpAfter = rangedMonster.player_.hp;
    rangedMonster.resolve(.5f, 20.f, 0.f); // esquiva para fora do alcance
    if (rangedMonster.player_.hp != hpAfter || rangedMonster.foes_[0].pendingSkillId != 0) {
        std::fprintf(stderr, "05-Q: tiro acertou apos jogador escapar\n");
        return false;
    }
    // Registros sem skills validas continuam com o golpe basico do 05-M.
    rangedMonster.foes_[0].attacks.clear();
    rangedMonster.foes_[0].x = 1.f;
    rangedMonster.foes_[0].homeX = 1.f;
    rangedMonster.foes_[0].cdLeft = 0.f;
    rangedMonster.resolve(.05f, 0.f, 0.f);
    if (rangedMonster.foes_[0].attackPending <= 0.f ||
        rangedMonster.foes_[0].pendingSkillId != 0) {
        std::fprintf(stderr, "05-Q: fallback do golpe basico nao iniciou\n");
        return false;
    }

    Play roll;
    ItemInfo junk;
    junk.id = 1;
    junk.name = "Poeira";
    junk.price = 1;
    roll.items_[1] = junk;
    Foe rare;
    rare.drops.push_back({1, 6838});
    roll.foes_.push_back(rare);
    roll.seed_ = 1;
    roll.dropLoot(roll.foes_[0]);
    if (!roll.loots_.empty()) {
        std::fprintf(stderr, "combate: drop abaixo do dado caiu\n");
        return false;
    }
    roll.seed_ = 1;
    roll.foes_[0].drops[0].rate = 6839;
    roll.dropLoot(roll.foes_[0]);
    if (roll.loots_.size() != 1) {
        std::fprintf(stderr, "combate: drop no dado nao caiu\n");
        return false;
    }

    Play heal;
    heal.player_.hp = 10;
    heal.player_.maxHp = 40;
    heal.player_.homeX = 18.f;
    heal.player_.homeY = 5.f;
    ItemInfo fruit;
    fruit.id = 3117;
    fruit.name = "Red Date";
    fruit.healHp = 20;
    heal.items_[3117] = fruit;
    heal.bag_.push_back({3117, 1});
    heal.clickBag(0);
    if (heal.player_.hp != 30 || !heal.bag_.empty()) {
        std::fprintf(stderr, "combate: a fruta nao curou\n");
        return false;
    }

    char medkitDirTemplate[]="/tmp/top-medkit-XXXXXX";
    char* medkitDirPath=::mkdtemp(medkitDirTemplate);
    if(!medkitDirPath) {
        std::fprintf(stderr,"medkit: nao criou diretorio temporario\n");
        return false;
    }
    const std::string medkitRoot(medkitDirPath);
    const std::string medkitUser=medkitRoot+"/user";
    if(::mkdir(medkitUser.c_str(),0700)!=0) {
        ::rmdir(medkitRoot.c_str());
        std::fprintf(stderr,"medkit: nao criou pasta de save\n");
        return false;
    }
    Play historicMedkit;
    historicMedkit.root_=medkitRoot;
    historicMedkit.profileId_=9061;
    historicMedkit.loadItems("Client/scripts/table/iteminfo.txt",{3909,6590,6591,6592});
    historicMedkit.loadHeals("Client/scripts/offline/calculate/variable.lua");
    historicMedkit.player_.hp=50;
    historicMedkit.player_.maxHp=100;
    historicMedkit.player_.homeX=100.f;
    historicMedkit.player_.homeY=50.f;
    historicMedkit.bagCapacity_=2;
    historicMedkit.bag_={Stack{6590,2}};
    historicMedkit.clickBag(0);
    const bool medkitPercentApplied=historicMedkit.player_.hp==60 && historicMedkit.player_.homeY==60.f &&
        historicMedkit.bagCount(6590)==1 && historicMedkit.itemCooldownRemaining_.count(6590)==1;
    const std::string medkitSave=historicMedkit.savePath();
    Play historicMedkitReloaded;
    historicMedkitReloaded.load(medkitSave);
    const bool medkitPersisted=historicMedkitReloaded.bagCount(6590)==1 &&
        historicMedkitReloaded.player_.hp==60 && historicMedkitReloaded.player_.homeY==60.f;
    historicMedkit.clickBag(0);
    const bool medkitCooldownPreserved=historicMedkit.player_.hp==60 && historicMedkit.bagCount(6590)==1;
    for(int tick=0;tick<20;++tick)historicMedkit.step(.05f,nullptr,0.f,0.f);
    historicMedkit.clickBag(0);
    const bool medkitCooldownExpired=historicMedkit.player_.hp==70 && historicMedkit.player_.homeY==70.f &&
        historicMedkit.bagCount(6590)==0;
    Play percentageKit;
    percentageKit.loadItems("Client/scripts/table/iteminfo.txt",{3909});
    percentageKit.loadHeals("Client/scripts/offline/calculate/variable.lua");
    percentageKit.player_.hp=50;percentageKit.player_.maxHp=100;
    percentageKit.player_.homeX=100.f;percentageKit.player_.homeY=50.f;
    percentageKit.bagCapacity_=2;percentageKit.bag_={Stack{3909,1}};
    percentageKit.clickBag(0);
    const bool hpPercentKitApplied=percentageKit.player_.hp==80 && percentageKit.player_.homeY==50.f &&
        percentageKit.bag_.empty();
    auto prepareFoodStateTest=[](Play& play) {
        play.player_.id=1;
        play.player_.level=100;
        play.job_=0;
        play.bstr_=play.bagi_=play.bdex_=play.bcon_=play.bsta_=20;
        play.sheetReady_=true;
        play.applyAttrs(false);
        play.player_.hp=1;
        play.player_.homeY=1.f;
        play.bagCapacity_=4;
    };
    Play cookingHp;
    cookingHp.loadItems("Client/scripts/table/iteminfo.txt",{4024,4028});
    cookingHp.loadHeals("Client/scripts/offline/calculate/variable.lua");
    prepareFoodStateTest(cookingHp);
    const int cookingBaseHp=cookingHp.player_.maxHp;
    const int cookingBaseDef=cookingHp.player_.def;
    cookingHp.bag_={Stack{4028,1}}; // State 165 level 10, 60 seconds
    cookingHp.clickBag(0);
    const bool cookingHpApplied=cookingHp.foodStates_[0].level==10 &&
        cookingHp.player_.maxHp==cookingBaseHp+10000 && cookingHp.player_.def==cookingBaseDef+8 &&
        cookingHp.bag_.empty();
    cookingHp.bag_={Stack{4024,1}}; // Lower level must not replace or grant a free heal.
    const int cookingHpBeforeReject=cookingHp.player_.hp;
    cookingHp.clickBag(0);
    const bool cookingLowerRejected=cookingHp.player_.hp==cookingHpBeforeReject && cookingHp.bagCount(4024)==1;
    for(int tick=0;tick<1201;++tick)cookingHp.step(.05f,nullptr,0.f,0.f);
    const bool cookingHpExpired=cookingHp.foodStates_[0].level==0 &&
        cookingHp.player_.maxHp==cookingBaseHp && cookingHp.player_.def==cookingBaseDef;
    Play cookingAtk;
    cookingAtk.loadItems("Client/scripts/table/iteminfo.txt",{6407});
    cookingAtk.loadHeals("Client/scripts/offline/calculate/variable.lua");
    prepareFoodStateTest(cookingAtk);
    const int cookingBaseAtk=cookingAtk.player_.minAtk;
    cookingAtk.bag_={Stack{6407,1}};
    cookingAtk.clickBag(0);
    const bool cookingAtkApplied=cookingAtk.foodStates_[1].level==1 &&
        cookingAtk.player_.minAtk==cookingBaseAtk+50 && cookingAtk.player_.hp==401 &&
        cookingAtk.bag_.empty();
    Play cookingSpr;
    cookingSpr.loadItems("Client/scripts/table/iteminfo.txt",{1084});
    cookingSpr.loadHeals("Client/scripts/offline/calculate/variable.lua");
    prepareFoodStateTest(cookingSpr);
    const float cookingBaseSp=cookingSpr.player_.homeX;
    cookingSpr.bag_={Stack{1084,1}};
    cookingSpr.clickBag(0);
    const bool cookingSprApplied=cookingSpr.foodStates_[2].level==1 &&
        cookingSpr.player_.homeX>cookingBaseSp && cookingSpr.player_.homeY==201.f &&
        cookingSpr.bag_.empty();
    std::remove(medkitSave.c_str());
    std::remove((medkitSave+".bak").c_str());
    ::rmdir(medkitUser.c_str());
    ::rmdir(medkitRoot.c_str());
    if(!medkitPercentApplied||!medkitPersisted||!medkitCooldownPreserved||
       !medkitCooldownExpired||!hpPercentKitApplied||!cookingHpApplied||!cookingLowerRejected||
       !cookingHpExpired||!cookingAtkApplied||!cookingSprApplied) {
        std::fprintf(stderr,"medkit: percent=%d persisted=%d cooldown=%d expired=%d hp_percent=%d food_hp=%d food_reject=%d food_expire=%d food_atk=%d food_sp=%d\n",
            medkitPercentApplied,medkitPersisted,medkitCooldownPreserved,medkitCooldownExpired,hpPercentKitApplied,
            cookingHpApplied,cookingLowerRejected,cookingHpExpired,cookingAtkApplied,cookingSprApplied);
        return false;
    }

    Play gear;
    gear.player_.level = 1;
    gear.baseMin_ = 4;
    gear.baseMax_ = 5;
    gear.baseDef_ = 3;
    gear.player_.minAtk = 4;
    gear.player_.maxAtk = 5;
    gear.player_.def = 3;
    ItemInfo knife;
    knife.id = 8;
    knife.name = "Newbie Knife";
    knife.minAtk = 14;
    knife.maxAtk = 18;
    knife.level = 1;
    knife.slot = 9;
    knife.price = 27;
    gear.items_[8] = knife;
    gear.bag_.push_back({8, 1});
    gear.clickBag(0);
    if (gear.player_.minAtk != 18 || gear.player_.maxAtk != 23 || !gear.isWorn(8)) {
        std::fprintf(stderr, "combate: a faca nao entrou no ataque\n");
        return false;
    }
    gear.clickBag(0);
    if (gear.player_.minAtk != 4 || gear.player_.maxAtk != 5 || gear.isWorn(8)) {
        std::fprintf(stderr, "combate: a faca nao saiu\n");
        return false;
    }
    gear.clickBag(0);
    gear.clickEquip(9);
    if (gear.isWorn(8) || gear.player_.minAtk != 4) {
        std::fprintf(stderr, "combate: o slot da arma nao tirou a faca\n");
        return false;
    }
    Play hold;
    ItemInfo dust;
    dust.id = 3;
    dust.name = "Poeira";
    dust.price = 1;
    dust.maxStack = 99;
    hold.items_[3] = dust;
    hold.addBag(3, 2);
    hold.stashBag(0);
    if (!hold.bag_.empty() || hold.temp_.size() != 1 || hold.temp_[0].id != 3 || hold.temp_[0].count != 2) {
        std::fprintf(stderr, "bolsa: a temporaria nao guardou\n");
        return false;
    }
    hold.takeTemp(0);
    if (hold.bag_.size() != 1 || hold.bag_[0].count != 2 || !hold.temp_.empty()) {
        std::fprintf(stderr, "bolsa: a temporaria nao devolveu\n");
        return false;
    }
    char bankTestDirTemplate[] = "/tmp/top-offline-bank-XXXXXX";
    char* bankTestDir = ::mkdtemp(bankTestDirTemplate);
    if (!bankTestDir) {
        std::fprintf(stderr, "banco: nao criou diretorio isolado de teste\n"); return false;
    }
    Play vault;
    vault.root_ = bankTestDir;
    vault.items_[3] = dust;
    vault.gold_ = 500;
    vault.addBag(3, 2);
    if (!vault.openBank() || vault.gold_ != 300 || !vault.bankOpen()) {
        std::fprintf(stderr, "banco: taxa historica/abertura invalida\n"); return false;
    }
    vault.depositBank(0);
    if (!vault.bag_.empty() || vault.bank_.size()!=1 || vault.bank_[0].count!=2) {
        std::fprintf(stderr, "banco: deposito no cofre falhou\n"); return false;
    }
    const std::string bankSavePath = vault.savePath();
    Play restoredVault;
    restoredVault.items_[3] = dust;
    restoredVault.root_ = bankTestDir;
    restoredVault.load(bankSavePath);
    if (restoredVault.bank_.size()!=1 || restoredVault.bank_[0].id!=3 || restoredVault.bank_[0].count!=2) {
        std::fprintf(stderr, "banco: conteudo nao persistiu no save\n"); return false;
    }
    restoredVault.bankOpen_=true;
    restoredVault.withdrawBank(0);
    if (restoredVault.bag_.size()!=1 || restoredVault.bag_[0].count!=2 || !restoredVault.bank_.empty()) {
        std::fprintf(stderr, "banco: retirada do cofre falhou\n"); return false;
    }
    Play failedVault;
    failedVault.root_ = "/proc/top-offline-bank-save-failure";
    failedVault.gold_ = 500;
    failedVault.items_[3] = dust;
    failedVault.addBag(3, 2);
    if (failedVault.openBank() || failedVault.gold_ != 500 || failedVault.bankOpen_) {
        std::fprintf(stderr, "banco: falha de save debitou a taxa ou abriu o cofre\n"); return false;
    }
    failedVault.bankOpen_ = true;
    failedVault.depositBank(0);
    if (failedVault.bag_.size() != 1 || failedVault.bag_[0].count != 2 || !failedVault.bank_.empty()) {
        std::fprintf(stderr, "banco: falha de save nao reverteu deposito\n"); return false;
    }
    failedVault.bank_.push_back({3, 1});
    failedVault.bag_.clear();
    failedVault.withdrawBank(0);
    if (!failedVault.bag_.empty() || failedVault.bank_.size() != 1 || failedVault.bank_[0].count != 1) {
        std::fprintf(stderr, "banco: falha de save nao reverteu retirada\n"); return false;
    }
    const std::string bankBackupPath = bankSavePath + ".bak";
    std::remove(bankSavePath.c_str());
    std::remove(bankBackupPath.c_str());
    ::rmdir(bankTestDir);
    // Column 20 controls the capacity; partial stacks spill into empty slots.
    Play full;
    full.items_[3] = dust;
    if (!full.addBag(3,99) || !full.addBag(3,1) || full.bag_.size()!=2 ||
        full.bag_[0].count!=99 || full.bag_[1].count!=1) {
        std::fprintf(stderr,"bolsa: pilhas de 99 nao dividiram corretamente\n");
        return false;
    }
    // Adding is atomic: no items lost when the grid cannot fit more stacks.
    Play noSpace;
    noSpace.items_[3]=dust;
    noSpace.bag_.assign(24,{3,99});
    if(noSpace.addBag(3,1) || noSpace.bag_.size()!=24 || noSpace.bag_[23].count!=99) {
        std::fprintf(stderr,"bolsa: inserir com inventario cheio perdeu itens\n");
        return false;
    }
    // Original iteminfo.txt IDs 3088..3093 add exactly four inventory slots
    // each. Wrong-order usage must not consume the upgrade item.
    Play expanded;
    expanded.items_[3] = dust;
    for (int id=3088; id<=3093; ++id) {
        ItemInfo token;
        token.id=id;
        token.name="Inventory Slot";
        token.maxStack=1;
        expanded.items_[id]=token;
    }
    expanded.bag_.assign(23,{3,99});
    expanded.bag_.push_back({3093,1});
    expanded.clickBag(23);
    if (expanded.bagCapacity_!=24 || expanded.bag_.size()!=24) {
        std::fprintf(stderr,"bolsa: upgrade fora de sequencia consumiu item\n");
        return false;
    }
    expanded.bag_.back()={3088,1};
    expanded.clickBag(23);
    if (expanded.bagCapacity_!=28 || expanded.bag_.size()!=23) {
        std::fprintf(stderr,"bolsa: primeiro item nao liberou 28 espacos\n");
        return false;
    }
    for (int id=3089;id<=3093;++id) {
        expanded.bag_.push_back({id,1});
        expanded.clickBag(static_cast<int>(expanded.bag_.size())-1);
        if (expanded.bagCapacity_!=inventory_rules::upgradeTarget(id)) {
            std::fprintf(stderr,"bolsa: upgrade %d nao mudou capacidade\n",id);
            return false;
        }
    }
    if(!expanded.addBag(3,(48-23)*99) || expanded.bag_.size()!=48 || expanded.addBag(3,1)) {
        std::fprintf(stderr,"bolsa: limite 48 nao foi respeitado\n");
        return false;
    }
    const char* capacitySave="/tmp/top-inventory-capacity-save.txt";
    expanded.save(capacitySave);
    Play restoredCapacity;
    restoredCapacity.items_=expanded.items_;
    restoredCapacity.load(capacitySave);
    std::remove(capacitySave);
    if (restoredCapacity.bagCapacity_!=48 || restoredCapacity.bag_.size()!=48 ||
        restoredCapacity.addBag(3,1)) {
        std::fprintf(stderr,"bolsa: capacidade nao persistiu no save\n");
        return false;
    }
    // Non-stackable swords must occupy different cells and both hands.
    Play dual;
    ItemInfo sword=knife;
    sword.id=1;
    sword.equipSlots={9,6};
    sword.maxStack=1;
    dual.items_[1]=sword;
    dual.player_.level=20;
    if(!dual.addBag(1,2) || dual.bag_.size()!=2 ||
       dual.bag_[0].count!=1 || dual.bag_[1].count!=1) {
        std::fprintf(stderr,"bolsa: duas espadas foram agrupadas\n");
        return false;
    }
    dual.clickBag(0);
    dual.clickBag(1);
    if(dual.worn_[9]!=1 || dual.worn_[6]!=1 ||
       dual.equippedSlotAtBagIndex(0)<0 || dual.equippedSlotAtBagIndex(1)<0) {
        std::fprintf(stderr,"equipamento: espadas nao ocupararam duas maos\n");
        return false;
    }
    dual.clickEquip(6);
    if(dual.worn_.count(6) || dual.worn_.count(9)!=1 ||
       dual.equippedSlotAtBagIndex(1)>=0) {
        std::fprintf(stderr,"equipamento: tirar arma secundaria removeu a principal\n");
        return false;
    }
    // A hat belongs to location 0 even without attack/defense bonuses.
    ItemInfo hat;
    hat.id=828; hat.name="Cap"; hat.slot=0; hat.equipSlots={0};
    dual.items_[828]=hat;
    dual.bag_.push_back({828,1});
    dual.clickBag(2);
    if(dual.worn_.count(0)==0 || dual.worn_[0]!=828) {
        std::fprintf(stderr,"equipamento: o chapeu nao entrou no slot 0\n");
        return false;
    }
    gear.gold_ = 27;
    gear.vendorOn_ = true;
    gear.setVendor(0.f, 0.f);
    gear.bag_.clear();
    gear.interact(9.f, 0.f);
    if (gear.gold_ != 27 || gear.hasBag(8)) {
        std::fprintf(stderr, "combate: comprou de longe\n");
        return false;
    }
    gear.interact(0.f, 0.f);
    if (!gear.shopOpen_ || gear.shopHit(965, 142, 1280, 720) != 0) {
        std::fprintf(stderr, "combate: painel da loja nao abriu\n");
        return false;
    }
    gear.clickShop(0);
    if (gear.gold_ != 0 || !gear.hasBag(8)) {
        std::fprintf(stderr, "combate: o vendedor nao vendeu a faca\n");
        return false;
    }

    Play merchant;
    merchant.setVendor(0.f,0.f,"Goldie",{8});
    merchant.talkNpc("Goldie","Welcome to my store",true);
    if(!merchant.npcDialogOpen() || merchant.shopOpen() || !merchant.npcOffersTrade_) {
        std::fprintf(stderr,"npc: loja abriu sem dialogo/Trade\n"); return false;
    }
    merchant.hereX_=merchant.hereY_=0.f;
    if(merchant.dialogActionAt(1280/2-160+42,720-316+272,1280,720)!=3) {
        std::fprintf(stderr,"npc: botao Trade incorreto\n"); return false;
    }
    merchant.clickDialog(3);
    if(!merchant.shopOpen() || merchant.npcDialogOpen()) {
        std::fprintf(stderr,"npc: Trade nao abriu a loja\n"); return false;
    }
    // 05-AD: Goldie possui Trade e Forge no r_trade01. A acao Forge
    // nao deve comprar, aceitar uma missao nem alterar o inventario.
    merchant.talkNpc("Goldie", "Welcome to my forge", true, true);
    if (!merchant.npcOffersForge_ ||
        merchant.dialogActionAt(1280/2-160+145,720-316+272,1280,720)!=4 ||
        merchant.dialogActionAt(1280/2-160+42,720-316+272,1280,720)!=3) {
        std::fprintf(stderr,"npc: botoes Forge/Trade incorretos\n"); return false;
    }
    merchant.clickDialog(4);
    if(merchant.npcDialogOpen() || merchant.npcOffersForge_ || merchant.shopOpen()) {
        std::fprintf(stderr,"npc: Forge nao encerrou conversa sem abrir loja\n");return false;
    }
    Play offered;
    offered.player_.level = 10;
    if (!offered.offerHistoricalMission(900, "Teste", "Visite o alvo", 12.f, 14.f, 0, 0, 10, 5) ||
        offered.historical_.active) {
        std::fprintf(stderr, "missao: oferta aceitou automaticamente\n");
        return false;
    }
    offered.talkNpc("NPC", "Voce aceita esta missao?");
    const int accept = offered.dialogActionAt(620, 680, 1280, 720);
    if (accept != 1) {
        std::fprintf(stderr, "missao: botao aceitar nao respondeu\n");
        return false;
    }
    offered.clickDialog(accept);
    if (!offered.historical_.active || offered.historical_.id != 900 || offered.npcTalkOpen_) {
        std::fprintf(stderr, "missao: aceite da janela nao iniciou progresso\n");
        return false;
    }

    Play grown;
    grown.player_.id = 1;
    grown.player_.name = "Lance";
    grown.player_.level = 1;
    grown.player_.hp = 40;
    grown.player_.maxHp = 40;
    grown.player_.minAtk = 4;
    grown.player_.maxAtk = 5;
    grown.player_.def = 3;
    grown.player_.homeX = 18.f;
    grown.player_.homeY = 18.f;
    grown.sheetHp_ = 40;
    grown.sheetSp_ = 18;
    grown.baseMin_ = 4;
    grown.baseMax_ = 5;
    grown.baseDef_ = 3;
    grown.sheetReady_ = true;
    grown.ap_ = 3;
    grown.bstr_ = 5;
    grown.bcon_ = 5;
    grown.bsta_ = 5;
    grown.bdex_ = 5;
    grown.bagi_ = 5;
    Play formulaParity;
    formulaParity.sheetReady_ = true;
    formulaParity.player_.level = 1;
    formulaParity.bstr_ = formulaParity.bagi_ = formulaParity.bdex_ =
        formulaParity.bcon_ = formulaParity.bsta_ = 5;
    formulaParity.applyAttrs(false);
    if (formulaParity.player_.maxHp != 100 || formulaParity.player_.homeX != 23.f ||
        formulaParity.player_.minAtk != 7 || formulaParity.player_.maxAtk != 7 ||
        formulaParity.player_.def != 2 || formulaParity.player_.crit != 15) {
        std::fprintf(stderr, "AttrCalculate: ficha Newbie base nao corresponde ao Lua (%d HP, %.0f SP, %d ATK, %d DEF)\n",
                     formulaParity.player_.maxHp, formulaParity.player_.homeX,
                     formulaParity.player_.minAtk, formulaParity.player_.def);
        return false;
    }
    Play hunterCrit;
    hunterCrit.sheetReady_ = true;
    hunterCrit.job_ = 2;
    hunterCrit.player_.level = 1;
    hunterCrit.bstr_ = hunterCrit.bagi_ = hunterCrit.bdex_ =
        hunterCrit.bcon_ = hunterCrit.bsta_ = 5;
    hunterCrit.applyAttrs(false);
    if (hunterCrit.player_.crit != 14) {
        std::fprintf(stderr, "AttrCalculate: coeficiente de LUK/critico do Hunter incorreto (%d)\n",
                     hunterCrit.player_.crit);
        return false;
    }
    grown.spendAttr(0);
    if (grown.ap_ != 2 || grown.bstr_ != 6 || grown.player_.minAtk != 9 || grown.player_.maxAtk != 9) {
        std::fprintf(stderr, "atributo: FOR nao subiu o ataque (%d-%d pontos %d)\n", grown.player_.minAtk,
                     grown.player_.maxAtk, grown.ap_);
        return false;
    }
    grown.spendAttr(2);
    if (grown.player_.maxHp != 109 || grown.bcon_ != 6) {
        std::fprintf(stderr, "atributo: CON nao subiu o HP (%d)\n", grown.player_.maxHp);
        return false;
    }
    grown.spendAttr(3);
    if (static_cast<int>(grown.player_.homeX) != 26 || grown.bsta_ != 6) {
        std::fprintf(stderr, "atributo: STA nao subiu o SP (%d)\n", static_cast<int>(grown.player_.homeX));
        return false;
    }
    grown.player_.level = 8;
    grown.gold_ = 2500;
    grown.setJob(1);
    if (grown.job_ != 0) {
        std::fprintf(stderr, "classe: promocao liberada antes do nivel 9\n");
        return false;
    }
    grown.player_.level = 9;
    grown.gold_ = 2499;
    grown.setJob(2);
    if (grown.job_ != 0 || grown.gold_ != 2499) {
        std::fprintf(stderr, "classe: promocao cobrou abaixo dos 2.500 ouro\n");
        return false;
    }
    grown.player_.level = 10;
    grown.gold_ = 2500;
    grown.ap_ = 1;
    grown.setJob(2);
    if (grown.gold_ != 0) {
        std::fprintf(stderr, "classe: promocao de Hunter nao cobrou 2.500 ouro\n");
        return false;
    }
    const int atkBeforeDex = grown.player_.minAtk;
    grown.spendAttr(4);
    if (grown.job_ != 2 || grown.player_.minAtk != atkBeforeDex + 2) {
        std::fprintf(stderr, "classe: Hunter nao usa DEX no ataque (%d -> %d)\n", atkBeforeDex, grown.player_.minAtk);
        return false;
    }
    grown.gold_ = 2500;
    grown.setJob(1);
    if (grown.job_ != 2 || grown.gold_ != 2500) {
        std::fprintf(stderr, "classe: primeira promocao permitiu trocar de classe/cobrou novamente\n");
        return false;
    }
    Play phyllisClass;
    phyllisClass.player_.id = 3;
    phyllisClass.player_.name = "Phyllis";
    phyllisClass.player_.level = 9;
    phyllisClass.gold_ = 2500;
    phyllisClass.setJob(1);
    if (phyllisClass.job_ != 1 || phyllisClass.gold_ != 0) {
        std::fprintf(stderr, "classe: Lua permite Phyllis como Swordsman no nivel 9/2.500 ouro\n");
        return false;
    }
    Play amiClass;
    amiClass.player_.id = 4;
    amiClass.player_.name = "Ami";
    amiClass.player_.level = 10;
    amiClass.sheetReady_ = true;
    amiClass.sheetHp_ = 40;
    amiClass.sheetSp_ = 18;
    amiClass.gold_ = 2500;
    amiClass.setJob(1);
    if (amiClass.job_ != 0) {
        std::fprintf(stderr, "classe: Ami virou Swordsman\n");
        return false;
    }
    amiClass.setJob(5);
    if (amiClass.job_ != 5 || amiClass.gold_ != 0) {
        std::fprintf(stderr, "classe: Ami nao virou Herbalist com custo historico\n");
        return false;
    }
    // Cada promocao deve alterar os coeficientes da ficha, e não apenas o
    // nome exibido: HP/SP, ataque/defesa, precisão, fuga e velocidade.
    auto classSnapshot = [](int job) {
        Play value;
        value.player_.id = 1;
        value.player_.level = 20;
        value.sheetReady_ = true;
        value.sheetHp_ = 40;
        value.sheetSp_ = 18;
        value.baseMin_ = 4;
        value.baseMax_ = 6;
        value.baseDef_ = 3;
        value.bstr_ = value.bagi_ = value.bdex_ = value.bcon_ = value.bsta_ = 10;
        value.job_ = job;
        value.applyAttrs(false);
        return std::array<int, 7>{value.player_.maxHp, static_cast<int>(value.player_.homeX),
                                  value.player_.minAtk, value.player_.def, value.hit_, value.flee_,
                                  static_cast<int>(value.player_.speed * 100.f)};
    };
    const auto swordsman = classSnapshot(1);
    const auto hunterStats = classSnapshot(2);
    const auto explorer = classSnapshot(4);
    const auto herbalist = classSnapshot(5);
    const auto sailor = classSnapshot(3);
    const auto championStats = classSnapshot(8);
    if (swordsman == hunterStats || swordsman == explorer || hunterStats == explorer ||
        swordsman[0] == herbalist[0] || swordsman[1] == herbalist[1] || sailor == swordsman ||
        championStats == swordsman || championStats[0] != 1027 || championStats[1] != 95) {
        std::fprintf(stderr, "classe: bonus de coeficientes nao diferenciou as fichas\n");
        return false;
    }
    // Herbalist and Explorer intentionally share these AttrCalculate.lua
    // coefficients; a previous local approximation had invented larger SP.
    if (herbalist != explorer) {
        std::fprintf(stderr, "classe: Herbalist divergiu dos coeficientes historicos\n"); return false;
    }
    Play championSave;
    championSave.player_.id = 1;
    championSave.player_.level = 20;
    championSave.sheetReady_ = true;
    championSave.sheetHp_ = 40; championSave.sheetSp_ = 18;
    championSave.baseMin_ = 4; championSave.baseMax_ = 6; championSave.baseDef_ = 3;
    championSave.bstr_ = championSave.bagi_ = championSave.bdex_ =
        championSave.bcon_ = championSave.bsta_ = 10;
    championSave.job_ = 8;
    championSave.applyAttrs(false);
    const char* championPath = "/tmp/top-class-champion-save.txt";
    if (!championSave.save(championPath)) {
        std::fprintf(stderr, "classe: save de Champion falhou\n"); return false;
    }
    Play championReload;
    championReload.load(championPath);
    championReload.applyAttrs(false); // same stat rebuild as Play::bind after load
    std::remove(championPath);
    if (championReload.job_ != 8 || championReload.player_.maxHp != championSave.player_.maxHp ||
        championReload.player_.homeX != championSave.player_.homeX ||
        std::string(jobName(championReload.job_)) != "Champion") {
        std::fprintf(stderr, "classe: save avancado perdeu a profissao/coeficientes\n"); return false;
    }
    Play championHands;
    championHands.player_.id = 2; // Carsise pode ser Champion.
    championHands.player_.level = 60;
    championHands.job_ = 8;
    ItemInfo shield;
    shield.id = 8100; shield.name = "Historical Shield"; shield.type = 11;
    shield.slot = 6; shield.equipSlots = {6}; shield.def = 5;
    championHands.items_[shield.id] = shield;
    if (!championHands.addBag(shield.id, 1) || championHands.equipBagToSlot(0, 6) ||
        championHands.worn_.count(6) != 0 || championHands.bagCount(shield.id) != 1) {
        std::fprintf(stderr, "classe: Champion conseguiu equipar escudo\n"); return false;
    }
    championHands.job_ = 1;
    if (!championHands.equipBagToSlot(0, 6) || championHands.worn_.count(6) != 1) {
        std::fprintf(stderr, "classe: regra de escudo foi aplicada fora de Champion\n"); return false;
    }
    Play handSwap;
    handSwap.player_.id = 1;
    handSwap.player_.level = 60;
    ItemInfo twoHanded;
    twoHanded.id = 8101; twoHanded.name = "Historical Two-Handed Weapon";
    twoHanded.slot = 9; twoHanded.equipSlots = {9}; twoHanded.requiredEquipSlots = {9, 6};
    ItemInfo handShield;
    handShield.id = 8102; handShield.name = "Historical Offhand Shield";
    handShield.slot = 6; handShield.equipSlots = {6}; handShield.type = 11;
    handSwap.items_[twoHanded.id] = twoHanded;
    handSwap.items_[handShield.id] = handShield;
    if (!handSwap.addBag(twoHanded.id, 1) || !handSwap.addBag(handShield.id, 1) ||
        !handSwap.equipBagToSlot(0, -1) || handSwap.worn_.count(9) != 1 ||
        !handSwap.equipBagToSlot(1, 6) || handSwap.worn_.count(9) != 0 || handSwap.worn_.count(6) != 1 ||
        !handSwap.equipBagToSlot(0, -1) || handSwap.worn_.count(9) != 1 || handSwap.worn_.count(6) != 0) {
        std::fprintf(stderr, "equipamento: ItemInfo switch location nao liberou as duas maos\n"); return false;
    }
    Play raceGear;
    raceGear.player_.id = 4;
    raceGear.player_.name = "Ami";
    raceGear.player_.level = 20;
    raceGear.baseMin_ = 4;
    raceGear.baseMax_ = 5;
    ItemInfo lanceOnly;
    lanceOnly.id = 7000;
    lanceOnly.name = "Armadura de Lance";
    lanceOnly.slot = 2;
    lanceOnly.def = 5;
    lanceOnly.races = {1};
    lanceOnly.jobs = {-1};
    raceGear.items_[lanceOnly.id] = lanceOnly;
    raceGear.bag_.push_back({lanceOnly.id, 1});
    raceGear.clickBag(0);
    if (raceGear.isWorn(lanceOnly.id)) {
        std::fprintf(stderr, "equipamento: Ami vestiu item exclusivo de Lance\n");
        return false;
    }
    Play learner;
    learner.player_.id = 4;
    learner.player_.name = "Ami";
    learner.player_.level = 10;
    learner.job_ = 5;
    learner.tp_ = 1;
    ItemInfo healBook;
    healBook.id = 3206;
    healBook.name = "Heal";
    learner.items_[healBook.id] = healBook;
    learner.skillBooks_[healBook.id] = 97;
    SkillRule healRule;
    healRule.id = 97;
    healRule.name = "Heal";
    healRule.jobs = {5, 13, 14};
    healRule.points = 1;
    learner.skillRules_[97] = healRule;
    learner.bag_.push_back({healBook.id, 1});
    learner.clickBag(0);
    if (learner.learnedSkills_[97] != 1 || learner.tp_ != 0 || !learner.bag_.empty()) {
        std::fprintf(stderr, "skill: livro de Heal nao foi aprendido\n");
        return false;
    }
    // O livro do Harden sem ter o Heal nao pode contar o Heal como aprendido.
    Play prereq;
    prereq.player_.id = 4;
    prereq.player_.name = "Ami";
    prereq.player_.level = 30;
    prereq.job_ = 5;
    prereq.tp_ = 5;
    ItemInfo hardenBook;
    hardenBook.id = 3210;
    hardenBook.name = "Harden";
    prereq.items_[hardenBook.id] = hardenBook;
    prereq.skillBooks_[hardenBook.id] = 225;
    SkillRule hardenRule;
    hardenRule.id = 225;
    hardenRule.name = "Harden";
    hardenRule.jobs = {5};
    hardenRule.prerequisite = 97;
    hardenRule.prerequisiteLevel = 3;
    hardenRule.points = 1;
    prereq.skillRules_[225] = hardenRule;
    prereq.bag_.push_back({hardenBook.id, 1});
    prereq.clickBag(0);
    if (prereq.learnedSkills_.count(97) || prereq.learnedSkills_.count(225) || prereq.bag_.empty()) {
        std::fprintf(stderr, "skill: o pre-requisito virou skill aprendida\n");
        return false;
    }
    Play seat;
    seat.player_.hp = 10;
    seat.player_.maxHp = 40;
    seat.player_.homeX = 20.f;
    seat.player_.homeY = 4.f;
    seat.sheetHp_ = 40;
    seat.sheetSp_ = 20;
    seat.sheetReady_ = true;
    seat.bcon_ = 5;
    seat.bsta_ = 5;
    seat.applyAttrs(false);
    seat.player_.hp = 10;
    seat.player_.homeY = 4.f;
    seat.toggleSit();
    seat.resolve(0.5f, 0.f, 0.f);
    if (!seat.sitting() || seat.player_.hp < 12 || seat.player_.homeY < 5.f) {
        std::fprintf(stderr, "sentar: HP %d SP %.0f\n", seat.player_.hp, seat.player_.homeY);
        return false;
    }
    Play stand;
    stand.player_.hp = 10;
    stand.player_.maxHp = 40;
    stand.player_.homeX = 20.f;
    stand.player_.homeY = 4.f;
    stand.resolve(0.5f, 0.f, 0.f);
    if (stand.player_.hp != 10 || stand.player_.homeY != 4.f) {
        std::fprintf(stderr, "sentar: em pe nao devia curar tao cedo\n");
        return false;
    }
    // Parado com a vida cheia o relogio nao pode acumular: o primeiro dano
    // depois disso nao volta de uma vez.
    Play idle;
    idle.player_.hp = 40;
    idle.player_.maxHp = 40;
    idle.player_.homeX = 20.f;
    idle.player_.homeY = 20.f;
    for (int i = 0; i < 400; ++i) idle.resolve(0.05f, 0.f, 0.f);
    idle.player_.hp = 10;
    idle.resolve(0.05f, 0.f, 0.f);
    if (idle.player_.hp != 10) {
        std::fprintf(stderr, "regeneracao: a vida voltou de uma vez (%d)\n", idle.player_.hp);
        return false;
    }

    Play magic;
    magic.skills_[0] = {99, "Spiritual Bolt", 6, 2.6f};
    magic.skills_[1] = {97, "Heal", 5, 6.f};
    magic.skills_[2] = {84, "Berserk", 5, 12.f};
    magic.skills_[3] = {225, "Harden", 6, 15.f};
    magic.learnedSkills_[99] = 1;
    magic.learnedSkills_[97] = 1;
    magic.learnedSkills_[84] = 1;
    magic.learnedSkills_[225] = 1;
    magic.player_.level = 3;
    magic.player_.hp = 10;
    magic.player_.maxHp = 200;
    magic.player_.homeX = 100.f;
    magic.player_.homeY = 100.f;
    magic.bsta_ = 5;
    magic.skillRules_[97] = healData->second;
    magic.skillRules_[84] = berserkData->second;
    magic.skillRules_[63] = willOfSteelData->second;
    ItemInfo spiritGear;
    spiritGear.id = 60099;
    spiritGear.stats.sta = 2;
    magic.items_[spiritGear.id] = spiritGear;
    magic.worn_[8] = spiritGear.id;
    magic.castSkill(1);
    if (magic.playerPose() != 5 ||
        magic.castingActionBase_ != choosePoseAction(healData->second.playActions, 0, 11)) {
        std::fprintf(stderr, "05-E: cast de cura nao iniciou a pose historica de habilidade\n");
        return false;
    }
    if (magic.player_.hp != 159 || magic.player_.homeY != 71.f || magic.skillCd_[1] <= 0.f) {
        std::fprintf(stderr, "skill: Heal nao aplicou custo/cura\n");
        return false;
    }
    magic.castSkill(2);
    magic.castSkill(3);
    magic.skills_[4] = {63, "Will of Steel", 15, 15.f};
    magic.learnedSkills_[63] = 3;
    magic.castSkill(4);
    if (magic.berserkFor_ != 20.f || std::fabs(magic.berserkAttackSpeedBonus_ - .215f) > .0001f ||
        magic.hardenFor_ != 180.f || magic.hardenDefense_ != 18 ||
        magic.willOfSteelFor_ != 15.f || magic.willOfSteelDefense_ != 9 ||
        magic.player_.homeY != 35.f || playerDefenseWithStates(20, magic.hardenDefense_, 0,
                                                               magic.willOfSteelDefense_) != 47) {
        std::fprintf(stderr, "skill: buffs divergem (berserk %.2f/%.3f, harden %.2f, steel %.2f/+%d, SP %.2f, defesa %d)\n",
                     magic.berserkFor_, magic.berserkAttackSpeedBonus_, magic.hardenFor_,
                     magic.willOfSteelFor_, magic.willOfSteelDefense_, magic.player_.homeY,
                     playerDefenseWithStates(20, magic.hardenDefense_, 0, magic.willOfSteelDefense_));
        return false;
    }
    Play hardenFormula;
    hardenFormula.player_.id=3;
    hardenFormula.player_.level=30;
    hardenFormula.job_=5;
    hardenFormula.player_.homeX=300.f;
    hardenFormula.player_.homeY=300.f;
    hardenFormula.skills_[0]={225,"Harden",3,0.f};
    hardenFormula.learnedSkills_[225]=3;
    hardenFormula.skillRules_[225]=hardenData->second;
    hardenFormula.castSkill(0);
    const int hardenCost=static_cast<int>(std::floor(hardenData->second.spBase+
                                                     hardenData->second.spPerLevel*3));
    if(hardenFormula.hardenFor_!=180.f || hardenFormula.hardenDefense_!=26 ||
       hardenFormula.player_.homeY!=300.f-hardenCost ||
       playerDefenseWithStates(20,hardenFormula.hardenDefense_,0,0)!=46) {
        std::fprintf(stderr,"skill: Harden 225 nao aplicou callback historico (%.1fs, +%d DEF, SP %.0f)\n",
                     hardenFormula.hardenFor_,hardenFormula.hardenDefense_,hardenFormula.player_.homeY);
        return false;
    }
    for(int tick=0;tick<3601;++tick)hardenFormula.resolve(.05f,0.f,0.f);
    if(hardenFormula.hardenFor_!=0.f || hardenFormula.hardenDefense_!=0) {
        std::fprintf(stderr,"skill: DEF do Harden nao expirou apos 180 segundos\n");
        return false;
    }
    Play expiringBuffs;
    expiringBuffs.willOfSteelFor_ = 15.f;
    expiringBuffs.willOfSteelDefense_ = 9;
    expiringBuffs.berserkFor_ = 20.f;
    expiringBuffs.berserkAttackSpeedBonus_ = .215f;
    expiringBuffs.resolve(15.01f, 0.f, 0.f);
    if (expiringBuffs.willOfSteelFor_ != 0.f || expiringBuffs.willOfSteelDefense_ != 0 ||
        expiringBuffs.berserkFor_ <= 0.f || expiringBuffs.berserkAttackSpeedBonus_ <= 0.f) {
        std::fprintf(stderr, "skill: Will of Steel nao expirou aos 15 s ou Berserk expirou cedo\n");
        return false;
    }
    expiringBuffs.resolve(5.01f, 0.f, 0.f);
    if (expiringBuffs.berserkFor_ != 0.f || expiringBuffs.berserkAttackSpeedBonus_ != 0.f) {
        std::fprintf(stderr, "skill: estado de Berserk nao expirou aos 20 s\n");
        return false;
    }
    Foe magicTarget;
    magicTarget.id = 71;
    magicTarget.name = "Alvo";
    magicTarget.level = 3;
    magicTarget.hp = 100;
    magicTarget.maxHp = 100;
    magicTarget.x = 1.f;
    magicTarget.range = 1.6f;
    magicTarget.cdLeft = 10.f;
    magic.foes_.push_back(magicTarget);
    magic.target_ = 0;
    magic.player_.minAtk = 1;
    magic.player_.maxAtk = 1;
    magic.player_.range = 1.6f;
    magic.player_.cd = 1.f;
    magic.castSkill(0);
    magic.pickFoe(0);
    magic.resolve(0.05f, 0.f, 0.f);
    if (magic.foes_[0].hp != 100) {
        std::fprintf(stderr, "skill: dano aconteceu antes da animacao\n");
        return false;
    }
    const float boltImpactWait = magic.pendingAttack_ + .01f;
    magic.resolve(boltImpactWait, 0.f, 0.f);
    if (magic.foes_[0].hp != 73 || magic.armedSkill_ != -1 ||
        std::fabs(magic.player_.cdLeft - (berserkIntervalMultiplier(1) - boltImpactWait)) > .002f) {
        std::fprintf(stderr, "skill: Spiritual Bolt/buff de velocidade incorreto hp %d cd %.2f\n", magic.foes_[0].hp,
                     magic.player_.cdLeft);
        return false;
    }
    Play lightningTest;
    lightningTest.skillRules_[214] = lightningBoltData->second;
    lightningTest.skills_[0] = {214, "Lightning Bolt", 1, 1.f};
    lightningTest.learnedSkills_[214] = 1;
    lightningTest.player_.level = 10;
    lightningTest.player_.homeX = lightningTest.player_.homeY = 100.f;
    lightningTest.player_.minAtk = lightningTest.player_.maxAtk = 10;
    lightningTest.player_.range = 1.6f;
    lightningTest.player_.cd = 1.f;
    Foe numbTarget;
    numbTarget.id = 71;
    numbTarget.level = 10;
    numbTarget.hp = numbTarget.maxHp = 10000;
    numbTarget.x = 1.f;
    numbTarget.range = 1.6f;
    numbTarget.cdLeft = 10.f;
    lightningTest.foes_.push_back(numbTarget);
    lightningTest.target_ = 0;
    lightningTest.castSkill(0);
    lightningTest.pickFoe(0);
    lightningTest.resolve(.05f, 0.f, 0.f);
    lightningTest.resolve(.25f, 0.f, 0.f);
    if (lightningTest.foes_[0].numbFor < 2.5f || lightningTest.foes_[0].numbLevel != 2) {
        std::fprintf(stderr, "Skill_Lj_End: Lightning Bolt nao aplicou STATE_MB level 2/3s\n");
        return false;
    }
    lightningTest.playerNumbFor_ = 3.f;
    lightningTest.playerNumbLevel_ = 2;
    if (std::fabs(lightningTest.moveSpeed() - 3.552f) > .01f) {
        std::fprintf(stderr, "STATE_MB: reducao historica de movimento nao refletida\n");
        return false;
    }
    Play incomingNumb;
    incomingNumb.skillRules_[214] = lightningBoltData->second;
    incomingNumb.player_.level = 30;
    incomingNumb.player_.hp = incomingNumb.player_.maxHp = 5000;
    incomingNumb.player_.speed = 4.8f;
    Foe lightningCaster;
    lightningCaster.id = 678;
    lightningCaster.level = 30;
    lightningCaster.minAtk = lightningCaster.maxAtk = 100;
    lightningCaster.hp = lightningCaster.maxHp = 1000;
    lightningCaster.x = lightningCaster.homeX = .1f;
    lightningCaster.leash = 10.f;
    lightningCaster.ai = 10;
    lightningCaster.provoked = true;
    lightningCaster.cd = 2.f;
    lightningCaster.attackPending = .01f;
    lightningCaster.pendingSkillId = 214;
    lightningCaster.pendingReach = 2.f;
    incomingNumb.foes_.push_back(lightningCaster);
    incomingNumb.resolve(.02f, .4f, 0.f, nullptr);
    if (incomingNumb.playerNumbFor_ < 2.9f || incomingNumb.playerNumbLevel_ != 2 ||
        std::fabs(incomingNumb.moveSpeed() - 3.552f) > .01f) {
        std::fprintf(stderr, "Skill_Lj_End inimiga: dano nao aplicou STATE_MB ao jogador\n");
        return false;
    }
    Play incomingPoison;
    incomingPoison.skillRules_[291] = monsterPoisonDartData->second;
    incomingPoison.player_.hp = incomingPoison.player_.maxHp = 5000;
    Foe poisonCaster;
    poisonCaster.id = 678;
    poisonCaster.level = 30;
    poisonCaster.minAtk = poisonCaster.maxAtk = 100;
    poisonCaster.hp = poisonCaster.maxHp = 1000;
    poisonCaster.x = poisonCaster.homeX = .1f;
    poisonCaster.leash = 10.f;
    poisonCaster.ai = 10;
    poisonCaster.provoked = true;
    poisonCaster.cd = 2.f;
    poisonCaster.cdLeft = 10.f;
    poisonCaster.attackPending = .01f;
    poisonCaster.pendingSkillId = 291;
    poisonCaster.pendingReach = 2.f;
    incomingPoison.foes_.push_back(poisonCaster);
    incomingPoison.resolve(.02f, .4f, 0.f, nullptr);
    if (incomingPoison.player_.hp != 4840 || incomingPoison.playerPoisonFor_ != 10.f ||
        incomingPoison.playerPoisonLevel_ != 1 || incomingPoison.playerPoisonTick_ != 1.f) {
        std::fprintf(stderr, "Skill_wldb_End: STATE_WLDB nao aplicou dano/duracao de 10 s\n");
        return false;
    }
    incomingPoison.resolve(1.01f, .4f, 0.f, nullptr);
    if (incomingPoison.player_.hp != 4680 || incomingPoison.playerPoisonFor_ >= 9.f) {
        std::fprintf(stderr, "STATE_WLDB: pulso periodico incorreto (HP %d, duracao %.3f, tick %.3f)\n",
                     incomingPoison.player_.hp, incomingPoison.playerPoisonFor_, incomingPoison.playerPoisonTick_);
        return false;
    }
    Play bossConch;
    bossConch.worldMap_ = "garner2";
    bossConch.skillRules_[216] = conchRayData->second;
    bossConch.skills_[0] = {216, "Conch Ray", 1, 1.f};
    bossConch.learnedSkills_[216] = 2;
    bossConch.player_.level = 20;
    bossConch.player_.homeX = bossConch.player_.homeY = 100.f;
    bossConch.player_.range = 1.6f;
    bossConch.player_.cd = 1.f;
    Foe bossTarget;
    bossTarget.id = 728;
    bossTarget.level = 20;
    bossTarget.con = 1; // math.random(1,200) >= CON sempre passa.
    bossTarget.sta = 10;
    bossTarget.hp = bossTarget.maxHp = 10000;
    bossTarget.x = 1.f;
    bossTarget.range = 1.6f;
    bossTarget.cdLeft = 10.f;
    bossConch.foes_.push_back(bossTarget);
    bossConch.target_ = 0;
    bossConch.castSkill(0);
    bossConch.pickFoe(0);
    bossConch.resolve(.05f, 0.f, 0.f);
    bossConch.resolve(.4f, 0.f, 0.f);
    for (int frame = 0; frame < 100 && bossConch.pendingAttack_ > 0.f; ++frame)
        bossConch.resolve(.05f, 0.f, 0.f);
    if (bossConch.foes_[0].stunFor <= .5f || bossConch.foes_[0].attackPending != 0.f) {
        std::fprintf(stderr, "Garner2 Conch Ray: ramo especial nao aplicou STATE_XY\n");
        return false;
    }
    Play incomingConch;
    incomingConch.worldMap_ = "garner2";
    incomingConch.skillRules_[216] = conchRayData->second;
    incomingConch.player_.id = 1;
    incomingConch.player_.level = 20;
    incomingConch.player_.hp = incomingConch.player_.maxHp = 5000;
    incomingConch.bcon_ = 1;
    Foe conchCaster;
    conchCaster.id = 740;
    conchCaster.level = 20;
    conchCaster.sta = 20;
    conchCaster.job = 14;
    conchCaster.hp = conchCaster.maxHp = 1000;
    conchCaster.x = conchCaster.homeX = .1f;
    conchCaster.leash = 10.f;
    conchCaster.ai = 10;
    conchCaster.provoked = true;
    conchCaster.cd = 2.f;
    conchCaster.attackPending = .01f;
    conchCaster.pendingSkillId = 216;
    conchCaster.pendingReach = 2.f;
    incomingConch.foes_.push_back(conchCaster);
    incomingConch.resolve(.02f, .4f, 0.f, nullptr);
    if (incomingConch.playerStunFor_ <= .9f || !incomingConch.lockMove()) {
        std::fprintf(stderr, "Garner2 Conch Ray inimiga: nao aplicou STATE_XY ao jogador\n");
        return false;
    }

    Play categories;
    SkillRule passiveRule;
    passiveRule.id = 62;
    passiveRule.name = "Sword Mastery";
    passiveRule.type = 2;
    categories.skillRules_[62] = passiveRule;
    categories.skills_[0] = {62, passiveRule.name, 9, 4.f};
    categories.learnedSkills_[62] = 1;
    categories.player_.homeX = categories.player_.homeY = 20.f;
    categories.castSkill(0);
    if (categories.player_.homeY != 20.f || categories.armedSkill_ >= 0) {
        std::fprintf(stderr, "skill: passiva foi tratada como ataque ou consumiu SP\n");
        return false;
    }
    SkillRule shieldRule;
    shieldRule.id = 103;
    shieldRule.name = "Angelic Shield";
    shieldRule.type = 1;
    shieldRule.helpful = 1;
    categories.skillRules_[103] = shieldRule;
    categories.skills_[1] = {103, shieldRule.name, 5, 6.f};
    categories.learnedSkills_[103] = 2;
    categories.castSkill(1);
    if (categories.genericBuffFor_ <= 0.f || categories.genericBuffDefense_ <= 0 || categories.armedSkill_ >= 0 ||
        categories.player_.homeY != 15.f) {
        std::fprintf(stderr, "skill: buff defensivo historico nao foi classificado\n");
        return false;
    }
    SkillRule areaRule;
    areaRule.id = 500;
    areaRule.name = "Area Test";
    areaRule.type = 1;
    areaRule.distance = 200;
    areaRule.radius = 300;
    categories.skillRules_[500] = areaRule;
    categories.skills_[2] = {500, areaRule.name, 3, 2.f};
    categories.learnedSkills_[500] = 1;
    categories.player_.level = 5;
    categories.player_.minAtk = categories.player_.maxAtk = 5;
    categories.player_.range = 2.f;
    categories.player_.cd = 1.f;
    Foe areaA;
    areaA.id = 71; areaA.hp = areaA.maxHp = 100; areaA.x = 1.f; areaA.range = 1.5f; areaA.cdLeft = 9.f;
    Foe areaB = areaA;
    areaB.x = 2.f;
    categories.foes_ = {areaA, areaB};
    // Previous self-buff action has its own pose timer; wait until that
    // animation completes before testing the independent offensive cast.
    categories.castingFor_ = 0.f;
    categories.player_.cdLeft = 0.f;
    categories.target_ = 0;
    categories.castSkill(2);
    categories.pickFoe(0);
    categories.resolve(.05f, 0.f, 0.f);
    categories.resolve(categories.pendingAttack_ + .01f, 0.f, 0.f);
    if (categories.foes_[0].hp >= 100 || categories.foes_[1].hp >= 100) {
        std::fprintf(stderr, "skill: raio historico nao atingiu alvos proximos\n");
        return false;
    }

    Play disk;
    disk.player_.level = 4;
    disk.player_.hp = 33;
    disk.player_.maxHp = 70;
    disk.exp_ = 9;
    disk.gold_ = 12;
    disk.job_ = 1;
    disk.ap_ = 2;
    disk.tp_ = 0;
    disk.bstr_ = 8;
    disk.bagi_ = 5;
    disk.bdex_ = 5;
    disk.bcon_ = 5;
    disk.bsta_ = 5;
    disk.sheetHp_ = 64;
    disk.sheetSp_ = 30;
    disk.baseMin_ = 7;
    disk.baseMax_ = 8;
    disk.baseDef_ = 3;
    disk.sheetReady_ = true;
    disk.historical_.id = 701;
    disk.historical_.active = true;
    disk.historical_.targetX = 2193.f;
    disk.historical_.targetY = 2706.f;
    disk.historical_.rewardExp = 9;
    disk.historical_.targetId = 127;
    disk.historical_.targetCount = 10;
    disk.historical_.have = 4;
    disk.historical_.neededItems = {{100, 2}, {101, 1}};
    disk.historical_.rewardItems = {{200, 3}};
    disk.save("/tmp/top-attr-save.txt");
    Play back;
    back.player_.level = 1;
    back.player_.maxHp = 40;
    back.player_.minAtk = 4;
    back.player_.maxAtk = 5;
    back.player_.def = 3;
    back.player_.homeX = 18.f;
    back.load("/tmp/top-attr-save.txt");
    std::remove("/tmp/top-attr-save.txt");
    if (back.player_.level != 4 || back.job_ != 1 || back.ap_ != 2 || back.bstr_ != 8 || back.sheetHp_ != 64 ||
        back.baseMin_ != 7 || back.player_.hp != 33 || back.historical_.id != 701 || !back.historical_.active ||
        back.historical_.targetX != 2193.f || back.historical_.rewardExp != 9 || back.historical_.targetId != 127 ||
        back.historical_.targetCount != 10 || back.historical_.have != 4 || back.historical_.neededItems.size() != 2 ||
        back.historical_.neededItems[0].id != 100 || back.historical_.neededItems[0].count != 2 ||
        back.historical_.rewardItems.size() != 1 || back.historical_.rewardItems[0].id != 200 ||
        back.historical_.rewardItems[0].count != 3) {
        std::fprintf(stderr, "save: a ficha nova nao voltou\n");
        return false;
    }
    // Nivel acima de 40 nao pode voltar ao 1.
    Play high;
    high.player_.level = 55;
    high.player_.hp = 90;
    high.player_.maxHp = 90;
    high.sheetHp_ = 90;
    high.sheetSp_ = 40;
    high.baseMin_ = 9;
    high.baseMax_ = 10;
    high.baseDef_ = 3;
    high.sheetReady_ = true;
    high.bstr_ = high.bagi_ = high.bdex_ = high.bcon_ = high.bsta_ = 5;
    high.save("/tmp/top-high-save.txt");
    Play highBack;
    highBack.player_.level = 1;
    highBack.player_.maxHp = 40;
    highBack.load("/tmp/top-high-save.txt");
    std::remove("/tmp/top-high-save.txt");
    if (highBack.player_.level != 55) {
        std::fprintf(stderr, "save: o nivel %d voltou como %d\n", 55, highBack.player_.level);
        return false;
    }
    Play legacy;
    legacy.player_.level = 1;
    legacy.player_.maxHp = 40;
    legacy.player_.hp = 40;
    legacy.player_.minAtk = 4;
    legacy.player_.maxAtk = 5;
    legacy.player_.def = 3;
    legacy.player_.homeX = 18.f;
    legacy.player_.homeY = 18.f;
    {
        FILE* old = std::fopen("/tmp/top-old-save.txt", "wb");
        if (!old) {
            std::fprintf(stderr, "save: nao abriu o teste antigo\n");
            return false;
        }
        std::fputs("3 1 10 20 0 0 0 0 0 0 |\n", old);
        std::fclose(old);
    }
    legacy.load("/tmp/top-old-save.txt");
    std::remove("/tmp/top-old-save.txt");
    if (legacy.player_.level != 3 || legacy.player_.maxHp != 56 || legacy.player_.minAtk != 6 ||
        static_cast<int>(legacy.player_.homeX) != 26 || legacy.ap_ != 6 || legacy.job_ != 0) {
        std::fprintf(stderr, "save: ficha antiga lv %d hp %d atk %d sp %d pontos %d\n", legacy.player_.level,
                     legacy.player_.maxHp, legacy.player_.minAtk, static_cast<int>(legacy.player_.homeX), legacy.ap_);
        return false;
    }
    Play historical;
    historical.player_.level = 1;
    historical.player_.maxHp = historical.player_.hp = 40;
    historical.player_.homeX = historical.player_.homeY = 18.f;
    historical.expNext_ = 25;
    historical.acceptHistoricalMission(701, "Blacksmith's Greetings", "Visite Goldie", 10.f, 20.f, 0, 0, 9, 3);
    historical.step(0.016f, nullptr, 10.f, 20.f);
    if (!historical.historical_.complete || historical.historical_.active || historical.exp_ != 9 || historical.gold_ != 3) {
        std::fprintf(stderr, "missao historica: destino ou premio incorreto\n");
        return false;
    }
    Play hunt;
    hunt.player_.level = 35;
    hunt.player_.maxHp = hunt.player_.hp = 80;
    hunt.player_.homeX = hunt.player_.homeY = 30.f;
    hunt.expNext_ = 10000;
    if (!hunt.acceptHistoricalMission(631, "The Slowpoke Snails", "Mate 2", -1.f, -1.f, 127, 2, 7, 4, 35, 40)) {
        std::fprintf(stderr, "missao historica: caca nao foi aceita\n");
        return false;
    }
    Foe snail;
    snail.id = 127;
    snail.level = 1;
    snail.name = "Slowpoke Snail";
    hunt.onKill(snail);
    if (!hunt.historical_.active || hunt.historical_.have != 1) {
        std::fprintf(stderr, "missao historica: primeiro abate nao contou\n");
        return false;
    }
    hunt.onKill(snail);
    if (!hunt.historical_.complete || hunt.historical_.have != 2 || hunt.exp_ != 27 || hunt.gold_ != 10) {
        std::fprintf(stderr, "missao historica: caca ou premio incorreto\n");
        return false;
    }
    Play delivery;
    delivery.player_.level = 20;
    delivery.player_.maxHp = delivery.player_.hp = 60;
    delivery.player_.homeX = delivery.player_.homeY = 24.f;
    delivery.expNext_ = 10000;
    delivery.addBag(100, 3);
    if (!delivery.acceptHistoricalMission(1001, "Entrega", "Entregue materiais", -1.f, -1.f, 0, 0, 5, 6, 1, 0,
                                          {{100, 2}}, {{200, 1}})) {
        std::fprintf(stderr, "missao historica: entrega de item nao foi aceita\n");
        return false;
    }
    delivery.step(0.016f, nullptr, 0.f, 0.f);
    if (!delivery.historical_.complete || delivery.bagCount(100) != 1 || delivery.bagCount(200) != 1 ||
        delivery.exp_ != 5 || delivery.gold_ != 6) {
        std::fprintf(stderr, "missao historica: consumo ou premio de item incorreto\n");
        return false;
    }

    Play herd;
    herd.player_.radius = 0.4f;
    herd.player_.hp = herd.player_.maxHp = 40;
    Foe bobcat;
    bobcat.ai = 10;
    bobcat.vision = 10.f;
    bobcat.leash = 4.f;
    bobcat.speed = 2.f;
    bobcat.range = 1.5f;
    bobcat.radius = 0.4f;
    bobcat.hp = bobcat.maxHp = 20;
    herd.foes_.push_back(bobcat);
    herd.moveFoes(1.f, nullptr, 6.f, 0.f);
    if (herd.foes_[0].x < 0.5f) {
        std::fprintf(stderr, "ia: agressivo nao perseguiu\n");
        return false;
    }
    for (int i = 0; i < 20; ++i) herd.moveFoes(0.5f, nullptr, 30.f, 0.f);
    if (herd.foes_[0].x > 5.f) {
        std::fprintf(stderr, "ia: agressivo passou da coleira\n");
        return false;
    }

    Play calm;
    calm.player_.radius = 0.4f;
    calm.seed_ = 1;
    Foe sleepy;
    sleepy.ai = 1;
    sleepy.vision = 10.f;
    sleepy.leash = 15.f;
    sleepy.speed = 2.f;
    sleepy.range = 1.f;
    sleepy.radius = 0.4f;
    sleepy.hp = sleepy.maxHp = 20;
    calm.foes_.push_back(sleepy);
    calm.moveFoes(0.2f, nullptr, 4.f, 0.f);
    if (calm.foes_[0].x > 0.05f) {
        std::fprintf(stderr, "ia: passivo perseguiu sem tomar dano\n");
        return false;
    }
    calm.foes_[0].provoked = true;
    calm.moveFoes(0.5f, nullptr, 4.f, 0.f);
    if (calm.foes_[0].x < 0.4f) {
        std::fprintf(stderr, "ia: passivo nao revidou\n");
        return false;
    }

    Play still;
    Foe shrub;
    shrub.ai = 10;
    shrub.speed = 0.f;
    shrub.vision = 10.f;
    shrub.x = shrub.homeX = 3.f;
    shrub.hp = shrub.maxHp = 5;
    still.foes_.push_back(shrub);
    still.moveFoes(1.f, nullptr, 3.2f, 0.f);
    if (still.foes_[0].x != 3.f || still.foes_[0].moving) {
        std::fprintf(stderr, "ia: monstro sem velocidade andou\n");
        return false;
    }

    Play walker;
    walker.seed_ = 3;
    Foe crab;
    crab.ai = 1;
    crab.speed = 2.f;
    crab.vision = 8.f;
    crab.leash = 15.f;
    crab.hp = crab.maxHp = 10;
    walker.foes_.push_back(crab);
    bool leftHome = false;
    for (int i = 0; i < 80; ++i) walker.moveFoes(1.f, nullptr, 80.f, 0.f);
    const float stroll = std::sqrt(walker.foes_[0].x * walker.foes_[0].x + walker.foes_[0].y * walker.foes_[0].y);
    if (stroll > 0.3f) leftHome = true;
    if (!leftHome || stroll > 6.5f) {
        std::fprintf(stderr, "ia: passeio fora dos 6 m do spawn (%.2f)\n", stroll);
        return false;
    }

    Play hunter;
    hunter.player_.radius = 0.4f;
    Foe dummy;
    dummy.x = 10.f;
    dummy.hp = dummy.maxHp = 5;
    dummy.radius = 0.4f;
    hunter.foes_.push_back(dummy);
    hunter.target_ = 0;
    hunter.pendingBasicStrike_ = true; // explicit basic-click command
    float approachX = 0.f;
    float approachY = 0.f;
    hunter.chasePoint(0.f, 0.f, approachX, approachY);
    const float stopDist = std::sqrt((10.f - approachX) * (10.f - approachX) + approachY * approachY);
    if (stopDist >= hunter.attackReach(hunter.foes_[0])) {
        std::fprintf(stderr, "alcance: a parada ficou fora do golpe (%.2f)\n", stopDist);
        return false;
    }
    Play illusionHunter;
    illusionHunter.player_.radius = 0.4f;
    illusionHunter.skillRules_[81] = illusionSlashData->second;
    illusionHunter.skills_[0].id = 81;
    illusionHunter.armedSkill_ = 0;
    Foe distantTarget;
    distantTarget.x = 8.5f;
    distantTarget.hp = distantTarget.maxHp = 10;
    distantTarget.radius = 0.4f;
    illusionHunter.foes_.push_back(distantTarget);
    illusionHunter.target_ = 0;
    float skillApproachX = 0.f;
    float skillApproachY = 0.f;
    illusionHunter.chasePoint(0.f, 0.f, skillApproachX, skillApproachY);
    if (illusionHunter.attackReach(illusionHunter.foes_[0]) < 8.79f ||
        std::fabs(skillApproachX) > 0.001f || std::fabs(skillApproachY) > 0.001f) {
        std::fprintf(stderr, "alcance: Illusion Slash nao respeita os 8 m da tabela historica\n");
        return false;
    }
    // 05-M: o dano nao ocorre se o alvo saiu do alcance durante o windup.
    Play evasion;
    evasion.player_.hp = evasion.player_.maxHp = 60;
    Foe striker;
    striker.ai = 10;
    striker.vision = 20.f;
    striker.x = 1.f;
    striker.hp = striker.maxHp = 25;
    striker.minAtk = striker.maxAtk = 9;
    striker.range = 1.6f;
    evasion.foes_.push_back(striker);
    evasion.resolve(.05f, 0.f, 0.f);
    if (evasion.foes_[0].attackPending <= 0.f) {
        std::fprintf(stderr, "05-M: ataque inimigo nao foi preparado\n");
        return false;
    }
    evasion.resolve(.2f, 10.f, 0.f);
    if (evasion.player_.hp != 60 || evasion.foes_[0].attackPending > 0.f) {
        std::fprintf(stderr, "05-M: ataque acertou depois da esquiva\n");
        return false;
    }
    // Ao soltar aggro, o monstro passivo esquece a provocacao e retorna.
    Play resetAggro;
    Foe passive;
    passive.ai = 1;
    passive.provoked = true;
    passive.x = 7.f;
    passive.homeX = passive.homeY = 0.f;
    passive.leash = 5.f;
    passive.speed = 2.f;
    passive.hp = passive.maxHp = 20;
    resetAggro.foes_.push_back(passive);
    resetAggro.moveFoes(.5f, nullptr, 2.f, 0.f);
    if (resetAggro.foes_[0].provoked || !resetAggro.foes_[0].goingHome || resetAggro.foes_[0].x >= 7.f) {
        std::fprintf(stderr, "05-M: monstro fora do leash nao voltou para casa\n");
        return false;
    }
    // A IA de fuga tambem precisa respeitar o limite de nascimento.
    Play escape;
    Foe coward;
    coward.ai = 2;
    coward.x = 4.5f;
    coward.homeX = coward.homeY = 0.f;
    coward.hp = coward.maxHp = 20;
    coward.speed = 3.f;
    coward.vision = 10.f;
    coward.leash = 5.f;
    escape.foes_.push_back(coward);
    escape.moveFoes(1.f, nullptr, 4.f, 0.f);
    if (escape.foes_[0].x > 5.001f || !escape.foes_[0].goingHome) {
        std::fprintf(stderr, "05-M: monstro em fuga saiu da regiao\n");
        return false;
    }
    escape.moveFoes(.5f, nullptr, 100.f, 0.f);
    if (escape.foes_[0].x >= 5.f) {
        std::fprintf(stderr, "05-M: monstro em fuga nao voltou\n");
        return false;
    }
    // 05-P: AI_ATK_FLEE recua antes de iniciar um golpe corpo-a-corpo.
    Play archer;
    archer.player_.hp = archer.player_.maxHp = 80;
    archer.player_.radius = .4f;
    Foe skirmisher;
    skirmisher.ai = 11;
    skirmisher.x = skirmisher.homeX = 2.f;
    skirmisher.y = skirmisher.homeY = 0.f;
    skirmisher.speed = 3.f;
    skirmisher.leash = 10.f;
    skirmisher.vision = 10.f;
    skirmisher.radius = .4f;
    skirmisher.range = 1.6f;
    skirmisher.minAtk = skirmisher.maxAtk = 8;
    skirmisher.hp = skirmisher.maxHp = 20;
    skirmisher.attackPending = .1f;
    archer.foes_.push_back(skirmisher);
    archer.moveFoes(.5f, nullptr, 0.f, 0.f);
    if (!archer.foes_[0].retreating || archer.foes_[0].x <= 2.f ||
        archer.foes_[0].attackPending > 0.f) {
        std::fprintf(stderr, "05-P: AI 11 nao recuou/cancelou golpe\n");
        return false;
    }
    archer.resolve(.25f, 0.f, 0.f);
    if (archer.player_.hp != 80) {
        std::fprintf(stderr, "05-P: IA em fuga causou dano\n");
        return false;
    }
    Play passiveLost;
    passiveLost.player_.hp = passiveLost.player_.maxHp = 30;
    Foe startled;
    startled.ai = 1;
    startled.provoked = true;
    startled.lostSightFor = 0.f;
    startled.x = startled.homeX = 1.f;
    startled.hp = startled.maxHp = 10;
    startled.vision = 1.f;
    startled.speed = 0.f;
    passiveLost.foes_.push_back(startled);
    passiveLost.moveFoes(2.f, nullptr, 30.f, 0.f);
    if (passiveLost.foes_[0].provoked) {
        std::fprintf(stderr, "05-P: monstro reativo nao esqueceu alvo perdido\n");
        return false;
    }
    Play idleNone;
    Foe statue;
    statue.ai = 0; statue.hp = statue.maxHp = 10; statue.speed = 4.f;
    idleNone.foes_.push_back(statue);
    for (int i = 0; i < 100; ++i) idleNone.moveFoes(1.f, nullptr, 100.f, 0.f);
    if (idleNone.foes_[0].x != 0.f || idleNone.foes_[0].y != 0.f) {
        std::fprintf(stderr, "05-P: AI_NONE se movimentou\n");
        return false;
    }
    // 05-T: a ponte Play -> Registry persiste morte e HP no mesmo mapa,
    // sem misturar instancias de duas criaturas com o mesmo character ID.
    Play world;
    world.setWorldMap("garner");
    world.worldClock_ = 15.;
    Foe firstSpawn;
    firstSpawn.id = 103;
    firstSpawn.spawnKey = 0x1000000000000001ULL;
    firstSpawn.respawnSeconds = 120;
    firstSpawn.homeX = firstSpawn.x = 120.f;
    firstSpawn.homeY = firstSpawn.y = 131.f;
    firstSpawn.hp = 0; firstSpawn.maxHp = 50;
    firstSpawn.deadFor = 1.f;
    Foe secondSpawn = firstSpawn;
    secondSpawn.spawnKey = 0x1000000000000002ULL;
    secondSpawn.hp = 19;
    secondSpawn.deadFor = 0.f;
    world.foes_ = {firstSpawn, secondSpawn};
    world.snapshotWorldMonsters();
    world.worldClock_ = 30.;
    int oldHp = 0;
    float oldX = 0.f, oldY = 0.f, oldYaw = 0.f, oldDeath = 0.f;
    if (!world.worldMonsters_.restore("garner", firstSpawn.spawnKey, world.worldClock_,
                                      50, 120.f, 131.f, 120,
                                      oldHp, oldX, oldY, oldYaw, oldDeath) ||
        oldHp != 0 || oldDeath < 15.f || oldDeath > 17.f) {
        std::fprintf(stderr, "05-T: morte Lua nao sobreviveu ao streaming\n");
        return false;
    }
    if (!world.worldMonsters_.restore("garner", secondSpawn.spawnKey, world.worldClock_,
                                      50, 120.f, 131.f, 120,
                                      oldHp, oldX, oldY, oldYaw, oldDeath) || oldHp != 19) {
        std::fprintf(stderr, "05-T: monstros do mesmo ID compartilharam HP\n");
        return false;
    }
    world.worldClock_ = 134.;
    if (!world.worldMonsters_.restore("garner", firstSpawn.spawnKey, world.worldClock_,
                                      50, 120.f, 131.f, 120,
                                      oldHp, oldX, oldY, oldYaw, oldDeath) || oldHp != 50) {
        std::fprintf(stderr, "05-T: respawn nao respeitou o tempo historico\n");
        return false;
    }
    // 05-V: salva SP corrente sem reordenar as secoes antigas do save.
    Play spSave;
    spSave.player_.level = 12;
    spSave.player_.maxHp = 180;
    spSave.player_.hp = 72;
    spSave.player_.homeX = 90.f;
    spSave.player_.homeY = 14.f;
    spSave.sheetHp_ = 180; spSave.sheetSp_ = 90;
    spSave.baseMin_ = 4; spSave.baseMax_ = 7; spSave.baseDef_ = 3;
    spSave.sheetReady_ = true;
    spSave.save("/tmp/top-05v-sp-save.txt");
    Play spBack;
    spBack.load("/tmp/top-05v-sp-save.txt");
    std::remove("/tmp/top-05v-sp-save.txt");
    if (spBack.player_.level != 12 || spBack.player_.hp != 72 ||
        std::fabs(spBack.restoredSp_ - 14.f) > .01f) {
        std::fprintf(stderr, "05-V: SP corrente nao persistiu no save\n");
        return false;
    }
    if (historical_stats::physicalDamage(500, 100, 27) != 265 ||
        historical_stats::criticalPermille(10, 73, 68) != 250) {
        std::fprintf(stderr, "05-V: PDEF/CRT inconsistentes com Lua\n");
        return false;
    }
    // 05-W: equipar/retirar bonus de iteminfo precisa atualizar derivados
    // sem curar automaticamente o personagem e sem alterar pontos do save.
    Play equipment;
    equipment.player_.level = 1;
    equipment.player_.hp = equipment.player_.maxHp = 25;
    equipment.player_.homeX = 18.f; equipment.player_.homeY = 7.f;
    equipment.player_.minAtk = 4; equipment.player_.maxAtk = 5;
    equipment.player_.speed = equipment.baseMoveSpeed_ = 4.8f;
    equipment.player_.def = equipment.baseDef_ = 3;
    equipment.baseMin_ = 4; equipment.baseMax_ = 5;
    equipment.sheetHp_ = 40; equipment.sheetSp_ = 18;
    equipment.baseLuck_ = 5; equipment.baseResist_ = 0;
    equipment.sheetReady_ = true;
    equipment.applyAttrs(false);
    const int oldHit = equipment.player_.hit;
    const int oldDodge = equipment.player_.dodge;
    const float oldCd = equipment.player_.cd;
    const int oldRegen = equipment.hrecGain_;
    ItemInfo trinket;
    trinket.id = 4418;
    trinket.stats.str = 2; trinket.stats.agi = 3;
    trinket.stats.con = 1; trinket.stats.sta = 2; trinket.stats.luk = 2;
    trinket.stats.dex = 4; trinket.stats.hp = 30; trinket.stats.sp = 15;
    trinket.stats.hit = 9; trinket.stats.dodge = 8;
    trinket.stats.minAttackCoefficient = 100;
    trinket.stats.maxAttackCoefficient = 200;
    trinket.stats.critical = 7; trinket.stats.resistance = 11;
    trinket.stats.hpRegen = 2; trinket.stats.moveSpeed = 30;
    trinket.minAtk = 4; trinket.maxAtk = 5; trinket.def = 6;
    equipment.items_[trinket.id] = trinket;
    equipment.worn_[7] = trinket.id;
    equipment.applyAttrs(false);
    if (equipment.player_.maxHp <= 70 || equipment.player_.homeX <= 33.f ||
        equipment.player_.hp != 25 || equipment.player_.homeY != 7.f ||
        equipment.player_.minAtk <= 8 || equipment.player_.def < 9 ||
        equipment.player_.minAtk != applyAttackCoefficient(equipment.baseMin_ + equipment.attrMin_, 100) + 4 ||
        equipment.player_.maxAtk != applyAttackCoefficient(equipment.baseMax_ + equipment.attrMax_, 200) + 5 ||
        equipment.player_.hit <= oldHit || equipment.player_.dodge <= oldDodge ||
        equipment.player_.crit != 24 || equipment.player_.resist != 11 ||
        equipment.player_.speed < 5.09f || equipment.player_.cd >= oldCd ||
        equipment.hrecGain_ <= oldRegen || equipment.bstr_ != 5) {
        std::fprintf(stderr, "05-W: bonuses divergiram (HP %d SP %.0f ATK %d-%d DEF %d HIT %d/%d CRT %d RES %d speed %.2f)\n",
                     equipment.player_.maxHp, equipment.player_.homeX, equipment.player_.minAtk,
                     equipment.player_.maxAtk, equipment.player_.def, equipment.player_.hit,
                     equipment.player_.dodge, equipment.player_.crit, equipment.player_.resist,
                     equipment.player_.speed);
        return false;
    }
    equipment.worn_.clear();
    equipment.applyAttrs(false);
    if (equipment.player_.maxHp != 100 || equipment.player_.homeX != 23.f ||
        equipment.player_.hp != 25 || equipment.player_.homeY != 7.f ||
        equipment.player_.minAtk != 7 || equipment.player_.maxAtk != 7 ||
        equipment.player_.def != 2 || equipment.player_.crit != 15 ||
        equipment.player_.resist != 0 || equipment.player_.hit != oldHit ||
        equipment.player_.dodge != oldDodge ||
        std::fabs(equipment.player_.cd - oldCd) > .001f) {
        std::fprintf(stderr, "05-W: desequipar nao restaurou atributos basicos\n");
        return false;
    }
    // Desequipar um item com +HP nao deve deixar HP acima do novo maximo.
    equipment.worn_[7] = trinket.id;
    equipment.applyAttrs(false);
    equipment.player_.hp = equipment.player_.maxHp;
    equipment.player_.homeY = equipment.player_.homeX;
    equipment.worn_.clear();
    equipment.applyAttrs(false);
    if (equipment.player_.hp != 100 || equipment.player_.homeY != 23.f) {
        std::fprintf(stderr, "05-W: desequipar nao limitou HP/SP\n");
        return false;
    }
    // 05-X: duas armas com coluna ASPD 37 devem acumular sua velocidade
    // sem modificar o atributo permanente de AGI nem curar o personagem.
    ItemInfo speedSword;
    speedSword.id=1;
    speedSword.stats.aspdCoefficient=50; // Short Sword iteminfo: 50,50
    equipment.items_[speedSword.id]=speedSword;
    equipment.worn_[9]=speedSword.id;
    equipment.applyAttrs(false);
    const float oneSwordCd=equipment.player_.cd;
    if (!(oneSwordCd < oldCd)) {
        std::fprintf(stderr,"05-X: primeira espada nao reduziu intervalo\n"); return false;
    }
    equipment.worn_[10]=speedSword.id;
    equipment.applyAttrs(false);
    const float twoSwordCd=equipment.player_.cd;
    if (!(twoSwordCd < oneSwordCd) || equipment.bagi_!=5) {
        std::fprintf(stderr,"05-X: duas espadas nao acumularam ASPD\n"); return false;
    }
    equipment.worn_.clear();
    equipment.applyAttrs(false);
    if (std::fabs(equipment.player_.cd-oldCd)>.001f) {
        std::fprintf(stderr,"05-X: ASPD nao restaurou ao retirar as espadas\n"); return false;
    }
    // 05-W1: nao vender com clique comum nem recolher automaticamente.
    Play inspect;
    inspect.items_[4319]=shell;
    inspect.bag_.push_back({4319,2});
    inspect.player_.hp=inspect.player_.maxHp=40;
    inspect.player_.homeX=inspect.player_.homeY=20.f;
    inspect.clickBag(0);
    if(inspect.gold_!=0 || inspect.bag_.size()!=1 || inspect.bag_[0].count!=2) {
        std::fprintf(stderr,"05-W1: clique de bolsa vendeu item fora da loja\n"); return false;
    }
    inspect.shopOpen_=true;
    if(!inspect.sellBag(0) || inspect.gold_!=2 || inspect.bag_[0].count!=1) {
        std::fprintf(stderr,"05-W1: venda manual na loja falhou\n"); return false;
    }
    inspect.shopOpen_=false;
    inspect.hereX_=2.f; inspect.hereY_=4.f;
    if(!inspect.discardBag(0) || !inspect.bag_.empty() || inspect.loots_.size()!=1) {
        std::fprintf(stderr,"05-W1: descarte manual falhou\n"); return false;
    }
    inspect.step(.02f,nullptr,2.f,4.f);
    if(!inspect.bag_.empty()) { std::fprintf(stderr,"05-W1: auto loot ainda ativo\n"); return false; }
    float lootX=0.f,lootY=0.f;
    if(!inspect.requestLoot(0,2.f,4.f,lootX,lootY) || inspect.bag_.size()!=1 || !inspect.loots_.empty()) {
        std::fprintf(stderr,"05-W1: recolha por clique falhou\n"); return false;
    }
    inspect.gmAddExperience(100);
    if(inspect.exp_<=0 && inspect.player_.level<=1) {
        std::fprintf(stderr,"05-W1: GM XP nao alterou progressao\n"); return false;
    }
    inspect.player_.hp=1;
    inspect.gmHeal();
    if(inspect.player_.hp!=inspect.player_.maxHp || !inspect.gmGiveItem(4319)) {
        std::fprintf(stderr,"05-W1: GM heal/item falhou\n"); return false;
    }
    // 05-Y: instancias da mesma espada devem ter bonus e UID independentes.
    Play instances;
    instances.player_.id=1;
    instances.player_.name="Lance";
    instances.player_.level=10;
    instances.player_.hp=instances.player_.maxHp=40;
    instances.player_.homeX=instances.player_.homeY=18.f;
    instances.sheetHp_=40; instances.sheetSp_=18;
    instances.baseMin_=4; instances.baseMax_=5;
    instances.baseDef_=3; instances.sheetReady_=true;
    ItemInfo pairSword;
    pairSword.id=777; pairSword.name="Twin blade";
    pairSword.slot=9; pairSword.equipSlots={9,6};
    pairSword.maxStack=1; pairSword.minAtk=2; pairSword.maxAtk=3;
    instances.items_[777]=pairSword;
    if (!instances.addBag(777,2) || instances.bag_.size()!=2 ||
        instances.bag_[0].uid==instances.bag_[1].uid) {
        std::fprintf(stderr,"05-Y: copias nao receberam UIDs distintos\n"); return false;
    }
    const int firstUid=instances.bag_[0].uid;
    const int secondUid=instances.bag_[1].uid;
    instances.bag_[0].extra.str=3;
    instances.bag_[1].extra.str=6;
    instances.bag_[1].forgeLevel=2;
    instances.bag_[1].gemIds={681,0,0};
    instances.clickBag(0);
    instances.clickBag(1);
    if (instances.equippedSlotAtBagIndex(0)!=9 || instances.equippedSlotAtBagIndex(1)!=6 ||
        instances.wornUid_[9]!=firstUid || instances.wornUid_[6]!=secondUid ||
        instances.gearBonuses().str!=9) {
        std::fprintf(stderr,"05-Y: bonus/UID das duas armas nao combinaram\n"); return false;
    }
    const char* instanceSave="/tmp/top-05y-instance-save.txt";
    instances.save(instanceSave);
    Play loadedInstances;
    loadedInstances.items_[777]=pairSword;
    loadedInstances.load(instanceSave);
    std::remove(instanceSave);
    if (loadedInstances.bag_.size()!=2 || loadedInstances.bag_[0].uid!=firstUid ||
        loadedInstances.bag_[1].uid!=secondUid || loadedInstances.wornUid_[9]!=firstUid ||
        loadedInstances.wornUid_[6]!=secondUid || loadedInstances.gearBonuses().str!=9 ||
        loadedInstances.bag_[1].forgeLevel!=2 || loadedInstances.bag_[1].gemIds[0]!=681) {
        std::fprintf(stderr,"05-Y: save misturou instancia/refino de duas armas iguais\n"); return false;
    }
    loadedInstances.clickEquip(6);
    loadedInstances.stashBag(1);
    if (loadedInstances.temp_.size()!=1 || loadedInstances.temp_[0].uid!=secondUid ||
        loadedInstances.temp_[0].extra.str!=6 || loadedInstances.gearBonuses().str!=3) {
        std::fprintf(stderr,"05-Y: bolsa temporaria perdeu dados de instancia\n"); return false;
    }
    loadedInstances.takeTemp(0);
    if (loadedInstances.bag_.size()!=2 || loadedInstances.bag_[1].uid!=secondUid ||
        loadedInstances.bag_[1].extra.str!=6) {
        std::fprintf(stderr,"05-Y: retorno da bolsa temporaria perdeu atributos\n"); return false;
    }
    loadedInstances.hereX_=2.f; loadedInstances.hereY_=4.f;
    if (!loadedInstances.discardBag(1) || loadedInstances.loots_.size()!=1 ||
        loadedInstances.loots_[0].instance.uid!=secondUid ||
        !loadedInstances.collectInstance(loadedInstances.loots_[0].instance) ||
        loadedInstances.bag_[1].uid!=secondUid || loadedInstances.bag_[1].extra.str!=6) {
        std::fprintf(stderr,"05-Y: descarte/recoleta perdeu atributos\n"); return false;
    }
    // Numa pilha stackavel o descarte de uma unidade nao pode duplicar
    // toda a pilha quando o jogador recolher o item do chao.
    Play stackDrop;
    ItemInfo stackItem;
    stackItem.id=4321; stackItem.name="Stack test"; stackItem.maxStack=99;
    stackDrop.items_[4321]=stackItem;
    stackDrop.hereX_=stackDrop.hereY_=1.f;
    if (!stackDrop.addBag(4321,5) || !stackDrop.discardBag(0) ||
        stackDrop.bagCount(4321)!=4 || stackDrop.loots_[0].instance.count!=1 ||
        !stackDrop.collectInstance(stackDrop.loots_[0].instance) || stackDrop.bagCount(4321)!=5) {
        std::fprintf(stderr,"05-Y: descarte de pilhas duplicou unidades\n"); return false;
    }
    // O limite historico de fgets(4096) truncava saves de bolsas expandidas.
    Play large;
    large.player_.id=1; large.player_.level=10;
    large.player_.hp=10; large.player_.maxHp=10;
    large.bagCapacity_=48;
    large.items_[777]=pairSword;
    if (!large.addBag(777,48)) {
        std::fprintf(stderr,"05-Y: nao preencheu bolsa expandida\n"); return false;
    }
    for (Stack& stack : large.bag_) {
        stack.extra.str=100000;
        stack.extra.agi=100000;
        stack.extra.dex=100000;
        stack.extra.con=100000;
        stack.extra.sta=100000;
        stack.extra.hit=100000;
        stack.extra.dodge=100000;
        stack.extra.defense=100000;
        stack.gemIds={681,682,683};
    }
    const char* bigSave="/tmp/top-05y-large-save.txt";
    large.save(bigSave);
    Play largeReload;
    largeReload.items_[777]=pairSword;
    largeReload.load(bigSave);
    std::remove(bigSave);
    if (largeReload.bag_.size()!=48 || largeReload.bagCapacity_!=48 ||
        largeReload.bag_[47].extra.defense!=100000 ||
        largeReload.bag_[0].gemIds[2]!=683 ||
        largeReload.bag_[0].uid==largeReload.bag_[47].uid) {
        std::fprintf(stderr,"05-Y: save extenso foi truncado ou perdeu IDs\n"); return false;
    }
    const char* oldSave="/tmp/top-05y-legacy-save.txt";
    if (FILE* file=std::fopen(oldSave,"wb")) {
        std::fprintf(file,"1 0 0 10 0 0 0 0 0 0 |777,1;777,1;|9,777;6,777;\n");
        std::fclose(file);
    } else return false;
    Play oldInstance;
    oldInstance.items_[777]=pairSword;
    oldInstance.load(oldSave);
    std::remove(oldSave);
    if (oldInstance.bag_.size()!=2 || oldInstance.bag_[0].uid<=0 ||
        oldInstance.bag_[1].uid==oldInstance.bag_[0].uid ||
        oldInstance.wornUid_.size()!=2 || oldInstance.wornUid_[9]==oldInstance.wornUid_[6]) {
        std::fprintf(stderr,"05-Y: conversao de save antigo falhou\n"); return false;
    }
    // 05-Z: forja com custo, consumiveis de IDs distintos, instancia certa,
    // atributos efetivos e persistencia dos encaixes ao recarregar.
    Play forge;
    forge.player_.id=1;
    forge.gold_=400000;
    ItemInfo blade; blade.id=777;blade.name="Twin blade";blade.type=2;
    blade.slot=9;blade.equipSlots={9,6};blade.minAtk=5;blade.maxAtk=6;
    ItemInfo stone; stone.id=879;stone.name="Furious Gem";stone.type=49;
    ItemInfo refiner; refiner.id=885;refiner.name="Refining Gem";refiner.type=50;
    forge.items_[777]=blade;forge.items_[879]=stone;forge.items_[885]=refiner;
    forge.gemCatalog_[879]={879,6,1,6,"ITEMATTR_VAL_MNATK",{2,3,4,7,9}};
    if(!forge.addBag(777,2)||!forge.addBag(879,1)||!forge.addBag(885,1)||
       forge.forgeBag(0,2,3) || forge.bag_.size()!=4 || forge.gold_!=400000){
        std::fprintf(stderr,"05-Z: forja aceitou equipamento sem encaixe\n");return false;
    }
    if(!forge.gmOpenSockets(0,1)||forge.gmOpenSockets(2,1)){
        std::fprintf(stderr,"05-Z: GM abriu encaixe em gema ou recusou espada\n");return false;
    }
    forge.worn_[9]=777;forge.wornUid_[9]=forge.bag_[0].uid;
    forge.worn_[6]=777;forge.wornUid_[6]=forge.bag_[1].uid;
    const auto preForge=forge.previewWorkbench(0,0,2,3);
    if(!preForge.valid || preForge.cost!=100000 || preForge.chance<=0 ||
       forge.previewWorkbench(0,0,3,2).valid || forge.previewWorkbench(0,0,2,2).valid){
        std::fprintf(stderr,"05-AC: preview de forja incoerente\n");return false;
    }
    if(forge.forgeCost(0)!=100000 || !forge.forgeBag(0,2,3) ||
       forge.gold_!=300000 || forge.bag_.size()!=2 ||
       forge.bag_[0].gemIds[0]!=879 || forge.bag_[0].gemLevels[0]!=1 ||
       forge.bag_[1].gemIds[0]!=0 || forge.gearBonuses().minAttack!=16){
        std::fprintf(stderr,"05-Z: forja nao consumiu materiais ou misturou duas espadas\n");return false;
    }
    const char* forgeSave="/tmp/top-05z-forged-save.txt";
    forge.save(forgeSave);
    Play forgeReload;
    forgeReload.items_[777]=blade;forgeReload.items_[879]=stone;forgeReload.items_[885]=refiner;
    forgeReload.gemCatalog_=forge.gemCatalog_;
    forgeReload.load(forgeSave);
    std::remove(forgeSave);
    if(forgeReload.bag_.size()!=2 || forgeReload.bag_[0].socketCount!=1 ||
       forgeReload.bag_[0].gemLevels[0]!=1 || forgeReload.bag_[0].gemIds[0]!=879 ||
       forgeReload.gearBonuses().minAttack!=16 ||
       forgeReload.wornUid_[9]!=forgeReload.bag_[0].uid){
        std::fprintf(stderr,"05-Z: Z1 nao restaurou gema/nivel/UID\n");return false;
    }
    // Combinacao Lv1: chance historica 100%, pergaminho tipo 47 + pedras tipo 49.
    Play merging;
    merging.gold_=10000;
    ItemInfo scroll;scroll.id=900;scroll.type=47;scroll.name="Combination Scroll";
    merging.items_[900]=scroll;
    merging.items_[879]=stone;
    merging.gemCatalog_[879]=forge.gemCatalog_.at(879);
    merging.items_[900].maxStack=99;
    merging.addBag(900,5);merging.addBag(879,2);
    const int leftGemUid=merging.bag_[1].uid;
    if(!merging.combineGems(0,1,2) || merging.gold_!=5000 ||
       merging.bag_.size()!=2 || merging.bag_[0].count!=4 ||
       merging.bag_[1].uid!=leftGemUid || merging.bag_[1].stoneLevel!=2){
        std::fprintf(stderr,"05-Z: combinacao historica nao preservou UID ou consumiu errado\n");return false;
    }
    // 05-AA: perfuracao Lua: dois materiais (ordem livre), custo progressivo,
    // limitacao do equipamento e persistencia por UID; rejeicao sem debito.
    Play milling;
    milling.player_.id=1;
    milling.socketConfig_={3,50000};
    milling.gold_=310000;
    ItemInfo drillBlade=blade;drillBlade.maxDurability=10000;drillBlade.maxSockets=3;
    ItemInfo stab;stab.id=890;stab.name="Equipment Stabilizer";stab.type=51;stab.maxStack=99;
    ItemInfo catal; catal.id=891;catal.name="Equipment Catalyst";catal.type=52;catal.maxStack=99;
    milling.items_[777]=drillBlade;milling.items_[890]=stab;milling.items_[891]=catal;
    if(!milling.addBag(777,2)||!milling.addBag(890,4)||!milling.addBag(891,4) ||
       milling.socketCost(0)!=50000 || milling.socketEquipment(0,2,3)==false ||
       milling.bag_[0].socketCount!=1 || milling.bag_[1].socketCount!=0 ||
       milling.gold_!=260000 || milling.bag_[2].count!=3 || milling.bag_[3].count!=3){
        std::fprintf(stderr,"05-AA: primeiro encaixe falhou ou misturou espadas\n");return false;
    }
    const auto preSocket=milling.previewWorkbench(2,0,2,3);
    if(!preSocket.valid || preSocket.cost!=100000 || preSocket.chance!=10000 ||
       !milling.previewWorkbench(2,0,3,2).valid ||
       milling.previewWorkbench(2,0,2,2).valid){
        std::fprintf(stderr,"05-AC: preview de perfuracao incoerente\n");return false;
    }
    if(!milling.socketEquipment(0,3,2) || milling.socketCost(0)!=150000 ||
       milling.gold_!=160000 || milling.bag_[0].socketCount!=2){
        std::fprintf(stderr,"05-AA: segundo encaixe ou custo progressivo falhou\n");return false;
    }
    const int beforeBad=milling.gold_;
    if(milling.socketEquipment(0,2,2) || milling.gold_!=beforeBad ||
       milling.socketEquipment(0,1,2) || milling.gold_!=beforeBad){
        std::fprintf(stderr,"05-AA: entrada invalida consumiu ouro\n");return false;
    }
    if(!milling.socketEquipment(0,2,3) || milling.bag_[0].socketCount!=3 ||
       milling.socketCost(0)!=-1 || milling.gold_!=10000 ||
       milling.socketEquipment(0,2,3)){
        std::fprintf(stderr,"05-AA: limite de tres encaixes falhou\n");return false;
    }
    const char* socketSave="/tmp/top-05aa-socket-save.txt";
    milling.save(socketSave);
    Play socketReload;
    socketReload.items_[777]=drillBlade;
    socketReload.socketConfig_=milling.socketConfig_;
    socketReload.load(socketSave);
    std::remove(socketSave);
    if(socketReload.bag_.size()<2 || socketReload.bag_[0].socketCount!=3 ||
       socketReload.bag_[1].socketCount!=0 ||
       socketReload.bag_[0].uid==socketReload.bag_[1].uid){
        std::fprintf(stderr,"05-AA: Z1 nao restaurou encaixes por UID\n");return false;
    }
    // 05-AK: selecionar missao 2 quando a primeira estiver bloqueada;
    // detalhes nao aceitam sozinho, recusar volta a lista e abandonar salva estado.
    Play questMenu;
    questMenu.player_.level=15;
    HistoricalMission blocked; blocked.id=8001;blocked.title="Nivel 30";
    blocked.minLevel=30;blocked.targetId=12;blocked.targetCount=1;
    HistoricalMission eligible;eligible.id=8002;eligible.title="Missao disponivel";
    eligible.minLevel=10;eligible.targetId=34;eligible.targetCount=2;
    questMenu.setNpcMissionList({blocked,eligible});
    questMenu.talkNpc("Quest NPC","Veja minhas missoes",false,false);
    const int left=(1280-320)/2,top=720-316;
    if(questMenu.npcMissionChoices_.size()!=1 || questMenu.npcMissionChoices_[0].id!=8002 ||
       questMenu.dialogActionAt(left+30,top+107,1280,720)!=10 ||
       questMenu.dialogActionAt(left+30,top+137,1280,720)==11) {
        std::fprintf(stderr,"05-AO: lista precisa ocultar missao bloqueada\n");return false;
    }
    questMenu.clickDialog(10); // primeira oferta visivel, elegivel
    if(!questMenu.offer_.active || questMenu.offer_.id!=8002 ||
       questMenu.dialogActionAt(left+35,top+267,1280,720)!=1) {
        std::fprintf(stderr,"05-AK: missao seguinte nao apareceu\n");return false;
    }
    questMenu.clickDialog(1); // aceitar
    if(!questMenu.historical_.active || questMenu.historical_.id!=8002 ||questMenu.npcTalkOpen_) {
        std::fprintf(stderr,"05-AK: aceitar nao persistiu missao correta\n");return false;
    }
    if(!questMenu.abandonHistoricalMission() ||questMenu.historical_.active ||
       questMenu.historical_.id!=0 ||questMenu.abandonHistoricalMission()) {
        std::fprintf(stderr,"05-AK: abandonar missao alterou estado incorreto\n");return false;
    }
    Play apparel;
    ItemInfo clothing=drillBlade;clothing.maxDurability=25000;clothing.maxSockets=3;
    apparel.items_[777]=clothing;apparel.socketConfig_={3,50000};apparel.addBag(777,1);
    if(apparel.socketCost(0)!=-1){std::fprintf(stderr,"05-AA: apparel recebeu encaixe\n");return false;}
    // 05-AL: parallel hunts, independent progress, v2 save and targeted abandon.
    Play parallel;
    parallel.player_.level=40;
    parallel.player_.maxHp=parallel.player_.hp=100;
    parallel.player_.homeX=parallel.player_.homeY=20.f;
    parallel.expNext_=100000;
    if(!parallel.acceptHistoricalMission(9101,"First hunt","2 slimes",-1.f,-1.f,127,2,8,10) ||
       !parallel.acceptHistoricalMission(9102,"Second hunt","3 slimes",-1.f,-1.f,127,3,9,11) ||
       !parallel.acceptHistoricalMission(9103,"Visit","Go to place",12.f,14.f,0,0,10,12) ||
       parallel.historicalActiveCount()!=3 ||
       parallel.acceptHistoricalMission(9101,"Repeat","repeat",-1.f,-1.f,127,2,8,10)) {
        std::fprintf(stderr,"05-AL: quests paralelas nao aceitas ou duplicadas\n");return false;
    }
    Foe shared;
    shared.id=127;shared.level=1;shared.name="Test Snail";
    parallel.onKill(shared);
    if(parallel.historical_.have!=1 || parallel.additionalMissions_[0].have!=1) {
        std::fprintf(stderr,"05-AL: abate nao registrado em ambas\n");return false;
    }
    parallel.save("/tmp/top-05al-multimission.txt");
    Play loadedMissions;
    loadedMissions.load("/tmp/top-05al-multimission.txt");
    std::remove("/tmp/top-05al-multimission.txt");
    if(loadedMissions.historical_.id!=9101 || loadedMissions.historical_.title!="First hunt" ||
       loadedMissions.historical_.objective!="2 slimes" || loadedMissions.historical_.have!=1 ||
       loadedMissions.additionalMissions_.size()!=2 ||
       loadedMissions.additionalMissions_[0].have!=1 ||
       loadedMissions.additionalMissions_[0].title!="Second hunt" ||
       loadedMissions.additionalMissions_[1].objective!="Go to place") {
        std::fprintf(stderr,"05-AL: save paralelo nao preservou progresso/textos\n");return false;
    }
    loadedMissions.player_.level=40;
    loadedMissions.player_.maxHp=loadedMissions.player_.hp=100;
    loadedMissions.player_.homeX=loadedMissions.player_.homeY=20.f;
    loadedMissions.expNext_=100000;
    loadedMissions.onKill(shared); // 1st 2/2 finishes; 2nd 2/3 remains
    if(!loadedMissions.historical_.complete || loadedMissions.additionalMissions_[0].have!=2 ||
       !loadedMissions.additionalMissions_[0].active) {
        std::fprintf(stderr,"05-AL: missoes terminaram simultaneamente sem requisito\n");return false;
    }
    loadedMissions.onKill(shared);
    if(!loadedMissions.additionalMissions_[0].complete || loadedMissions.historicalActiveCount()!=1) {
        std::fprintf(stderr,"05-AL: segunda missao nao completou\n");return false;
    }
    if(!loadedMissions.selectJournalAction(3) ||
       !loadedMissions.abandonHistoricalMission() ||
       loadedMissions.historicalActiveCount()!=0 ||
       loadedMissions.findHistoricalMission(9103)) {
        std::fprintf(stderr,"05-AL: selecao/abandono da missao errada\n");return false;
    }
    if(loadedMissions.abandonHistoricalMission(9101) ||
       loadedMissions.acceptHistoricalMission(9102,"Dup","Dup",-1.f,-1.f,127,1,1,1)) {
        std::fprintf(stderr,"05-AL: concluida foi abandonada ou repetida\n");return false;
    }
    // Legacy local quests remain in the diary and must never be abandoned.
    if(!loadedMissions.selectJournalAction(3) || loadedMissions.journalSelectedId_!=-1 ||
       loadedMissions.abandonHistoricalMission()) {
        std::fprintf(stderr,"05-AL: missao local foi confundida com historica\n");return false;
    }
    Play maxQuests;
    maxQuests.player_.level=40;
    for(int i=0;i<8;++i) {
        if(!maxQuests.acceptHistoricalMission(9200+i,"Quest","Explore",10.f+i,20.f,0,0,1,1)) {
            std::fprintf(stderr,"05-AL: limite de 8 bloqueou antes do necessario\n");return false;
        }
    }
    if(maxQuests.historicalActiveCount()!=8 ||
       maxQuests.acceptHistoricalMission(9999,"Ninth","Explore",40.f,50.f,0,0,1,1)) {
        std::fprintf(stderr,"05-AL: nona missao ultrapassou limite\n");return false;
    }
    // 05-AM: requisito entre missoes atraves do ID historico de registro;
    // progresso concluido sobrevive ao save 05-AL e libera a etapa seguinte.
    Play gated;
    gated.player_.level=15;
    gated.job_=0;
    gated.setMissionRecordIndex({{700,{9301}},{701,{9302}}});
    HistoricalMission first;first.id=9301;first.title="Primeira";
    first.targetId=127;first.targetCount=1;
    HistoricalMission follow;follow.id=9302;follow.title="Segunda";
    follow.targetId=127;follow.targetCount=1;follow.minLevel=10;
    follow.gate.needsCompleted={700};follow.gate.needsNotActive={701};
    HistoricalMission jobRequired;jobRequired.id=9303;jobRequired.title="Oficio";
    jobRequired.targetId=127;jobRequired.targetCount=1;
    jobRequired.gate.jobEqual=4;
    HistoricalMission unsupported;unsupported.id=9304;unsupported.title="Especial";
    unsupported.targetId=127;unsupported.targetCount=1;
    unsupported.gate.unsupported="CheckConvertProfession";
    gated.setNpcMissionList({first,follow,jobRequired,unsupported});
    if(gated.npcMissionChoices_.size()!=1 || gated.npcMissionChoices_[0].id!=9301 ||
       !gated.missionAvailable(gated.npcMissionChoices_[0]) ||
       gated.missionLockReason(9302,10,0,follow.gate).find("700")==std::string::npos ||
       gated.acceptHistoricalMission(9302,"Segunda","",-1,-1,127,1,1,1)) {
        std::fprintf(stderr,"05-AM: prerequisito/profissao Lua nao bloqueou quest\n");return false;
    }
    if(!gated.acceptHistoricalMission(9301,"Primeira","",-1,-1,127,1,1,1)) {
        std::fprintf(stderr,"05-AM: quest primaria bloqueada\n");return false;
    }
    Foe gateFoe;gateFoe.id=127;gateFoe.level=1;
    gated.onKill(gateFoe);
    gated.setNpcMissionList({first,follow,jobRequired,unsupported});
    if(!gated.historical_.complete || gated.npcMissionChoices_.size()!=1 ||
       gated.npcMissionChoices_[0].id!=9302 ||
       !gated.missionAvailable(gated.npcMissionChoices_[0]) ||
       !gated.acceptHistoricalMission(9302,"Segunda","",-1,-1,127,1,1,1)) {
        std::fprintf(stderr,"05-AM: quest secundaria nao desbloqueou\n");return false;
    }
    const char* gateSave="/tmp/top-05am-mission-gates.txt";
    gated.save(gateSave);
    Play gatedReload;
    gatedReload.load(gateSave);
    std::remove(gateSave);
    gatedReload.setMissionRecordIndex({{700,{9301}},{701,{9302}}});
    gatedReload.player_.level=15;
    if(!gatedReload.findHistoricalMission(9301) ||
       !gatedReload.findHistoricalMission(9301)->complete ||
       gatedReload.missionLockReason(9302,10,0,follow.gate)!="Missao em andamento" ||
       !gatedReload.abandonHistoricalMission(9302) ||
       !gatedReload.missionLockReason(9302,10,0,follow.gate).empty()) {
        std::fprintf(stderr,"05-AM: registros e dependencia nao sobreviveram ao save\n");return false;
    }
    // 05-AO: Welcome so e entregue a Senna, recompensa original 6 EXP
    // (DEXP[2]=5), desbloqueando as missões com HasRecord(1).
    Play welcome;
    welcome.worldMap_="garner";
    welcome.player_.level=1;
    welcome.expNext_=5;
    welcome.historical_.id=1;
    welcome.historical_.active=true;
    welcome.historical_.title="Welcome";
    welcome.historical_.rewardExp=6;
    welcome.historical_.targetX=2223.f;
    welcome.historical_.targetY=2785.f;
    if(!welcome.hasWelcomeMission() || welcome.deliverWelcomeToSenna("Goldie") ||
       !welcome.deliverWelcomeToSenna("Newbie Guide - Senna") ||
       welcome.hasWelcomeMission() || welcome.player_.level!=2 ||
       welcome.exp_!=1 || welcome.deliverWelcomeToSenna("Newbie Guide - Senna")) {
        std::fprintf(stderr,"05-AO: Welcome/Senna/EXP incorreto\n");return false;
    }
    Play discovery;
    discovery.worldMap_="garner";
    discovery.player_.level=2;
    QuestNpcMarker marker;
    marker.npcName="Senna"; marker.x=2223.f;marker.y=2785.f;
    marker.missions.push_back({1,1,0,true,{},"Welcome"});
    marker.missions.push_back({2,1,0,true,{},"Next"});
    MissionGate gatedMission;gatedMission.needsCompleted={9999};
    marker.missions.push_back({3,1,0,true,gatedMission,"Locked"});
    marker.missions.push_back({4,20,0,true,{},"High level"});
    discovery.questNpcMarkers_.push_back(marker);
    discovery.historical_.id=1;discovery.historical_.complete=true;
    auto foundQuests=discovery.availableMapQuests();
    if(foundQuests.size()!=1 || foundQuests.front().id!=2 ||
       foundQuests.front().npcName!="Senna") {
        std::fprintf(stderr,"05-AO: mapa expos misses sem prerequisitos\n");return false;
    }
    HistoricalMission locked;
    locked.id=3;locked.title="Locked";locked.targetId=127;locked.targetCount=1;
    locked.gate=gatedMission;
    HistoricalMission open;
    open.id=2;open.title="Next";open.targetId=127;open.targetCount=1;
    discovery.setNpcMissionList({locked,open});
    if(discovery.npcMissionChoices_.size()!=1||discovery.npcMissionChoices_[0].id!=2) {
        std::fprintf(stderr,"05-AO: NPC mostrou missao bloqueada\n");return false;
    }
    // 05-AP: two independent NPC result missions. Neither kill nor inventory
    // collection is allowed to pay before the correct NPC receives the quest.
    Play turns;
    turns.player_.level=25;
    turns.expNext_=1000000;
    HistoricalMission firstTurn;
    firstTurn.id=8121;firstTurn.title="Materials";
    firstTurn.neededItems={{5001,2}};
    firstTurn.rewardItems={{5002,1}};
    firstTurn.rewardExp=40;firstTurn.rewardGold=100;
    firstTurn.turnInRequired=true;
    HistoricalMission secondTurn;
    secondTurn.id=8122;secondTurn.title="Hunt";
    secondTurn.targetId=127;secondTurn.targetCount=2;
    secondTurn.rewardExp=70;secondTurn.rewardGold=20;
    secondTurn.turnInRequired=true;
    turns.setNpcMissionList({firstTurn});
    turns.talkNpc("Physician - Ditto","Collect leaves");
    if(!turns.acceptHistoricalMission(8121,"Materials","Collect",-1,-1,0,0,40,100,1,0,
                                      {{5001,2}},{{5002,1}})) {
        std::fprintf(stderr,"05-AP: cannot accept item hand-in\n");return false;
    }
    turns.closeDialog();
    turns.setNpcMissionList({secondTurn});
    turns.talkNpc("Newbie Guide - Senna","Hunt");
    if(!turns.acceptHistoricalMission(8122,"Hunt","Defeat",-1,-1,127,2,70,20)) {
        std::fprintf(stderr,"05-AP: cannot accept concurrent hunt\n");return false;
    }
    turns.closeDialog();
    if(!turns.addBag(5001,2) || !turns.historicalObjectivesReady(turns.historical_) ||
       turns.historical_.complete || turns.deliverHistoricalToNpc("Wrong Physician") ||
       !turns.deliverHistoricalToNpc("Physician - Ditto") ||
       !turns.historical_.complete || turns.historical_.active ||
       turns.bagCount(5001)!=0 || turns.bagCount(5002)!=1 || turns.gold_!=100) {
        std::fprintf(stderr,"05-AP: item NPC hand-in/reward mismatch\n");return false;
    }
    Foe turnFoe; turnFoe.id=127;turnFoe.level=1;
    turns.onKill(turnFoe);
    if(turns.deliverHistoricalToNpc("Newbie Guide - Senna") ||
       !turns.additionalMissions_[0].active || turns.additionalMissions_[0].have!=1) {
        std::fprintf(stderr,"05-AP: incomplete hunt delivered\n");return false;
    }
    turns.onKill(turnFoe);
    if(!turns.historicalObjectivesReady(turns.additionalMissions_[0]) ||
       !turns.additionalMissions_[0].active || turns.deliverHistoricalToNpc("Physician - Ditto")) {
        std::fprintf(stderr,"05-AP: hunt auto-completed or wrong NPC accepted\n");return false;
    }
    const char* handInSave="/tmp/top-05ap-hand-in.txt";
    turns.save(handInSave);
    Play turnsLoaded;
    turnsLoaded.load(handInSave);
    std::remove(handInSave);
    if(turnsLoaded.additionalMissions_.size()!=1 ||
       !turnsLoaded.additionalMissions_[0].turnInRequired ||
       turnsLoaded.additionalMissions_[0].turnInNpc!="Newbie Guide - Senna" ||
       turnsLoaded.additionalMissions_[0].have!=2 ||
       turnsLoaded.historical_.active || !turnsLoaded.historical_.complete ||
       turnsLoaded.bagCount(5002)!=1 || turnsLoaded.gold_!=turns.gold_ ||
       turnsLoaded.deliverHistoricalToNpc("Wrong Senna") ||
       !turnsLoaded.deliverHistoricalToNpc("Newbie Guide - Senna") ||
       turnsLoaded.gold_!=turns.gold_+20 || turnsLoaded.additionalMissions_[0].active ||
       !turnsLoaded.additionalMissions_[0].complete ||
       turnsLoaded.deliverHistoricalToNpc("Newbie Guide - Senna")) {
        std::fprintf(stderr,"05-AP: persisted NPC binding/completion/idempotence\n");return false;
    }
    // A completed quest remains non-claimable after another save/reload.
    turnsLoaded.save(handInSave);
    Play completedReload;
    completedReload.load(handInSave);
    std::remove(handInSave);
    if(completedReload.deliverHistoricalToNpc("Newbie Guide - Senna") ||
       completedReload.gold_!=turns.gold_+20 ||
       completedReload.additionalMissions_.empty() ||
       !completedReload.additionalMissions_[0].complete) {
        std::fprintf(stderr,"05-AP: duplicate reward after final reload\n");return false;
    }
    // 05-AO saves lacking Q3 still load and preserve legacy auto flow.
    Play legacyTurn;
    legacyTurn.player_.level=20;
    legacyTurn.expNext_=1000000;
    legacyTurn.acceptHistoricalMission(8190,"Legacy","Hunt",-1,-1,127,1,10,1);
    const char* oldFile="/tmp/top-05ap-legacy.txt";
    legacyTurn.save(oldFile);
    // Simulate the exact older layout by stripping the new optional field.
    if(FILE* previous=std::fopen(oldFile,"rb")) {
        char text[65536];
        if(std::fgets(text,sizeof text,previous)) {
            std::string oldRow(text);
            if(const auto pos=oldRow.find("|Q3:");pos!=std::string::npos) {
                oldRow.resize(pos); oldRow.push_back('\n');
                std::fclose(previous);previous=nullptr;
                if(FILE* writer=std::fopen(oldFile,"wb")) {
                    std::fwrite(oldRow.data(),1,oldRow.size(),writer);
                    std::fclose(writer);
                }
            }
        }
        if(previous)std::fclose(previous);
    }
    Play oldLoaded;oldLoaded.load(oldFile);
    std::remove(oldFile);
    oldLoaded.onKill(turnFoe);
    if(!oldLoaded.historical_.complete || oldLoaded.historical_.turnInRequired) {
        std::fprintf(stderr,"05-AP: 05-AO save lost legacy behavior\n");return false;
    }
    // 05-AQ: per-mission flag state must survive Q4 and be required for
    // delivering; flags from another mission must not unlock this one.
    Play flagged;
    flagged.player_.level=20;
    flagged.expNext_=1000000;
    HistoricalMission flagRule;
    flagRule.id=8301;
    flagRule.turnInRequired=true;
    flagRule.beginFlags={1};
    flagRule.resultHasFlags={1};
    flagRule.resultNoFlags={10};
    flagged.setNpcMissionList({flagRule});
    flagged.talkNpc("NPC Flags","Teste de entrega");
    if(!flagged.acceptHistoricalMission(8301,"Flags","Entregar",-1,-1,0,0,11,4,1,0,{{5001,1}})) {
        std::fprintf(stderr,"05-AQ: could not accept flagged mission\n");return false;
    }
    flagged.closeDialog();
    if(flagged.historical_.flags!=std::vector<int>{1} ||
       flagged.historical_.requiredFlags!=std::vector<int>{1} ||
       flagged.historical_.forbiddenFlags!=std::vector<int>{10} ||
       !flagged.addBag(5001,1) || !flagged.historicalObjectivesReady(flagged.historical_)) {
        std::fprintf(stderr,"05-AQ: initial flag state incorrect\n");return false;
    }
    const char* flagSave="/tmp/top-05aq-flags-save.txt";
    flagged.save(flagSave);
    Play flagLoaded;flagLoaded.load(flagSave);
    std::remove(flagSave);
    if(flagLoaded.historical_.flags!=std::vector<int>{1} ||
       flagLoaded.historical_.requiredFlags!=std::vector<int>{1} ||
       flagLoaded.historical_.forbiddenFlags!=std::vector<int>{10} ||
       !flagLoaded.historicalObjectivesReady(flagLoaded.historical_)) {
        std::fprintf(stderr,"05-AQ: flag Q4 state not persisted\n");return false;
    }
    // A wrong state MUST never consume ingredients or grant rewards.
    flagLoaded.historical_.flags.push_back(10);
    if(flagLoaded.deliverHistoricalToNpc("NPC Flags") ||
       flagLoaded.bagCount(5001)!=1 || flagLoaded.historical_.complete) {
        std::fprintf(stderr,"05-AQ: forbidden flag allowed turn-in\n");return false;
    }
    flagLoaded.historical_.flags.erase(std::remove(flagLoaded.historical_.flags.begin(),
        flagLoaded.historical_.flags.end(),10),flagLoaded.historical_.flags.end());
    if(!flagLoaded.deliverHistoricalToNpc("NPC Flags") ||
       flagLoaded.deliverHistoricalToNpc("NPC Flags") ||
       flagLoaded.bagCount(5001)!=0 || flagLoaded.gold_!=4 ||
       !flagLoaded.historical_.complete) {
        std::fprintf(stderr,"05-AQ: flag turn-in was not idempotent\n");return false;
    }
    // 05-AR: the intermediate NPC consumes the letter, sets only its own
    // mission's flag and never completes or rewards the main mission.
    Play exchanged;
    exchanged.player_.level=2;exchanged.expNext_=100000;
    HistoricalMission senna;
    senna.id=701;senna.recordId=701;senna.turnInRequired=true;
    senna.beginFlags={1};senna.resultHasFlags={10};
    senna.beginItems.push_back({3950,1});
    exchanged.setNpcMissionList({senna});
    exchanged.talkNpc("Newbie Guide - Senna","Entrega de carta");
    if(!exchanged.acceptHistoricalMission(701,"Blacksmith's Greetings","Goldie",
        2193.f,2706.f,0,0,9,0,1,0,{}) || exchanged.bagCount(3950)!=1 ||
        exchanged.deliverHistoricalToNpc("Newbie Guide - Senna")) {
        std::fprintf(stderr,"05-AR: accept failed or rewarded before Goldie\n");return false;
    }
    exchanged.closeDialog();
    HistoricalMissionStep side;side.displayId=703;side.recordId=701;
    side.absentFlag=10;side.resultingFlag=10;side.takeItems.push_back({3950,1});
    HistoricalMissionStep wrong=side;wrong.recordId=702;
    if(exchanged.applyNpcMissionStep(wrong) ||
       exchanged.historical_.flags!=std::vector<int>{1} ||
       exchanged.bagCount(3950)!=1) {
       std::fprintf(stderr,"05-AR: wrong record affected mission\n");return false;
    }
    if(!exchanged.applyNpcMissionStep(side) || exchanged.applyNpcMissionStep(side) ||
       exchanged.bagCount(3950)!=0 || exchanged.historical_.complete ||
       !exchanged.historicalObjectivesReady(exchanged.historical_)) {
        std::fprintf(stderr,"05-AR: duplicate or incomplete Goldie handoff\n");return false;
    }
    const char* arSave="/tmp/top-05ar-crossnpc-save.txt";
    exchanged.save(arSave);
    Play afterGoldie;afterGoldie.load(arSave);std::remove(arSave);
    if(afterGoldie.historical_.flags!=std::vector<int>({1,10}) ||
       afterGoldie.deliverHistoricalToNpc("Blacksmith - Goldie") ||
       !afterGoldie.deliverHistoricalToNpc("Newbie Guide - Senna") ||
       !afterGoldie.historical_.complete || afterGoldie.deliverHistoricalToNpc("Newbie Guide - Senna")) {
        std::fprintf(stderr,"05-AR: save, return to Senna, or duplicate reward\n");return false;
    }
    // 05-AS: quests that were level-capped in Lua remain available for
    // offline catch-up. Missing minimum/prerequisite still blocks.
    Play catchUp;
    catchUp.player_.level=10;
    catchUp.expNext_=1000000;
    HistoricalMission oldQuest;
    oldQuest.id=8401;oldQuest.title="Quest antiga";oldQuest.objective="Entregue o item";
    oldQuest.minLevel=5;oldQuest.maxLevel=6;
    oldQuest.neededItems.push_back({5001,1});oldQuest.turnInRequired=true;
    oldQuest.rewardExp=12;oldQuest.rewardGold=8;
    catchUp.setNpcMissionList({oldQuest});
    catchUp.talkNpc("NPC Antigo","Uma missao antiga");
    if(catchUp.npcMissionChoices_.size()!=1 ||
       !catchUp.missionAvailable(catchUp.npcMissionChoices_.front())) {
        std::fprintf(stderr,"05-AS: older quest must stay available at Lv 10\n");return false;
    }
    catchUp.clickDialog(10);
    if(!catchUp.offer_.active || catchUp.dialogActionAt(0,0,1280,720)!=-1) {
        std::fprintf(stderr,"05-AS: quest accept details missing\n");return false;
    }
    catchUp.clickDialog(1);
    if(!catchUp.historical_.active || catchUp.historical_.complete ||
       catchUp.deliverHistoricalToNpc("Wrong",8401)) {
        std::fprintf(stderr,"05-AS: quest started or completed incorrectly\n");return false;
    }
    catchUp.setNpcMissionList({oldQuest});
    catchUp.talkNpc("NPC Antigo","Volte ao terminar");
    if(catchUp.npcMissionChoices_.size()!=1 ||
       catchUp.missionReadyAtNpc(8401,"NPC Antigo") ||
       catchUp.dialogActionAt((1280-320)/2+40,720-316+275,1280,720)==5) {
        std::fprintf(stderr,"05-AS: incomplete mission shows Complete\n");return false;
    }
    catchUp.closeDialog();
    if(!catchUp.addBag(5001,1))return false;
    catchUp.setNpcMissionList({oldQuest});
    catchUp.talkNpc("NPC Antigo","Entrega");
    if(catchUp.npcMissionChoices_.size()!=1 ||
       !catchUp.missionReadyAtNpc(8401,"NPC Antigo")){
        std::fprintf(stderr,"05-AS: finished objective did not show hand-in\n");return false;
    }
    catchUp.clickDialog(10);
    if(catchUp.dialogActionAt((1280-320)/2+40,720-316+275,1280,720)!=5){
        std::fprintf(stderr,"05-AS: Complete button not clickable\n");return false;
    }
    const auto catchupGoldBefore=catchUp.gold_;
    catchUp.clickDialog(5);
    if(!catchUp.historical_.complete || catchUp.historical_.active ||
       catchUp.gold_!=catchupGoldBefore+8 || catchUp.bagCount(5001)!=0 ||
       catchUp.npcDialogOpen()) {
        std::fprintf(stderr,"05-AS: explicit Complete did not award exactly once\n");return false;
    }
    catchUp.setNpcMissionList({oldQuest});
    catchUp.talkNpc("NPC Antigo","Novo dialogo");
    if(!catchUp.npcMissionChoices_.empty() || catchUp.missionReadyAtNpc(8401,"NPC Antigo") ||
       catchUp.deliverHistoricalToNpc("NPC Antigo",8401) || catchUp.gold_!=catchupGoldBefore+8) {
        std::fprintf(stderr,"05-AS: finished quest appeared again or double rewarded\n");return false;
    }
    Play showWelcome;
    showWelcome.worldMap_="garner";
    showWelcome.historical_.id=1;showWelcome.historical_.active=true;
    showWelcome.historical_.title="Welcome";showWelcome.historical_.rewardExp=6;
    showWelcome.setNpcMissionList({});
    showWelcome.talkNpc("Newbie Guide - Senna","Converse comigo");
    if(showWelcome.npcMissionChoices_.size()!=1 ||
       !showWelcome.missionReadyAtNpc(1,"Newbie Guide - Senna")) {
        std::fprintf(stderr,"05-AS: Welcome has no manual completion entry\n");return false;
    }
    showWelcome.clickDialog(10);
    showWelcome.clickDialog(5);
    if(!showWelcome.historical_.complete || showWelcome.historical_.active ||
       showWelcome.deliverWelcomeToSenna("Newbie Guide - Senna")) {
        std::fprintf(stderr,"05-AS: Welcome completion regressions\n");return false;
    }
    // One Complete click must target exactly one of several ready quests.
    Play multiple;
    multiple.player_.level=12;multiple.expNext_=1000000;
    HistoricalMission firstReady,secondReady;
    firstReady.id=8501;firstReady.title="Entrega 1";firstReady.turnInRequired=true;
    firstReady.objective="Primeiro item";firstReady.neededItems.push_back({5001,1});
    firstReady.rewardGold=13;
    secondReady.id=8502;secondReady.title="Entrega 2";secondReady.turnInRequired=true;
    secondReady.objective="Segundo item";secondReady.neededItems.push_back({5002,1});
    secondReady.rewardGold=23;
    multiple.setNpcMissionList({firstReady,secondReady});
    multiple.talkNpc("NPC das entregas","Missao 1");
    multiple.clickDialog(10);multiple.clickDialog(1);
    multiple.setNpcMissionList({firstReady,secondReady});
    multiple.talkNpc("NPC das entregas","Missao 2");
    multiple.clickDialog(10);multiple.clickDialog(1);
    if(!multiple.addBag(5001,1) || !multiple.addBag(5002,1))return false;
    multiple.setNpcMissionList({firstReady,secondReady});
    multiple.talkNpc("NPC das entregas","Prontas");
    if(multiple.npcMissionChoices_.size()!=2 ||
       !multiple.missionReadyAtNpc(8501,"NPC das entregas") ||
       !multiple.missionReadyAtNpc(8502,"NPC das entregas")) {
        std::fprintf(stderr,"05-AS: multiple ready quests missing from NPC dialogue\n");return false;
    }
    multiple.clickDialog(11); // Only the second mission.
    multiple.clickDialog(5);
    if(!multiple.findHistoricalMission(8501)->active ||
       !multiple.findHistoricalMission(8502)->complete ||
       multiple.gold_!=23 || multiple.bagCount(5001)!=1 || multiple.bagCount(5002)!=0){
        std::fprintf(stderr,"05-AS: one click completed multiple quests\n");return false;
    }
    const char* asSave="/tmp/top-05as-multiple-save.txt";
    multiple.save(asSave);
    Play afterSingle;afterSingle.load(asSave);std::remove(asSave);
    if(!afterSingle.findHistoricalMission(8501) || !afterSingle.findHistoricalMission(8502) ||
       !afterSingle.findHistoricalMission(8501)->active ||
       !afterSingle.findHistoricalMission(8502)->complete ||
       afterSingle.deliverHistoricalToNpc("NPC das entregas",8502) ||
       !afterSingle.deliverHistoricalToNpc("NPC das entregas",8501) ||
       afterSingle.gold_!=36) {
        std::fprintf(stderr,"05-AS: save or individual mission completion mismatch\n");return false;
    }
    // 05-AU: real archived mission 721 has two independent TE_GETITEM
    // streams (1573 x10, 1574 x3). Q5 must restore partial counts without
    // awarding items or XP before the NPC Complete action.
    MissionCatalog eventCatalog;
    HistoricalMission leaves;
    if(!eventCatalog.mission(721,leaves) || !leaves.turnInRequired ||
       leaves.eventTriggers.size()!=2 || !leaves.gate.unsupported.empty()) {
        std::fprintf(stderr,"05-AU: archival Leaves Collection trigger mismatch\n");return false;
    }
    Play events;
    events.player_.level=10;events.expNext_=1000000;
    events.setNpcMissionList({leaves});
    events.talkNpc("Physician - Ditto","Recolha as folhas");
    std::vector<MissionItem> need;
    for(const auto& item:leaves.neededItems)need.push_back({item.id,item.count});
    if(!events.acceptHistoricalMission(721,leaves.title,leaves.objective,-1,-1,0,0,
                                       leaves.rewardExp,leaves.rewardGold,1,0,need)) {
        std::fprintf(stderr,"05-AU: cannot accept two-item event mission\n");return false;
    }
    events.closeDialog();
    events.onHistoricalEvent(MissionEventKind::GetItem,1573,6);
    events.onHistoricalEvent(MissionEventKind::GetItem,1574,1);
    if(events.historical_.triggerCounts!=std::vector<int>({6,1}) ||
       events.historicalObjectivesReady(events.historical_) || events.historical_.complete) {
        std::fprintf(stderr,"05-AU: partial counters or premature hand-in\n");return false;
    }
    // 05-AV: o diario deve mostrar TODOS os contadores independentes da
    // coleta, sem confundir progresso historico com quantidade na bolsa.
    const auto eventJournal=events.missionJournalView();
    const auto hasLine=[](const std::vector<std::string>& rows,const std::string& text){
        return std::any_of(rows.begin(),rows.end(),[&](const std::string& row){
            return row.find(text)!=std::string::npos;
        });
    };
    if(!hasLine(eventJournal.lines,"Coletar Item 1573 6/10") ||
       !hasLine(eventJournal.lines,"Coletar Item 1574 1/3") ||
       !eventJournal.canAbandon) {
        std::fprintf(stderr,"05-AV: counters missing from modular journal\n");return false;
    }
    const char* eventSave="/tmp/top-05au-partial-events.txt";
    events.save(eventSave);
    Play reloadedEvents;reloadedEvents.load(eventSave);std::remove(eventSave);
    if(reloadedEvents.historical_.triggerCounts!=std::vector<int>({6,1}) ||
       reloadedEvents.historical_.triggers.size()!=2 ||
       reloadedEvents.historicalObjectivesReady(reloadedEvents.historical_) ||
       reloadedEvents.deliverHistoricalToNpc("Physician - Ditto")) {
        std::fprintf(stderr,"05-AU: Q5 partial save and NPC protection\n");return false;
    }
    const auto restoredJournal=reloadedEvents.missionJournalView();
    if(!hasLine(restoredJournal.lines,"Coletar Item 1573 6/10") ||
       !hasLine(restoredJournal.lines,"Coletar Item 1574 1/3")) {
        std::fprintf(stderr,"05-AV: journal counters not restored from Q5\n");return false;
    }
    reloadedEvents.onHistoricalEvent(MissionEventKind::GetItem,1573,4);
    reloadedEvents.onHistoricalEvent(MissionEventKind::GetItem,1574,2);
    if(!reloadedEvents.addBag(1573,10) || !reloadedEvents.addBag(1574,3) ||
       !reloadedEvents.historicalObjectivesReady(reloadedEvents.historical_) ||
       reloadedEvents.deliverHistoricalToNpc("Wrong NPC") ||
       !reloadedEvents.deliverHistoricalToNpc("Physician - Ditto") ||
       reloadedEvents.deliverHistoricalToNpc("Physician - Ditto") ||
       !reloadedEvents.historical_.complete) {
        std::fprintf(stderr,"05-AU: missing items, wrong NPC or duplicate reward\n");return false;
    }
    // Synthetic multi-kill progression uses the rules extracted from the
    // authentic 736 (whose historical profession gate remains unsupported).
    HistoricalMission multiSource;
    if(!eventCatalog.mission(736,multiSource) || multiSource.eventTriggers.size()!=3)return false;
    Play multiEvents;
    multiEvents.player_.level=15;multiEvents.expNext_=1000000;
    HistoricalMission multiSafe;
    multiSafe.id=8601;multiSafe.title="Multi Kill";multiSafe.turnInRequired=true;
    multiSafe.resultHasFlags={14,29};multiSafe.beginFlags={1};
    multiSafe.eventTriggers=multiSource.eventTriggers;
    multiSafe.neededItems.push_back({1844,5});
    multiEvents.setNpcMissionList({multiSafe});
    multiEvents.talkNpc("Quest NPC","Objetivos independentes");
    if(!multiEvents.acceptHistoricalMission(8601,"Multi Kill","Dois monstros e item",
                                            -1,-1,0,0,25,0,1,0,{{1844,5}}))return false;
    Foe marsh;marsh.id=104;marsh.level=1;
    Foe questSnail;questSnail.id=39;questSnail.level=1;
    for(int i=0;i<5;++i)multiEvents.onKill(marsh);
    for(int i=0;i<9;++i)multiEvents.onKill(questSnail);
    multiEvents.onHistoricalEvent(MissionEventKind::GetItem,1844,5);
    if(!multiEvents.addBag(1844,5) || multiEvents.historicalObjectivesReady(multiEvents.historical_) ||
       multiEvents.historical_.complete)return false;
    const auto mixedJournal=multiEvents.missionJournalView();
    if(!hasLine(mixedJournal.lines,"Derrotar Monstro 104 5/5") ||
       !hasLine(mixedJournal.lines,"Derrotar Monstro 39 9/10") ||
       !hasLine(mixedJournal.lines,"Coletar Item 1844 5/5")) {
        std::fprintf(stderr,"05-AV: missing multi-kill/mixed counters\n");return false;
    }
    multiEvents.onKill(questSnail);
    if(!multiEvents.historicalObjectivesReady(multiEvents.historical_) ||
       multiEvents.historical_.complete || multiEvents.historical_.flags.size()<3)return false;
    // 05-AW: callback nao suportado nao pode consumir o item silenciosamente.
    Play itemSafety;
    ItemInfo gift; gift.id=60001; gift.name="Lua Chest"; gift.type=41;
    gift.maxStack=1; gift.scriptAction="ItemUse_SaintCloth";
    itemSafety.items_[gift.id]=gift;
    if(!itemSafety.addBag(gift.id,1))return false;
    itemSafety.clickBag(0);
    if(itemSafety.bagCount(gift.id)!=1 || itemSafety.log_.empty() ||
       itemSafety.log_.back().find("ItemUse_SaintCloth")==std::string::npos) {
        std::fprintf(stderr,"05-AW: unsupported callback consumed item\n");return false;
    }
    // ItemInfo 9,6 permite equipar tambem pela segunda mao no drag.
    ItemInfo ambi; ambi.id=60002; ambi.name="Two-slot Sword"; ambi.type=1;
    ambi.slot=9; ambi.equipSlots={9,6}; ambi.maxStack=1;
    itemSafety.items_[ambi.id]=ambi;
    if(!itemSafety.addBag(ambi.id,1))return false;
    itemSafety.dragInventory(201,106);
    if(!itemSafety.worn_.count(6) || itemSafety.worn_.at(6)!=ambi.id ||
       itemSafety.wornUid_.at(6)!=itemSafety.bag_[1].uid) {
        std::fprintf(stderr,"05-AW: inventory drag rejected legal secondary slot\n");return false;
    }
    // 05-AX: abertura real de baus com GiveItem fixo, verificando o limite
    // ANTES do uso, atomicidade, UID, condicionamento por personagem e
    // ausencia de ganho repetido no mesmo clique.
    Play weaponChest;
    weaponChest.player_.id = 1;
    ItemInfo box; box.id=215; box.name="Relic Weapon Box";
    box.scriptAction="ItemUse_SCBoxRYBZ"; box.maxStack=1;
    ItemInfo reward;reward.id=109;reward.name="Relic Weapon";reward.maxStack=1;
    weaponChest.items_[box.id]=box;
    weaponChest.items_[reward.id]=reward;
    weaponChest.bagCapacity_=24;
    if(!weaponChest.addBag(215,1)) return false;
    // 23 outros itens ocupam todas as posicoes. Nada pode ser consumido.
    for (int i=0;i<23;++i) {
        ItemInfo filler;filler.id=60010+i;filler.name="Filler";filler.maxStack=1;
        weaponChest.items_[filler.id]=filler;
        if(!weaponChest.addBag(filler.id,1))return false;
    }
    weaponChest.clickBag(0);
    if(weaponChest.bagCount(215)!=1 || weaponChest.bagCount(109)!=0)return false;
    weaponChest.bag_.pop_back();
    weaponChest.clickBag(0);
    if(weaponChest.bagCount(215)!=0 || weaponChest.bagCount(109)!=1)return false;
    const int rewardUid=weaponChest.bag_.back().uid;
    if(rewardUid<=0)return false;
    for(const auto& st:weaponChest.bag_)if(st.id!=109&&st.uid==rewardUid)return false;
    // Race 4 recebe um quarto item; race 1 nao recebe o item 828.
    Play specialChest;
    specialChest.player_.id=4;
    ItemInfo kylin; kylin.id=192;kylin.name="Chest of Kylin";
    kylin.scriptAction="ItemUse_SaintCloth";kylin.maxStack=1;
    specialChest.items_[kylin.id]=kylin;
    for(int id: {825,826,827,828}) {
        ItemInfo giftItem;giftItem.id=id;giftItem.name="Kylin reward";giftItem.maxStack=1;
        specialChest.items_[id]=giftItem;
    }
    if(!specialChest.addBag(192,1))return false;
    specialChest.clickBag(0);
    if(specialChest.bagCount(192) || specialChest.bagCount(825)!=1 ||
       specialChest.bagCount(826)!=1 || specialChest.bagCount(827)!=1 ||
       specialChest.bagCount(828)!=1)return false;
    // 05-CA: caminho local completo do evento Star of Unity da Senna.
    char starTestDirTemplate[] = "/tmp/top-star-unity-XXXXXX";
    char* starTestDirPath = ::mkdtemp(starTestDirTemplate);
    if (!starTestDirPath) return false;
    Play starEvent;
    starEvent.root_ = starTestDirPath;
    starEvent.player_.id = 1;
    starEvent.player_.level = 10;
    starEvent.job_ = 8; // primeiro job elegivel no callback Transfer_TeamStar
    ItemInfo unityChest; unityChest.id=1033; unityChest.name="Star of Unity Chest";
    unityChest.scriptAction="ItemUse_TunDuiZhiXing"; unityChest.maxStack=1;
    ItemInfo unityStar; unityStar.id=1034; unityStar.name="Star of Unity"; unityStar.maxStack=1;
    ItemInfo classWeapon; classWeapon.id=1382; classWeapon.name="Class weapon"; classWeapon.maxStack=1;
    starEvent.items_[1033]=unityChest;
    starEvent.items_[1034]=unityStar;
    starEvent.items_[1382]=classWeapon;
    bool starEventOk = starEvent.grantSennaUnityChest()==StarActionResult::Success &&
                       starEvent.bagCount(1033)==1 && starEvent.bag_.front().quality==4;
    if (starEventOk) {
        starEvent.clickBag(0);
        starEventOk = starEvent.bagCount(1033)==0 && starEvent.bagCount(1034)==1 &&
                      starEvent.bag_.front().quality==4 && starEvent.bag_.front().durability==100 &&
                      starEvent.bag_.front().maxDurability==20100;
    }
    starEvent.player_.level=41;
    if (starEventOk) {
        starEventOk = starEvent.redeemTeamStar()==StarActionResult::Success &&
                      starEvent.bagCount(1034)==0 && starEvent.bagCount(1382)==1 &&
                      starEvent.bag_.front().quality==22;
    }
    const std::string starSavePath=starEvent.savePath();
    std::remove(starSavePath.c_str());
    const std::string starUserDir=std::string(starTestDirPath)+"/user";
    ::rmdir(starUserDir.c_str());
    ::rmdir(starTestDirPath);
    if (!starEventOk) {
        std::fprintf(stderr,"05-CA: fluxo offline Senna/Star of Unity incorreto\n");
        return false;
    }
    // Um campo ausente do catalogo bloqueia a operacao inteira.
    Play missingItem;
    missingItem.player_.id=1;
    missingItem.items_[box.id]=box;
    if(!missingItem.addBag(215,1))return false;
    missingItem.clickBag(0);
    if(missingItem.bagCount(215)!=1 || missingItem.bagCount(109)!=0)return false;
    // 05-AY: abrir Relic Armor Box utiliza os mesmos snapshots da 05-AX
    // mas respeita as diferentes restricoes de classe e espacos.
    Play armorTest;
    armorTest.player_.id=1;
    ItemInfo armorBox; armorBox.id=209;armorBox.name="Relic Armor Box";
    armorBox.scriptAction="ItemUse_SCBoxYXTZ";armorBox.maxStack=1;
    armorTest.items_[209]=armorBox;
    for(int id: {395,587,747}) {
        ItemInfo awarded;awarded.id=id;awarded.name="Relic armor";awarded.maxStack=1;
        armorTest.items_[id]=awarded;
    }
    armorTest.bagCapacity_=24;
    if(!armorTest.addBag(209,1))return false;
    armorTest.clickBag(0);
    if(armorTest.bagCount(209)!=0 || armorTest.bagCount(395)!=1 ||
       armorTest.bagCount(587)!=1 || armorTest.bagCount(747)!=1 ||
       armorTest.bag_.size()!=3) {
        std::fprintf(stderr,"05-AY: Relic Armor Box failed to grant historical equipment\n");return false;
    }
    Play armorWrongRace;
    armorWrongRace.player_.id=2;
    armorWrongRace.items_=armorTest.items_;
    if(!armorWrongRace.addBag(209,1))return false;
    armorWrongRace.clickBag(0);
    if(armorWrongRace.bagCount(209)!=1 || armorWrongRace.bagCount(395)!=0) {
        std::fprintf(stderr,"05-AY: Relic Armor Box opened with wrong character type\n");return false;
    }
    // Tickets permanecem na bolsa ate que um mapa REAL seja carregado
    // pelo frontend. Mesma instância UID deve ser removida somente depois.
    Play ticketTest;
    ticketTest.player_.id=1;
    ticketTest.player_.level=10;
    ticketTest.player_.hp=ticketTest.player_.maxHp=100;
    ticketTest.player_.homeX=ticketTest.player_.homeY=50.f;
    ticketTest.worldMap_="garner";
    ItemInfo ticketDef;ticketDef.id=4602;ticketDef.name="Argent ticket";
    ticketDef.scriptAction="Ticket";ticketDef.maxStack=99;
    ticketTest.items_[4602]=ticketDef;
    if(!ticketTest.addBag(4602,2))return false;
    const int ticketUid=ticketTest.bag_.front().uid;
    ticketTest.setTicketBoatMode(true);
    ticketTest.clickBag(0);
    if(ticketTest.takeTicketTravelRequest() || ticketTest.bagCount(4602)!=2)return false;
    ticketTest.setTicketBoatMode(false);
    ticketTest.player_.hp=49;
    ticketTest.clickBag(0);
    if(ticketTest.takeTicketTravelRequest() || ticketTest.bagCount(4602)!=2)return false;
    ticketTest.player_.hp=50;
    ticketTest.clickBag(0);
    auto request=ticketTest.takeTicketTravelRequest();
    if(!request || request->uid!=ticketUid || request->itemId!=4602 ||
       request->map!="garner" || request->x!=2232 || request->y!=2787 ||
       ticketTest.bagCount(4602)!=2) {
        std::fprintf(stderr,"05-AY: ticket prematurely consumed or wrong historical map\n");return false;
    }
    const char* ticketSave="/tmp/top-05ay-ticket-save.txt";
    ticketTest.reportTicketTravelFailed();
    if(ticketTest.bagCount(4602)!=2 || ticketTest.takeTicketTravelRequest())return false;
    ticketTest.save(ticketSave);
    Play ticketReload;
    ticketReload.items_[4602]=ticketDef;
    ticketReload.load(ticketSave);
    if(ticketReload.bagCount(4602)!=2 || ticketReload.bag_.front().uid!=ticketUid ||
       !ticketReload.consumeTicketAfterTravel(request->uid,request->itemId) ||
       ticketReload.bagCount(4602)!=1 ||
       ticketReload.consumeTicketAfterTravel(0,4602) ||
       ticketReload.consumeTicketAfterTravel(ticketUid,3051)) {
        std::fprintf(stderr,"05-AY: ticket save/UID/consumption failed\n");return false;
    }
    ticketReload.save(ticketSave);
    Play ticketFinal;
    ticketFinal.items_[4602]=ticketDef;
    ticketFinal.load(ticketSave);
    std::remove(ticketSave);
    if(ticketFinal.bagCount(4602)!=1 || ticketFinal.bag_.front().uid!=ticketUid)return false;
    // 05-AZ: abertura real do Newbie Chest 436 com inventario transacional,
    // ouro, UID e save/reload. IDs e recompensa de NewbieChest() historico.
    Play newbie;
    newbie.player_.id=1;
    newbie.player_.level=5;
    newbie.gold_=70;
    newbie.bagCapacity_=12;
    ItemInfo starterBox;starterBox.id=436;starterBox.name="Newbie Chest";
    starterBox.scriptAction="NewbieChest";starterBox.maxStack=1;
    newbie.items_[436]=starterBox;
    for (int rewardId:{437,9,4308,444,6050}) {
        ItemInfo entry;entry.id=rewardId;entry.name="starter reward";entry.maxStack=1;
        newbie.items_[rewardId]=entry;
    }
    if(!newbie.addBag(436,1))return false;
    newbie.player_.level=4;
    newbie.clickBag(0);
    if(newbie.bagCount(436)!=1 || newbie.gold_!=70)return false;
    newbie.player_.level=5;
    newbie.setTicketBoatMode(true);
    newbie.clickBag(0);
    if(newbie.bagCount(436)!=1 || newbie.gold_!=70)return false;
    newbie.setTicketBoatMode(false);
    newbie.items_.erase(6050); // recompensa ausente => rollback integral.
    newbie.clickBag(0);
    if(newbie.bagCount(436)!=1 || newbie.bagCount(437)!=0 || newbie.gold_!=70)return false;
    ItemInfo lastReward;lastReward.id=6050;lastReward.name="starter reward";lastReward.maxStack=1;
    newbie.items_[6050]=lastReward;
    newbie.bagCapacity_=4; // tres vagas antes da abertura: minimo historico 4 nao satisfeito.
    newbie.clickBag(0);
    if(newbie.bagCount(436)!=1 || newbie.bagCount(437)!=0 || newbie.gold_!=70)return false;
    newbie.bagCapacity_=12;
    newbie.clickBag(0);
    if(newbie.bagCount(436)!=0 || newbie.bag_.size()!=5 || newbie.gold_!=2570 ||
       newbie.bagCount(437)!=1 || newbie.bagCount(4308)!=1 ||
       newbie.bagCount(444)!=1 || newbie.bagCount(6050)!=1)return false;
    std::unordered_set<int> rewardUids;
    for(const auto& reward:newbie.bag_) if(reward.uid<=0 || !rewardUids.insert(reward.uid).second)return false;
    const char* newbieFile="/tmp/top-05az-newbie-save.txt";
    newbie.save(newbieFile);
    Play newbieReload;
    newbieReload.items_=newbie.items_;
    newbieReload.load(newbieFile);
    std::remove(newbieFile);
    if(newbieReload.gold_!=2570 || newbieReload.bagCount(436)!=0 ||
       newbieReload.bagCount(437)!=1 || newbieReload.bag_.size()!=5)return false;
    for(const auto& reward:newbieReload.bag_)
        if(reward.uid<=0 || !rewardUids.count(reward.uid))return false;
    // 05-BB: a última caixa usa GiveItem(event=95) e MakeItem(event=11).
    Play finalStarter;
    finalStarter.player_.id=1;finalStarter.player_.level=60;finalStarter.job_=8;
    ItemInfo lastChest;lastChest.id=443;lastChest.name="Advancement Chest";
    lastChest.scriptAction="NewbieChest";lastChest.maxStack=1;
    finalStarter.items_[443]=lastChest;
    for (const auto& definition:std::array<std::tuple<int,int,int>,6>{{
             {300,22,40},{15,2,40},{301,22,45},{20,2,45},{450,41,0},{855,41,0}}}) {
        const auto [id,type,level]=definition;
        ItemInfo reward;reward.id=id;reward.name="historical instance reward";
        reward.type=type;reward.level=level;reward.maxStack=id==855?99:1;
        if(id==15 || id==20 || id==300 || id==301) {
            reward.chInstance=true;reward.maxDurability=10000;reward.maxDurabilityRange=20000;
            reward.maxEnergy=1000;reward.maxEnergyRange=1100;
            reward.statsMaximum.str=12;reward.statsMaximum.agi=12;reward.statsMaximum.dex=12;
            reward.statsMaximum.con=12;reward.statsMaximum.sta=12;
            if(type<=10) {
                reward.stats.minAttack=100;reward.statsMaximum.minAttack=200;
                reward.stats.maxAttack=150;reward.statsMaximum.maxAttack=250;
            } else {
                reward.stats.defense=100;reward.statsMaximum.defense=200;
                reward.stats.resistance=20;reward.statsMaximum.resistance=120;
            }
        }
        finalStarter.items_[id]=reward;
    }
    if(!finalStarter.addBag(443,1))return false;
    finalStarter.clickBag(0);
    if(finalStarter.bagCount(443)!=0 || finalStarter.gold_!=200000 ||
       finalStarter.bagCount(300)!=1 || finalStarter.bagCount(15)!=1 ||
       finalStarter.bagCount(301)!=1 || finalStarter.bagCount(20)!=1 ||
       finalStarter.bagCount(450)!=1 || finalStarter.bagCount(855)!=49)return false;
    const auto weaponReward=std::find_if(finalStarter.bag_.begin(),finalStarter.bag_.end(),
        [](const Stack& stack){return stack.id==20;});
    const auto armorReward=std::find_if(finalStarter.bag_.begin(),finalStarter.bag_.end(),
        [](const Stack& stack){return stack.id==300;});
    if(weaponReward==finalStarter.bag_.end() || armorReward==finalStarter.bag_.end() ||
       weaponReward->quality!=3 || weaponReward->maxEnergy<1300 || weaponReward->maxEnergy>1305 ||
       weaponReward->maxDurability!=10300 ||
       weaponReward->extra.minAttack!=30 || weaponReward->extra.maxAttack!=30 ||
       armorReward->quality!=3 || armorReward->maxEnergy<1300 || armorReward->maxEnergy>1320 ||
       armorReward->maxDurability!=10300 ||
       armorReward->extra.defense!=30 ||
       armorReward->extra.resistance!=30)return false;
    const char* generatedChestFile="/tmp/top-05bb-generated-chest.txt";
    finalStarter.save(generatedChestFile);
    Play generatedChestReload;
    generatedChestReload.items_=finalStarter.items_;
    generatedChestReload.load(generatedChestFile);
    std::remove(generatedChestFile);
    const auto persistedWeapon=std::find_if(generatedChestReload.bag_.begin(),generatedChestReload.bag_.end(),
        [](const Stack& stack){return stack.id==20;});
    if(persistedWeapon==generatedChestReload.bag_.end() || persistedWeapon->uid!=weaponReward->uid ||
       persistedWeapon->quality!=3 || persistedWeapon->maxEnergy!=weaponReward->maxEnergy ||
       persistedWeapon->extra.minAttack!=weaponReward->extra.minAttack ||
       persistedWeapon->extra.str!=weaponReward->extra.str)return false;
    return true;

    }
};

bool Play::selfTestExplicitCombat() {
    // This fixture needs no external Client assets. The original selection
    // interaction must be testable even in a headless build.
    Play play;
    play.player_.hp = play.player_.maxHp = 1000;
    play.player_.hit = 1000;
    play.player_.homeY = play.player_.homeX = 200.f;
    play.player_.cd = .6f;
    play.player_.radius = .4f;
    play.player_.range = 1.6f;
    SkillRule slash;
    slash.id = 81;
    slash.name = "Illusion Slash";
    slash.distance = 800;
    slash.spBase = 10;
    slash.cooldownBaseMs = 5000;
    slash.playActions = {14};
    slash.actionDurations = {.8f};
    slash.actionImpacts = {.3f};
    slash.animationDuration = .8f;
    slash.flightEffectId = 1018;
    slash.flightSpeed = 16.f;
    play.skillRules_[81] = slash;
    play.skills_[0] = {81,"Illusion Slash",10,5.f};
    play.learnedSkills_[81] = 1;
    Foe target;
    target.id = 71;
    target.x = 20.f;
    target.hp = target.maxHp = 100000;
    target.radius = .4f;
    target.cdLeft = 1000.f;
    play.foes_.push_back(target);
    const float sp = play.player_.homeY;
    play.target_ = 0; // previous selection must NOT cause an immediate cast
    play.castSkill(0); // opens targeting mode even with an old selected foe
    if (play.requestedSkill_ != 0 || play.armedSkill_ != -1 || play.skillCd_[0] != 0.f ||
        play.player_.homeY != sp || play.pendingBasicStrike_ || play.target_ != -1) return false;
    play.resolve(.05f,0.f,0.f); // no target => no damage, no melee
    if (play.pendingTarget_ >= 0) return false;
    play.pickFoe(0); // out of range: approach instead of rejecting
    float walkX = -10.f, walkY = -10.f;
    if (!play.chasePoint(0.f,0.f,walkX,walkY) ||
        walkX <= 10.f || walkX >= 20.f || walkY != 0.f || play.requestedSkill_ != 0 ||
        play.player_.homeY != sp || play.skillCd_[0] != 0.f) return false;
    play.resolve(.01f,0.f,0.f); // still too far, no cast
    if (play.player_.homeY != sp || play.skillCd_[0] != 0.f || play.pendingTarget_ >= 0) return false;
    play.resolve(.01f,12.f,0.f); // actor moved into range: one cast
    if (play.player_.homeY >= sp || play.skillCd_[0] <= 0.f ||
        play.armedSkill_ != -1 || play.requestedSkill_ != -1 || play.pendingBasicStrike_) return false;
    if (play.pendingSkill_ != 0 || play.pendingTarget_ != 0 || !play.swingIsSkill_) return false;
    // Clear the in-flight action, then verify no attack repeats after cooldown.
    play.resolve(5.5f,0.f,0.f);
    play.resolve(1.f,0.f,0.f);
    if (play.pendingTarget_ != -1 || play.armedSkill_ != -1 ||
        play.pendingBasicStrike_ || play.requestedSkill_ != -1 || play.swing_ > 0.f) return false;
    if (play.chasePoint(0.f,0.f,walkX,walkY)) return false;
    // Clicking a foe without a skill should issue exactly one basic swing.
    Play basic;
    basic.player_.hp = basic.player_.maxHp = 1000;
    basic.player_.hit = 1000;
    basic.player_.cd = .6f;
    basic.player_.range = 2.f;
    target.x = 1.f;
    basic.foes_.push_back(target);
    basic.target_ = 0;  // selected only
    basic.resolve(.05f,0.f,0.f);
    if (basic.pendingTarget_ >= 0 || basic.swing_ > 0.f) return false;
    basic.pickFoe(0);
    basic.resolve(.05f,0.f,0.f);
    if (basic.pendingTarget_ != 0 || basic.pendingBasicStrike_) return false;
    basic.resolve(3.f,0.f,0.f);
    basic.resolve(.05f,0.f,0.f);
    if (basic.pendingTarget_ >= 0 || basic.swing_ > 0.f) return false;
    // Explicit re-click should allow another strike.
    basic.pickFoe(0);
    basic.resolve(.05f,0.f,0.f);
    if (basic.pendingTarget_ != 0) return false;
    basic.clearTarget();
    if (basic.armedSkill_ >= 0 || basic.pendingBasicStrike_) return false;
    return true;
}

bool Play::selfTestHistoricalSkillExecution() {
    // A second offensive skill must use precisely the same input, approach
    // and single-shot execution as Illusion Slash, without special ID checks.
    Play play;
    play.player_.hp = play.player_.maxHp = 1000;
    play.player_.hit = 1000;
    play.player_.homeY = play.player_.homeX = 200.f;
    play.player_.radius = .4f;
    play.player_.cd = .6f;
    SkillRule skill;
    skill.id = 3; // another historical offensive skill (not 81)
    skill.name = "Illusion Slash (catalog 3)";
    skill.type = 1;
    skill.helpful = 0;
    skill.target = 4;
    skill.distance = 800;
    skill.spBase = 13;
    skill.cooldownBaseMs = 2500;
    skill.playActions = {14};
    skill.actionDurations = {.8f};
    skill.actionImpacts = {.3f};
    skill.animationDuration = .8f;
    play.skillRules_[3] = skill;
    play.skills_[0] = {3, skill.name, 13, 2.5f};
    play.learnedSkills_[3] = 1;
    Foe foe;
    foe.id = 71;
    foe.x = 20.f;
    foe.hp = foe.maxHp = 100000;
    foe.radius = .4f;
    foe.cdLeft = 1000.f;
    play.foes_.push_back(foe);
    const float sp = play.player_.homeY;
    play.target_ = 0;
    play.castSkill(0); // selecting skill must discard the previous target
    if (play.target_ != -1 || play.requestedSkill_ != 0 ||
        play.player_.homeY != sp || play.skillCd_[0] != 0.f) return false;
    play.pickFoe(0);
    float toX = -1.f, toY = -1.f;
    if (!play.chasePoint(0.f, 0.f, toX, toY) || toX < 10.f || toX >= 20.f ||
        play.player_.homeY != sp) return false;
    play.resolve(.01f, 0.f, 0.f); // too far; no resource use
    if (play.pendingTarget_ != -1 || play.player_.homeY != sp) return false;
    play.resolve(.01f, 12.f, 0.f); // entering the authored radius
    if (play.pendingSkill_ != 0 || play.pendingTarget_ != 0 ||
        play.player_.homeY >= sp || play.skillCd_[0] <= 0.f ||
        play.requestedSkill_ != -1 || play.armedSkill_ != -1) return false;
    play.resolve(5.f, 12.f, 0.f);
    play.resolve(5.f, 12.f, 0.f);
    if (play.pendingTarget_ != -1 || play.swing_ > 0.f ||
        play.requestedSkill_ != -1) return false; // never auto-attack

    Play canceled;
    canceled.skillRules_[3] = skill;
    canceled.skills_[0] = {3, skill.name, 13, 2.5f};
    canceled.learnedSkills_[3] = 1;
    canceled.player_.homeY = 100.f;
    canceled.foes_.push_back(foe);
    canceled.castSkill(0);
    canceled.pickFoe(0);
    canceled.clearTarget();
    canceled.resolve(.1f, 12.f, 0.f);
    if (canceled.player_.homeY != 100.f || canceled.requestedSkill_ != -1 ||
        canceled.pendingTarget_ >= 0) return false;
    return true;
}

bool Play::selfTest() { return PlaySelfTest::run(); }

} // namespace top

namespace top {
bool Play::selfTestPetStats() {
    Play p;p.setReadOnlyPreview(true);p.root_="Client";
    p.loadItems("Client/scripts/table/iteminfo.txt",{});
    p.loadSkillRules("Client/scripts/table/skillinfo.txt");
    auto check=[](bool ok,const char* text){if(!ok)std::fprintf(stderr,"PET/STATS: %s\n",text);return ok;};
    if(!check(p.configureStatPreview("base"),"baseline uses current tables"))return false;
    const float speed=p.moveSpeed(),interval=p.effectiveAttackInterval();
    const int defense=p.effectiveDefense();
    if(!check(p.configureStatPreview("necklace") && std::fabs(p.moveSpeed()-speed-.2f)<1e-5f,
              "Pendant of Sunstone adds authored 20 cm/s"))return false;
    if(!check(p.configureStatPreview("berserk") &&
              std::fabs(interval/p.effectiveAttackInterval()-1.215f)<1e-4f,
              "State_Kb_Add level 1: +0.2+0.015 ASPD for combat and status"))return false;
    for(int frame=0;frame<1201;++frame)p.step(1.f/60.f,nullptr,0,0);
    if(!check(std::fabs(p.effectiveAttackInterval()-interval)<1e-5f,
              "Berserk expires and displayed/combat ASPD returns to baseline"))return false;
    if(!check(p.configureStatPreview("harden") && p.effectiveDefense()==defense+18,
              "State_Shpf level 1 defense applied to the shared value"))return false;
    if(!check(p.configureStatPreview("fairy") && p.equippedFairyItemId()==183 &&
              p.effectivePrimaryAttributes()==std::array<int,5>{{10,8,7,6,9}},
              "equipped fairy's persisted STR/CON/AGI/DEX/SPR enter derived stats"))return false;
    p.bag_.back().durability=49;p.applyAttrs(false);
    if(!check(p.equippedFairyItemId()==0 && p.effectivePrimaryAttributes()==std::array<int,5>{{5,5,5,5,5}},"RefreshLink hides fairy with stamina below 50"))return false;
    if(!check(p.configureStatPreview("base"),"passive baseline"))return false;
    const int hp=p.player_.maxHp,sp=static_cast<int>(p.player_.homeX),hit=p.hit_,dodge=p.player_.dodge;
    const int hrec=p.hrecGain_,srec=p.srecGain_;
    if(!check(p.configureStatPreview("passives") && p.player_.maxHp==hp+35 && p.player_.homeX==sp+40 &&
        p.hit_==hit+1 && p.player_.dodge==dodge+3 && p.hrecGain_==hrec+1 && p.srecGain_==srec+2 &&
        std::fabs(p.moveSpeed()-4.896f)<1e-5f,"seven authored Use callbacks enter final stats"))return false;
    p.learnedSkills_[75]=2;p.step(0.f,nullptr,0,0);
    if(!check(std::fabs(p.moveSpeed()-4.992f)<1e-5f,"learning/upgrading refreshes passives without casting"))return false;
    p.worldMap_="garner2";p.learnedSkills_[63]=3;p.applyAttrs(false);
    if(!check(p.player_.resist==p.baseResist_+3,"Strengthen's historical garner2 branch"))return false;
    // Verify read-only diagnostics cannot clobber an existing file.
    const auto path=std::string("/tmp/top-pet-stats-readonly-")+std::to_string(::getpid());
    {std::ofstream out(path);out<<"unchanged";}
    if(!p.save(path))return false;
    std::ifstream input(path);std::string text;input>>text;::unlink(path.c_str());
    if(!check(text=="unchanged","screenshot diagnostics never save mutations"))return false;
    std::puts("PET/STATS: equipment movement, Berserk/expiry, Harden, fairy attributes/stamina and read-only diagnostics OK");
    return true;
}
}
