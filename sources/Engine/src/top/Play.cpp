#include "top/Play.h"
#include "top/HistoricalSkillExecution.h"
#include "top/SaveRecovery.h"
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
#include <numeric>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <tuple>

namespace top {
using namespace play_combat;
namespace {

// 05-AL: encoded text/requirements never contain the old save delimiters.
std::string questHex(const std::string& input) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string output;
    for (unsigned char c : input.substr(0, 1024)) {
        output.push_back(digits[c >> 4]);
        output.push_back(digits[c & 15]);
    }
    return output;
}
int questNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
bool questUnhex(const std::string& encoded, std::string& decoded) {
    if (encoded.size() > 2048 || encoded.size() % 2 != 0) return false;
    std::string result;
    for (std::size_t i=0; i<encoded.size(); i+=2) {
        int hi=questNibble(encoded[i]), lo=questNibble(encoded[i+1]);
        if (hi<0 || lo<0) return false;
        result.push_back(static_cast<char>((hi<<4)|lo));
    }
    decoded=std::move(result);
    return true;
}
std::vector<std::string> questSplit(const std::string& input, char delim) {
    std::vector<std::string> parts;
    std::size_t pos=0;
    while (pos<=input.size()) {
        std::size_t next=input.find(delim,pos);
        if (next==std::string::npos) next=input.size();
        parts.push_back(input.substr(pos,next-pos));
        if (next==input.size()) break;
        pos=next+1;
    }
    return parts;
}

std::string cell(const std::vector<std::string>& cols, int index) {
    if (index < 0 || index >= static_cast<int>(cols.size())) return {};
    return cols[static_cast<std::size_t>(index)];
}

int cellInt(const std::vector<std::string>& cols, int index, int fallback) {
    const std::string text = cell(cols, index);
    if (text.empty()) return fallback;
    return std::atoi(text.c_str());
}

std::string asciiOf(const std::string& name) {
    std::string ascii;
    for (unsigned char ch : name) {
        if (ch >= 32 && ch < 127) ascii.push_back(static_cast<char>(ch));
        else if (!ascii.empty() && ascii.back() != ' ') ascii.push_back(' ');
    }
    while (!ascii.empty() && ascii.back() == ' ') ascii.pop_back();
    return ascii;
}

std::string lowerCopy(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

struct ScalarFormula { int base = 0; float perLevel = 0.f; };

std::unordered_map<std::string, ScalarFormula> scalarSkillFormulas(const std::string& path) {
    std::unordered_map<std::string, ScalarFormula> out;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return out;
    char raw[4096];
    std::string function;
    while (std::fgets(raw, sizeof raw, file)) {
        std::string line(raw);
        const std::size_t fn = line.find("function Skill");
        if (fn != std::string::npos) {
            const std::size_t begin = fn + 9;
            const std::size_t end = line.find('(', begin);
            function = end == std::string::npos ? std::string() : line.substr(begin, end - begin);
            continue;
        }
        if (function.empty()) continue;
        const std::string lowered = lowerCopy(line);
        const bool wantsSp = function.rfind("SkillSp_", 0) == 0;
        const bool assignment = (wantsSp && (lowered.find("sp_reduce") != std::string::npos ||
                                             lowered.find("local sp ") != std::string::npos)) ||
                                (!wantsSp && lowered.find("cooldown") != std::string::npos);
        const std::size_t equal = line.find('=');
        std::string expr;
        if (assignment && equal != std::string::npos) {
            expr = line.substr(equal + 1);
        } else {
            const std::size_t returned = lowered.find("return");
            if (returned == std::string::npos) continue;
            expr = line.substr(returned + 6);
        }
        const std::size_t comment = expr.find("--");
        if (comment != std::string::npos) expr.resize(comment);
        expr.erase(std::remove_if(expr.begin(), expr.end(), [](unsigned char c) {
            return std::isspace(c) || c == '(' || c == ')';
        }), expr.end());
        const std::string floorToken = "math.floor";
        const std::size_t floorAt = expr.find(floorToken);
        if (floorAt != std::string::npos) expr.erase(floorAt, floorToken.size());
        for (std::size_t named = expr.find("SkillLevel"); named != std::string::npos; named = expr.find("SkillLevel", named + 4))
            expr.replace(named, 10, "sklv");
        const std::size_t bareLevel = expr.find("sklv");
        if (bareLevel != std::string::npos && expr.find("sklv*") == std::string::npos)
            expr.insert(bareLevel + 4, "*1");
        const std::size_t level = expr.find("sklv*");
        if (level == std::string::npos) {
            char* end = nullptr;
            const long value = std::strtol(expr.c_str(), &end, 10);
            if (end && *end == '\0') out[function] = {static_cast<int>(value), 0};
            continue;
        }
        const float factor = std::strtof(expr.c_str() + level + 5, nullptr);
        const std::string before = expr.substr(0, level);
        if (!before.empty() && (before.back() == '+' || before.back() == '-')) {
            const float slope = before.back() == '-' ? -factor : factor;
            const int base = std::atoi(before.substr(0, before.size() - 1).c_str());
            out[function] = {base, slope};
        } else if (level == 0) {
            const std::size_t plus = expr.find('+', 5);
            const std::size_t minus = expr.find('-', 5);
            const std::size_t op = plus != std::string::npos ? plus : minus;
            int base = 0;
            if (op != std::string::npos) base = std::atoi(expr.c_str() + op + 1) * (expr[op] == '-' ? -1 : 1);
            out[function] = {base, factor};
        }
    }
    std::fclose(file);
    return out;
}

// Cria cada pasta do caminho (Client, Client/user, ...). Ignora erro: a pasta
// pode ja existir, e se nao der para criar o fopen do save falha do mesmo jeito.
void makeParents(const std::string& path) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (path[i] != '/') continue;
        (void)mkdir(path.substr(0, i).c_str(), 0755);
    }
}

// Um save por personagem: trocar de Lance para Ami nao pode carregar a bolsa,
// o ouro e a classe do outro. Raiz vazia (nos testes) nao grava em lugar nenhum.

// O cliente antigo pedia texture/icon/%s.png. No Linux o nome tem que bater
// maiusculas e minusculas; se o arquivo nao existe, fica o error.png.
std::string iconPathFor(const std::string& tablePath, const std::string& name) {
    const std::string marker = "/scripts/table/iteminfo.txt";
    std::string root = tablePath;
    const auto cut = root.rfind(marker);
    if (cut != std::string::npos) root.resize(cut);
    auto exists = [](const std::string& path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0;
    };
    const std::string exact = "texture/icon/" + name + ".png";
    if (exists(root + "/" + exact)) return exact;
    static std::unordered_map<std::string, std::string> byLower;
    static std::string indexed;
    if (indexed != root) {
        byLower.clear();
        DIR* dir = opendir((root + "/texture/icon").c_str());
        if (dir) {
            while (const dirent* ent = readdir(dir)) {
                const std::string file = ent->d_name;
                if (file.size() < 5) continue;
                byLower[lowerCopy(file)] = file;
            }
            closedir(dir);
        }
        indexed = root;
    }
    const auto it = byLower.find(lowerCopy(name + ".png"));
    if (it != byLower.end()) return "texture/icon/" + it->second;
    if (exists(root + "/texture/icon/error.png")) return "texture/icon/error.png";
    return {};
}

// Le o arquivo de uma vez e separa as linhas. Antes era um fgetc por byte,
// e o iteminfo.txt tem varios MB.
bool readLines(const std::string& path, std::vector<std::string>& lines) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::string data;
    char chunk[65536];
    std::size_t got = 0;
    while ((got = std::fread(chunk, 1, sizeof chunk, file)) > 0) data.append(chunk, got);
    std::fclose(file);
    std::size_t start = 0;
    while (start < data.size()) {
        std::size_t end = data.find('\n', start);
        const bool last = end == std::string::npos;
        if (last) end = data.size();
        std::string line = data.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!last || !line.empty()) lines.push_back(std::move(line));
        start = end + 1;
    }
    return !lines.empty();
}

std::vector<std::string> tabs(const std::string& line) {
    std::vector<std::string> out;
    std::size_t cursor = 0;
    while (cursor <= line.size()) {
        const std::size_t tab = line.find('\t', cursor);
        if (tab == std::string::npos) {
            out.push_back(line.substr(cursor));
            break;
        }
        out.push_back(line.substr(cursor, tab - cursor));
        cursor = tab + 1;
    }
    return out;
}

void commaInts(const std::string& text, std::vector<int>& out) {
    std::string token;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == ',') {
            if (!token.empty()) out.push_back(std::atoi(token.c_str()));
            token.clear();
        } else if (text[i] != ' ') {
            token.push_back(text[i]);
        }
    }
}

int leadingInt(const std::string& text) { return std::atoi(text.c_str()); }

// Le a tabela uma unica vez e guarda so as linhas pedidas. Antes cada monstro
// relia o characterinfo.txt inteiro.
bool loadRows(const std::string& path, const std::unordered_set<int>& wanted,
              std::unordered_map<int, std::vector<std::string>>& rows) {
    std::vector<std::string> lines;
    if (!readLines(path, lines)) return false;
    for (const std::string& line : lines) {
        if (line.size() < 2 || line[0] == '/') continue;
        const int id = std::atoi(line.c_str());
        if (id <= 0 || !wanted.count(id) || rows.count(id)) continue;
        rows[id] = tabs(line);
    }
    return !rows.empty();
}


}  // namespace

int Play::damageOf(int minA, int maxA, int level, int def, int roll, int resist) {
    if (maxA < minA) std::swap(minA, maxA);
    if (maxA <= 0) return 1; // registro sem ataque (monstros passivos)
    const int span = maxA - minA;
    const int attack = minA + (span > 0 ? (roll % (span + 1)) : 0);
    // skilleffect.lua Phy_Dmg, sem o antigo teto que enfraquecia chefes.
    return historical_stats::physicalDamage(attack, def, resist);
}

float Play::attackReach(const Foe& target) const {
    int skillId = 25;  // Bare Hand
    const int selectedSlot = requestedSkill_ >= 0 ? requestedSkill_ : armedSkill_;
    if (selectedSlot >= 0 && selectedSlot < 8 && skills_[selectedSlot].id > 0) {
        skillId = skills_[selectedSlot].id;
    } else {
        int weaponType = 0;
        int swordCount = 0;
        for (int slot : {6, 9}) {
            const auto worn = worn_.find(slot);
            if (worn == worn_.end()) continue;
            const auto item = items_.find(worn->second);
            if (item == items_.end()) continue;
            if (item->second.type == 1) ++swordCount;
            if (slot == 9 || weaponType == 0) weaponType = item->second.type;
        }
        if (swordCount >= 2) skillId = 38;
        else {
            static const int defaultSkill[] = {25, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37};
            if (weaponType >= 1 && weaponType <= 10) skillId = defaultSkill[weaponType];
        }
    }
    const auto skill = skillRules_.find(skillId);
    const int distanceCm = skill == skillRules_.end() ? 100 : skill->second.distance;
    return historical_skill::reachMeters({0, 0, 4, distanceCm}, player_.radius, target.radius);
}

int Play::roll(int span) {
    seed_ = seed_ * 1103515245u + 12345u;
    if (span <= 1) return 0;
    return static_cast<int>((seed_ >> 16) % static_cast<unsigned>(span));
}

bool Play::summonHistoricalMonster(ActorSkin* actors) {
    if (!actors) return false;
    const int activeSummons = static_cast<int>(std::count_if(foes_.begin(), foes_.end(),
        [](const Foe& foe) { return foe.summoned && foe.summonRemaining > 0.f && foe.hp > 0; }));
    if (activeSummons >= 5) return false;

    int id = 789;
    switch (roll(5)) {
    case 0: id = 40 + roll(41); break;
    case 1: id = 98 + roll(49); break;
    case 2: id = 194 + roll(48); break;
    case 3: id = 500 + roll(72); break;
    default: break;
    }
    return createHistoricalSummon(id, 50, 40, actors);
}

bool Play::summonAriesGuardian(ActorSkin* actors) {
    return createHistoricalSummon(1009, 3600, 3600, actors);
}

bool Play::summonGoddessGuardian(ActorSkin* actors) {
    return createHistoricalSummon(857, 3700, 3600, actors);
}

bool Play::summonMedicalReportCreature(ActorSkin* actors) {
    return createHistoricalSummon(858, 190, 180, actors);
}

bool Play::createHistoricalSummon(int id, int respawnSeconds, int lifetimeSeconds,
                                  ActorSkin* actors, bool followsPlayer,
                                  float spawnOffsetX, float spawnOffsetY) {
    if (!actors || id <= 0 || respawnSeconds <= 0 || lifetimeSeconds <= 0) return false;
    const auto found = summonMonsterRows_.find(id);
    if (found == summonMonsterRows_.end()) return false;
    const std::vector<std::string>& row = found->second;
    const bool duplicateModel = std::any_of(foes_.begin(), foes_.end(),
        [id](const Foe& foe) { return foe.id == id; });
    const int actorSlot = actors->appendMonsterModel(id, duplicateModel);
    if (actorSlot < 0) return false;

    Foe foe;
    foe.id = id;
    foe.name = asciiOf(cell(row, 1));
    if (foe.name.size() < 3) foe.name = "Monstro " + std::to_string(id);
    const auto stats = historical_stats::monsterFromColumns(
        cellInt(row, 60, 1), cellInt(row, 61, 20), cellInt(row, 62, 0),
        cellInt(row, 65, 0), cellInt(row, 66, 0), cellInt(row, 68, 0),
        cellInt(row, 69, 0), cellInt(row, 70, 0), cellInt(row, 67, 0), cellInt(row, 71, 0));
    foe.level = stats.level;
    foe.maxHp = foe.hp = stats.hp;
    foe.minAtk = stats.minAttack;
    foe.maxAtk = stats.maxAttack;
    foe.def = stats.defense;
    foe.hit = stats.hit;
    foe.dodge = stats.dodge;
    foe.resist = stats.physicalResistance;
    foe.crit = stats.criticalPercent;
    foe.con = std::max(0, cellInt(row, 83, 0));
    foe.sta = std::max(0, cellInt(row, 84, 0));
    foe.job = std::max(0, cellInt(row, 89, 0));
    foe.expReward = stats.experience > 0 ? stats.experience : -1;
    const int reach = std::max(1, cellInt(row, 76, 100));
    foe.range = static_cast<float>(reach) / 100.f;
    foe.radius = static_cast<float>(std::max(0, cellInt(row, 32, 40))) / 100.f;
    foe.vision = static_cast<float>(std::max(0, cellInt(row, 55, 1000))) / 100.f;
    foe.leash = static_cast<float>(std::max(0, cellInt(row, 77, 1500))) / 100.f;
    foe.ai = cellInt(row, 53, 1);
    // ItemEffect.lua seasonal pets spawn five historical coordinate units
    // beside the player (the map uses 100 units per world meter).
    foe.x = hereX_ + (followsPlayer ? .05f : spawnOffsetX);
    foe.y = hereY_ + (followsPlayer ? .05f : spawnOffsetY);
    foe.homeX = hereX_;
    foe.homeY = hereY_;
    foe.yaw = 145.f * 0.017453292519943295f;
    foe.summoned = true;
    foe.followsPlayer = followsPlayer;
    foe.respawnSeconds = respawnSeconds; // CreateChaX(..., Refresh)
    foe.summonRemaining = static_cast<float>(lifetimeSeconds); // SetChaLifeTime(new, Life)
    foe.actorSlot = actorSlot;
    std::vector<int> attackIds, attackRates;
    commaInts(cell(row, 44), attackIds);
    commaInts(cell(row, 45), attackRates);
    for (std::size_t k = 0; k < attackIds.size() && k < 16; ++k) {
        if (k >= attackRates.size() || attackRates[k] <= 0 || attackRates[k] > 10000) continue;
        const auto rule = skillRules_.find(attackIds[k]);
        if (rule == skillRules_.end() || rule->second.helpful != 0 || rule->second.type == 2 ||
            rule->second.target != 4) continue;
        const monster_skills::Attack attack{attackIds[k], attackRates[k],
            static_cast<float>(rule->second.distance) / 100.f, rule->second.flightSpeed};
        if (monster_skills::valid(attack)) foe.attacks.push_back(attack);
    }
    foe.speed = std::min(8.f, static_cast<float>(std::max(0, cellInt(row, 78, 0))) / 100.f);
    foe.cd = std::clamp(static_cast<float>(cellInt(row, 75, 1400)) / 1000.f, .7f, 2.2f);
    std::vector<int> dropIds, dropRates;
    commaInts(cell(row, 46), dropIds);
    commaInts(cell(row, 47), dropRates);
    for (std::size_t d = 0; d < dropIds.size(); ++d) {
        const int rate = d < dropRates.size() ? dropRates[d] : 0;
        if (dropIds[d] > 0 && rate > 0) foe.drops.push_back({dropIds[d], rate});
    }
    for (int action = 1; action <= 54; ++action) {
        const auto timing = actors->monsterActionTiming(actorSlot, action);
        foe.actionDurations[static_cast<std::size_t>(action)] = timing.duration;
        foe.actionImpacts[static_cast<std::size_t>(action)] = timing.impact;
    }
    foes_.push_back(std::move(foe));
    return true;
}

void Play::say(const std::string& text) {
    log_.push_back(text);
    if (log_.size() > 4) log_.erase(log_.begin());
}

void Play::makeQuests() {
    quests_[0] = {"Caracois da praca", 71, 0, 3, 0, 30, 15, false};
    quests_[1] = {"Flores do jardim", 75, 185, 2, 0, 40, 20, false};
    quests_[2] = {"Espirito da mata", 103, 0, 1, 0, 50, 25, false};
}

void Play::load(const std::string& path) {
    saveRecoveredFromBackup_ = false;
    saveRecoveryPath_.clear();
    ownedBoatIds_.clear();
    ownedBoatHp_.clear();
    ownedBoatSp_.clear();
    historicalRecords_.clear();
    fame_ = 0;
    fairyCoinChestDay_ = 0;
    fairyCoinChestOpened_ = 0;
    hairStyleItemId_ = 0;
    std::string saveLine;
    bool recovered = false;
    if (!save_recovery::read(path, saveLine, recovered)) return;
    if (recovered) {
        saveRecoveredFromBackup_ = true;
        saveRecoveryPath_ = path;
        say("Save principal invalido; progresso recuperado do backup.");
    }
    const char* buf = saveLine.c_str();
    lifeSkills_.clear();
    lifeSkillPoints_ = 0;
    int level = 1;
    int hp = 0;
    int have0 = 0;
    int have1 = 0;
    int have2 = 0;
    int claimed0 = 0;
    int claimed1 = 0;
    int claimed2 = 0;
    if (std::sscanf(buf, "%d %d %d %d %d %d %d %d %d %d", &level, &exp_, &gold_, &hp, &have0, &have1, &have2, &claimed0,
                    &claimed1, &claimed2) == 10) {
        additionalMissions_.clear();
        journalSelectedId_=0; journalPage_=0;
        bool explicitStats = false;
        int job = 0;
        int ap = 0;
        int tp = 0;
        int str = 5;
        int agi = 5;
        int dex = 5;
        int con = 5;
        int sta = 5;
        int sheetHp = 0;
        int sheetSp = 0;
        int sheetMin = 0;
        int sheetMax = 0;
        int sheetDef = 0;
        const char* pipe = std::strchr(buf, '|');
        const char* pipe2 = pipe ? std::strchr(pipe + 1, '|') : nullptr;
        const char* pipe3 = pipe2 ? std::strchr(pipe2 + 1, '|') : nullptr;
        const char* pipe4 = pipe3 ? std::strchr(pipe3 + 1, '|') : nullptr;
        const char* pipe5 = pipe4 ? std::strchr(pipe4 + 1, '|') : nullptr;
        if (pipe3 && std::sscanf(pipe3 + 1, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &job, &ap, &tp, &str, &agi, &dex,
                                 &con, &sta, &sheetHp, &sheetSp, &sheetMin, &sheetMax, &sheetDef) == 13) {
            explicitStats = true;
        }
        // O limite de 40 era so uma trava de sanidade e jogava de volta ao
        // nivel 1 quem passasse dele.
        constexpr int kMaxSavedLevel = 100;
        if (explicitStats) {
            if (level >= 1 && level < kMaxSavedLevel) player_.level = level;
            job_ = job >= 0 && job <= 18 ? job : 0;
            ap_ = std::max(0, ap);
            tp_ = std::max(0, tp);
            bstr_ = std::max(5, str);
            bagi_ = std::max(5, agi);
            bdex_ = std::max(5, dex);
            bcon_ = std::max(5, con);
            bsta_ = std::max(5, sta);
            sheetHp_ = std::max(1, sheetHp);
            sheetSp_ = std::max(1, sheetSp);
            baseMin_ = std::max(0, sheetMin);
            baseMax_ = std::max(baseMin_, sheetMax);
            baseDef_ = std::max(0, sheetDef);
            player_.maxHp = sheetHp_;
            player_.minAtk = baseMin_;
            player_.maxAtk = baseMax_;
            player_.def = baseDef_;
            player_.homeX = static_cast<float>(sheetSp_);
            sheetReady_ = true;
        } else if (level > player_.level && level < kMaxSavedLevel) {
            // Save antigo: o nivel já tinha virado +8 HP, +1 ataque e +4 SP.
            // Esses números continuam o chão. Os pontos daqueles níveis ficam por gastar.
            const int gained = level - player_.level;
            player_.level = level;
            player_.maxHp += 8 * gained;
            player_.maxAtk += gained;
            player_.minAtk += gained;
            player_.homeX += 4.f * static_cast<float>(gained);
            sheetHp_ = player_.maxHp;
            sheetSp_ = std::max(1, static_cast<int>(player_.homeX));
            baseMin_ = player_.minAtk;
            baseMax_ = player_.maxAtk;
            baseDef_ = player_.def;
            ap_ = 4;
            tp_ = 0;
            for (int lv = 2; lv <= player_.level; ++lv) {
                ap_ += progression::attrPointAt(lv);
                tp_ += progression::skillPointAt(lv);
            }
            job_ = 0;
            bstr_ = bagi_ = bdex_ = bcon_ = bsta_ = 5;
            sheetReady_ = true;
        }
        if (pipe4) {
            int missionId = 0;
            int active = 0;
            int complete = 0;
            float targetX = -1.f;
            float targetY = -1.f;
            int rewardExp = 0;
            int rewardGold = 0;
            int targetId = 0;
            int targetCount = 0;
            int have = 0;
            const int fields = std::sscanf(pipe4 + 1, "%d,%d,%d,%f,%f,%d,%d,%d,%d,%d", &missionId, &active,
                                           &complete, &targetX, &targetY, &rewardExp, &rewardGold, &targetId,
                                           &targetCount, &have);
            if (fields >= 7 && missionId > 0) {
                historical_.id = missionId;
                historical_.title = "Missao historica " + std::to_string(missionId);
                historical_.targetX = targetX;
                historical_.targetY = targetY;
                historical_.rewardExp = std::max(0, rewardExp);
                historical_.rewardGold = std::max(0, rewardGold);
                historical_.active = active != 0;
                historical_.complete = complete != 0;
                if (fields >= 10) {
                    historical_.targetId = targetId;
                    historical_.targetCount = std::max(0, targetCount);
                    historical_.have = std::max(0, std::min(have, historical_.targetCount));
                }
            }
        }
        temp_.clear();
        const char* pipe6 = pipe5 ? std::strchr(pipe5 + 1, '|') : nullptr;
        const char* pipe7 = pipe6 ? std::strchr(pipe6 + 1, '|') : nullptr;
        const char* pipe8 = pipe7 ? std::strchr(pipe7 + 1, '|') : nullptr;
        const char* pipe9 = pipe8 ? std::strchr(pipe8 + 1, '|') : nullptr;
        // Optional trailing field introduced by 05-C. Legacy saves have no
        // capacity field and always start with 24 slots.
        const char* pipe10 = pipe9 ? std::strchr(pipe9 + 1, '|') : nullptr;
        // 05-V: campo final optativo, sem mudar qualquer campo antigo.
        const char* pipe11 = pipe10 ? std::strchr(pipe10 + 1, '|') : nullptr;
        // 05-Y: campos extras apos o SP (legados continuam aceitos).
        const char* pipe12 = pipe11 ? std::strchr(pipe11 + 1, '|') : nullptr;
        const char* pipe13 = pipe12 ? std::strchr(pipe12 + 1, '|') : nullptr;
        const char* pipe14 = pipe13 ? std::strchr(pipe13 + 1, '|') : nullptr;
        const char* pipe15 = pipe14 ? std::strchr(pipe14 + 1, '|') : nullptr;
        const char* pipe16 = pipe15 ? std::strchr(pipe15 + 1, '|') : nullptr;
        const char* pipe17 = pipe16 ? std::strchr(pipe16 + 1, '|') : nullptr;
        const char* pipe18 = pipe17 ? std::strchr(pipe17 + 1, '|') : nullptr;
        const char* lifeTag = std::strstr(buf, "|LS1:");
        if (lifeTag) {
            lifeSkillPoints_ = 0;
            lifeSkills_.clear();
            const char* field = lifeTag + 5;
            int points = 0;
            int used = 0;
            if (std::sscanf(field, "%d%n", &points, &used) == 1 && used > 0) {
                lifeSkillPoints_ = std::max(0, std::min(points, 1000000));
                field += used;
                if (*field == ';') ++field;
                while (*field && *field != '|' && *field != '\n' && *field != '\r') {
                    int id = 0, skillLevel = 0, consumed = 0;
                    if (std::sscanf(field, "%d,%d%n", &id, &skillLevel, &consumed) != 2 || consumed <= 0) break;
                    if (id > 0 && skillLevel > 0 && skillLevel <= 10) lifeSkills_[id] = skillLevel;
                    field += consumed;
                    if (*field == ';') ++field;
                }
            }
        }
        restoredSp_ = -1.f;
        if (pipe11) {
            float currentSp = -1.f;
            if (std::sscanf(pipe11 + 1, "%f", &currentSp) == 1 &&
                std::isfinite(currentSp) && currentSp >= 0.f && currentSp <= 1000000.f)
                restoredSp_ = currentSp;
        }
        if (pipe10) bagCapacity_ = inventory_rules::validatedCapacity(std::atoi(pipe10 + 1));
        if (pipe6) {
            const char* hold = pipe6 + 1;
            while (*hold && *hold != '\n' && *hold != '\r' && *hold != '|') {
                int id = 0;
                int count = 0;
                int used = 0;
                if (std::sscanf(hold, "%d,%d%n", &id, &count, &used) == 2 && used > 0) {
                    if (id > 0 && count > 0) {
                        const auto it = items_.find(id);
                        addStack(temp_,id,count,it==items_.end()?1:it->second.maxStack,16);
                    }
                    hold += used;
                    if (*hold == ';') ++hold;
                } else {
                    break;
                }
            }
        }
        if (pipe5) {
            const char* skill = pipe5 + 1;
            while (*skill && *skill != '\n' && *skill != '\r' && *skill != '|') {
                int id = 0;
                int levelSkill = 0;
                int used = 0;
                if (std::sscanf(skill, "%d,%d%n", &id, &levelSkill, &used) != 2 || used <= 0) break;
                if (id > 0 && levelSkill > 0) learnedSkills_[id] = levelSkill;
                skill += used;
                if (*skill == ';') ++skill;
            }
        }
        // Saves feitos pelo primeiro UI de teste podiam ter skills profissionais
        // no mesmo mapa das skills de combate. Migra os IDs identificados pela
        // tabela historica sem descartar o nivel que o jogador ja testou.
        for (const auto& entry : lifeSkillBooks_) {
            const int skillId = entry.second.skillId;
            const auto legacy = learnedSkills_.find(skillId);
            if (legacy == learnedSkills_.end()) continue;
            lifeSkills_[skillId] = std::max(lifeSkills_[skillId], std::min(10, legacy->second));
            learnedSkills_.erase(legacy);
        }
        if (pipe7) {
            int ids[8]{};
            const int loaded = std::sscanf(pipe7 + 1, "%d,%d,%d,%d,%d,%d,%d,%d",
                    &ids[0], &ids[1], &ids[2], &ids[3], &ids[4], &ids[5], &ids[6], &ids[7]);
            if (loaded >= 4) {
                for (int slot = 0; slot < 8; ++slot) {
                    const auto rule = skillRules_.find(ids[slot]);
                    if (ids[slot] > 0 && learnedSkills_.count(ids[slot]) && rule != skillRules_.end())
                        skills_[slot] = {ids[slot], rule->second.name, rule->second.sp, rule->second.cooldown};
                    else skills_[slot] = {};
                }
                hotbarLoaded_ = true;
            }
        }
        auto loadMissionItems = [](const char* begin, std::vector<MissionItem>& out) {
            out.clear();
            if (!begin) return;
            while (*begin && *begin != '\n' && *begin != '\r' && *begin != '|') {
                int id = 0;
                int count = 0;
                int used = 0;
                if (std::sscanf(begin, "%d,%d%n", &id, &count, &used) != 2 || used <= 0) break;
                if (id > 0 && count > 0) out.push_back({id, count});
                begin += used;
                if (*begin == ';') ++begin;
            }
        };
        if (pipe8) loadMissionItems(pipe8 + 1, historical_.neededItems);
        if (pipe9) loadMissionItems(pipe9 + 1, historical_.rewardItems);
        expNext_ = experience_.needed(player_.level);
        player_.hp = std::max(1, hp);
        player_.homeY = player_.homeX;
        quests_[0].have = have0;
        quests_[1].have = have1;
        quests_[2].have = have2;
        quests_[0].claimed = claimed0 != 0;
        quests_[1].claimed = claimed1 != 0;
        quests_[2].claimed = claimed2 != 0;
        const char* bar = std::strchr(buf, '|');
        if (bar) {
            ++bar;
            bag_.clear();
            while (*bar && *bar != '|') {
                int id = 0;
                int count = 0;
                int used = 0;
                if (std::sscanf(bar, "%d,%d%n", &id, &count, &used) == 2 && used > 0) {
                    if (id > 0 && count > 0) addBag(id, count);
                    bar += used;
                    if (*bar == ';') ++bar;
                } else {
                    break;
                }
            }
            if (*bar == '|') ++bar;
            worn_.clear();
            while (*bar && *bar != '\n' && *bar != '\r' && *bar != '|') {
                int slot = 0;
                int id = 0;
                int used = 0;
                if (std::sscanf(bar, "%d,%d%n", &slot, &id, &used) == 2 && used > 0) {
                    if (id > 0) worn_[slot] = id;
                    bar += used;
                    if (*bar == ';') ++bar;
                } else {
                    break;
                }
            }
        }
        // Fazer a associacao UID apenas depois de ler pilhas e slots.
        // Nunca aplicar atributos de metadados que nao combinem com ID/quantidade.
        if (pipe12 && (std::strncmp(pipe12 + 1, "Y1:", 3) == 0 ||
                       std::strncmp(pipe12 + 1, "Y2:", 3) == 0 ||
                       std::strncmp(pipe12 + 1, "Y3:", 3) == 0 ||
                       std::strncmp(pipe12 + 1, "Y4:", 3) == 0 ||
                       std::strncmp(pipe12 + 1, "Y5:", 3) == 0))
            item_instance::decodeStacks(pipe12 + 4, bag_);
        if (pipe13 && (std::strncmp(pipe13 + 1, "T1:", 3) == 0 ||
                       std::strncmp(pipe13 + 1, "T2:", 3) == 0 ||
                       std::strncmp(pipe13 + 1, "T3:", 3) == 0 ||
                       std::strncmp(pipe13 + 1, "T4:", 3) == 0 ||
                       std::strncmp(pipe13 + 1, "T5:", 3) == 0))
            item_instance::decodeStacks(pipe13 + 4, temp_);
        assignItemUids();
        wornUid_.clear();
        if (pipe14 && std::strncmp(pipe14 + 1, "W1:", 3) == 0)
            item_instance::decodeWorn(pipe14 + 4, worn_, wornUid_, bag_);
        if(pipe15 && std::strncmp(pipe15+1,"Z1:",3)==0)
            forge_instance::decode(pipe15+4,bag_);
        if(pipe16 && std::strncmp(pipe16+1,"ZT1:",4)==0)
            forge_instance::decode(pipe16+5,temp_);
        if(pipe17 && std::strncmp(pipe17+1,"Q2:",3)==0)
            decodeAdditionalMissions(pipe17+4);
        if(pipe18 && std::strncmp(pipe18+1,"QH2:",4)==0 && historical_.id>0) {
            std::string meta(pipe18+5);
            const auto end=meta.find_first_of("|\r\n");
            if(end!=std::string::npos)meta.resize(end);
            const auto comma=meta.find(',');
            std::string title,objective;
            if(comma!=std::string::npos && questUnhex(meta.substr(0,comma),title) &&
               questUnhex(meta.substr(comma+1),objective)) {
                if(!title.empty())historical_.title=std::move(title);
                historical_.objective=std::move(objective);
            }
        }
        // 05-AP: optional bound NPC name for quests accepted with historical
        // hand-in rules. Older saves have no Q3 and retain their 05-AO flow.
        const char* pipe19=pipe18?std::strchr(pipe18+1,'|'):nullptr;
        if(pipe19 && std::strncmp(pipe19+1,"Q3:",3)==0) {
            std::string field(pipe19+4);
            const auto end=field.find_first_of("|\r\n");
            if(end!=std::string::npos)field.resize(end);
            if(field.size()<=8192)for(const auto& token:questSplit(field,';')) {
                const auto delim=token.find('~');
                if(delim==std::string::npos)continue;
                int id=0,used=0;
                if(std::sscanf(token.substr(0,delim).c_str(),"%d%n",&id,&used)!=1 ||
                   used!=int(delim) || id<=0)continue;
                std::string npc;
                if(!questUnhex(token.substr(delim+1),npc) || npc.empty() || npc.size()>128 ||
                   npc.find('\0')!=std::string::npos)continue;
                if(auto* quest=findHistoricalMission(id);quest && quest->active) {
                    quest->turnInRequired=true;
                    quest->turnInNpc=std::move(npc);
                }
            }
        }
        // 05-AQ: Q4 is untrusted optional save data. Reject malformed entries
        // entirely, never partially apply flag state to an active mission.
        const char* pipe20=pipe19?std::strchr(pipe19+1,'|'):nullptr;
        if(pipe20 && std::strncmp(pipe20+1,"Q4:",3)==0) {
            std::string field(pipe20+4);
            const auto end=field.find_first_of("|\r\n");
            if(end!=std::string::npos)field.resize(end);
            auto parseFlagList=[](const std::string& text,std::vector<int>& out) {
                if(text=="-")return true;
                if(text.empty() || text.size()>160)return false;
                for(const auto& part:questSplit(text,'.')) {
                    if(part.empty() || part.size()>3 || out.size()>=32)return false;
                    for(char ch:part)if(ch<'0'||ch>'9')return false;
                    int flag=std::atoi(part.c_str());
                    if(flag<1 || flag>255 ||
                       std::find(out.begin(),out.end(),flag)!=out.end())return false;
                    out.push_back(flag);
                }
                return true;
            };
            if(field.size()<=8192)for(const auto& token:questSplit(field,';')) {
                if(token.empty())continue;
                auto pieces=questSplit(token,'~');
                if(pieces.size()!=4 || pieces[0].empty() || pieces[0].size()>8)continue;
                bool numeric=true;
                for(char ch:pieces[0])if(ch<'0'||ch>'9')numeric=false;
                if(!numeric)continue;
                const int id=std::atoi(pieces[0].c_str());
                auto* mission=findHistoricalMission(id);
                if(!mission || !mission->active || !mission->turnInRequired)continue;
                std::vector<int> active,required,forbidden;
                if(!parseFlagList(pieces[1],active) || !parseFlagList(pieces[2],required) ||
                   !parseFlagList(pieces[3],forbidden))continue;
                mission->flags=std::move(active);
                mission->requiredFlags=std::move(required);
                mission->forbiddenFlags=std::move(forbidden);
            }
        }
        // 05-AU: trust only validated rules from the historical Lua catalog.
        // A malformed Q5 must NEVER create a new mission or trigger.
        const char* pipe21=pipe20?std::strchr(pipe20+1,'|'):nullptr;
        if(pipe21 && std::strncmp(pipe21+1,"Q5:",3)==0) {
            std::string field(pipe21+4);
            const auto end=field.find_first_of("|\r\n");
            if(end!=std::string::npos)field.resize(end);
            if(field.size()<=8192) {
                MissionCatalog catalog;
                for(const auto& row:questSplit(field,';')) {
                    if(row.empty())continue;
                    const auto sep=row.find('~');
                    if(sep==std::string::npos || sep<1 || sep>8)continue;
                    int id=0,used=0;
                    if(std::sscanf(row.substr(0,sep).c_str(),"%d%n",&id,&used)!=1 || used!=int(sep))continue;
                    HistoricalProgress* mission=findHistoricalMission(id);
                    if(!mission || !mission->active || !mission->turnInRequired)continue;
                    HistoricalMission definition;
                    if(!catalog.mission(id,definition) || !definition.turnInRequired ||
                       definition.eventTriggers.empty() || !definition.gate.unsupported.empty())continue;
                    std::vector<int> counts(definition.eventTriggers.size(),-1);
                    bool valid=true;
                    const auto tokens=questSplit(row.substr(sep+1),'.');
                    if(tokens.size()!=counts.size())continue;
                    for(const auto& token:tokens) {
                        int triggerId=0,count=0,n=0;
                        if(std::sscanf(token.c_str(),"%d=%d%n",&triggerId,&count,&n)!=2 || n!=int(token.size())) {
                            valid=false;break;
                        }
                        const auto it=std::find_if(definition.eventTriggers.begin(),definition.eventTriggers.end(),
                                      [&](const auto& event){return event.triggerId==triggerId;});
                        if(it==definition.eventTriggers.end() || count<0 || count>it->required) {valid=false;break;}
                        const auto index=static_cast<std::size_t>(it-definition.eventTriggers.begin());
                        if(counts[index]!=-1){valid=false;break;}
                        counts[index]=count;
                    }
                    if(!valid || std::find(counts.begin(),counts.end(),-1)!=counts.end())continue;
                    mission->triggers=definition.eventTriggers;
                    mission->triggerCounts=std::move(counts);
                    for(std::size_t i=0;i<mission->triggers.size();++i)
                        if(mission->triggerCounts[i]==mission->triggers[i].required) {
                            const auto& event=mission->triggers[i];
                            const int finalFlag=event.firstFlag+event.required-1;
                            if(std::find(mission->flags.begin(),mission->flags.end(),finalFlag)==mission->flags.end())
                                mission->flags.push_back(finalFlag);
                        }
                    // Reconstruct known expected result flags from scripts,
                    // not from optional untrusted save metadata.
                    mission->requiredFlags=definition.resultHasFlags;
                    mission->forbiddenFlags=definition.resultNoFlags;
                }
            }
        }
        // Saves antigos so tem (slot,itemId): casar por ordem de ocorrencia,
        // com IDs fisicos distintos para duas armas iguais.
        std::vector<int> slots;
        for (const auto& entry : worn_) slots.push_back(entry.first);
        std::sort(slots.begin(), slots.end());
        for (int slot : slots) {
            if (wornUid_.count(slot)) continue;
            for (const Stack& stack : bag_) {
                if (stack.id != worn_[slot] || stack.uid <= 0) continue;
                bool used = false;
                for (const auto& entry : wornUid_) if (entry.second == stack.uid) used = true;
                if (!used) { wornUid_[slot] = stack.uid; break; }
            }
        }
    }
    if (const char* fameTag = std::strstr(buf, "|FM1:")) {
        int fame = 0;
        int used = 0;
        if (std::sscanf(fameTag + 5, "%d%n", &fame, &used) == 1 && used > 0 &&
            fame >= 0 && fame <= 99999999)
            fame_ = fame;
    }
    fairyCoinChestDay_ = 0;
    fairyCoinChestOpened_ = 0;
    if (const char* chestTag = std::strstr(buf, "|FC1:")) {
        int day = 0;
        int opened = 0;
        int used = 0;
        if (std::sscanf(chestTag + 5, "%d,%d%n", &day, &opened, &used) == 2 && used > 0 &&
            day >= 19700101 && day <= 99991231 && opened >= 0 && opened <= 1000000) {
            fairyCoinChestDay_ = day;
            fairyCoinChestOpened_ = opened;
        }
    }
    if (const char* boatTag = std::strstr(buf, "|BT1:")) {
        const char* cursor = boatTag + 5;
        const char* end = std::strpbrk(cursor, "|\r\n");
        const std::string encoded(cursor, end ? static_cast<std::size_t>(end - cursor) : std::strlen(cursor));
        for (std::size_t at = 0; at < encoded.size() && ownedBoatIds_.size() < 3;) {
            const std::size_t stop = encoded.find(';', at);
            const std::string token = encoded.substr(at, stop == std::string::npos ? stop : stop - at);
            int id = 0, consumed = 0;
            if (std::sscanf(token.c_str(), "%d%n", &id, &consumed) == 1 && consumed == int(token.size()) &&
                id > 0 &&
                std::any_of(boatBuildOptions_.begin(), boatBuildOptions_.end(),
                            [id](const BoatBuildOption& option) { return option.id == id; }) &&
                std::find(ownedBoatIds_.begin(), ownedBoatIds_.end(), id) == ownedBoatIds_.end()) {
                ownedBoatIds_.push_back(id);
                const auto option = std::find_if(boatBuildOptions_.begin(), boatBuildOptions_.end(),
                                                 [id](const BoatBuildOption& row) { return row.id == id; });
                ownedBoatHp_[id] = option->maxHp;
                ownedBoatSp_[id] = option->maxSp;
            }
            if (stop == std::string::npos) break;
            at = stop + 1;
        }
    }
    if (const char* boatTag = std::strstr(buf, "|BT2:")) {
        const char* cursor = boatTag + 5;
        const char* end = std::strpbrk(cursor, "|\r\n");
        const std::string encoded(cursor, end ? static_cast<std::size_t>(end - cursor) : std::strlen(cursor));
        for (std::size_t at = 0; at < encoded.size();) {
            const std::size_t stop = encoded.find(';', at);
            const std::string token = encoded.substr(at, stop == std::string::npos ? stop : stop - at);
            int id = 0, hp = 0, sp = 0, consumed = 0;
            if (std::sscanf(token.c_str(), "%d,%d,%d%n", &id, &hp, &sp, &consumed) == 3 &&
                consumed == static_cast<int>(token.size()) && ownsBoat(id)) {
                const int maxHp = boatMaxHp(id);
                const int maxSp = boatMaxSp(id);
                ownedBoatHp_[id] = std::clamp(hp, 0, maxHp);
                ownedBoatSp_[id] = std::clamp(sp, 0, maxSp);
            }
            if (stop == std::string::npos) break;
            at = stop + 1;
        }
    }
    if (const char* recordsTag = std::strstr(buf, "|GR1:")) {
        const char* cursor = recordsTag + 5;
        const char* end = std::strpbrk(cursor, "|\r\n");
        const std::string encoded(cursor, end ? static_cast<std::size_t>(end - cursor) : std::strlen(cursor));
        for (std::size_t at = 0; at < encoded.size() && historicalRecords_.size() < 4096;) {
            const std::size_t stop = encoded.find(';', at);
            const std::string token = encoded.substr(at, stop == std::string::npos ? stop : stop - at);
            int id = 0, consumed = 0;
            if (std::sscanf(token.c_str(), "%d%n", &id, &consumed) == 1 &&
                consumed == static_cast<int>(token.size()) && id > 0 && id <= 100000000)
                historicalRecords_.insert(id);
            if (stop == std::string::npos) break;
            at = stop + 1;
        }
    }
    if (const char* hairTag = std::strstr(buf, "|HA1:")) {
        int itemId=0, consumed=0;
        if (std::sscanf(hairTag+5,"%d%n",&itemId,&consumed)==1 && consumed>0 &&
            itemId>0 && itemId<=1000000) hairStyleItemId_=itemId;
    }
    bank_.clear();
    if (const char* bankTag = std::strstr(buf, "|B1:")) {
        const char* begin = bankTag + 4;
        const char* end = std::strpbrk(begin, "|\r\n");
        const std::size_t length = end ? static_cast<std::size_t>(end - begin) : std::strlen(begin);
        std::string encoded(begin, length);
        std::size_t at = 0;
        while (at < encoded.size() && bank_.size() < 80) {
            const std::size_t stop = encoded.find(';', at);
            const std::string record = encoded.substr(at, stop == std::string::npos ? stop : stop - at);
            int id = 0, count = 0;
            if (std::sscanf(record.c_str(), "%d,%d", &id, &count) == 2 && id > 0 && count > 0)
                bank_.push_back({id, count});
            if (stop == std::string::npos) break;
            at = stop + 1;
        }
        assignItemUids();
        if (const char* instanceTag = std::strstr(buf, "|BI4:"))
            item_instance::decodeStacks(instanceTag + 5, bank_);
        else if (const char* instanceTag = std::strstr(buf, "|BI3:"))
            item_instance::decodeStacks(instanceTag + 5, bank_);
        else if (const char* instanceTag = std::strstr(buf, "|BI2:"))
            item_instance::decodeStacks(instanceTag + 5, bank_);
        else if (const char* instanceTag = std::strstr(buf, "|BI1:"))
            item_instance::decodeStacks(instanceTag + 5, bank_);
        if (const char* forgeTag = std::strstr(buf, "|ZB1:")) {
            begin = forgeTag + 5;
            end = std::strpbrk(begin, "|\r\n");
            encoded.assign(begin, end ? static_cast<std::size_t>(end - begin) : std::strlen(begin));
            forge_instance::decode(encoded.c_str(), bank_);
        }
    }
}

// 05-AL: extra missions are appended after the 05-Z save fields. Q2 never
// replaces or reorders the legacy historical mission. Invalid entries are
// ignored rather than risking invalid inventory/reward state.
std::string Play::encodeAdditionalMissions() const {
    std::ostringstream encoded;
    encoded << std::fixed << std::setprecision(2);
    auto encodeItems=[](const std::vector<MissionItem>& items){
        std::string text;
        for(const auto& i:items) {
            if(i.id<=0||i.count<=0)continue;
            if(!text.empty())text+='.';
            text+=std::to_string(i.id)+'~'+std::to_string(i.count);
        }
        return text;
    };
    for(const auto& m:additionalMissions_){
        if(m.id<=0)continue;
        if(encoded.tellp()>60000)break;
        encoded<<m.id<<','<<(m.active?1:0)<<','<<(m.complete?1:0)<<','
               <<m.targetX<<','<<m.targetY<<','<<m.rewardExp<<','<<m.rewardGold<<','
               <<m.targetId<<','<<m.targetCount<<','<<m.have<<','
               <<questHex(m.title)<<','<<questHex(m.objective)<<','
               <<encodeItems(m.neededItems)<<','<<encodeItems(m.rewardItems)<<';';
    }
    return encoded.str();
}
void Play::decodeAdditionalMissions(const char* data) {
    additionalMissions_.clear();
    if(!data)return;
    std::string field(data);
    const auto stop=field.find_first_of("|\r\n");
    if(stop!=std::string::npos)field.resize(stop);
    if(field.size()>100000)return;
    auto parseItems=[](const std::string& encoded,std::vector<MissionItem>& out){
        if(encoded.size()>2048)return false;
        for(const auto& token:questSplit(encoded,'.')) {
            if(token.empty())continue;
            int id=0,count=0,used=0;
            if(std::sscanf(token.c_str(),"%d~%d%n",&id,&count,&used)!=2 || used!=int(token.size()) ||
               id<=0 || count<=0 || count>1000000 || out.size()>=16)return false;
            out.push_back({id,count});
        }
        return true;
    };
    for(const auto& row:questSplit(field,';')) {
        if(row.empty())continue;
        if(additionalMissions_.size()>=kMaxSavedExtraMissions)break;
        auto fields=questSplit(row,',');
        if(fields.size()!=14)continue;
        HistoricalProgress m;
        int active=0,complete=0;
        auto readInt=[](const std::string& field,int& value){
            int used=0;
            return std::sscanf(field.c_str(),"%d%n",&value,&used)==1&&used==int(field.size());
        };
        auto readFloat=[](const std::string& field,float& value){
            int used=0;
            return std::sscanf(field.c_str(),"%f%n",&value,&used)==1 &&
                   used==int(field.size()) && std::isfinite(value) && std::abs(value)<1000000.f;
        };
        if(!readInt(fields[0],m.id) || !readInt(fields[1],active) || !readInt(fields[2],complete) ||
           !readFloat(fields[3],m.targetX) || !readFloat(fields[4],m.targetY) ||
           !readInt(fields[5],m.rewardExp) || !readInt(fields[6],m.rewardGold) ||
           !readInt(fields[7],m.targetId) || !readInt(fields[8],m.targetCount) ||
           !readInt(fields[9],m.have) || !questUnhex(fields[10],m.title) ||
           !questUnhex(fields[11],m.objective) ||
           !parseItems(fields[12],m.neededItems) || !parseItems(fields[13],m.rewardItems) ||
           m.id<=0 || m.id>100000000 || (active!=0&&active!=1) || (complete!=0&&complete!=1) ||
           (active&&complete) || m.targetCount<0 || m.targetCount>1000000 ||
           m.have<0 || m.have>m.targetCount || m.rewardExp<0 || m.rewardGold<0 ||
           findHistoricalMission(m.id))continue;
        m.active=active==1;m.complete=complete==1;
        additionalMissions_.push_back(std::move(m));
    }
    // A corrupted save must never grant more active quests than the UI cap.
    std::size_t active=historical_.active?1u:0u;
    for(auto& m:additionalMissions_){
        if(m.active && ++active>kMaxActiveMissions){m.active=false;m.complete=false;}
    }
}

bool Play::save(const std::string& path) {
    if(readOnlyPreview_)return true;
    if (path.empty()) return false;
    auto failed = [&]() {
        if (!saveFailureReported_) say("Falha ao salvar o personagem; confira espaco e permissoes do disco.");
        saveFailureReported_ = true;
        return false;
    };
    makeParents(path);
    std::string temporaryPath = path + ".tmp.XXXXXX";
    std::vector<char> temporaryName(temporaryPath.begin(), temporaryPath.end());
    temporaryName.push_back('\0');
    const int temporaryFd = ::mkstemp(temporaryName.data());
    if (temporaryFd < 0) return failed();
    temporaryPath.assign(temporaryName.data());
    FILE* file = ::fdopen(temporaryFd, "wb");
    if (!file) { ::close(temporaryFd); ::unlink(temporaryPath.c_str()); return failed(); }
    std::fprintf(file, "%d %d %d %d %d %d %d %d %d %d |", player_.level, exp_, gold_, player_.hp, quests_[0].have,
                 quests_[1].have, quests_[2].have, quests_[0].claimed ? 1 : 0, quests_[1].claimed ? 1 : 0,
                 quests_[2].claimed ? 1 : 0);
    for (const Stack& stack : bag_) std::fprintf(file, "%d,%d;", stack.id, stack.count);
    std::fputc('|', file);
    for (const auto& worn : worn_) std::fprintf(file, "%d,%d;", worn.first, worn.second);
    std::fprintf(file, "|%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", job_, ap_, tp_, bstr_, bagi_, bdex_, bcon_, bsta_,
                 sheetHp_, sheetSp_, baseMin_, baseMax_, baseDef_);
    std::fprintf(file, "|%d,%d,%d,%.2f,%.2f,%d,%d,%d,%d,%d|", historical_.id, historical_.active ? 1 : 0,
                 historical_.complete ? 1 : 0, historical_.targetX, historical_.targetY, historical_.rewardExp,
                 historical_.rewardGold, historical_.targetId, historical_.targetCount, historical_.have);
    for (const auto& skill : learnedSkills_) std::fprintf(file, "%d,%d;", skill.first, skill.second);
    std::fputc('|', file);
    for (const Stack& stack : temp_) std::fprintf(file, "%d,%d;", stack.id, stack.count);
    std::fprintf(file, "|%d,%d,%d,%d,%d,%d,%d,%d", skills_[0].id, skills_[1].id, skills_[2].id,
                 skills_[3].id,skills_[4].id,skills_[5].id,skills_[6].id,skills_[7].id);
    std::fputc('|', file);
    for (const MissionItem& item : historical_.neededItems) std::fprintf(file, "%d,%d;", item.id, item.count);
    std::fputc('|', file);
    for (const MissionItem& item : historical_.rewardItems) std::fprintf(file, "%d,%d;", item.id, item.count);
    std::fprintf(file, "|%d|%.3f", bagCapacity_, static_cast<double>(std::max(0.f, player_.homeY)));
    // Conteudo anterior preservado; readers antigos ignoram os campos novos.
    std::fprintf(file, "|Y5:%s|T5:%s|W1:%s", item_instance::encodeStacks(bag_).c_str(),
                 item_instance::encodeStacks(temp_).c_str(), item_instance::encodeWorn(wornUid_).c_str());
    std::fprintf(file,"|Z1:%s|ZT1:%s",forge_instance::encode(bag_).c_str(),forge_instance::encode(temp_).c_str());
    std::fprintf(file,"|Q2:%s",encodeAdditionalMissions().c_str());
    std::fprintf(file,"|QH2:%s,%s",questHex(historical_.title).c_str(),questHex(historical_.objective).c_str());
    // Versioned optional field; old positional fields/Q2/QH2 are untouched.
    std::fputs("|Q3:",file);
    auto writeBinding=[&](const HistoricalProgress& m) {
        if(m.id>0 && m.active && m.turnInRequired && !m.turnInNpc.empty())
            std::fprintf(file,"%d~%s;",m.id,questHex(m.turnInNpc).c_str());
    };
    writeBinding(historical_);
    for(const auto& m:additionalMissions_)writeBinding(m);
    // 05-AQ: optional Q4 mission-local flag state, checked at NPC turn-in.
    // No existing field moves; old clients can ignore this tail field.
    auto encodeFlags=[](const std::vector<int>& flags) {
        if(flags.empty())return std::string("-");
        std::string value;
        for(int flag:flags) {
            if(!value.empty())value.push_back('.');
            value+=std::to_string(flag);
        }
        return value;
    };
    std::fputs("|Q4:",file);
    auto writeFlags=[&](const HistoricalProgress& m) {
        if(m.id>0 && m.active && m.turnInRequired &&
           (!m.flags.empty() || !m.requiredFlags.empty() || !m.forbiddenFlags.empty()))
            std::fprintf(file,"%d~%s~%s~%s;",m.id,encodeFlags(m.flags).c_str(),
                         encodeFlags(m.requiredFlags).c_str(),encodeFlags(m.forbiddenFlags).c_str());
    };
    writeFlags(historical_);
    for(const auto& m:additionalMissions_)writeFlags(m);
    // 05-AU: optional Q5 event counters; no changes to Q2/Q3/Q4.
    std::fputs("|Q5:",file);
    auto writeTriggers=[&](const HistoricalProgress& m) {
        if(!m.active || !m.turnInRequired || m.triggers.empty() ||
           m.triggers.size()!=m.triggerCounts.size())return;
        std::fprintf(file,"%d~",m.id);
        for(std::size_t i=0;i<m.triggers.size();++i) {
            if(i)std::fputc('.',file);
            std::fprintf(file,"%d=%d",m.triggers[i].triggerId,m.triggerCounts[i]);
        }
        std::fputc(';',file);
    };
    writeTriggers(historical_);
    for(const auto& m:additionalMissions_)writeTriggers(m);
    // Optional tail fields: preserve all legacy pipe offsets and quest tags.
    std::fputs("|B1:", file);
    for (const Stack& stack : bank_) std::fprintf(file, "%d,%d;", stack.id, stack.count);
    std::fprintf(file,"|BI4:%s|ZB1:%s",item_instance::encodeStacks(bank_).c_str(),forge_instance::encode(bank_).c_str());
    std::fprintf(file,"|LS1:%d;", lifeSkillPoints_);
    for (const auto& skill : lifeSkills_) std::fprintf(file, "%d,%d;", skill.first, skill.second);
    std::fprintf(file,"|FM1:%d",std::clamp(fame_,0,99999999));
    std::fprintf(file,"|FC1:%d,%d",fairyCoinChestDay_,std::clamp(fairyCoinChestOpened_,0,1000000));
    std::fputs("|BT1:", file);
    for (int boatId : ownedBoatIds_) std::fprintf(file, "%d;", boatId);
    std::fputs("|BT2:", file);
    for (int boatId : ownedBoatIds_)
        std::fprintf(file, "%d,%d,%d;", boatId, boatHp(boatId), boatSp(boatId));
    std::fputs("|GR1:", file);
    for (int recordId : historicalRecords_) std::fprintf(file, "%d;", recordId);
    std::fprintf(file,"|HA1:%d",std::max(0,hairStyleItemId_));
    bool okay = save_recovery::appendChecksum(file) &&
                std::fflush(file) == 0 && !std::ferror(file);
    if (okay && ::fsync(::fileno(file)) != 0) okay = false;
    if (std::fclose(file) != 0) okay = false;
    if (!okay) { ::unlink(temporaryPath.c_str()); return failed(); }
    // Do not replace a known-good backup with the corrupt primary from which
    // this session was recovered. The new primary becomes the next recovery
    // point only after the durable commit succeeds.
    const bool sameRecoveredPath = saveRecoveredFromBackup_ && saveRecoveryPath_ == path;
    if (!sameRecoveredPath && !save_recovery::preservePrevious(path)) {
        ::unlink(temporaryPath.c_str());
        return failed();
    }
    if (::rename(temporaryPath.c_str(), path.c_str()) != 0) {
        ::unlink(temporaryPath.c_str());
        return failed();
    }
    const std::size_t slash = path.find_last_of('/');
    const std::string parent = slash == std::string::npos ? "." :
                               (slash == 0 ? "/" : path.substr(0, slash));
    const int dirFd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (dirFd < 0) return failed();
    const bool durable = ::fsync(dirFd) == 0;
    ::close(dirFd);
    if (!durable) return failed();
    saveFailureReported_ = false;
    if (sameRecoveredPath) {
        saveRecoveredFromBackup_ = false;
        saveRecoveryPath_.clear();
    }
    return true;
}

std::string Play::savePath(const std::string& root) const {
    const std::string& base = root.empty() ? root_ : root;
    const int identity = profileId_ > 0 ? profileId_ : player_.id;
    return save_paths::character(base, identity);
}

void Play::ensureWorldFoeRow(int id) {
    if (id <= 0 || summonMonsterRows_.count(id)) return;
    std::unordered_map<int, std::vector<std::string>> rows;
    loadRows(root_ + "/scripts/table/characterinfo.txt", std::unordered_set<int>{id}, rows);
    auto found = rows.find(id);
    if (found != rows.end()) summonMonsterRows_.emplace(id, std::move(found->second));
}

Play::Foe Play::createWorldFoe(const MonsterSlot& slot, const ActorSkin& actors, int ordinal) {
    Foe foe;
    foe.id = slot.id;
    foe.spawnKey = slot.passive ? 0 : slot.spawnKey;
    foe.respawnSeconds = slot.respawnSeconds;
    foe.x = slot.x;
    foe.y = slot.y;
    foe.homeX = slot.x;
    foe.homeY = slot.y;
    // CreateCha guarda a direcao inicial em graus; a IA usa radianos.
    // NPCs passivos continuam usando a orientacao historica do ActorSkin.
    foe.yaw = slot.yaw * 0.017453292519943295f;
    foe.name = "Monstro " + std::to_string(slot.id);
    const auto rowIt = summonMonsterRows_.find(slot.id);
    if (rowIt != summonMonsterRows_.end()) {
        const std::vector<std::string>& row = rowIt->second;
        const std::string name = asciiOf(cell(row, 1));
        if (name.size() >= 3) foe.name = name;
        foe.level = std::max(1, cellInt(row, 60, 1));
        // characterinfo: Lv 60, HP 61, EXP 62, ATK 65/66,
        // DEF 68, HIT 69, FLEE 70. Nao reduzir o HP de bosses.
        const auto stats = historical_stats::monsterFromColumns(
            cellInt(row, 60, 1), cellInt(row, 61, 20), cellInt(row, 62, 0),
            cellInt(row, 65, 0), cellInt(row, 66, 0), cellInt(row, 68, 0),
            cellInt(row, 69, 0), cellInt(row, 70, 0),
            cellInt(row, 67, 0), cellInt(row, 71, 0));
        foe.level = stats.level;
        foe.maxHp = stats.hp;
        foe.hp = stats.hp;
        foe.minAtk = stats.minAttack;
        foe.maxAtk = stats.maxAttack;
        foe.def = stats.defense;
        foe.hit = stats.hit;
        foe.dodge = stats.dodge;
        foe.resist = stats.physicalResistance;
        foe.crit = stats.criticalPercent;
        foe.con = std::max(0, cellInt(row, 83, 0));
        foe.sta = std::max(0, cellInt(row, 84, 0));
        foe.job = std::max(0, cellInt(row, 89, 0));
        // A maioria dos registros tem EXP=0, embora o servidor original
        // atribua recompensas em outros pontos. Preservar ganho offline
        // provisório para não bloquear a progressão do personagem.
        foe.expReward = stats.experience > 0 ? stats.experience : -1;
        // Centimetros da ficha, o mesmo lADis / lVision / lCDis do servidor.
        int dist = cellInt(row, 76, 0);
        if (dist <= 0) dist = 100;
        foe.range = static_cast<float>(dist) / 100.f;
        foe.radius = static_cast<float>(std::max(0, cellInt(row, 32, 40))) / 100.f;
        foe.vision = static_cast<float>(std::max(0, cellInt(row, 55, 1000))) / 100.f;
        foe.leash = static_cast<float>(std::max(0, cellInt(row, 77, 1500))) / 100.f;
        foe.ai = cellInt(row, 53, 1);
        // characterinfo: Skill ID (44), Skill Rate (45). Parear por
        // indice, descartar -1/passivas/buffs e usar o alcance de cada
        // habilidade ofensiva do skillinfo, nao o alcance do autoataque.
        std::vector<int> attackIds, attackRates;
        commaInts(cell(row, 44), attackIds);
        commaInts(cell(row, 45), attackRates);
        for (std::size_t k = 0; k < attackIds.size() && k < 16; ++k) {
            if (k >= attackRates.size() || attackRates[k] <= 0 || attackRates[k] > 10000) continue;
            const auto rule = skillRules_.find(attackIds[k]);
            if (rule == skillRules_.end() || rule->second.helpful != 0 ||
                rule->second.type == 2 || rule->second.target != 4) continue;
            const monster_skills::Attack attack{attackIds[k], attackRates[k],
                static_cast<float>(rule->second.distance) / 100.f,
                rule->second.flightSpeed};
            if (monster_skills::valid(attack)) foe.attacks.push_back(attack);
        }
        int spd = cellInt(row, 78, 0);
        foe.speed = static_cast<float>(std::max(0, spd)) / 100.f;
        if (foe.speed > 8.f) foe.speed = 8.f;
        int atk = cellInt(row, 75, 1400);
        foe.cd = static_cast<float>(atk) / 1000.f;
        if (foe.cd < 0.7f) foe.cd = 0.7f;
        if (foe.cd > 2.2f) foe.cd = 2.2f;
        std::vector<int> dropIds;
        std::vector<int> dropRates;
        commaInts(cell(row, 46), dropIds);
        commaInts(cell(row, 47), dropRates);
        for (std::size_t d = 0; d < dropIds.size(); ++d) {
            Foe::Drop drop;
            drop.id = dropIds[d];
            drop.rate = d < dropRates.size() ? dropRates[d] : 0;
            if (drop.id > 0 && drop.rate > 0) foe.drops.push_back(drop);
        }
    }
    for (int action = 1; action <= 54; ++action) {
        const auto timing = actors.monsterActionTiming(ordinal, action);
        foe.actionDurations[action] = timing.duration;
        foe.actionImpacts[action] = timing.impact;
    }
    // Nao reviver monstros simplesmente por trocar o setor de streaming.
    // O registro por mapa e spawn identifica inclusive multiplos do mesmo ID.
    if (foe.spawnKey != 0)
        worldMonsters_.restore(worldMap_, foe.spawnKey, worldClock_, foe.maxHp,
                               foe.homeX, foe.homeY, foe.respawnSeconds,
                               foe.hp, foe.x, foe.y, foe.yaw, foe.deadFor);
    return foe;
}

bool Play::bind(const std::string& clientRoot, const ActorSkin& actors, int profileId,
                const std::string& profileName) {
    // Troca de personagem inicia uma simulacao propria, sem herdar derrotas.
    if (player_.id > 0 && player_.id != actors.playerId()) {
        worldMonsters_.clear();
        worldClock_ = 0.;
    }
    pendingTicketTravel_.reset();
    pendingAltarMaterialUid_ = 0;
    pendingCloakUpgradeUid_ = 0;
    pendingApparelConverterUid_ = 0;
    usingTicketBoat_ = false;
    hereSafeZone_ = false;
    fairyCoinChestDay_ = 0;
    fairyCoinChestOpened_ = 0;
    itemCooldownRemaining_.clear();
    foodStates_.fill(FoodState{});
    itemStrengthState_ = ItemAttributeState{};
    itemConstitutionState_ = ItemAttributeState{};
    itemAgilityState_ = ItemAttributeState{};
    itemAccuracyState_ = ItemAttributeState{};
    itemSpiritState_ = ItemAttributeState{};
    itemPotionStrengthState_ = ItemAttributeState{};
    itemPotionAgilityState_ = ItemAttributeState{};
    itemPotionConstitutionState_ = ItemAttributeState{};
    itemPotionAccuracyState_ = ItemAttributeState{};
    itemPotionSpiritState_ = ItemAttributeState{};
    itemPotionMspdState_ = ItemAttributeState{};
    itemPotionAttackState_ = ItemAttributeState{};
    itemPotionAspdState_ = ItemAttributeState{};
    itemPotionHitState_ = ItemAttributeState{};
    itemPotionMaxHpState_ = ItemAttributeState{};
    itemKaraVictoryState_ = ItemAttributeState{};
    itemLotteryDefenseState_ = ItemAttributeState{};
    itemLanternDefenseState_ = ItemAttributeState{};
    itemFairyMarchState_ = ItemAttributeState{};
    itemMaliciousHornState_ = ItemAttributeState{};
    itemInnocenceState_ = ItemAllAttributeState{};
    itemForgeBoostFor_ = 0.f;
    itemForgeBoostLevel_ = 0;
    itemCombineBoostFor_ = 0.f;
    itemCombineBoostLevel_ = 0;
    experienceBoost_ = ItemBoostState{};
    dropRateBoost_ = ItemBoostState{};
    fairyGrowthBoost_ = ItemBoostState{};
    root_ = clientRoot;
    loadBoatBuildCatalog(clientRoot);
    itemRandomRewards_.load(clientRoot + "/scripts/offline/script/calculate/ItemEffect.lua",
                            clientRoot + "/scripts/offline/calculate/variable.lua");
    historicalChestRules_.load(clientRoot + "/scripts/offline/script/calculate/ItemEffect.lua");
    itemStatRules_.load(clientRoot + "/scripts/offline/calculate/variable.lua");
    itemProductionRules_.load(clientRoot + "/scripts/offline/script/calculate/variable.lua");
    itemGemVoucherRules_.load((clientRoot + "/scripts/offline/script/calculate/ItemEffect.lua").c_str());
    profileId_ = profileId > 0 ? profileId : actors.playerId();
    profileName_ = profileName;
    saveFailureReported_ = false;
    gemCatalog_ = forge_rules::load(clientRoot+"/scripts/offline/calculate/variable.lua");
    socketConfig_ = socket_rules::load(clientRoot+"/scripts/offline/calculate/variable.lua");
    restoredSp_ = -1.f;
    vendorOn_ = false;
    shopOpen_ = false;
    vendorStock_.clear();
    npcTalkFor_ = 0.f;
    npcTalkOpen_ = false;
    npcOffersTrade_ = false;
    npcOffersForge_ = false;
    offer_ = MissionOffer{};
    npcTalkName_.clear();
    npcTalkText_.clear();
    historical_ = HistoricalProgress{};
    additionalMissions_.clear();
    npcMissionGates_.clear();npcMissionFlagRules_.clear();
    journalSelectedId_=0; journalPage_=0;
    skillBooks_.clear();
    lifeSkillBooks_.clear();
    lifeSkillIds_.clear();
    skillRules_.clear();
    learnedSkills_.clear();
    lifeSkills_.clear();
    lifeSkillPoints_ = 0;
    effectEvents_.clear();
    scheduledEffects_.clear();
    armedSkill_ = -1;
    skillTestMode_ = false;
    fairySystemTimer_ = 0.f;
    fairySystemUid_ = 0;
    requestedSkill_ = -1;
    pendingBasicStrike_ = false;
    berserkFor_ = 0.f;
    berserkAttackSpeedBonus_ = 0.f;
    hardenFor_ = 0.f;
    hardenDefense_ = 0;
    itemHardenFor_ = 0.f;
    itemHardenDefense_ = 0;
    itemEtherCloverFor_ = 0.f;
    itemEtherCloverDodge_ = 0;
    itemBodyStateId_ = 0;
    itemBodyStateFor_ = 0.f;
    itemBlessedPotionFor_ = 0.f;
    itemBodyMoveSpeedBonus_ = 0.f;
    itemBodyAttackBonus_ = 0;
    itemBodyDefenseBonus_ = 0;
    willOfSteelFor_ = 0.f;
    willOfSteelDefense_ = 0;
    genericBuffFor_ = 0.f;
    playerNumbFor_ = 0.f;
    playerNumbLevel_ = 0;
    playerStunFor_ = 0.f;
    playerPoisonFor_ = 0.f;
    playerPoisonTick_ = 0.f;
    playerPoisonLevel_ = 0;
    genericBuffAttack_ = 0;
    genericBuffDefense_ = 0;
    for (float& cooldown : skillCd_) cooldown = 0.f;
    for (SkillDef& skill : skills_) skill = {};
    nextSkillSlot_ = 0;
    hotbarLoaded_ = false;
    foes_.clear();
    summonMonsterRows_.clear();
    loots_.clear();
    requestedLoot_=-1;
    pops_.clear();
    target_ = -1;
    // Cada save e de um personagem. Sem este reset, trocar de personagem sem
    // save mantinha a bolsa, o ouro e a classe do anterior.
    bag_.clear();
    bagCapacity_ = inventory_rules::kBaseBagCapacity;
    worn_.clear();
    wornUid_.clear();
    nextItemUid_ = 1;
    temp_.clear();
    gold_ = 0;
    exp_ = 0;
    job_ = 0;
    ap_ = 0;
    tp_ = 0;
    bstr_ = bagi_ = bdex_ = bcon_ = bsta_ = 5;
    sheetReady_ = false;
    sitting_ = false;
    deadFor_ = 0.f;
    swing_ = 0.f;
    pendingAttack_ = 0.f;
    pendingTarget_ = -1;
    pendingSkill_ = -1;
    attackSequence_ = 0;
    attackActionBase_ = 7;
    castingActionBase_ = 0;
    castingFor_ = 0.f;
    swingIsSkill_ = false;
    playerFightArea_ = true;
    attackWindup_ = actors.playerAttackWindup();
    attackDuration_ = actors.playerAttackDuration();
    regen_ = 0.f;
    makeQuests();
    const std::string table = clientRoot + "/scripts/table/characterinfo.txt";
    loadSkillBooks((clientRoot + "/scripts/offline/calculate/variable.lua").c_str());
    loadSkillBooks("Client/scripts/offline/calculate/variable.lua");
    loadLifeSkillBooks((clientRoot + "/scripts/offline/script/calculate/variable.lua").c_str());
    loadLifeSkillBooks("Client/scripts/offline/script/calculate/variable.lua");
    loadSkillRules((clientRoot + "/scripts/table/skillinfo.txt").c_str());
    loadSkillRules("Client/scripts/table/skillinfo.txt");
    actionTimingMode_ = -1;
    syncActionTiming(actors);

    const int chosenPlayer = actors.playerId();
    const int count = actors.monsterCount();
    std::vector<MonsterSlot> slots;
    std::unordered_set<int> wanted;
    wanted.insert(chosenPlayer);
    for (int i = 0; i < count; ++i) {
        slots.push_back(actors.monsterSlot(i));
        wanted.insert(slots.back().id);
    }
    // ItemEffect.lua summon callbacks: random ItemUse_MonsterSummon pools,
    // guardians, the medical report creature, and fixed summon-scroll targets.
    // Cache rows for on-demand model/stat creation.
    // They do not become active world spawns here.
    for (const auto range : {std::pair{40,80}, std::pair{98,146}, std::pair{194,241}, std::pair{500,571}})
        for (int id = range.first; id <= range.second; ++id) wanted.insert(id);
    wanted.insert(789);
    wanted.insert(857);
    wanted.insert(858);
    wanted.insert(1009);
    // Fixed historical summon scroll callbacks: commander, captain, fox spirit,
    // and Black Dragon Phantom. Cache their character rows on demand.
    for (const int id : {757, 761, 786, 952}) wanted.insert(id);
    // Evil Generator tiers 1..4: exact candidate IDs from ItemEffect.lua.
    for (const int id : {841,842,843,845,846,229,274,847,848,849,850,851,852,
                         211,706,673,690,691,692,693,106,289,679,678,707,708,
                         776,74,805,807,788}) wanted.insert(id);
    // Seasonal pet scrolls HJZHQ/MLZHQ/XRZHQ/SDZHQ use these follower rows.
    for (const int id : {929, 930, 931, 932}) wanted.insert(id);
    std::unordered_map<int, std::vector<std::string>> rows;
    loadRows(table, wanted, rows);
    for (int id : wanted) {
        if (id == chosenPlayer) continue;
        const auto row = rows.find(id);
        if (row != rows.end()) summonMonsterRows_.emplace(id, row->second);
    }

    // Origem: Client/scripts/offline/calculate/variable.lua DEXP[1..100].
    // DEXP e cumulativa no servidor; exp_ no save offline e relativa ao nivel.
    experience_.load(clientRoot + "/scripts/offline/calculate/variable.lua");
    player_.id = chosenPlayer;
    player_.cdLeft = 0.f;
    const auto playerRow = rows.find(chosenPlayer);
    if (playerRow != rows.end()) {
        const std::vector<std::string>& cols = playerRow->second;
        static const char* names[] = {"", "Lance", "Carsise", "Phyllis", "Ami"};
        player_.name = names[chosenPlayer >= 1 && chosenPlayer <= 4 ? chosenPlayer : 0];
        player_.level = std::max(1, cellInt(cols, 60, 1));
        player_.maxHp = std::max(1, cellInt(cols, 61, 40));
        player_.hp = player_.maxHp;
        player_.minAtk = cellInt(cols, 65, 4);
        player_.maxAtk = cellInt(cols, 66, 5);
        player_.def = cellInt(cols, 68, 3);
        player_.resist = std::max(0, cellInt(cols, 67, 0));
        player_.crit = std::max(0, cellInt(cols, 71, 1));
        int sp = cellInt(cols, 63, 18);
        player_.deadFor = static_cast<float>(std::max(0, sp));
        player_.range = 1.6f;
        player_.radius = static_cast<float>(std::max(0, cellInt(cols, 32, 40))) / 100.f;
        player_.speed = static_cast<float>(std::max(0, cellInt(cols, 78, 480))) / 100.f;
        player_.cd = 0.85f;
    } else {
        player_.level = 1;
        player_.maxHp = 40;
        player_.hp = 40;
        player_.minAtk = 4;
        player_.maxAtk = 5;
        player_.def = 3;
        player_.resist = 0;
        player_.crit = 1;
        player_.deadFor = 18.f;
        player_.cd = 0.85f;
        player_.range = 1.6f;
        player_.radius = 0.4f;
        player_.speed = 4.8f;
    }
    if (!profileName_.empty()) player_.name = profileName_;
    baseResist_ = player_.resist;
    baseLuck_ = playerRow != rows.end() ? std::max(0, cellInt(playerRow->second, 85, 5)) : 5;
    baseMoveSpeed_ = player_.speed;
    // deadFor guarda o SP maximo ate o primeiro step. O SP corrente fica em cd do jogador? Nao.
    // Uso homeX como SP max e homeY como SP corrente para nao abrir outro campo no Foe do jogador.
    player_.homeX = player_.deadFor;
    player_.homeY = player_.deadFor;
    player_.deadFor = 0.f;
    expNext_ = experience_.needed(player_.level);
    for (const MonsterSlot& slot : slots) {
        if (summonMonsterRows_.find(slot.id) == summonMonsterRows_.end()) {
            const auto row = rows.find(slot.id);
            if (row != rows.end()) summonMonsterRows_.emplace(slot.id, row->second);
        }
        foes_.push_back(createWorldFoe(slot, actors, static_cast<int>(foes_.size())));
    }
    // Tabela inteira: a bolsa guarda itens que nao caem de monstro, e o icone
    // e a coluna 2 (texture/icon/%s.png), como CItemRecord::GetIconFile.
    loadItems(clientRoot + "/scripts/table/iteminfo.txt", {});
    resolveNamedSkillBooks();
    loadHeals((clientRoot + "/scripts/offline/calculate/variable.lua").c_str());
    loadHeals("Client/scripts/offline/calculate/variable.lua");
    loadFairyRations(clientRoot + "/scripts/offline/script/calculate/variable.lua");
    loadFairyRations("Client/scripts/offline/script/calculate/variable.lua");
    loadFairyEggs(clientRoot + "/scripts/offline/script/calculate/variable.lua");
    loadFairyEggs("Client/scripts/offline/script/calculate/variable.lua");
    loadFairySkills(clientRoot + "/scripts/offline/script/calculate/variable.lua");
    loadFairySkills("Client/scripts/offline/script/calculate/variable.lua");
    loadFairyTrainingBooks(clientRoot + "/scripts/offline/script/calculate/variable.lua");
    loadFairyTrainingBooks("Client/scripts/offline/script/calculate/variable.lua");
    loadFairyFruits(clientRoot + "/scripts/offline/script/calculate/variable.lua");
    loadFairyFruits("Client/scripts/offline/script/calculate/variable.lua");
    // Skills only appear on the bar after being learned or restored from a save.
    for (SkillDef& skill : skills_) skill = {};
    // Save do personagem. O arquivo unico antigo (offline-save.txt) so e lido
    // pelo Lance e so enquanto ele ainda nao tem o seu proprio.
    std::string characterSavePath = savePath(clientRoot);
    if (profileId_ == player_.id && player_.id == 1 && !fileExists(characterSavePath)) {
        const std::string legacy = clientRoot + "/user/offline-save.txt";
        if (fileExists(legacy)) characterSavePath = legacy;
    }
    const bool freshCharacter = !fileExists(characterSavePath);
    load(characterSavePath);
    // The historical ApparelConverter output IDs are referenced by Lua but
    // absent from this client's iteminfo.txt. Rebuild only those saved apparel
    // records from their persisted FuseID source equipment, after the full
    // item table and save have both been loaded.
    for (const auto* collection : {&bag_, &temp_, &bank_}) {
        for (const Stack& stack : *collection) {
            if (stack.fuseId > 0) ensureApparelInfo(stack.id, stack.fuseId);
        }
    }
    // 05-AO: WorldMission 1 em EudemonScript.lua; Senna conclui com 6 EXP.
    // Nao atribuir de novo a saves existentes nem importar automaticamente
    // em personagens antigos; a missao fica no formato historico ja salvo.
    if (freshCharacter && worldMap_=="garner" && player_.level==1 && historical_.id==0 && additionalMissions_.empty()) {
        historical_.id=1;
        historical_.title="Welcome";
        historical_.objective="Procure Newbie Guide - Senna em Argent City (2223, 2785)";
        historical_.targetX=2223.f; historical_.targetY=2785.f;
        historical_.rewardExp=6; historical_.rewardGold=0;
        historical_.active=true; historical_.complete=false;
        journalSelectedId_=1;
    }
    // 05-W2: do not assign learned skills automatically to the hotbar.
    // Four-slot saves from older versions remain readable; new characters
    // start with eight empty slots until a skill is explicitly dragged.
    if (!sheetReady_) {
        sheetHp_ = std::max(1, player_.maxHp);
        sheetSp_ = std::max(1, static_cast<int>(player_.homeX));
        baseMin_ = player_.minAtk;
        baseMax_ = player_.maxAtk;
        baseDef_ = player_.def;
        ap_ = 4;
        tp_ = 0;
        for (int lv = 2; lv <= player_.level; ++lv) {
            ap_ += progression::attrPointAt(lv);
            tp_ += progression::skillPointAt(lv);
        }
        sheetReady_ = true;
    }
    applyAttrs(false);
    player_.homeY = restoredSp_ >= 0.f ? std::clamp(restoredSp_, 0.f, player_.homeX) : player_.homeX;
    if (player_.hp > player_.maxHp) player_.hp = player_.maxHp;
    if(freshCharacter && historical_.id==1 && historical_.active)
        save(savePath());
    say("Clique num monstro. O loot fica no chao. I abre a bolsa.");
    if (ap_ > 0) say("Pontos de atributo: " + std::to_string(ap_) + ". C abre o status.");
    // Mapas sem tabela de nascimento continuam validos e jogaveis.
    return true;
}

void Play::loadItems(const std::string& path, const std::vector<int>& ids) {
    std::unordered_set<int> want(ids.begin(), ids.end());
    std::vector<std::string> lines;
    if (!readLines(path, lines)) return;
    for (const std::string& row : lines) {
        if (row.size() < 2 || row[0] == '/') continue;
        const int id = std::atoi(row.c_str());
        if (items_.count(id)) continue;
        if (!want.empty() && !want.count(id)) continue;
        const std::vector<std::string> cols = tabs(row);
        ItemInfo item;
        item.id = id;
        item.name = asciiOf(cell(cols, 1));
        if (item.name.size() < 2) item.name = "Item " + std::to_string(id);
        item.price = std::max(0, cellInt(cols, 22, 0));
        item.type = std::max(0, cellInt(cols, 10, 0));
        item.chInstance = cellInt(cols, 21, 0) != 0;
        item.maxEnergy = std::max(0, leadingInt(cell(cols, 75)));
        item.maxEnergyRange = std::max(item.maxEnergy, equipment_stats::secondValue(cell(cols, 75)));
        item.maxStack = std::clamp(cellInt(cols, 20, 1), 1, 99);
        item.maxDurability = std::max(0, leadingInt(cell(cols, 76)));
        item.maxDurabilityRange = std::max(item.maxDurability, equipment_stats::secondValue(cell(cols, 76)));
        item.maxSockets = std::clamp(cellInt(cols, 77, 0), 0, 3);
        commaInts(cell(cols, 28), item.equipSlots);
        item.equipSlots.erase(std::remove_if(item.equipSlots.begin(), item.equipSlots.end(),
                                   [](int slot){ return slot < 0 || slot > 30; }), item.equipSlots.end());
        commaInts(cell(cols, 29), item.requiredEquipSlots);
        item.requiredEquipSlots.erase(std::remove_if(item.requiredEquipSlots.begin(), item.requiredEquipSlots.end(),
                                   [](int slot){ return slot < 0 || slot > 30; }), item.requiredEquipSlots.end());
        item.level = std::max(0, cellInt(cols, 24, 0));
        commaInts(cell(cols, 23), item.races);
        commaInts(cell(cols, 25), item.jobs);
        item.slot = leadingInt(cell(cols, 28));
        const std::string icon = cell(cols, 2);
        if (icon.size() >= 2 && icon != "0") item.icon = iconPathFor(path, icon);
        item.minAtk = std::max(0, leadingInt(cell(cols, 60)));
        item.maxAtk = std::max(0, leadingInt(cell(cols, 61)));
        item.def = std::max(0, leadingInt(cell(cols, 62)));
        item.stats = equipment_stats::fromColumns(cols);
        item.statsMaximum = equipment_stats::maximumFromColumns(cols);
        item.scriptAction = cell(cols, 86);
        item.medkit = item.scriptAction == "ItemMedKit";
        items_[id] = item;
    }
}

bool Play::ensureApparelInfo(int apparelId, int sourceEquipmentId) {
    const auto existing = items_.find(apparelId);
    if (existing != items_.end()) return existing->second.maxStack == 1;
    const auto source = items_.find(sourceEquipmentId);
    if (apparelId < 5287 || apparelId > 5297 || source == items_.end() ||
        source->second.slot < 0 || source->second.equipSlots.empty()) return false;

    // PORT_APPROX: client iteminfo.txt omits historical apparel IDs 5287-5297.
    // The Lua mapping and FuseID behavior are historical; slot restrictions
    // and icon are inherited to keep the item usable, while apparel bonuses,
    // icon art, and localized output name remain unknown.
    ItemInfo apparel = source->second;
    apparel.id = apparelId;
    apparel.name = "PORT_APPROX: " + source->second.name + " Apparel";
    apparel.price = 0;
    apparel.chInstance = false;
    apparel.maxEnergy = 0;
    apparel.maxEnergyRange = 0;
    apparel.maxDurability = 25000;
    apparel.maxDurabilityRange = 25000;
    apparel.maxSockets = 0;
    apparel.maxStack = 1;
    apparel.minAtk = 0;
    apparel.maxAtk = 0;
    apparel.def = 0;
    apparel.stats = {};
    apparel.statsMaximum = {};
    apparel.healHp = 0;
    apparel.healSp = 0;
    apparel.healHpPercent = 0.0;
    apparel.healSpPercent = 0.0;
    apparel.healCooldownSeconds = 0;
    apparel.healStateId = 0;
    apparel.healStateLevel = 0;
    apparel.healStateSeconds = 0;
    apparel.fairyRationAmount = 0;
    apparel.fairyEggTargetId = 0;
    apparel.fairySkillId = 0;
    apparel.fairySkillLevel = 0;
    apparel.learnedSkillId = 0;
    apparel.learnedSkillLevel = 0;
    apparel.fairyFruitAttribute = -1;
    apparel.fairyFruitLevels = 0;
    apparel.fairyFruitImproved = false;
    apparel.medkit = false;
    apparel.scriptAction.clear();
    items_.emplace(apparelId, std::move(apparel));
    return true;
}

void Play::loadHeals(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    std::string line;
    int byte = 0;
    while ((byte = std::fgetc(file)) != EOF) {
        if (byte != '\n') {
            line.push_back(static_cast<char>(byte));
            continue;
        }
        const char* key = std::strstr(line.c_str(), "Restore.Life[");
        if (key) {
            const int id = std::atoi(key + 13);
            const auto it = items_.find(id);
            if (it != items_.end() && it->second.medkit) {
                auto numberAfter = [](const char* begin, const char* field, double fallback) {
                    const char* value = begin ? std::strstr(begin, field) : nullptr;
                    if (!value) return fallback;
                    value += std::strlen(field);
                    char* end = nullptr;
                    const double parsed = std::strtod(value, &end);
                    return end == value || !std::isfinite(parsed) ? fallback : parsed;
                };
                const char* amount = std::strstr(line.c_str(), "Amount");
                const char* percentage = std::strstr(line.c_str(), "Percentage");
                if (amount) {
                    it->second.healHp = std::max(0, static_cast<int>(numberAfter(amount, "HP =", 0.0)));
                    it->second.healSp = std::max(0, static_cast<int>(numberAfter(amount, "SP =", 0.0)));
                }
                if (percentage) {
                    it->second.healHpPercent = std::max(0.0, numberAfter(percentage, "HP =", 0.0));
                    it->second.healSpPercent = std::max(0.0, numberAfter(percentage, "SP =", 0.0));
                }
                it->second.healStateId = std::max(0, static_cast<int>(
                    numberAfter(line.c_str(), "State =", 0.0)));
                it->second.healStateLevel = std::max(0, static_cast<int>(
                    numberAfter(line.c_str(), "StateLevel =", 0.0)));
                it->second.healStateSeconds = std::max(0, static_cast<int>(
                    numberAfter(line.c_str(), "StateTime =", 0.0)));
                it->second.healCooldownSeconds = std::max(0, static_cast<int>(
                    numberAfter(line.c_str(), "Cooldown =", 0.0)));
            }
        }
        line.clear();
    }
    std::fclose(file);
}

void Play::loadFairyRations(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    char raw[4096];
    while (std::fgets(raw, sizeof raw, file)) {
        const char* autoThreshold = std::strstr(raw, "Server.Fairy.AutoFeed.Stamina =");
        if (autoThreshold) {
            double value = 0.0;
            if (std::sscanf(autoThreshold, "Server.Fairy.AutoFeed.Stamina = %lf", &value) == 1 &&
                value >= 0.0 && value <= 1.0)
                fairyAutoFeedThreshold_ = value;
        }
        const char* normalId = std::strstr(raw, "Server.Fairy.AutoFeed.Normal =");
        if (normalId) {
            int value = 0;
            if (std::sscanf(normalId, "Server.Fairy.AutoFeed.Normal = %d", &value) == 1 && value > 0)
                fairyAutoFeedNormalId_ = value;
        }
        const char* greatId = std::strstr(raw, "Server.Fairy.AutoFeed.Great =");
        if (greatId) {
            int value = 0;
            if (std::sscanf(greatId, "Server.Fairy.AutoFeed.Great = %d", &value) == 1 && value > 0)
                fairyAutoFeedGreatId_ = value;
        }
        const char* begin = std::strstr(raw, "Server.Fairy.Ration[");
        if (!begin) continue;
        int itemId = 0;
        int amount = 0;
        if (std::sscanf(begin, "Server.Fairy.Ration[%d] = %d", &itemId, &amount) != 2 ||
            itemId <= 0 || amount <= 0) continue;
        const auto item = items_.find(itemId);
        if (item != items_.end() && item->second.scriptAction == "FairyRation")
            item->second.fairyRationAmount = amount;
    }
    std::fclose(file);
}

void Play::updateFairyAutoFeed() {
    if (usingTicketBoat_ || fairyAutoFeedSlot_ < 0 ||
        fairyAutoFeedSlot_ >= static_cast<int>(bag_.size())) return;
    const auto equipped = worn_.find(16);
    const auto equippedUid = wornUid_.find(16);
    if (equipped == worn_.end() || equippedUid == wornUid_.end()) return;
    const auto fairyDefinition = items_.find(equipped->second);
    if (fairyDefinition == items_.end() || fairyDefinition->second.type != 59) return;
    auto fairyStack = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& candidate) {
        return candidate.uid == equippedUid->second && candidate.id == equipped->second &&
               candidate.count == 1;
    });
    if (fairyStack == bag_.end()) return;
    const int maximum = fairyStack->maxDurability > 0 ? fairyStack->maxDurability :
                        fairyDefinition->second.maxDurability;
    if (maximum <= 0 || fairyStack->durability < 0 || fairyStack->durability >= maximum ||
        static_cast<double>(fairyStack->durability) / maximum > fairyAutoFeedThreshold_) return;
    Stack& ration = bag_[static_cast<std::size_t>(fairyAutoFeedSlot_)];
    if (ration.id != fairyAutoFeedNormalId_ && ration.id != fairyAutoFeedGreatId_) return;
    const auto rationDefinition = items_.find(ration.id);
    if (rationDefinition == items_.end() || rationDefinition->second.type != 57 ||
        rationDefinition->second.fairyRationAmount <= 0 || ration.count <= 0) return;

    const int previousStamina = fairyStack->durability;
    const long long restored = static_cast<long long>(rationDefinition->second.fairyRationAmount) * 50LL;
    fairyStack->maxDurability = maximum;
    fairyStack->durability = static_cast<int>(std::min<long long>(maximum,
        static_cast<long long>(previousStamina) + restored));
    const int currentStamina = fairyStack->durability;
    if (--ration.count <= 0) bag_.erase(bag_.begin() + fairyAutoFeedSlot_);
    say("Fairy auto feed: " + rationDefinition->second.name + " consumida; stamina " +
        std::to_string(previousStamina) + " -> " + std::to_string(currentStamina) + ".");
    if(sheetReady_)applyAttrs(false);
    save(savePath());
}

void Play::updateFairySystem(float dt) {
    if (!std::isfinite(dt) || dt <= 0.f) return;
    const auto equipped = worn_.find(16);
    const auto equippedUid = wornUid_.find(16);
    if (equipped == worn_.end() || equippedUid == wornUid_.end()) {
        fairySystemTimer_ = 0.f;
        fairySystemUid_ = 0;
        return;
    }
    const auto definition = items_.find(equipped->second);
    if (definition == items_.end() || definition->second.type != 59) {
        fairySystemTimer_ = 0.f;
        fairySystemUid_ = 0;
        return;
    }
    if (fairySystemUid_ != equippedUid->second) {
        fairySystemUid_ = equippedUid->second;
        fairySystemTimer_ = 0.f;
    }
    auto fairy = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& candidate) {
        return candidate.uid == equippedUid->second && candidate.id == equipped->second &&
               candidate.count == 1;
    });
    if (fairy == bag_.end()) return;
    const int level = std::accumulate(fairy->fairyStats.begin(), fairy->fairyStats.end(), 0);
    const int interval = 60 + std::max(0, level - 27) * 5; // FairyFreq(Level)
    fairySystemTimer_ += dt;
    bool changed = false;
    while (fairySystemTimer_ >= static_cast<float>(interval)) {
        fairySystemTimer_ -= static_cast<float>(interval);
        fairy = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& candidate) {
            return candidate.uid == equippedUid->second && candidate.id == equipped->second &&
                   candidate.count == 1;
        });
        if (fairy == bag_.end()) break;
        const int maximumEnergy = fairy->maxEnergy > 0 ? fairy->maxEnergy : definition->second.maxEnergy;
        const int maximumStamina = fairy->maxDurability > 0 ? fairy->maxDurability : definition->second.maxDurability;
        const int oldStamina = fairy->durability;
        int tick = fairy->fairySystemTick >= 1200 ? 1 : fairy->fairySystemTick + 1;
        if (level >= 0) {
            auto grantCoin = [&](int itemId) {
                const auto coin = items_.find(itemId);
                if (coin == items_.end() || coin->second.maxStack <= 0 ||
                    !addStack(bag_, itemId, 1, coin->second.maxStack, bagCapacity_)) return;
                auto granted = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& value) {
                    return value.id == itemId;
                });
                if (granted != bag_.end()) granted->quality = 4;
                assignItemUids();
                changed = true;
            };
            if (oldStamina >= 50) {
                if (tick % 3 == 0) grantCoin(855);
                if (tick % 30 == 0) grantCoin(2588);
                if (tick % 60 == 0) grantCoin(2588);
                if (tick % 120 == 0) grantCoin(2588);
                if (tick % 1200 == 0) grantCoin(2589);
            }
        }
        if (tick == 1200) tick = 0;
        fairy = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& candidate) {
            return candidate.uid == equippedUid->second && candidate.id == equipped->second &&
                   candidate.count == 1;
        });
        if (fairy == bag_.end()) break;
        fairy->fairySystemTick = tick;
        if (oldStamina > 49) {
            fairy->maxDurability = std::max(fairy->maxDurability, maximumStamina);
            fairy->durability = std::max(49, oldStamina - 50); // Take_ElfURE
            if (fairy->durability > 49 && maximumEnergy > 0) {
                fairy->maxEnergy = std::max(fairy->maxEnergy, maximumEnergy);
                const int growthRate = fairyExperienceRate_ *
                    (fairyGrowthBoost_.remaining > 0.f ? fairyGrowthBoost_.multiplierPercent : 100) / 100;
                fairy->energy = std::min(fairy->maxEnergy, fairy->energy + growthRate);
            }
        }
        changed = true;
    }
    if (changed) {
        if(sheetReady_)applyAttrs(false);
        save(savePath());
    }
}

void Play::loadFairyEggs(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    char raw[4096];
    while (std::fgets(raw, sizeof raw, file)) {
        const char* begin = std::strstr(raw, "Server.Fairy.Egg[");
        if (!begin) continue;
        int eggId = 0;
        int fairyId = 0;
        if (std::sscanf(begin, "Server.Fairy.Egg[%d] = {ID = %d", &eggId, &fairyId) != 2 ||
            eggId <= 0 || fairyId <= 0) continue;
        const auto egg = items_.find(eggId);
        if (egg != items_.end() && egg->second.scriptAction == "ItemUse_Egg")
            egg->second.fairyEggTargetId = fairyId;
    }
    std::fclose(file);
}

void Play::loadFairySkills(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    char raw[4096];
    while (std::fgets(raw, sizeof raw, file)) {
        const char* begin = std::strstr(raw, "Server.Fairy.Skill[");
        if (!begin) continue;
        int itemId = 0;
        int skillId = 0;
        int skillLevel = 0;
        if (std::sscanf(begin, "Server.Fairy.Skill[%d] = {Skill = %d, Level = %d",
                        &itemId, &skillId, &skillLevel) != 3 ||
            itemId <= 0 || skillId <= 0 || skillId > 99 || skillLevel <= 0 || skillLevel > 9) continue;
        const auto item = items_.find(itemId);
        if (item != items_.end() && item->second.scriptAction == "AddFairySkill" && item->second.type == 58) {
            item->second.fairySkillId = skillId;
            item->second.fairySkillLevel = skillLevel;
        }
    }
    std::fclose(file);
}

void Play::loadFairyTrainingBooks(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    char raw[4096];
    while (std::fgets(raw, sizeof raw, file)) {
        const char* begin = std::strstr(raw, "Server.Fairy.Possession.Skill[");
        if (!begin) begin = std::strstr(raw, "Server.Fairy.Funny.Skill[");
        if (!begin) continue;
        int itemId = 0;
        int skillId = 0;
        int skillLevel = 0;
        const char* bracket = std::strchr(begin, '[');
        if (!bracket || std::sscanf(bracket, "[%d] = {Skill = %d, Level = %d",
                        &itemId, &skillId, &skillLevel) != 3 ||
            itemId <= 0 || skillId <= 0 || skillLevel <= 0 || skillLevel > 9) continue;
        const auto item = items_.find(itemId);
        if (item != items_.end() && item->second.type == 34 &&
            (item->second.scriptAction == "AddFairyBody" || item->second.scriptAction == "AddFunnySkill")) {
            item->second.learnedSkillId = skillId;
            item->second.learnedSkillLevel = skillLevel;
        }
    }
    std::fclose(file);
}

void Play::loadFairyFruits(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    char raw[4096];
    while (std::fgets(raw, sizeof raw, file)) {
        int value = 0;
        if (std::sscanf(raw, "Server.Fairy.Level.Maximum = %d", &value) == 1 && value > 0 && value <= 99)
            fairyMaximumLevel_ = value;
        if (std::sscanf(raw, "Server.Fairy.Level.Normal = %d", &value) == 1 && value > 0 && value <= 99)
            fairyNormalFruitMaximum_ = value;
        if (std::sscanf(raw, "Server.Fairy.Level.Improved = %d", &value) == 1 && value > 0 && value <= 99)
            fairyImprovedFruitMaximum_ = value;
        if (std::sscanf(raw, "Server.Rates.Global.FairyEXP = %d", &value) == 1 && value > 0 && value <= 100000)
            fairyExperienceRate_ = value;

        const char* begin = std::strstr(raw, "Server.Fairy.Fruit[");
        if (!begin) continue;
        int itemId = 0;
        int levels = 0;
        char attribute[32]{};
        if (std::sscanf(begin, "Server.Fairy.Fruit[%d] = {LV = %d, ATTR = ITEMATTR_VAL_%31[A-Z]",
                        &itemId, &levels, attribute) != 3 || itemId <= 0 || levels <= 0 || levels > 10)
            continue;
        int attributeIndex = -1;
        if (std::strcmp(attribute, "STR") == 0) attributeIndex = 0;
        else if (std::strcmp(attribute, "CON") == 0) attributeIndex = 1;
        else if (std::strcmp(attribute, "AGI") == 0) attributeIndex = 2;
        else if (std::strcmp(attribute, "DEX") == 0) attributeIndex = 3;
        else if (std::strcmp(attribute, "STA") == 0) attributeIndex = 4;
        const auto item = items_.find(itemId);
        if (attributeIndex >= 0 && item != items_.end() && item->second.type == 58 &&
            (item->second.scriptAction == "FairyFruit" || item->second.scriptAction == "FairyFruitImprvd")) {
            item->second.fairyFruitAttribute = attributeIndex;
            item->second.fairyFruitLevels = levels;
            item->second.fairyFruitImproved = item->second.scriptAction == "FairyFruitImprvd";
        }
    }
    std::fclose(file);
}

void Play::loadSkillBooks(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    char raw[4096];
    while (std::fgets(raw, sizeof raw, file)) {
        const char* begin = std::strstr(raw, "Server.Player.Skill[");
        if (!begin) continue;
        int item = 0;
        int skill = 0;
        if (std::sscanf(begin, "Server.Player.Skill[%d] = {Skill = %d", &item, &skill) == 2 && item > 0 && skill > 0)
            skillBooks_[item] = skill;
    }
    std::fclose(file);
}

void Play::resolveNamedSkillBooks() {
    std::unordered_map<std::string,int> skillsByName;
    std::unordered_set<std::string> ambiguousNames;
    for (const auto& entry : skillRules_) {
        const std::string key = lowerCopy(entry.second.name);
        if (key.empty()) continue;
        const auto inserted = skillsByName.emplace(key, entry.first);
        if (!inserted.second && inserted.first->second != entry.first)
            ambiguousNames.insert(key);
    }
    for (const auto& entry : items_) {
        const ItemInfo& item = entry.second;
        if (item.scriptAction != "AddPlayerSkill" || item.type != 34 ||
            skillBooks_.count(item.id)) continue;
        const std::string key = lowerCopy(item.name);
        const auto skill = skillsByName.find(key);
        if (skill == skillsByName.end() || ambiguousNames.count(key)) continue;
        // PORT_APPROX: the Lua callback has no Server.Player.Skill row for
        // this book. A unique exact name match in iteminfo/skillinfo supplies
        // its ID; eligibility still uses the existing historical skill rule.
        skillBooks_[item.id] = skill->second;
        inferredSkillBooks_.insert(item.id);
    }
}

void Play::loadLifeSkillBooks(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    char raw[4096];
    while (std::fgets(raw, sizeof raw, file)) {
        const char* begin = std::strstr(raw, "Server.Life.Skill[");
        if (!begin) continue;
        LifeSkillBookRule rule;
        if (std::sscanf(begin, "Server.Life.Skill[%d] = {Skill = %d, AddLevel = %d, Level = %d",
                        &rule.itemId, &rule.skillId, &rule.addLevel, &rule.expectedLevel) != 4 ||
            rule.itemId <= 0 || rule.skillId <= 0 || rule.addLevel <= 0) continue;
        const char* prerequisite = std::strstr(begin, "Prerequisite = {Skill = ");
        if (prerequisite) {
            prerequisite += std::strlen("Prerequisite = {Skill = ");
            if (std::sscanf(prerequisite, "%d", &rule.prerequisiteSkill) != 1) rule.prerequisiteSkill = 0;
            const char* level = std::strstr(prerequisite, "Level = ");
            if (level) std::sscanf(level + std::strlen("Level = "), "%d", &rule.prerequisiteLevel);
        }
        const char* give = std::strstr(begin, "Give = ");
        if (give) std::sscanf(give + std::strlen("Give = "), "%d", &rule.rewardItem);
        lifeSkillBooks_[rule.itemId] = rule;
        lifeSkillIds_.insert(rule.skillId);
    }
    std::fclose(file);
}

void Play::loadSkillRules(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    std::string dataRoot = path;
    const std::string marker = "/scripts/table/skillinfo.txt";
    const std::size_t rootEnd = dataRoot.rfind(marker);
    if (rootEnd != std::string::npos) dataRoot.resize(rootEnd);
    const auto scalars = scalarSkillFormulas(dataRoot + "/scripts/offline/script/calculate/skilleffect.lua");
    char raw[16384];
    while (std::fgets(raw, sizeof raw, file)) {
        std::string row(raw);
        if (row.size() < 2 || row[0] == '/') continue;
        const std::vector<std::string> cols = tabs(row);
        if (cols.size() < 14) continue;
        SkillRule rule;
        rule.id = std::atoi(cols[0].c_str());
        if (rule.id <= 0) continue;
        rule.name = asciiOf(cols[1]);
        if (cols.size() > 67 && !cols[67].empty() && cols[67] != "0")
            rule.icon = "texture/icon/" + cols[67];
        std::size_t at = 0;
        while (at <= cols[3].size()) {
            const std::size_t end = cols[3].find(';', at);
            const std::string entry = cols[3].substr(at, end == std::string::npos ? std::string::npos : end - at);
            const int job = std::atoi(entry.c_str());
            if (job >= 0) rule.jobs.push_back(job);
            if (end == std::string::npos) break;
            at = end + 1;
        }
        rule.level = std::max(0, std::atoi(cols[11].c_str()));
        if (cols[12] != "-1") std::sscanf(cols[12].c_str(), "%d,%d", &rule.prerequisite, &rule.prerequisiteLevel);
        rule.points = std::max(0, std::atoi(cols[13].c_str()));
        rule.type = std::atoi(cols[9].c_str());
        rule.helpful = std::atoi(cols[10].c_str());
        rule.target = std::atoi(cols[17].c_str());
        rule.distance = std::max(0, std::atoi(cols[16].c_str()));
        rule.radius = std::max(0, std::atoi(cols[20].c_str()));
        if (cols.size() > 29) rule.effectFormula = cols[29];
        if (cols.size() > 30 && cols[30]!="0") rule.passiveFormula=cols[30];
        if (rule.effectFormula == "Skill_Sj_End") {
            rule.damageFormula = SkillRule::DamageFormula::Enfeeble;
            rule.damageScaleBase = 100.0;
            rule.damageScalePerLevel = 10.0;
        } else if (rule.effectFormula == "Skill_Melee_End" || rule.effectFormula == "Skill_Range_End" ||
            rule.effectFormula == "Skill_Staff_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            rule.damageScaleBase = 1.0;
        } else if (rule.effectFormula == "Skill_Hyz_End")
            rule.damageFormula = SkillRule::DamageFormula::IllusionSlash;
        else if (rule.effectFormula == "Skill_Zj_End")
            rule.damageFormula = SkillRule::DamageFormula::MightyStrike;
        else if (rule.effectFormula == "Skill_Swzq_End")
            rule.damageFormula = SkillRule::DamageFormula::PrimalRage;
        else if (rule.effectFormula == "Skill_Lzj_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            rule.damageScaleBase = 1.5;
            rule.damageScalePerLevel = 0.15;
        } else if (rule.effectFormula == "Skill_Bdj_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            rule.damageScaleBase = 1.2;
        } else if (rule.effectFormula == "Skill_Lxjy_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            rule.damageScaleBase = 0.5;
            rule.damageScalePerLevel = 0.1;
        } else if (rule.effectFormula == "Skill_Lh_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            rule.damageScaleBase = 1.0;
            rule.damageScalePerLevel = 0.05;
        } else if (rule.effectFormula == "Skill_Guz_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            rule.damageScaleBase = 1.0;
            rule.damageScalePerLevel = 0.1;
        } else if (rule.effectFormula == "Skill_Ldc_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            rule.damageScaleBase = 2.0;
            rule.damageScalePerLevel = 0.2;
        } else if (rule.effectFormula == "Skill_Ctd_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            rule.damageScaleBase = 1.0;
            rule.damageScalePerLevel = 0.2;
        } else if (rule.effectFormula == "Skill_Fer_End" ||
                   rule.effectFormula == "Skill_Xik_End" ||
                   rule.effectFormula == "Skill_Nt_End" ||
                   rule.effectFormula == "Skill_BlackDrgDeadHit_End" ||
                   rule.effectFormula == "Skill_Wzxf_End" ||
                   rule.effectFormula == "Skill_JBXZSB_End" ||
                   rule.effectFormula == "Skill_BLGJ_End") {
            // Monster callbacks with literal multipliers around Atk_Dmg.
            // Keep callback identity exact; unknown Lua remains Generic.
            rule.damageFormula = SkillRule::DamageFormula::PhysicalLinear;
            if (rule.effectFormula == "Skill_Fer_End") rule.damageScaleBase = 2.0;
            else if (rule.effectFormula == "Skill_Xik_End") rule.damageScaleBase = 20.0;
            else if (rule.effectFormula == "Skill_Nt_End" || rule.effectFormula == "Skill_Wzxf_End")
                rule.damageScaleBase = 3.0;
            else if (rule.effectFormula == "Skill_BlackDrgDeadHit_End" ||
                     rule.effectFormula == "Skill_BLGJ_End") rule.damageScaleBase = 1.5;
            else rule.damageScaleBase = 1.0;
        } else if (rule.effectFormula == "Skill_Zj_End" || rule.effectFormula == "Skill_Ymsl_End" ||
                   rule.effectFormula == "Skill_Ssd_End") {
            rule.damageFormula = SkillRule::DamageFormula::AttackRaise;
            if (rule.effectFormula == "Skill_Zj_End") {
                rule.damageScaleBase = 1.2;
                rule.damageScalePerLevel = 0.05;
            } else if (rule.effectFormula == "Skill_Ymsl_End") {
                rule.damageScaleBase = 1.5;
                rule.damageScalePerLevel = 0.1;
            } else {
                rule.damageScaleBase = 1.2;
                rule.damageScalePerLevel = 0.15;
            }
        } else if (rule.effectFormula == "Skill_Tj_End") {
            rule.damageFormula = SkillRule::DamageFormula::PhysicalPlus;
            rule.damageScaleBase = 1.0;
            rule.damageScalePerLevel = 0.05;
        } else if (rule.effectFormula == "Skill_Bc_End") {
            rule.damageFormula = SkillRule::DamageFormula::Backstab;
        } else if (rule.effectFormula == "Skill_Lj_End") {
            rule.damageFormula = SkillRule::DamageFormula::LightningBolt;
        } else if (rule.effectFormula == "Skill_Bkcj_End") {
            rule.damageFormula = SkillRule::DamageFormula::ConchRay;
        } else if (rule.effectFormula == "Skill_BlackHyz_End") {
            rule.damageFormula = SkillRule::DamageFormula::MonsterPhysicalDouble;
        } else if (rule.effectFormula == "Skill_hqgj_End") {
            rule.damageFormula = SkillRule::DamageFormula::MonsterPhysicalFloor20;
            rule.damageScaleBase = 1.5;
        } else if (rule.effectFormula == "Skill_wlfd_End" || rule.effectFormula == "Skill_wllk_End") {
            rule.damageFormula = SkillRule::DamageFormula::MonsterPhysicalFloor20;
        } else if (rule.effectFormula == "Skill_wlsj_End") {
            rule.damageFormula = SkillRule::DamageFormula::MonsterPhysicalPlus800;
        } else if (rule.effectFormula == "Skill_HyzHX_End") {
            rule.damageFormula = SkillRule::DamageFormula::MonsterAttackSpeed;
        } else if (rule.effectFormula == "Skill_Paodan_End") {
            // Fire_Dmg/Pow_Dmg plus the distance falloff in skilleffect.lua.
            rule.damageFormula = SkillRule::DamageFormula::MonsterFireDistance;
        } else if (rule.effectFormula == "Skill_Pj_End") {
            // Break Armor applies STATE_PJ only; it does not call a damage helper.
            rule.damageFormula = SkillRule::DamageFormula::StateOnly;
        }
        if (cols.size() > 30) rule.positiveFormula = cols[30];
        if (cols.size() > 31) rule.negativeFormula = cols[31];
        const std::string spFormula = cols[24];
        const std::string cooldownFormula = cols.size() > 44 ? cols[44] : std::string();
        const auto sp = scalars.find(spFormula);
        if (sp != scalars.end()) {
            rule.spBase = sp->second.base;
            rule.spPerLevel = sp->second.perLevel;
            rule.sp = std::max(0, static_cast<int>(std::floor(rule.spBase + rule.spPerLevel)));
        } else if (spFormula.rfind("SkillSp_", 0) == 0) {
            rule.spFormulaMissing = true;
            // Passivas e skills de monstro não consomem SP no personagem.
            if (rule.type == 2 || (cols.size() > 70 && cols[70].find("Monster Skill") != std::string::npos)) {
                rule.spBase = rule.sp = 0;
            }
        }
        const auto cooldown = scalars.find(cooldownFormula);
        if (cooldown != scalars.end()) {
            rule.cooldownBaseMs = cooldown->second.base;
            rule.cooldownPerLevelMs = cooldown->second.perLevel;
            rule.cooldown = std::max(.05f, static_cast<float>(rule.cooldownBaseMs + rule.cooldownPerLevelMs) / 1000.f);
            // Fire speed controls cyclic actions. Non-cyclic skills play
            // at 30 FPS in CWaitAttackState::_UseSkill, regardless of cooldown.
            rule.animationTimeBaseMs = cooldown->second.base;
            rule.animationTimePerLevelMs = cooldown->second.perLevel;
        } else if (cooldownFormula.rfind("SkillCooldown_", 0) == 0) {
            rule.cooldownFormulaMissing = true;
            if (rule.type == 2) rule.cooldownBaseMs = 0;
        }
        if (cols.size() > 45) rule.harmMode = std::atoi(cols[45].c_str());
        if (cols.size() > 64) rule.hitKeyFrame = std::atoi(cols[64].c_str());
        if (cols.size() > 66) rule.hasHitVisual = std::atoi(cols[63].c_str())>0 ||
            std::atoi(cols[65].c_str())>0 || std::atoi(cols[66].c_str())>0;
        if (cols.size() > 47) rule.playActions = parsePoseActions(cols[47]);
        if (cols.size() > 48) rule.actionKeyFrame = std::atoi(cols[48].c_str());
        auto readItemEffectKeyFrame = [&](std::size_t column) {
            if (cols.size() <= column) return;
            int effectId=0,keyFrame=-3;
            if (std::sscanf(cols[column].c_str(), "%d,%d", &effectId, &keyFrame)==2 &&
                effectId>0) rule.itemEffectKeyFrames.push_back(keyFrame);
        };
        readItemEffectKeyFrame(54);
        readItemEffectKeyFrame(55);
        if (cols.size() > 56) rule.flightActionKeyFrame = std::atoi(cols[56].c_str());
        if (cols.size() > 59) rule.flightEffectId = std::max(0, std::atoi(cols[59].c_str()));
        if (cols.size() > 60) rule.flightSpeed = std::max(0.f, std::min(300.f,
            static_cast<float>(std::atoi(cols[60].c_str())) / 100.f));
        if (cols.size() > 68) rule.cyclicAnimation = std::atoi(cols[68].c_str()) == 1;
        if (cols.size() > 70) rule.description = asciiOf(cols[70]);
        skillRules_[rule.id] = std::move(rule);
    }
    std::fclose(file);
}

equipment_stats::Bonus Play::gearBonuses() const {
    equipment_stats::Bonus total;
    for (const auto& equipped : worn_) {
        const auto it = items_.find(equipped.second);
        if (it == items_.end()) continue;
        equipment_stats::Bonus b = it->second.stats;
        // Manter compatibilidade com equipamentos de testes/saves que usam
        // os campos ATK/DEF legados, inclusive fixtures sem iteminfo completo.
        b.minAttack = it->second.minAtk;
        b.maxAttack = it->second.maxAtk;
        b.defense = it->second.def;
        const auto uid = wornUid_.find(equipped.first);
        if (uid != wornUid_.end()) {
            for (const Stack& stack : bag_) {
                if (stack.uid == uid->second && stack.id == equipped.second) {
                    if(it->second.type==59 && stack.durability<50) {
                        b={};break; // Take_ElfURE -> SetChaEquipValid(16,0)
                    }
                    b += stack.extra; // atributos individuais nao relacionados as gemas
                    // Fairy fruits persist their primary values separately
                    // from equipment extra rolls. ChangeItem adds these item
                    // attributes before the normal derived-stat formulas.
                    if(it->second.type==59) {
                        b.str+=stack.fairyStats[0];b.con+=stack.fairyStats[1];
                        b.agi+=stack.fairyStats[2];b.dex+=stack.fairyStats[3];b.sta+=stack.fairyStats[4];
                    }
                    // forge.lua check_item_final_data: efeito por nivel, em cada encaixe.
                    // Somente gemas reconhecidas e compativeis com o item equipado.
                    if(forge_rules::validSockets(stack.socketCount,stack.gemIds,stack.gemLevels)) {
                        for(int i=0;i<3;++i) {
                            if(stack.gemIds[i]==0)continue;
                            const auto gem=gemCatalog_.find(stack.gemIds[i]);
                            if(gem!=gemCatalog_.end() && gem->second.compatible(it->second.type))
                                b+=forge_rules::effect(gem->second,stack.gemLevels[i]);
                        }
                    }
                    break;
                }
            }
        }
        total += b;
    }
    return total;
}

}  // namespace top
