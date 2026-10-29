#include "top/NpcMissionSelection.h"
// Primeiro alvo nativo. Janela GLX, device OpenGL 3.3, terreno do .map original.
// Eixos do MindPower (x, y no chao, z = altura) viram OpenGL (x, altura, y).

#include "top/AssetAudit.h"
#include "top/HistoricalPaths.h"
#include "top/ActorSkin.h"
#include "top/ActorGpuCache.h"
#include "top/AreaLighting.h"
#include "top/AudioSystem.h"
#include "top/Cursor.h"
#include "top/EffectSystem.h"
#include "top/HistoricalPetCatalog.h"
#include "top/HistoricalSkillPreviewBinding.h"
#include "top/GfxDevice.h"
#include "top/Window.h"
#include "top/GameMain.h"
#include "top/PlatformServices.h"
#include "top/Image.h"
#include "top/MapFile.h"
#include "top/PathFinder.h"
#include "top/Play.h"
#include "top/JobNames.h"
#include "top/PortalSystem.h"
#include "top/NpcSystem.h"
#include "top/MonsterSpawnSystem.h"
#include "top/OfflineCharacterCatalog.h"
#include "top/SceneWorld.h"
#include "top/TerrainMesh.h"
#include "top/TerrainTable.h"
#include "top/ScreenPick.h"
#include "top/ClassicCamera.h"
#include "top/InventoryConfirm.h"
#include "top/ItemTicketRules.h"
#include "top/SelectionRing.h"
#include "top/PerformanceMeter.h"
#include "top/WorldStreamSchedule.h"
#include "top/StaticBatchCache.h"
#include "top/StartupProfiler.h"
#include "top/DevWorldPosition.h"
#include "top/DevSeaTravel.h"
#include "HudComposer.h"
#include "UIRenderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// Camera inicial: pi olha a cara. O desvio tira a palmeira do rosto.
constexpr float kPi = 3.14159265f;
constexpr float kStartCamYaw = kPi - 0.35f;

struct HistoricalHairStyle {
    int serial=0;
    std::string name,color,modelName;
    std::vector<std::pair<int,int>> materials;
    int gold=0,relatedModel=0,failedModel=0,itemId=0;
    std::array<int,4> usable{};
};

std::vector<std::string> splitTabFields(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin=0;
    while(begin<=line.size()) {
        const std::size_t end=line.find('\t',begin);
        fields.push_back(line.substr(begin,end==std::string::npos?end:end-begin));
        if(end==std::string::npos)break;
        begin=end+1;
    }
    return fields;
}

bool parseHairMaterial(const std::string& field,std::pair<int,int>& item) {
    const std::size_t comma=field.find(',');
    if(comma==std::string::npos)return false;
    char* end=nullptr;
    const std::string idField=field.substr(0,comma);
    const long id=std::strtol(idField.c_str(),&end,10);
    if(idField.empty() || end==idField.c_str() || *end || id<0 || id>1000000)return false;
    const std::string quantityField=field.substr(comma+1);
    const long quantity=std::strtol(quantityField.c_str(),&end,10);
    if(quantityField.empty() || end==quantityField.c_str() || *end || quantity<0 || quantity>1000000 ||
       ((id==0)!=(quantity==0)))return false;
    item={static_cast<int>(id),static_cast<int>(quantity)};
    return true;
}

std::vector<HistoricalHairStyle> loadHistoricalHairStyles(const std::string& clientRoot) {
    std::vector<HistoricalHairStyle> styles;
    const std::string path=top::historicalFilePath(clientRoot,"scripts/table/hairs.txt");
    FILE* file=path.empty()?nullptr:std::fopen(path.c_str(),"rb");
    if(!file)return styles;
    char raw[4096];
    while(std::fgets(raw,sizeof(raw),file)) {
        std::string line(raw);
        while(!line.empty() && (line.back()=='\n'||line.back()=='\r'))line.pop_back();
        if(line.empty() || line.rfind("//",0)==0)continue;
        const auto columns=splitTabFields(line);
        if(columns.size()!=16)continue;
        auto integer=[&](std::size_t index,int& out,int low,int high) {
            if(index>=columns.size() || columns[index].empty())return false;
            char* end=nullptr;
            const long value=std::strtol(columns[index].c_str(),&end,10);
            if(end==columns[index].c_str() || *end || value<low || value>high)return false;
            out=static_cast<int>(value);return true;
        };
        HistoricalHairStyle style;
        if(!integer(0,style.serial,1,100000) || columns[1].empty() || columns[2].empty() ||
           !integer(7,style.gold,0,1000000000) || !integer(8,style.relatedModel,1,1000000) ||
           !integer(9,style.failedModel,0,1000000) || !integer(14,style.itemId,1,1000000) ||
           columns[15].empty())continue;
        style.name=columns[1];style.color=columns[2];style.modelName=columns[15];
        bool valid=true;
        for(std::size_t material=3;material<=6;++material) {
            std::pair<int,int> parsed;
            if(!parseHairMaterial(columns[material],parsed)){valid=false;break;}
            if(parsed.first>0)style.materials.push_back(parsed);
        }
        for(std::size_t race=0;race<4 && valid;++race)
            valid=integer(10+race,style.usable[race],0,1);
        if(valid && style.itemId==style.relatedModel)styles.push_back(std::move(style));
    }
    std::fclose(file);
    std::sort(styles.begin(),styles.end(),[](const auto& a,const auto& b){return a.serial<b.serial;});
    return styles;
}

bool safeMapName(const std::string& name);

std::string findMap(const std::string& name) {
    const char* roots[] = {
        "Client/map/",
        "../Client/map/",
        "../../Client/map/",
        "../../../Client/map/",
    };
    if (!safeMapName(name)) return {};
    for (const char* base : roots) {
        // Retem a grafia real (.map/.MAP, DreamIsland/PKmap/sdBoss).
        const std::string path = top::historicalFilePath(base, name + ".map");
        if (!path.empty()) return path;
    }
    return {};
}

std::vector<std::string> discoverMaps() {
    const char* roots[] = {"Client/map", "../Client/map", "../../Client/map", "../../../Client/map"};
    std::set<std::string> unique;
    for (const char* root : roots) {
        DIR* dir = opendir(root);
        if (!dir) continue;
        while (dirent* entry = readdir(dir)) {
            std::string file = entry->d_name;
            if (file.size() <= 4) continue;
            std::string extension = file.substr(file.size() - 4);
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension != ".map") continue;
            file.resize(file.size() - 4);
            if (safeMapName(file)) unique.insert(std::move(file));
        }
        closedir(dir);
    }
    std::vector<std::string> result(unique.begin(), unique.end());
    std::sort(result.begin(), result.end(), [](const std::string& a, const std::string& b) {
        std::string lowA = a;
        std::string lowB = b;
        std::transform(lowA.begin(), lowA.end(), lowA.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(lowB.begin(), lowB.end(), lowB.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lowA == lowB ? a < b : lowA < lowB;
    });
    return result;
}

bool safeMapName(const std::string& name) {
    if (name.empty() || name.size() > 48) return false;
    for (unsigned char ch : name) {
        if (!std::isalnum(ch) && ch != '_') return false;
    }
    return true;
}

bool mapSpawn(const std::string& root, const std::string& mapName, float& x, float& y) {
    FILE* file = std::fopen((root + "/scripts/table/mapinfo.txt").c_str(), "rb");
    if (!file) return false;
    char line[2048];
    bool found = false;
    while (std::fgets(line, sizeof line, file)) {
        if (line[0] == '/') continue;
        std::vector<std::string> cols;
        std::string row(line);
        std::size_t at = 0;
        while (at <= row.size()) {
            const std::size_t tab = row.find('\t', at);
            if (tab == std::string::npos) {
                cols.push_back(row.substr(at));
                break;
            }
            cols.push_back(row.substr(at, tab - at));
            at = tab + 1;
        }
        if (cols.size() < 5 || cols[1] != mapName) continue;
        if (std::sscanf(cols[4].c_str(), "%f,%f", &x, &y) == 2) found = true;
        break;
    }
    std::fclose(file);
    return found;
}

void perspective(float fovY, float aspect, float nearZ, float farZ, float* m) {
    std::memset(m, 0, sizeof(float) * 16);
    const float t = std::tan(fovY * 0.5f);
    m[0] = 1.f / (aspect * t);
    m[5] = 1.f / t;
    m[10] = (farZ + nearZ) / (nearZ - farZ);
    m[11] = -1.f;
    m[14] = (2.f * farZ * nearZ) / (nearZ - farZ);
}

void lookAt(float ex, float ey, float ez, float cx, float cy, float cz, float* m) {
    float fx = cx - ex;
    float fy = cy - ey;
    float fz = cz - ez;
    float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (fl < 1e-4f) fl = 1.f;
    fx /= fl;
    fy /= fl;
    fz /= fl;
    float sx = fy * 0.f - fz * 1.f;
    float sy = fz * 0.f - fx * 0.f;
    float sz = fx * 1.f - fy * 0.f;
    float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (sl < 1e-4f) sl = 1.f;
    sx /= sl;
    sy /= sl;
    sz /= sl;
    const float ux = sy * fz - sz * fy;
    const float uy = sz * fx - sx * fz;
    const float uz = sx * fy - sy * fx;
    std::memset(m, 0, sizeof(float) * 16);
    m[0] = sx;
    m[4] = sy;
    m[8] = sz;
    m[1] = ux;
    m[5] = uy;
    m[9] = uz;
    m[2] = -fx;
    m[6] = -fy;
    m[10] = -fz;
    m[12] = -(sx * ex + sy * ey + sz * ez);
    m[13] = -(ux * ex + uy * ey + uz * ez);
    m[14] = fx * ex + fy * ey + fz * ez;
    m[15] = 1.f;
}

bool invert4(const float* m, float* inv) {
    float a[16];
    float b[16];
    std::memcpy(a, m, sizeof a);
    std::memset(b, 0, sizeof b);
    b[0] = b[5] = b[10] = b[15] = 1.f;
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row) {
            if (std::fabs(a[col * 4 + row]) > std::fabs(a[col * 4 + pivot])) pivot = row;
        }
        if (std::fabs(a[col * 4 + pivot]) < 1e-8f) return false;
        for (int k = 0; k < 4; ++k) {
            std::swap(a[k * 4 + col], a[k * 4 + pivot]);
            std::swap(b[k * 4 + col], b[k * 4 + pivot]);
        }
        const float diag = a[col * 4 + col];
        for (int k = 0; k < 4; ++k) {
            a[k * 4 + col] /= diag;
            b[k * 4 + col] /= diag;
        }
        for (int row = 0; row < 4; ++row) {
            if (row == col) continue;
            const float factor = a[col * 4 + row];
            for (int k = 0; k < 4; ++k) {
                a[k * 4 + row] -= factor * a[k * 4 + col];
                b[k * 4 + row] -= factor * b[k * 4 + col];
            }
        }
    }
    std::memcpy(inv, b, sizeof b);
    return true;
}

void unproject(const float* inv, float x, float y, float z, float& ox, float& oy, float& oz) {
    const float vx = inv[0] * x + inv[4] * y + inv[8] * z + inv[12];
    const float vy = inv[1] * x + inv[5] * y + inv[9] * z + inv[13];
    const float vz = inv[2] * x + inv[6] * y + inv[10] * z + inv[14];
    float vw = inv[3] * x + inv[7] * y + inv[11] * z + inv[15];
    if (std::fabs(vw) < 1e-6f) vw = 1.f;
    ox = vx / vw;
    oy = vy / vw;
    oz = vz / vw;
}

void multiply(const float* a, const float* b, float* out) {
    float r[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0] + a[1 * 4 + row] * b[col * 4 + 1] +
                               a[2 * 4 + row] * b[col * 4 + 2] + a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    std::memcpy(out, r, sizeof r);
}

bool rayTerrain(top::MapFile& map, float x0, float y0, float z0, float x1, float y1, float z1,
                float& hitX, float& hitY) {
    const float horizontal = std::hypot(x1 - x0, z1 - z0);
    const int steps = std::max(64, std::min(4096, static_cast<int>(std::ceil(horizontal * 2.f))));
    bool havePrevious = false;
    float previousT = 0.f;
    float previousDelta = 0.f;
    for (int sample = 0; sample <= steps; ++sample) {
        const float t = static_cast<float>(sample) / static_cast<float>(steps);
        const float x = x0 + (x1 - x0) * t;
        const float y = y0 + (y1 - y0) * t;
        const float z = z0 + (z1 - z0) * t;
        const top::Tile tile = map.at(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(z)));
        if (!tile.valid) {
            havePrevious = false;
            continue;
        }
        const float delta = y - map.heightAt(x, z);
        if (havePrevious && previousDelta >= 0.f && delta <= 0.f) {
            float low = previousT;
            float high = t;
            for (int iteration = 0; iteration < 14; ++iteration) {
                const float middle = (low + high) * .5f;
                const float mx = x0 + (x1 - x0) * middle;
                const float my = y0 + (y1 - y0) * middle;
                const float mz = z0 + (z1 - z0) * middle;
                if (my >= map.heightAt(mx, mz)) low = middle;
                else high = middle;
            }
            const float resolved = (low + high) * .5f;
            hitX = x0 + (x1 - x0) * resolved;
            hitY = z0 + (z1 - z0) * resolved;
            return true;
        }
        havePrevious = true;
        previousT = t;
        previousDelta = delta;
    }
    return false;
}

struct Actor {
    float x = 2232.5f;
    float y = 2787.5f;
    float yaw = 0.f;
};

struct LocalBerth {
    int id;
    float dockX;
    float dockY;
    float shipX;
    float shipY;
    float shipYaw;
};

constexpr LocalBerth kGarnerBerths[] = {
    {1, 2250.65f, 2819.84f, 2231.f, 2827.f, 0.f},
    {2, 3260.48f, 3309.19f, 3260.f, 3280.f, 0.f},
    {3, 2305.72f, 1123.47f, 2273.f, 1122.f, 0.f},
    {4, 3632.13f, 747.76f, 3590.f, 755.f, 0.f},
    {5, 1058.31f, 1331.51f, 1058.f, 1323.f, 0.f},
};

bool placeOnLand(top::MapFile& map, Actor& actor) {
    const int ox = static_cast<int>(actor.x);
    const int oy = static_cast<int>(actor.y);
    for (int ring = 0; ring <= 40; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring) continue;
                const float x = static_cast<float>(ox + dx) + 0.5f;
                const float y = static_cast<float>(oy + dy) + 0.5f;
                if (!map.blockedAt(x, y)) {
                    actor.x = x;
                    actor.y = y;
                    return true;
                }
            }
        }
    }
    // Alguns mapinfo antigos usam 1,1 como sentinela. Nesses mapas procura
    // uma secao valida no arquivo em vez de assumir a praca de Argent.
    for (int y = 2; y < map.height() - 2; y += 4) {
        for (int x = 2; x < map.width() - 2; x += 4) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            if (!map.blockedAt(px, py)) {
                actor.x = px;
                actor.y = py;
                return true;
            }
        }
    }
    // Alguns mapas de arena/instancia nao possuem mapinfo. Procura uma celula
    // transitavel em toda a extensao, primeiro em grade grossa e depois o
    // placeOnLand normal refina a posicao escolhida.
    const int step = std::max(1, std::min(map.width(), map.height()) / 96);
    int bestX = -1;
    int bestY = -1;
    float bestDistance = 0.f;
    for (int y = step / 2; y < map.height(); y += step) {
        for (int x = step / 2; x < map.width(); x += step) {
            const top::Tile tile = map.at(x, y);
            if (!tile.valid || tile.blocked) continue;
            const float dx = static_cast<float>(x - ox);
            const float dy = static_cast<float>(y - oy);
            const float distance = dx * dx + dy * dy;
            if (bestX >= 0 && distance >= bestDistance) continue;
            bestX = x;
            bestY = y;
            bestDistance = distance;
        }
    }
    if (bestX < 0) return false;
    actor.x = static_cast<float>(bestX) + 0.5f;
    actor.y = static_cast<float>(bestY) + 0.5f;
    return true;
}

bool placeNearTerrain(top::MapFile& map, Actor& actor, bool water, int radius = 60) {
    const int ox = static_cast<int>(actor.x);
    const int oy = static_cast<int>(actor.y);
    for (int ring = 1; ring <= radius; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring) continue;
                const float x = static_cast<float>(ox + dx) + 0.5f;
                const float y = static_cast<float>(oy + dy) + 0.5f;
                const bool okay = water ? map.sailableAt(x, y) : !map.blockedAt(x, y);
                if (!okay) continue;
                actor.x = x;
                actor.y = y;
                return true;
            }
        }
    }
    return false;
}

std::string clientRootFrom(const std::string& mapPath) {
    const std::size_t pos = mapPath.rfind("/map/");
    if (pos == std::string::npos) return "Client";
    return mapPath.substr(0, pos);
}

bool writePpm(const std::string& path, const std::uint8_t* rgb, int width, int height) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    std::fprintf(file, "P6\n%d %d\n255\n", width, height);
    const bool ok = std::fwrite(rgb, 1, static_cast<std::size_t>(width * height * 3), file) ==
                    static_cast<std::size_t>(width * height * 3);
    std::fclose(file);
    return ok;
}

int loadCharacterChoice(const std::string& root, int fallback) {
    FILE* file = std::fopen((root + "/user/offline-character.txt").c_str(), "rb");
    if (!file) return fallback;
    int id = fallback;
    std::fscanf(file, "%d", &id);
    std::fclose(file);
    return id >= 1 && id <= 4 ? id : fallback;
}

void saveCharacterChoice(const std::string& root, int id) {
    if (id < 1 || id > 4) return;
    FILE* file = std::fopen((root + "/user/offline-character.txt").c_str(), "wb");
    if (!file) return;
    std::fprintf(file, "%d\n", id);
    std::fclose(file);
}

const char* characterRaceName(int id) {
    static const char* names[] = {"", "Lance", "Carsise", "Phyllis", "Ami"};
    return id >= 1 && id <= 4 ? names[id] : "";
}

struct OfflineSettings {
    float cameraDistance = top::classic_camera::kDefaultDistance;
    float cameraLift = top::classic_camera::kDefaultLift;
    int audioVolume = 96;
    int boatId = 1;
};

OfflineSettings loadSettings(const std::string& root) {
    OfflineSettings out;
    FILE* file = std::fopen((root + "/user/offline-settings.txt").c_str(), "rb");
    if (!file) return out;
    std::fscanf(file, "%f %f %d %d", &out.cameraDistance, &out.cameraLift, &out.audioVolume, &out.boatId);
    std::fclose(file);
    out.cameraDistance = top::classic_camera::distance(out.cameraDistance);
    out.cameraLift = top::classic_camera::fromSavedLift(out.cameraLift);
    out.audioVolume = std::max(0, std::min(128, out.audioVolume));
    out.boatId = std::max(1, std::min(17, out.boatId));
    return out;
}

void saveSettings(const std::string& root, const OfflineSettings& settings) {
    FILE* file = std::fopen((root + "/user/offline-settings.txt").c_str(), "wb");
    if (!file) return;
    std::fprintf(file, "%.3f %.3f %d %d\n", settings.cameraDistance, settings.cameraLift, settings.audioVolume,
                 settings.boatId);
    std::fclose(file);
}

top::MeshVertex markerVertex(float x, float y, float z, float r, float g, float b) {
    top::MeshVertex vertex{};
    vertex.x = x;
    vertex.y = y;
    vertex.z = z;
    vertex.r = r;
    vertex.g = g;
    vertex.b = b;
    vertex.a = 1.f;
    return vertex;
}

std::vector<top::MonsterSlot> actorRoster(const std::vector<top::MonsterSlot>& monsters, const top::NpcSystem& npcs,
                                          float x, float y) {
    std::vector<top::MonsterSlot> result = monsters;
    // O mapa inteiro permanece no NpcSystem; esta janela visual acompanha a
    // regiao do jogador para nao deformar centenas de modelos por quadro.
    std::vector<top::Npc> nearby = npcs.visible(x, y, 52.f);
    std::sort(nearby.begin(), nearby.end(), [=](const top::Npc& a, const top::Npc& b) {
        const float adx = a.x - x;
        const float ady = a.y - y;
        const float bdx = b.x - x;
        const float bdy = b.y - y;
        return adx * adx + ady * ady < bdx * bdx + bdy * bdy;
    });
    for (std::size_t i = 0; i < nearby.size() && i < 36; ++i) {
        const top::Npc& npc = nearby[i];
        result.push_back({npc.characterId, npc.x, npc.y, true, npc.yaw});
    }
    return result;
}

}  // namespace

int top::runGame(int argc, char** argv) {
    top::startup::Profiler startup;
    // 05-AF: diagnostico isolado antes de GLX, audio, mapa e qualquer save.
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--audit-assets") == 0)
            return top::runAssetAuditCli(argc, argv);
    std::puts("top_gl offline - migracao 05-BA (instancias de itens e UI de teleporte)");
    bool checkOnly = false;
    bool spawnsOnly = false;
    bool startNearSpawn = false;
    bool explicitMap = false;
    bool showPerf = false;
    bool profilePerf = false;
    bool profileStartup = false;
    std::string startupReport = "startup_profile.csv";
    bool useGpuCache = true;
    bool useActorGpuCache = true;
    bool useFrustumCulling = true;
    int requestedVsync = -1; // -1: leave current driver/compositor policy unchanged
    std::string shotPath;
    int previewSkillId=0;
    int previewPetItem=-1;
    bool petSelectorRequested=false;
    bool previewWalk=false;
    std::string statsPreview;
    float previewSkillTime=.35f;
    top::EffectEvent::Kind previewSkillKind=top::EffectEvent::Cast;
    std::string mapName = "garner";
    int playerId = 1;
    bool playerChoiceGiven = false;
    int gmMonsterId=201,gmItemId=0,gmExperience=100000;
    int forgeEquipIndex=0,forgeGemIndex=1,forgeRefinerIndex=2;
    int socketEquipIndex=0,socketStabilizerIndex=1,socketCatalystIndex=2;
    int gmSocketCount=1,gmGoldAmount=1000000;
    int combineScrollIndex=0,combineLeftIndex=1,combineRightIndex=2;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--check") == 0) checkOnly = true;
        else if (std::strcmp(argv[i], "--spawns") == 0) spawnsOnly = true;
        else if (std::strcmp(argv[i], "--spawn-near") == 0) startNearSpawn = true;
        else if (std::strcmp(argv[i], "--perf") == 0) showPerf = true;
        else if (std::strcmp(argv[i], "--profile") == 0) { showPerf = true; profilePerf = true; }
        else if (std::strcmp(argv[i], "--startup-profile") == 0) profileStartup = true;
        else if (std::strcmp(argv[i], "--startup-report") == 0 && i + 1 < argc) {
            profileStartup = true;
            startupReport = argv[++i];
        }
        else if (std::strcmp(argv[i], "--no-gpu-cache") == 0) useGpuCache = false;
        else if (std::strcmp(argv[i], "--no-actor-cache") == 0) useActorGpuCache = false;
        else if (std::strcmp(argv[i], "--no-frustum-culling") == 0) useFrustumCulling = false;
        else if (std::strcmp(argv[i], "--vsync") == 0 && i + 1 < argc) {
            const std::string choice = argv[++i];
            if (choice == "on" || choice == "1") requestedVsync = 1;
            else if (choice == "off" || choice == "0") requestedVsync = 0;
            else { std::fprintf(stderr, "--vsync aceita on/off (ou 1/0)\n"); return 1; }
        }
        else if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shotPath = argv[++i];
        else if (std::strcmp(argv[i], "--pet-preview")==0 && i+1<argc) previewPetItem=std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--preview-walk")==0) previewWalk=true;
        else if (std::strcmp(argv[i], "--pet-selector")==0) petSelectorRequested=true;
        else if (std::strcmp(argv[i], "--stats-preview")==0 && i+1<argc) statsPreview=argv[++i];
        else if (std::strcmp(argv[i], "--skill-preview") == 0 && i + 1 < argc) previewSkillId=std::atoi(argv[++i]);
        else if ((std::strcmp(argv[i], "--skill-preview-time") == 0 || std::strcmp(argv[i], "--visual-preview-time")==0) && i + 1 < argc) {
            const float value=std::strtof(argv[++i],nullptr);
            if(std::isfinite(value))previewSkillTime=std::clamp(value,0.f,
                std::strcmp(argv[i-1],"--visual-preview-time")==0?60.f:10.f);
        }
        else if (std::strcmp(argv[i], "--skill-preview-channel") == 0 && i + 1 < argc) {
            const std::string channel=argv[++i];
            if(channel=="hit")previewSkillKind=top::EffectEvent::SkillHit;
            else if(channel=="flight")previewSkillKind=top::EffectEvent::Flight;
            else if(channel!="cast") {
                std::fprintf(stderr,"--skill-preview-channel aceita cast/hit/flight\n");
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
            mapName = argv[++i]; explicitMap = true;
        }
        else if (std::strcmp(argv[i], "--gm-mob") == 0 && i + 1 < argc)
            gmMonsterId=std::clamp(std::atoi(argv[++i]),1,100000);
        else if (std::strcmp(argv[i], "--gm-item") == 0 && i + 1 < argc)
            gmItemId=std::clamp(std::atoi(argv[++i]),1,100000);
        else if (std::strcmp(argv[i], "--gm-xp") == 0 && i + 1 < argc)
            gmExperience=std::clamp(std::atoi(argv[++i]),1,100000000);
        else if (std::strcmp(argv[i], "--socket-equip") == 0 && i+1<argc)
            socketEquipIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--socket-stabilizer") == 0 && i+1<argc)
            socketStabilizerIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--socket-catalyst") == 0 && i+1<argc)
            socketCatalystIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--forge-equip") == 0 && i+1<argc)
            forgeEquipIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--forge-gem") == 0 && i+1<argc)
            forgeGemIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--forge-refiner") == 0 && i+1<argc)
            forgeRefinerIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--combine-scroll") == 0 && i+1<argc)
            combineScrollIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--combine-left") == 0 && i+1<argc)
            combineLeftIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--combine-right") == 0 && i+1<argc)
            combineRightIndex=std::clamp(std::atoi(argv[++i]),0,47);
        else if (std::strcmp(argv[i], "--gm-sockets") == 0 && i+1<argc)
            gmSocketCount=std::clamp(std::atoi(argv[++i]),1,3);
        else if (std::strcmp(argv[i], "--gm-gold") == 0 && i+1<argc)
            gmGoldAmount=std::clamp(std::atoi(argv[++i]),1,100000000);
        else if (std::strcmp(argv[i], "--character") == 0 && i + 1 < argc) {
            playerChoiceGiven = true;
            std::string choice = argv[++i];
            std::transform(choice.begin(), choice.end(), choice.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (choice == "lance" || choice == "1") playerId = 1;
            else if (choice == "carsise" || choice == "2") playerId = 2;
            else if (choice == "phyllis" || choice == "philipes" || choice == "3") playerId = 3;
            else if (choice == "ami" || choice == "4") playerId = 4;
        }
    }
    if (profileStartup) { startup.enable(startupReport); startup.mark("argumentos processados"); }
    if (!safeMapName(mapName)) {
        std::fprintf(stderr, "nome de mapa invalido: %s\n", mapName.c_str());
        return 1;
    }
    const std::vector<std::string> availableMaps = discoverMaps();
    std::string mapPath = findMap(mapName);
    startup.mark("catalogo e resolucao do mapa");
    if (mapPath.empty()) {
        // Diagnostico de scripts nao exige que o arquivo grafico .map exista.
        // Um mapa sem recursos 3D nao pode ser aberto no jogo, mas seus
        // CreateCha podem ser inspecionados sem executar o servidor Lua.
        if (spawnsOnly) {
            top::MonsterSpawnSystem diagnostic;
            diagnostic.load(mapName);
            std::printf("mapa %s: %zu regioes ChaSpn, %zu Lua (%zu scripts)\n",
                        mapName.c_str(), diagnostic.regions().size(),
                        diagnostic.scripted().size(), diagnostic.scriptFileCount());
            std::printf("sem map/%s.map: existem dados de spawn, mas nao ha mapa grafico para abrir\n",
                        mapName.c_str());
            for (std::size_t i = 0; i < diagnostic.scripted().size() && i < 8; ++i) {
                const auto& spawn = diagnostic.scripted()[i];
                std::printf("  CreateCha %d em %.2f,%.2f [%s], respawn %d s\n",
                            spawn.id, spawn.x, spawn.y, spawn.script.c_str(), spawn.respawnSeconds);
            }
            return diagnostic.regions().empty() && diagnostic.scripted().empty() ? 1 : 0;
        }
        std::fprintf(stderr, "nao achei map/%s.map no Client nem no acervo\n", mapName.c_str());
        return 1;
    }
    top::MapFile map;
    if (!map.open(mapPath)) {
        std::fprintf(stderr, "mapa invalido: %s\n", mapPath.c_str());
        return 1;
    }
    startup.mark("leitura do cabecalho do mapa");
    Actor actor;
    std::string mapRoot = clientRootFrom(mapPath);
    const std::string compactMap = findMap("garner");
    const std::string root = compactMap.empty() ? mapRoot : clientRootFrom(compactMap);
    const std::string actorRoot = root;
    bool characterSelect = false;
    top::OfflineCharacterCatalog characterCatalog;
    const bool characterCatalogReady = characterCatalog.load(root);
    if (!characterCatalogReady)
        std::fprintf(stderr, "catalogo de personagens invalido em %s/user; mantendo selecao legada\n", root.c_str());
    if (characterCatalogReady && characterCatalog.profiles().empty() && !playerChoiceGiven && !checkOnly) {
        int legacyRace = loadCharacterChoice(root, 0);
        if (legacyRace == 0) {
            FILE* legacySave = std::fopen((root + "/user/offline-save.txt").c_str(), "rb");
            if (legacySave) { std::fclose(legacySave); legacyRace = 1; }
        }
        std::vector<top::BirthPoint> legacyBirthPoints;
        if (legacyRace >= 1 && legacyRace <= 4 &&
            top::loadHistoricalBirthPoints(root + "/scripts/offline/maps/script/birth/birth_conf.lua",
                                           legacyBirthPoints)) {
            const auto birth = std::find_if(legacyBirthPoints.begin(), legacyBirthPoints.end(),
                [](const top::BirthPoint& point) { return point.city == "Argent City"; });
            if (birth != legacyBirthPoints.end() &&
                characterCatalog.importLegacy(legacyRace, characterRaceName(legacyRace), *birth))
                characterCatalog.save();
        }
    }
    int profileId = 0;
    std::string profileName;
    int selectedProfileId = characterCatalog.activeId();
    if (playerChoiceGiven) {
        if (!checkOnly) saveCharacterChoice(root, playerId);
    } else {
        const top::OfflineCharacterProfile* activeProfile = characterCatalog.find(characterCatalog.activeId());
        if (activeProfile) {
            playerId = activeProfile->raceId;
            profileId = activeProfile->id;
            profileName = activeProfile->name;
        } else {
            const int savedCharacter = loadCharacterChoice(root, 0);
            if (savedCharacter >= 1 && savedCharacter <= 4) playerId = savedCharacter;
            else characterSelect = !checkOnly;
        }
    }
    // DEV-SEA-01: position was never stored by Play::save(), so normal startup
    // always returned to mapinfo / the Argent nudge. Use a separate, per-profile
    // location file; --map / --spawn-near always override it.
    auto positionIdentity = [&]() { return profileId > 0 ? profileId : playerId; };
    top::dev_world::Location restoredLocation;
    bool positionRestored = false;
    if (!explicitMap && !startNearSpawn && !checkOnly && !characterSelect &&
        top::dev_world::read(top::dev_world::path(root, positionIdentity()), restoredLocation)) {
        std::string restoredPath = findMap(restoredLocation.map);
        if (!restoredPath.empty()) {
            if (restoredLocation.map != mapName) {
                top::MapFile restoredMap;
                if (restoredMap.open(restoredPath)) {
                    map.swap(restoredMap);
                    restoredMap.close();
                    mapPath = restoredPath;
                    mapName = restoredLocation.map;
                    mapRoot = clientRootFrom(mapPath);
                }
            }
            if (restoredLocation.map == mapName &&
                restoredLocation.x < map.width() && restoredLocation.y < map.height() &&
                ((restoredLocation.boat && map.sailableAt(restoredLocation.x, restoredLocation.y)) ||
                 (!restoredLocation.boat && !map.blockedAt(restoredLocation.x, restoredLocation.y)))) {
                actor.x = restoredLocation.x;
                actor.y = restoredLocation.y;
                actor.yaw = restoredLocation.yaw;
                positionRestored = true;
                std::printf("posicao restaurada: %s %.2f,%.2f barco=%d\n",
                            mapName.c_str(), actor.x, actor.y, restoredLocation.boat ? 1 : 0);
            }
        }
    }
    if (!positionRestored && !mapSpawn(mapRoot, mapName, actor.x, actor.y))
        mapSpawn(root, mapName, actor.x, actor.y);
    if (startNearSpawn) {
        // Opcao de diagnostico: iniciar dentro de uma regiao historica real
        // para inspecionar os modelos do mapa. Nao muda os portais normais.
        top::MonsterSpawnSystem probeSpawns;
        probeSpawns.load(mapName);
        top::MonsterSpawnRegion nearest;
        float distance = 0.f;
        if (probeSpawns.closestRegion(actor.x, actor.y, nearest, distance)) {
            actor.x = (nearest.x0 + nearest.x1) * 0.5f;
            actor.y = (nearest.y0 + nearest.y1) * 0.5f;
            std::printf("diagnostico de spawn: regiao #%d %s, centro %.1f,%.1f\n",
                        nearest.serial, nearest.name.c_str(), actor.x, actor.y);
        } else {
            std::printf("diagnostico de spawn: mapa %s sem regiao historica para iniciar\n", mapName.c_str());
        }
    }
    if (!positionRestored && !placeOnLand(map, actor)) {
        std::fprintf(stderr, "sem chao andavel perto de Argent\n");
        return 1;
    }
    // O tile inicial cai debaixo do coreto. Com a camera no yaw 0, +X e a
    // esquerda da tela e -Y e o lado da camera. O passo cai no calcamento.
    if (mapName == "garner" && !startNearSpawn && !positionRestored) {
        const float homeX = actor.x;
        const float homeY = actor.y;
        const float nudge[][2] = {{16.f, -6.f}, {14.f, 0.f}, {-16.f, -6.f}, {0.f, -14.f}};
        for (const auto& step : nudge) {
            Actor next;
            next.x = homeX + step[0];
            next.y = homeY + step[1];
            if (!placeOnLand(map, next)) continue;
            const float dx = next.x - homeX;
            const float dy = next.y - homeY;
            if (dx * dx + dy * dy < 64.f) continue;
            const top::Tile tile = map.at(static_cast<int>(next.x), static_cast<int>(next.y));
            if (!tile.valid || tile.height < 0.3f || tile.height > 3.f) continue;
            actor = next;
            break;
        }
    }
    startup.mark("posicao inicial e terreno navegavel");
    const top::Tile ground = map.at(static_cast<int>(actor.x), static_cast<int>(actor.y));
    top::TerrainTable terrain;
    if (!terrain.load(root + "/scripts/table/terraininfo.txt")) {
        std::fprintf(stderr, "sem terraininfo.txt em %s\n", root.c_str());
        return 1;
    }
    top::AreaLighting areaLighting;
    if (!areaLighting.open(root + "/scripts/table/areaset.txt")) {
        std::fprintf(stderr, "sem areaset.txt em %s\n", root.c_str());
        return 1;
    }
    startup.mark("tabelas de terreno e iluminacao");
    int texOk = 0;
    int texFail = 0;
    top::MonsterSpawnSystem monsterSpawns;
    monsterSpawns.load(mapName);
    startup.mark("leitura dos spawns historicos");
    constexpr int kMaxLocalMonsters = 20;
    constexpr float kLocalMonsterRadius = 84.f;
    std::vector<top::MonsterSlot> localMonsters =
        monsterSpawns.nearest(actor.x, actor.y, kMaxLocalMonsters, kLocalMonsterRadius);
    if (spawnsOnly) {
        std::puts("spawns historicos por mapa (regioes validas, sem monstros artificiais):");
        for (const std::string& candidate : availableMaps) {
            top::MonsterSpawnSystem listing;
            listing.load(candidate);
            if (!listing.regions().empty() || !listing.scripted().empty())
                std::printf("  %-22s %zu regioes ChaSpn, %zu Lua (%zu scripts)\n", candidate.c_str(),
                            listing.regions().size(), listing.scripted().size(), listing.scriptFileCount());
        }
        std::printf("mapa %s: %zu regioes ChaSpn, %zu registros Lua em %zu scripts, %zu monstros perto de %.1f,%.1f\n",
                    mapName.c_str(), monsterSpawns.regions().size(), monsterSpawns.scripted().size(),
                    monsterSpawns.scriptFileCount(), localMonsters.size(), actor.x, actor.y);
        top::MonsterSpawnRegion nearest;
        float distance = 0.f;
        if (monsterSpawns.closestRegion(actor.x, actor.y, nearest, distance))
            std::printf("regiao proxima: #%d %s; distancia %.1f m; area %.1f,%.1f a %.1f,%.1f\n",
                        nearest.serial, nearest.name.c_str(), distance,
                        nearest.x0, nearest.y0, nearest.x1, nearest.y1);
        else std::puts("sem nascimento historico ChaSpn/Lua para esse mapa");
        for (const auto& mob : localMonsters) {
            const auto& scripts = monsterSpawns.scripted();
            const auto it = std::find_if(scripts.begin(), scripts.end(), [&](const top::ScriptMonsterSpawn& entry) {
                return entry.id == mob.id && std::fabs(entry.x - mob.x) < .01f &&
                       std::fabs(entry.y - mob.y) < .01f;
            });
            if (it != scripts.end())
                std::printf("  monstro ID %d em %.2f,%.2f  Lua:%s (respawn %d s)\n",
                            mob.id, mob.x, mob.y, it->script.c_str(), it->respawnSeconds);
            else
                std::printf("  monstro ID %d em %.2f,%.2f  ChaSpn\n", mob.id, mob.x, mob.y);
        }
        return 0;
    }
    top::NpcSystem visualNpcs;
    visualNpcs.load(mapName);
    startup.mark("leitura dos NPCs visuais");
    std::vector<top::MonsterSlot> visualRoster = actorRoster(localMonsters, visualNpcs, actor.x, actor.y);
    if (checkOnly) {
        int validMaps = 0;
        for (const std::string& candidate : availableMaps) {
            top::MapFile probe;
            const std::string candidatePath = findMap(candidate);
            if (!candidatePath.empty() && probe.open(candidatePath) && probe.width() > 0 && probe.height() > 0)
                ++validMaps;
            else
                std::fprintf(stderr, "mapa historico invalido: %s\n", candidate.c_str());
        }
        if (validMaps < 50 || validMaps != static_cast<int>(availableMaps.size())) {
            std::fprintf(stderr, "catalogo de mapas incompleto: %d/%zu validos\n", validMaps, availableMaps.size());
            return 1;
        }
        std::printf("catalogo offline %d mapas historicos validos\n", validMaps);
        for (int id = 1; id <= 80; ++id) {
            const std::string* file = terrain.fileFor(id);
            if (!file) continue;
            top::Image image;
            if (image.load(root + "/" + *file) && image.width > 0) ++texOk;
            else {
                ++texFail;
                std::fprintf(stderr, "textura %d falhou: %s\n", id, file->c_str());
            }
        }
        top::Image atlas;
        const bool atlasOk = atlas.load(root + "/texture/terrain/alpha/total.tga");
        if (!atlasOk) ++texFail;
        int waterFrames = 0;
        for (int frame = 1; frame <= 30; ++frame) {
            char name[96];
            std::snprintf(name, sizeof name, "/texture/terrain/water/ocean_h.%02d.bmp", frame);
            top::Image water;
            if (water.load(root + name) && water.width > 0 && water.height > 0) ++waterFrames;
            else std::fprintf(stderr, "agua: quadro historico %d ausente ou invalido\n", frame);
        }
        std::printf("mapa %s %dx%d  jogador %.1f, %.1f  altura %.2f  tex %u\n", mapPath.c_str(), map.width(), map.height(),
                    actor.x, actor.y, ground.height, ground.layer[0].tex);
        const top::AreaLight& spawnLight = areaLighting.at(ground.island);
        std::printf("areas %d  setor %u %s  ambiente %.2f,%.2f,%.2f  luz %.2f,%.2f,%.2f\n",
                    areaLighting.count(), ground.island, spawnLight.name.c_str(), spawnLight.environment[0],
                    spawnLight.environment[1], spawnLight.environment[2], spawnLight.diffuse[0],
                    spawnLight.diffuse[1], spawnLight.diffuse[2]);
        if (areaLighting.count() != 255 || (ground.island > 0 && !spawnLight.valid)) {
            std::fprintf(stderr, "iluminacao: tabela ou setor historico incompleto\n");
            return 1;
        }
        std::printf("texturas %d ok, %d falhas, atlas %dx%d\n", texOk, texFail, atlas.width, atlas.height);
        std::printf("agua animada %d/30 quadros historicos\n", waterFrames);
        if (waterFrames != 30) return 1;
        std::vector<top::PathPoint> pathProbe;
        bool pathOk = false;
        for (int halfCells = 1; halfCells <= 24 && !pathOk; ++halfCells) {
            static constexpr int direction[][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1},
                                                    {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
            for (const auto& dir : direction) {
                const float distance = static_cast<float>(halfCells) * .5f;
                const float tx = actor.x + static_cast<float>(dir[0]) * distance;
                const float ty = actor.y + static_cast<float>(dir[1]) * distance;
                if (map.blockedAt(tx, ty)) continue;
                if (top::PathFinder::find(map, actor.x, actor.y, tx, ty, false, pathProbe)) {
                    pathOk = true;
                    break;
                }
            }
        }
        // Certas arenas historicas nascem numa unica celula liberada e nao
        // oferecem vizinho caminhavel; isso e dado do mapa, nao erro do A*.
        if (pathOk && !pathProbe.empty()) std::printf("caminhada grade 50cm  %zu curvas\n", pathProbe.size());
        else std::printf("caminhada nascimento isolado pela colisao historica\n");
        top::SceneWorld scene;
        if (!scene.open(root, mapName, mapRoot) || !top::SceneWorld::selfTest()) {
            std::fprintf(stderr, "cena: nao abri %s.obj / sceneobjinfo, ou o giro falhou\n", mapName.c_str());
            return 1;
        }
        top::SceneMesh preview;
        scene.setAnimationTime(0.f);
        scene.build(map, actor.x, actor.y, 64.f, preview);
        if (pathOk && !pathProbe.empty()) {
            const float goalX = pathProbe.back().x;
            const float goalY = pathProbe.back().y;
            const float playerFeet = map.heightAt(actor.x, actor.y);
            auto sceneBlocked = [&](float x, float y) {
                const float terrain = map.heightAt(x, y);
                const float floor = preview.surfaceAt(x, y, playerFeet, terrain);
                return preview.solidAt(x, y, floor) ||
                       (floor > terrain + 0.2f && floor > playerFeet + 0.78f);
            };
            auto footprintBlocked = [&](float x, float y) {
                constexpr float radius = 0.20f;
                auto blocked = [&](float px, float py) {
                    return map.blockedAt(px, py) || sceneBlocked(px, py);
                };
                return blocked(x, y) || blocked(x + radius, y) || blocked(x - radius, y) ||
                       blocked(x, y + radius) || blocked(x, y - radius);
            };
            std::vector<top::PathPoint> scenePath;
            if (!top::PathFinder::find(map, actor.x, actor.y, goalX, goalY, false,
                                       scenePath, footprintBlocked) || scenePath.empty()) {
                std::fprintf(stderr, "caminhada: A* nao encontrou rota com colisao dos objetos de cena\n");
                return 1;
            }
            std::printf("caminhada com colisao estatica da cena  %zu curvas\n", scenePath.size());
        }
        bool sceneAnimationMoves = false;
        if (preview.animatedParts > 0) {
            top::SceneMesh animatedPreview;
            scene.setAnimationTime(1.f);
            scene.build(map, actor.x, actor.y, 64.f, animatedPreview);
            for (std::size_t bi = 0; bi < preview.batches.size() && bi < animatedPreview.batches.size() && !sceneAnimationMoves; ++bi) {
                const auto& a = preview.batches[bi].verts;
                const auto& b = animatedPreview.batches[bi].verts;
                const std::size_t n = std::min(a.size(), b.size());
                for (std::size_t vi = 0; vi < n; ++vi) {
                    if (std::fabs(a[vi].x - b[vi].x) + std::fabs(a[vi].y - b[vi].y) +
                            std::fabs(a[vi].z - b[vi].z) > 0.002f) {
                        sceneAnimationMoves = true;
                        break;
                    }
                }
            }
            scene.setAnimationTime(0.f);
        }
        int sceneTexOk = 0;
        int sceneTexFail = 0;
        std::unordered_map<std::string, int> seenTex;
        for (const top::SceneBatch& batch : preview.batches) {
            if (batch.texture.empty() || !seenTex.emplace(batch.texture, 1).second) continue;
            top::Image image;
            if (image.load(batch.texture) && image.width > 0) ++sceneTexOk;
            else {
                ++sceneTexFail;
                std::fprintf(stderr, "textura de cena falhou: %s\n", batch.texture.c_str());
            }
        }
        std::printf("cena objetos %d  animados %d/%d  uv %d  opacidade %d  imagens %d  sombras %d  movimento %d  modelos falhos %d  triangulos %d  sem material %d  texturas %d ok, %d falhas, %zu ausentes\n",
                    preview.placed, preview.animatedObjects, preview.animatedParts, preview.animatedUv,
                    preview.animatedOpacity, preview.animatedImages, preview.staticShadows, sceneAnimationMoves ? 1 : 0,
                    preview.modelFail, preview.triangles, preview.untexturedTriangles,
                    sceneTexOk, sceneTexFail, preview.missingTextures.size());
        for (const std::string& texture : preview.missingTextures)
            std::fprintf(stderr, "textura de cena ausente: %s\n", texture.c_str());
        if (preview.animatedParts > 0 && !sceneAnimationMoves) {
            std::fprintf(stderr, "cena: controladores matriciais foram lidos, mas nao produziram movimento\n");
            return 1;
        }
        if (!top::ActorSkin::selfTest()) {
            std::fprintf(stderr, "giro do personagem falhou\n");
            return 1;
        }
        top::ActorSkin actors;
        if (!top::MonsterSpawnSystem::selfTest()) {
            std::fprintf(stderr, "nascimentos: parser ChaSpn falhou\n");
            return 1;
        }
        if (!actors.open(actorRoot, mapName, visualRoster, playerId)) {
            std::fprintf(stderr, "personagem: nao abri .lab ou .lgo\n");
            return 1;
        }
        if (!actors.minimapReady()) actors.buildMinimapFallback(map);
        if (!actors.setEquipment({{2, 289}, {9, 8}})) {
            std::fprintf(stderr, "equipamento: Newbie Vest/Knife nao apareceu no personagem %d\n", playerId);
            return 1;
        }
        if (actors.report().playerPoseMode != 1) {
            std::fprintf(stderr, "05-D: Newbie Knife deveria selecionar animacao de espada simples\n");
            return 1;
        }
        if (!actors.setEquipment({{2, 289}, {6, 8}, {9, 8}}) ||
            actors.report().playerPoseMode != 3) {
            std::fprintf(stderr, "05-D: duas espadas deveriam selecionar animacao dual\n");
            return 1;
        }
        if (!actors.setEquipment({{2, 289}, {9, 8}})) {
            std::fprintf(stderr, "05-D: nao conseguiu voltar para espada simples\n");
            return 1;
        }
        for (int shipId = 1; shipId <= 17; ++shipId) {
            if (!actors.setBoatMode(true, shipId) ||
                std::fabs(actors.boatSeaHeight() - (-0.10f)) > 0.001f ||
                !actors.setBoatMode(false)) {
                std::fprintf(stderr, "barco: casco %d sem SeaHeight historico de -0,10 m\n", shipId);
                return 1;
            }
        }
        // Regressao da transicao de mapa: ActorSkin::open recria os corpos e
        // limpa boatMode; a viagem deve recolocar o casco depois do open.
        if (!actors.setBoatMode(true, 1) || !actors.open(actorRoot, mapName, visualRoster, playerId) ||
            actors.boatMode() || !actors.setBoatMode(true, 1) || !actors.boatMode() ||
            std::fabs(actors.boatSeaHeight() - (-0.10f)) > 0.001f ||
            std::fabs(actors.boatMoveSpeed() - 4.5f) > 0.001f ||
            !actors.setBoatMode(false) || !actors.setEquipment({{2, 289}, {9, 8}})) {
            std::fprintf(stderr, "barco: modo embarcado nao sobreviveu a recriacao do roster\n");
            return 1;
        }
        if (!actors.report().weaponAttached || !actors.report().weaponMoves || !actors.report().weaponGripCorrect ||
            actors.report().weaponHeight < 0.45f || actors.report().weaponHeight > 1.65f) {
            std::fprintf(stderr, "equipamento: arma nao seguiu a mao (dummy=%d movimento=%d empunhadura=%d altura=%.2f)\n",
                         actors.report().weaponAttached ? 1 : 0, actors.report().weaponMoves ? 1 : 0,
                         actors.report().weaponGripCorrect ? 1 : 0,
                         actors.report().weaponHeight);
            return 1;
        }
        const float feet = map.heightAt(actor.x, actor.y);
        actors.setPlayer(actor.x, actor.y, actor.yaw, feet, false, 0.f);
        actors.build(map);
        actors.toggleSystemWindow();
        actors.toggleBoatWindow();
        actors.setNpcWindow(true, "Blacksmith - Goldie", "Welcome. I can repair equipment and offer you a mission.");
        actors.setShopWindow(true, "Blacksmith - Goldie", {"27", "80"}, 0, 2, 100,
                             {"texture/icon/w0092.png", "texture/icon/w0092.png"}, {"Newbie Knife", "Short Sword"});
        actors.setQuests({"[Ativa] Blacksmith's Greetings", "Visite Goldie"});
        actors.setBagView({{0, 8, 1, 9, true, "texture/icon/w0092.png", "Newbie Knife"},
                           {1, 289, 1, 2, false, "texture/icon/e0271.png", "Newbie Vest"}},
                          {{9, 8}}, {}, 1234);
        actors.setSkillView({{99, "Spiritual Bolt", "texture/icon/s0099.png", 6, true, false},
                             {97, "Heal", "texture/icon/s0097.png", 5, true, false},
                             {84, "Berserk", "texture/icon/s0084.png", 5, true, false},
                             {225, "Harden", "texture/icon/s0225.png", 6, true, false}}, 4);
        actors.togglePanel(0);
        actors.togglePanel(1);
        actors.togglePanel(2);
        actors.togglePanel(3);
        actors.togglePanel(4);
        actors.setCharacterWindow(true);
        actors.buildHud(1280, 720);
        if (mapName == "garner" && (!actors.minimapReady() || !actors.minimapLand())) {
            std::fprintf(stderr, "minimapa: garner.pk sem chao (%d tiles)\n", actors.minimapTiles());
            return 1;
        }
        if (actors.minimapReady()) std::printf("minimapa %s %d tiles%s\n", mapName.c_str(), actors.minimapTiles(),
                                               actors.minimapFallback() ? " (fallback .map)" : "");
        else std::printf("minimapa %s indisponivel; usando mar\n", mapName.c_str());
        int actorTexOk = 0;
        int actorTexFail = 0;
        std::unordered_map<std::string, int> seenActor;
        auto countTex = [&](const std::vector<top::ActorBatch>& batches) {
            for (const top::ActorBatch& batch : batches) {
                if (batch.texture.empty() || batch.texture == "font" || batch.texture == "white" ||
                    batch.texture == "minimap" || !seenActor.emplace(batch.texture, 1).second)
                    continue;
                top::Image image;
                if (image.load(batch.texture) && image.width > 0) ++actorTexOk;
                else {
                    ++actorTexFail;
                    std::fprintf(stderr, "textura de personagem falhou: %s\n", batch.texture.c_str());
                }
            }
        };
        countTex(actors.batches());
        countTex(actors.hud());
        const top::ActorReport& rep = actors.report();
        std::printf(
            "atores %d  pecas %d  pecas falhas %d  ossos %d  triangulos %d  sem textura %d  sombras %d  anima %d  arma z %.2f  corpo z %.2f..%.2f  tex %d ok, %d falhas, %zu ausentes\n",
            rep.actors, rep.parts, rep.partFail, rep.bones, rep.triangles, rep.untexturedTriangles, rep.shadows,
            rep.animMoves ? 1 : 0, rep.weaponHeight, rep.bodyMinZ, rep.bodyMaxZ,
            actorTexOk, actorTexFail, rep.missingTextures.size());
        std::printf("poses monstros %d bancos validos, %d Action IDs remapeados, %d fallbacks, %d sem pose\n",
                    rep.monsterPoseReady, rep.monsterPoseRemaps,
                    rep.monsterPoseFallbacks, rep.monsterPoseMissing);
        for (const std::string& texture : rep.missingTextures)
            std::fprintf(stderr, "textura de personagem ausente: %s\n", texture.c_str());
        std::fflush(stdout);
        if (!atlasOk || texFail != 0 || (preview.placed > 0 && preview.triangles == 0)) return 1;
        if (sceneTexFail > sceneTexOk) return 1;
        // Ami e menor (cerca de 1,42 m); Carsise e bem mais alto. Aceita as quatro fichas jogaveis.
        const bool bodySized = rep.bodyMinZ > -1.f && rep.bodyMinZ < 1.2f && rep.bodyMaxZ > 1.1f && rep.bodyMaxZ < 6.f;
        if (!actors.fontReady()) {
            std::fprintf(stderr, "fonte DejaVu ou Noto nao abriu\n");
            return 1;
        }
        // A camera inicial (kStartCamYaw) fica no +Y. O corpo com yaw 0 tambem
        // olha para +Y, entao o primeiro quadro ve o rosto, nao as costas.
        // O olho esta em -cos(yaw) no eixo Y, entao precisa ser positivo.
        const float eyeY = -std::cos(kStartCamYaw);
        if (eyeY < 0.9f) {
            std::fprintf(stderr, "camera inicial nao olha o rosto (eyeY %.2f)\n", eyeY);
            return 1;
        }
        const bool requiresActorShadow = mapName == "garner" || mapName == "eastgoaf" || mapName == "magicsea" || mapName == "darkblue";
        if (rep.actors < 2 || rep.parts < 6 || rep.triangles == 0 ||
            (requiresActorShadow && rep.shadows == 0) || !rep.animMoves || !bodySized || actorTexFail != 0) return 1;
        if (actors.monsterCount() < 4) {
            std::fprintf(stderr, "combate: poucos monstros (%d)\n", actors.monsterCount());
            return 1;
        }
        if (!top::Play::selfTest()) return 1;
        top::PortalSystem portals;
        if (!top::PortalSystem::selfTest()) return 1;
        portals.load(mapName);
        top::NpcSystem npcs;
        if (!top::NpcSystem::selfTest()) return 1;
        npcs.load(mapName);
        std::size_t shopCount = 0;
        std::size_t shopItems = 0;
        std::size_t dialogCount = 0;
        std::unordered_map<std::string, std::vector<int>> checkedCatalogs;
        std::unordered_map<std::string, std::string> checkedDialogs;
        for (const top::Npc& npc : npcs.npcs()) {
            if (npc.script.empty()) continue;
            auto talk = checkedDialogs.find(npc.script);
            if (talk == checkedDialogs.end()) {
                std::string text;
                npcs.dialog(npc.script, text);
                talk = checkedDialogs.emplace(npc.script, std::move(text)).first;
            }
            if (!talk->second.empty()) ++dialogCount;
            if (npc.service <= 0) continue;
            auto catalog = checkedCatalogs.find(npc.script);
            if (catalog == checkedCatalogs.end()) {
                std::vector<int> stock;
                npcs.catalog(npc.script, stock);
                catalog = checkedCatalogs.emplace(npc.script, std::move(stock)).first;
            }
            if (catalog->second.empty()) continue;
            ++shopCount;
            shopItems += catalog->second.size();
        }
        if (!npcs.npcs().empty() && dialogCount == 0) {
            std::fprintf(stderr, "npc: nenhuma fala historica foi extraida em %s\n", mapName.c_str());
            return 1;
        }
        std::vector<int> sennaMissions;
        top::HistoricalMission greeting;
        top::HistoricalMission slowpoke;
        top::HistoricalMission secondTask;
        if (mapName == "garner") {
            if (!npcs.missionIds("BT_NewUser001", sennaMissions) ||
                std::find(sennaMissions.begin(), sennaMissions.end(), 701) == sennaMissions.end() ||
                !npcs.mission(701, greeting) || greeting.title != "Blacksmith's Greetings" || greeting.targetX < 2192.f ||
                greeting.targetX > 2194.f || greeting.rewardExp != 9) {
                std::fprintf(stderr, "missao: cadeia historica de Senna/Goldie nao abriu\n");
                return 1;
            }
            if (!npcs.mission(631, slowpoke) || slowpoke.targetId != 127 || slowpoke.targetCount != 10 ||
                slowpoke.rewardExp != 6100 || slowpoke.rewardGold != 461 || slowpoke.minLevel != 35) {
                std::fprintf(stderr, "missao: caca historica 631 nao abriu\n");
                return 1;
            }
            if (!npcs.mission(1001, secondTask) || secondTask.neededItems.empty() ||
                secondTask.neededItems[0].id != 3910 || secondTask.neededItems[0].count != 1) {
                std::fprintf(stderr, "missao: requisito historico de item 1001 nao abriu\n");
                return 1;
            }
        }
        top::EffectSystem effects;
        if (!top::EffectSystem::selfTest() || !effects.open(root)) {
            std::fprintf(stderr, "efeito: nao abri a tabela, o .eff ou sua textura\n");
            return 1;
        }
        effects.emit({top::EffectEvent::Cast, actor.x, actor.y, actor.x, actor.y});
        effects.emit({top::EffectEvent::SkillHit, actor.x, actor.y, actor.x + 2.f, actor.y});
        effects.step(1.f / 30.f);
        effects.build(map, 1.f, 0.f);
        if (effects.batches().empty() || effects.batches()[0].verts.size() < 12) {
            std::fprintf(stderr, "efeito: simulacao nao gerou billboards\n");
            return 1;
        }
        if (!top::GameCursor::selfTest(root)) return 1;
        if (!top::AudioSystem::selfTest(actorRoot)) {
            std::fprintf(stderr, "audio: musicinfo, OGG ou WAV historico nao abriu\n");
            return 1;
        }
        const top::EffectReport& effectRep = effects.report();
        if (effectRep.particleFiles < 2 || effectRep.particleSystems < 8) {
            std::fprintf(stderr, "efeito: controladores .par historicos incompletos (%d arquivos, %d sistemas)\n",
                         effectRep.particleFiles, effectRep.particleSystems);
            return 1;
        }
        std::printf("efeito %s  refs %d  particulas %d arquivos/%d sistemas  textura %s\n",
                    effectRep.legacyFile.c_str(), effectRep.references, effectRep.particleFiles,
                    effectRep.particleSystems, effectRep.textureFile.c_str());
        std::printf("skills visuais historicas %d registros (IDs de efeito, fallback OpenGL)\n",
                    effectRep.skillVisualRecords);
        std::printf("efeitos magicsingle %d registros  carregamento sob demanda (fallback para arquivos ausentes)\n",
                    effectRep.magicEffectRecords);
        std::printf("efeitos de cena %d registros (sceneffectinfo: conjuracao/impacto)\n",
                    effectRep.sceneEffectRecords);
        std::printf("audio historico ok  musica OGG e efeitos WAV\n");
        std::printf("combate teste ok  monstros %d\n", actors.monsterCount());
        std::printf("nascimentos %s %zu regioes ChaSpn, %zu Lua (%zu scripts), %zu monstros locais\n",
                    mapName.c_str(), monsterSpawns.regions().size(), monsterSpawns.scripted().size(),
                    monsterSpawns.scriptFileCount(), localMonsters.size());
        top::MonsterSpawnRegion closest;
        float distanceToSpawn = 0.f;
        if (monsterSpawns.closestRegion(actor.x, actor.y, closest, distanceToSpawn))
            std::printf("regiao de monstros mais proxima #%d %s, %.0f m\n", closest.serial,
                        closest.name.c_str(), distanceToSpawn);
        else std::printf("mapa %s sem spawns ChaSpn/Lua: nenhum monstro artificial\n", mapName.c_str());
        std::printf("portais %s %zu\n", mapName.c_str(), portals.portals().size());
        std::printf("npcs %s %zu\n", mapName.c_str(), npcs.npcs().size());
        std::printf("lojas %s %zu  catalogo %zu itens\n", mapName.c_str(), shopCount, shopItems);
        std::printf("dialogos %s %zu\n", mapName.c_str(), dialogCount);
        if (greeting.id != 0)
            std::printf("missao %d %s  alvo %.0f,%.0f  exp %d\n", greeting.id, greeting.title.c_str(), greeting.targetX,
                        greeting.targetY, greeting.rewardExp);
        if (slowpoke.id != 0)
            std::printf("missao %d %s  monstro %d x%d  nivel %d  exp %d ouro %d\n", slowpoke.id,
                        slowpoke.title.c_str(), slowpoke.targetId, slowpoke.targetCount, slowpoke.minLevel,
                        slowpoke.rewardExp, slowpoke.rewardGold);
        std::fflush(stdout);
        return 0;
    }
    std::printf("mapa %s %dx%d  jogador %.1f, %.1f  altura %.2f  tex %u\n", mapPath.c_str(), map.width(), map.height(),
                actor.x, actor.y, ground.height, ground.layer[0].tex);

    startup.mark("dados iniciais prontos (antes da janela)");
    top::Window window;
    if (!window.open(1280, 720, "Tales of Pirates - OpenGL")) return 1;
    startup.mark("criacao da janela GLX");
    if (requestedVsync >= 0 && !window.setSwapInterval(requestedVsync))
        std::fprintf(stderr, "aviso: GLX nao permite alterar VSync neste driver\n");
    std::printf("sincronizacao: %s | pedido=%s | jogo sem limitador proprio de FPS\n",
                window.swapControlInfo().c_str(),
                requestedVsync < 0 ? "padrao do driver" : requestedVsync ? "on" : "off");
    top::GfxDevice device;
    if (!device.init()) {
        window.close();
        return 1;
    }
    device.resize(window.width(), window.height());
    startup.mark("inicializacao OpenGL e shaders");

    top::Image atlasImage;
    unsigned atlas = 0;
    if (atlasImage.load(root + "/texture/terrain/alpha/total.tga")) {
        atlas = device.createTexture(atlasImage.rgba.data(), atlasImage.width, atlasImage.height);
    }
    std::vector<unsigned> waterTextures(30, 0);
    for (int frame = 1; frame <= 30; ++frame) {
        char name[96];
        std::snprintf(name, sizeof name, "/texture/terrain/water/ocean_h.%02d.bmp", frame);
        top::Image water;
        if (water.load(root + name)) {
            waterTextures[static_cast<std::size_t>(frame - 1)] =
                device.createTexture(water.rgba.data(), water.width, water.height);
        }
    }
    startup.mark("atlas e texturas de agua enviados a GPU");
    std::unordered_map<int, unsigned> textures;
    auto textureFor = [&](int id) -> unsigned {
        const auto it = textures.find(id);
        if (it != textures.end()) return it->second;
        unsigned handle = 0;
        if (const std::string* file = terrain.fileFor(id)) {
            top::Image image;
            if (image.load(root + "/" + *file)) {
                handle = device.createTexture(image.rgba.data(), image.width, image.height);
            }
        }
        textures[id] = handle;
        return handle;
    };

    top::SceneWorld scene;
    if (!scene.open(root, mapName, mapRoot))
        std::fprintf(stderr, "sem objetos de cena %s em %s\n", mapName.c_str(), mapRoot.c_str());
    startup.mark("arquivo de objetos do cenario");
    // SEA-03.1: ler o ID do casco ANTES da primeira ativacao do modo naval.
    // TOPLOC1 guarda o modo; offline-settings.txt guarda o modelo selecionado.
    const OfflineSettings loadedSettings = loadSettings(root);
    top::ActorSkin actors;
    if (!actors.open(actorRoot, mapName, visualRoster, playerId)) std::fprintf(stderr, "sem personagem em %s\n", actorRoot.c_str());
    if (!top::dev_sea::restoreInitialBoat(actors,
                                         positionRestored && restoredLocation.boat,
                                         loadedSettings.boatId))
        std::fprintf(stderr, "SEA-03.1: nao foi possivel restaurar o casco %d\n", loadedSettings.boatId);
    std::vector<top::BirthPoint> historicalBirthPoints;
    const bool birthPointsReady = top::loadHistoricalBirthPoints(
        root + "/scripts/offline/maps/script/birth/birth_conf.lua", historicalBirthPoints);
    const char* raceNames[] = {"", "Lance", "Carsise", "Phyllis", "Ami"};
    const char* birthCities[] = {"Argent City", "Shaitan City", "Icicle Castle"};
    auto refreshCharacterRoster = [&]() {
        std::vector<std::string> labels;
        int selected = -1;
        for (std::size_t i = 0; i < characterCatalog.profiles().size(); ++i) {
            const auto& profile = characterCatalog.profiles()[i];
            const std::string race = profile.raceId >= 1 && profile.raceId <= 4 ? raceNames[profile.raceId] : "?";
            labels.push_back(profile.name + " - " + race + " - " + profile.birthCity);
            if (profile.id == selectedProfileId) selected = static_cast<int>(i);
        }
        actors.setCharacterRoster(std::move(labels), selected);
    };
    auto reopenActorRoster = [&](int rosterPlayerId) {
        const bool restoreBoat = actors.boatMode();
        const int restoreBoatId = actors.boatId();
        if (!actors.open(actorRoot, mapName, visualRoster, rosterPlayerId)) return false;
        // ActorSkin::open limpa o roster inteiro, inclusive o modo de barco.
        // Reinstala o casco antes de voltar a atualizar/renderizar o mundo.
        if (restoreBoat && !actors.setBoatMode(true, restoreBoatId)) {
            std::fprintf(stderr, "roster: nao foi possivel restaurar o barco %d em %s\n",
                         restoreBoatId, mapName.c_str());
            return false;
        }
        return true;
    };
    startup.mark("personagens, modelos e animacoes");
    if (!actors.minimapReady()) actors.buildMinimapFallback(map);
    refreshCharacterRoster();
    actors.setCharacterWindow(characterSelect);
    unsigned fontTex = 0;
    if (actors.fontReady()) {
        fontTex = device.createTexture(actors.fontPixels(), actors.fontWidth(), actors.fontHeight());
    }
    const std::uint8_t whitePx[4] = {255, 255, 255, 255};
    const unsigned whiteTex = device.createTexture(whitePx, 1, 1);
    // Font and rectangles for a lightweight diagnostic overlay. Uses the
    // existing HUD renderer, not a separate GL pipeline or gameplay UI.
    top::Font devFont;
    devFont.open();
    bool devPanelOpen = false;
    bool heldF9 = false;
    float devAutoSaveElapsed = 0.f;
    bool devCommandEditing = false;
    std::string devCommand;
    constexpr int kDevPanelX = 20, kDevPanelY = 62;
    constexpr int kDevRowY = 174, kDevRowHeight = 30, kDevPanelWidth = 510;
    constexpr int kDevRows = 12;
    auto currentDevLocation = [&]() -> top::dev_world::Location {
        return {mapName, actor.x, actor.y, actor.yaw, actors.boatMode()};
    };
    auto saveCurrentLocation = [&](bool manual) {
        return top::dev_world::write(top::dev_world::path(root, positionIdentity(), manual),
                                     currentDevLocation());
    };
    auto historicalSpawnPath = [&]() {
        return root + "/user/offline-spawn-" + std::to_string(positionIdentity()) + ".txt";
    };
    std::printf("ferramentas de teste: F9 abre painel de locais; posicao salva automaticamente\n");
    unsigned miniTex = 0;
    top::GameCursor gameCursor;
    const bool cursorReady = gameCursor.load(root);
    std::vector<unsigned> cursorTex;
    if (cursorReady) {
        window.hidePointer();
        cursorTex.resize(gameCursor.frames().size());
        for (std::size_t i = 0; i < gameCursor.frames().size(); ++i) {
            const top::CursorFrame& frame = gameCursor.frames()[i];
            if (frame.w > 0 && !frame.rgba.empty()) {
                cursorTex[i] = device.createTexture(frame.rgba.data(), frame.w, frame.h);
            }
        }
    } else {
        std::fprintf(stderr, "cursor: nao abri %s/cursor\n", root.c_str());
    }
    startup.mark("fonte e texturas do cursor");
    const float feetSpawn = map.heightAt(actor.x, actor.y);
    actors.setPlayer(actor.x, actor.y, actor.yaw, feetSpawn, false, 0.f);
    actors.ensurePlaced(map);
    top::Play play;
    if(!statsPreview.empty() && shotPath.empty()){std::fprintf(stderr,"--stats-preview exige --shot (teste descartavel)\n");return 1;}
    play.setReadOnlyPreview(!statsPreview.empty());
    play.setWorldMap(mapName);
    if (!play.bind(actorRoot, actors, profileId, profileName))
        std::fprintf(stderr, "combate sem monstros em %s\n", actorRoot.c_str());
    std::uint64_t handledRespawnSerial = play.respawnSerial();
    bool characterPreviewSession = false;
    bool characterPreviewRestoreBoat = false;
    int characterPreviewRestoreBoatId = 0;
    auto showCharacterPreview = [&](int previewRaceId) {
        if (previewRaceId < 1 || previewRaceId > 4) return false;
        if (!characterPreviewSession) {
            characterPreviewRestoreBoat = actors.boatMode();
            characterPreviewRestoreBoatId = actors.boatId();
            characterPreviewSession = true;
        }
        // Reuse the historical 3D actor pipeline for a real, non-persistent
        // preview. Combat actors are restored when the modal is closed.
        if (!actors.open(actorRoot, mapName, {}, previewRaceId)) return false;
        actors.setPlayer(actor.x, actor.y, actor.yaw, 0.f, false, 0.f);
        actors.ensurePlaced(map);
        play.pushActors(actors);
        return true;
    };
    auto restoreCharacterPreview = [&]() {
        if (!characterPreviewSession) return true;
        if (!actors.open(actorRoot, mapName, visualRoster, playerId)) return false;
        if (characterPreviewRestoreBoat && !actors.setBoatMode(true, characterPreviewRestoreBoatId)) {
            std::fprintf(stderr, "personagem: nao foi possivel restaurar o barco %d apos a previa\n",
                         characterPreviewRestoreBoatId);
            return false;
        }
        actors.setPlayer(actor.x, actor.y, actor.yaw, 0.f, false, 0.f);
        actors.ensurePlaced(map);
        play.rebindWorld(actors);
        play.pushActors(actors);
        characterPreviewSession = false;
        return true;
    };
    startup.mark("Play: save, inventario e combate");
    top::EffectSystem effects;
    if (!effects.open(root)) std::fprintf(stderr, "efeitos legados indisponiveis em %s\n", root.c_str());
    startup.mark("efeitos historicos");
    top::AudioSystem audio;
    if (!audio.open(actorRoot, mapName)) std::fprintf(stderr, "tabela de audio indisponivel em %s\n", actorRoot.c_str());
    const top::Tile spawnAudioTile = map.at(static_cast<int>(actor.x), static_cast<int>(actor.y));
    if (spawnAudioTile.valid) audio.setArea(spawnAudioTile.island);
    startup.mark("musica e efeitos sonoros");
    top::PortalSystem portals;
    portals.load(mapName);
    top::NpcSystem npcs;
    npcs.load(mapName);
    const std::vector<HistoricalHairStyle> historicalHairStyles=loadHistoricalHairStyles(actorRoot);
    startup.mark("portais e scripts de NPC");
    const auto historicalPets=top::loadHistoricalPets(actorRoot);
    const bool visualDiagnostic=previewSkillId>0 || previewPetItem>=0 || petSelectorRequested || !statsPreview.empty();
    const float diagnosticStartX=actor.x,diagnosticStartY=actor.y;
    int lastLoggedPet=-2;
    if(!statsPreview.empty()) {
        if(!play.configureStatPreview(statsPreview)){std::fprintf(stderr,"Cenario de atributos invalido\n");return 1;}
        actors.togglePanel(1);
        std::fprintf(stderr,"[stats-preview-initial] %s\n",play.statDiagnostic().c_str());
    }
    std::vector<top::EffectEvent> frameVisualEvents;
    if (previewSkillId>0) {
        if(shotPath.empty() || !effects.skillVisuals().find(previewSkillId)) {
            std::fprintf(stderr,"skill-preview: requires --shot and a skill from skillinfo.txt\n");
            return 1;
        }
        const auto* visual=effects.skillVisuals().find(previewSkillId);
        const bool declared=previewSkillKind==top::EffectEvent::Cast?visual->castId()>0:
            previewSkillKind==top::EffectEvent::Flight?visual->flightId()>0:
            !visual->hitChannels(top::SkillEffectSurface::Unknown).empty();
        if(!declared) {
            std::fprintf(stderr,"skill-preview: selected skill has no declared channel of this kind\n");
            return 1;
        }
        const float targetX=previewSkillKind==top::EffectEvent::Flight?actor.x-5.f:actor.x;
        top::EffectEvent previewEvent{previewSkillKind,actor.x,actor.y,targetX,actor.y,previewSkillId,1.f};
        top::historical_skill_preview_binding::qualifyPlayer(previewEvent);
        frameVisualEvents.push_back(previewEvent);
        std::fprintf(stderr,"[skill-preview] id=%d channel=%d time=%.3f action=%d (visual only)\n",
            previewSkillId,int(previewSkillKind),previewSkillTime,
            effects.skillVisuals().find(previewSkillId)->firstAction);
    } else if (!shotPath.empty() && !visualDiagnostic) {
        effects.emit({top::EffectEvent::Cast, actor.x, actor.y, actor.x, actor.y});
        effects.emit({top::EffectEvent::SkillHit, actor.x, actor.y, actor.x - 2.5f, actor.y});
    }
    struct ShopSpot {
        top::Npc npc;
        std::vector<int> stock;
    };
    std::vector<ShopSpot> shops;
    auto loadShops = [&]() {
        shops.clear();
        std::unordered_map<std::string, std::vector<int>> cache;
        for (const top::Npc& npc : npcs.npcs()) {
            if (npc.service <= 0 || npc.script.empty()) continue;
            auto it = cache.find(npc.script);
            if (it == cache.end()) {
                std::vector<int> stock;
                npcs.catalog(npc.script, stock);
                it = cache.emplace(npc.script, std::move(stock)).first;
            }
            if (!it->second.empty()) shops.push_back({npc, it->second});
        }
    };
    auto placeVendor = [&]() {
        play.clearVendor();
        const ShopSpot* nearest = nullptr;
        float best = 0.f;
        for (const ShopSpot& shop : shops) {
            const float dx = shop.npc.x - actor.x;
            const float dy = shop.npc.y - actor.y;
            const float distance = dx * dx + dy * dy;
            if (!nearest || distance < best) {
                nearest = &shop;
                best = distance;
            }
        }
        if (nearest) play.setVendor(nearest->npc.x, nearest->npc.y, nearest->npc.name, nearest->stock);
    };
    auto refreshQuestNpcMarkers=[&]() {
        std::vector<top::Play::QuestNpcMarker> markers;
        std::unordered_map<std::string,std::vector<top::Play::QuestNpcMarker::Mission>> cache;
        for(const auto& npc:npcs.npcs()) {
            if(npc.script.empty())continue;
            auto found=cache.find(npc.script);
            if(found==cache.end()) {
                std::vector<int> ids;
                std::vector<top::Play::QuestNpcMarker::Mission> rule;
                if(npcs.missionIds(npc.script,ids)) {
                    for(int id:ids) {
                        top::HistoricalMission m;
                        if(!npcs.missionByDisplay(id,m))continue;
                        bool valid=(m.targetX>=0.f&&m.targetY>=0.f)||
                                   (m.targetId>0&&m.targetCount>0)||!m.neededItems.empty();
                        rule.push_back({m.id,m.minLevel,m.maxLevel,valid,m.gate,m.title});
                        if(rule.size()>=128)break;
                    }
                }
                found=cache.emplace(npc.script,std::move(rule)).first;
            }
            auto offers=found->second;
            if(npc.name.find("Senna")!=std::string::npos && mapName=="garner")
                offers.push_back({1,1,0,true,{},"Welcome"});
            if(!offers.empty())markers.push_back({npc.x,npc.y,npc.name,std::move(offers)});
        }
        play.setMissionRecordIndex(npcs.missionRecordIndex());
        play.setQuestNpcMarkers(std::move(markers));
    };
    refreshQuestNpcMarkers();
    startup.mark("indicadores de missao (! e ?) - consulta Lua");
    std::vector<top::Portal> npcTeleportTargets;
    std::vector<top::Portal> historicalIslandDestinations;
    unsigned npcForgeModes=0;
    std::string npcDialogScript, npcDialogName;
    std::vector<top::NpcLuaData::DialogOption> npcDialogOptions;
    int npcDialogPage=0;
    bool npcTutorialOpen=false, npcDialogHasMissions=false, npcDialogVendor=false;
    top::Npc npcForgeOrigin{};
    std::string npcForgeMap;
    auto fillClassTestPage=[&](int page) {
        npcDialogOptions.clear();
        npcDialogPage=page;
        auto profession=[&](const char* label,int job) {
            top::NpcLuaData::DialogOption option;
            option.label=label;option.triggerId=100+job;
            npcDialogOptions.push_back(std::move(option));
        };
        auto pageLink=[&](const char* label,int target) {
            top::NpcLuaData::DialogOption option;
            option.label=label;option.nextPage=target;
            npcDialogOptions.push_back(std::move(option));
        };
        if(page==1) {
            profession("Swordsman",1);profession("Hunter",2);
            profession("Explorer",4);profession("Herbalist",5);
            pageLink("More professions...",2);
            top::NpcLuaData::DialogOption pets;pets.label="Pets (teste visual)";pets.triggerId=300;
            npcDialogOptions.push_back(std::move(pets));
        } else if(page==2) {
            profession("Sailor",3);profession("Artisan",6);profession("Merchant",7);
            pageLink("Advanced professions...",3);
            pageLink("Back",1);
        } else {
            const int first=8+(page-3)*3;
            for(int job=first;job<std::min(first+3,19);++job)
                profession(top::jobName(job),job);
            if(page==3) {
                top::NpcLuaData::DialogOption skills;
                skills.label=play.skillTestMode()?"Desativar skills gratis":"Todas as skills (upar gratis)";
                skills.triggerId=200;
                npcDialogOptions.push_back(std::move(skills));
            }
            if(page<6)pageLink("More professions...",page+1);
            if(page>3)pageLink("Back",page-1);
        }
    };
    auto openPetSelector=[&]() {
        npcDialogScript="TOP_TEST_PET";npcDialogName="Pets - teste visual";
        npcDialogOptions.clear();npcTutorialOpen=true;npcDialogHasMissions=false;npcDialogVendor=false;
        auto choice=[&](const std::string& label,int trigger) {
            top::NpcLuaData::DialogOption option;option.label=label;option.triggerId=trigger;
            npcDialogOptions.push_back(std::move(option));
        };
        choice("Usar a fada equipada",1);choice("Ocultar a previa",2);
        for(const auto& pet:historicalPets)choice(pet.name+" (#"+std::to_string(pet.itemId)+")"+
            (pet.definitionAvailable?"":" - imagem ausente"),100000+pet.itemId);
        play.talkNpc(npcDialogName,"Escolha qualquer fada para ver no personagem. A previa nao adiciona itens nem concede atributos. Alt+Y reabre a lista.",false,false);
    };
    auto fillHairStylePage=[&](int page) {
        std::vector<const HistoricalHairStyle*> usable;
        if(playerId>=1 && playerId<=4)
            for(const auto& style:historicalHairStyles)
                if(style.usable[static_cast<std::size_t>(playerId-1)]!=0)usable.push_back(&style);
        const int pageCount=std::max(1,(static_cast<int>(usable.size())+2)/3);
        page=std::clamp(page,1,pageCount);
        npcDialogOptions.clear();
        npcDialogPage=100+page;
        const int begin=(page-1)*3;
        for(int i=begin;i<std::min(begin+3,static_cast<int>(usable.size()));++i) {
            const auto& style=*usable[static_cast<std::size_t>(i)];
            top::NpcLuaData::DialogOption option;
            option.label=style.name+" / "+style.color+" ("+std::to_string(style.gold)+"G)";
            option.triggerId=1000+style.serial;
            npcDialogOptions.push_back(std::move(option));
        }
        if(page>1) {
            top::NpcLuaData::DialogOption previous;
            previous.label="Previous styles";previous.nextPage=100+page-1;
            npcDialogOptions.push_back(std::move(previous));
        }
        if(page<pageCount) {
            top::NpcLuaData::DialogOption next;
            next.label="More styles";next.nextPage=100+page+1;
            npcDialogOptions.push_back(std::move(next));
        }
        top::NpcLuaData::DialogOption close;
        close.label="Cancel";close.close=true;
        npcDialogOptions.push_back(std::move(close));
    };
    auto fillSinbadBoatPage=[&]() {
        npcDialogOptions.clear();
        npcDialogPage=1;
        for (const int shipId : {1,2,3,6}) {
            const auto found=std::find_if(play.boatBuildOptions().begin(),play.boatBuildOptions().end(),
                [shipId](const top::Play::BoatBuildOption& option){return option.id==shipId;});
            if(found==play.boatBuildOptions().end())continue;
            top::NpcLuaData::DialogOption option;
            option.label="Build "+found->name+" - "+std::to_string(found->cost)+"G (Lv "+
                         std::to_string(found->requiredLevel)+")";
            option.triggerId=5000+shipId;
            npcDialogOptions.push_back(std::move(option));
        }
    };
    auto fillShirleyBoatPage=[&]() {
        npcDialogOptions.clear();
        npcDialogPage=1;
        for(std::size_t index=0;index<play.ownedBoatIds().size();++index) {
            const int shipId=play.ownedBoatIds()[index];
            const auto found=std::find_if(play.boatBuildOptions().begin(),play.boatBuildOptions().end(),
                [shipId](const top::Play::BoatBuildOption& option){return option.id==shipId;});
            if(found==play.boatBuildOptions().end())continue;
            top::NpcLuaData::DialogOption option;
            option.label="Set sail: "+found->name+" #"+std::to_string(index+1);
            option.triggerId=6000+static_cast<int>(index);
            npcDialogOptions.push_back(std::move(option));
            top::NpcLuaData::DialogOption repair;
            repair.label="Repair "+found->name+" #"+std::to_string(index+1)+" ("+
                          std::to_string(play.boatHp(shipId))+"/"+std::to_string(play.boatMaxHp(shipId))+" HP, "+
                          std::to_string(play.boatRepairCost(shipId))+"G)";
            repair.triggerId=6100+static_cast<int>(index);
            npcDialogOptions.push_back(std::move(repair));
            top::NpcLuaData::DialogOption refuel;
            refuel.label="Refuel "+found->name+" #"+std::to_string(index+1)+" ("+
                          std::to_string(play.boatSp(shipId))+"/"+std::to_string(play.boatMaxSp(shipId))+" SP, "+
                          std::to_string(play.boatSupplyCost(shipId))+"G)";
            refuel.triggerId=6200+static_cast<int>(index);
            npcDialogOptions.push_back(std::move(refuel));
        }
    };
    auto fillDonnaRewardPage=[&](int page) {
        npcDialogOptions.clear();
        npcDialogPage=page;
        struct Reward { int fame; int item; int count; const char* name; };
        static constexpr Reward standard[]={{20,2602,1,"Novice Endeavor Emblem"},
                                             {200,2603,1,"Standard Endeavor Emblem"},
                                             {2000,2604,1,"Expert Endeavor Emblem"}};
        static constexpr Reward royal[]={{500,855,5,"Fairy Coin x5"},
                                          {5000,3094,3,"Amplifier of Strive x3"},
                                          {20000,610,1,"Novice Self Destruct"},
                                          {50000,885,1,"Level 1 Refining Gem"},
                                          {200000,862,1,"Gem of Colossus"},
                                          {600000,1012,1,"Gem of Soul"}};
        auto addReward=[&](int index,const Reward& reward) {
            top::NpcLuaData::DialogOption option;
            option.label=std::string(reward.name)+" - "+std::to_string(reward.fame)+" reputation";
            option.triggerId=8100+index;
            npcDialogOptions.push_back(std::move(option));
        };
        auto addPage=[&](const char* label,int target) {
            top::NpcLuaData::DialogOption option;
            option.label=label;
            option.nextPage=target;
            npcDialogOptions.push_back(std::move(option));
        };
        if(page==5) {
            for(int i=0;i<3;++i)addReward(i,standard[i]);
            addPage("Royal Pirate rewards",9007);
        } else if(page==9007) {
            for(int i=0;i<4;++i)addReward(3+i,royal[i]);
            addPage("More Royal rewards",9008);
        } else {
            addReward(7,royal[4]);
            addReward(8,royal[5]);
            addPage("Back to Royal rewards",9007);
            addPage("Back to Donna's rewards",5);
        }
    };
    auto fillIslandTeleportPage=[&](int page) {
        npcDialogOptions.clear();
        npcDialogPage=page;
        const int first=(page-1)*4;
        for(int i=first;i<std::min(first+4,static_cast<int>(historicalIslandDestinations.size()));++i) {
            top::NpcLuaData::DialogOption option;
            option.label="Teleport: "+historicalIslandDestinations[static_cast<std::size_t>(i)].name;
            option.triggerId=3000+i;
            npcDialogOptions.push_back(std::move(option));
        }
        if(page*4<static_cast<int>(historicalIslandDestinations.size())) {
            top::NpcLuaData::DialogOption more;
            more.label="More destinations...";more.nextPage=page+1;
            npcDialogOptions.push_back(std::move(more));
        }
        if(page>1) {
            top::NpcLuaData::DialogOption back;
            back.label="Back";back.nextPage=page-1;
            npcDialogOptions.push_back(std::move(back));
        }
    };
    loadShops();
    startup.mark("lojas NPC e catalogos de itens");
    placeVendor();
    startup.mark("posicionamento do vendedor");
    auto interactNpc = [&](const top::Npc* chosen = nullptr) {
        if (play.shopOpen()) {
            play.interact(actor.x, actor.y);
            return;
        }
        top::Npc npc;
        if (chosen) npc=*chosen;
        else if (!npcs.closest(actor.x, actor.y, -1, npc)) return;
        const bool testVendor = npc.script == "TOP_TEST_APPAREL" || npc.script == "TOP_TEST_WEAPONS" ||
                                npc.script == "TOP_TEST_CHESTS" || npc.script == "TOP_TEST_GENERAL";
        const float dx = npc.x - actor.x;
        const float dy = npc.y - actor.y;
        if (dx * dx + dy * dy > 2.4f * 2.4f) return;
        // Monica's and Macurdo's historical scripts charge for bank access
        // from an explicit Vault option. Other bank-service NPCs keep the
        // existing direct interaction path.
        const bool paidVaultDialog=mapName=="garner" &&
            ((npc.script=="r_talk13" && npc.name.find("Monica")!=std::string::npos) ||
             (npc.script=="r_talk29" && npc.name.find("Macurdo")!=std::string::npos));
        if (npcs.bankService(npc.script) &&
            !paidVaultDialog) {
            if (play.openBank()) actors.openBankPanel();
            return;
        }
        // 05-AS: dialogue opens BEFORE any mission completion. Only the
        // explicit Complete button can award the result, including Welcome.
        // 05-AR: side-result scripts attached to this exact NPC can
        // update a running quest before its eventual hand-in elsewhere.
        bool stepApplied=false;
        std::vector<int> npcStepIds;
        if(npcs.missionIds(npc.script,npcStepIds))for(int id:npcStepIds) {
            top::HistoricalMissionStep step;
            if(npcs.missionStep(id,step) && play.applyNpcMissionStep(step))stepApplied=true;
        }
        if(stepApplied)refreshQuestNpcMarkers();
        std::string dialog;
        npcs.dialog(npc.script, dialog);
        const bool salvierWelcomeNpc = mapName=="garner" && npc.script=="r_talk01" &&
                                      npc.name.find("Salvier")!=std::string::npos;
        if (testVendor) {
            dialog = npc.name + ": loja de teste permanente. Os estoques usam itens e precos da tabela historica.";
        }
        npcTeleportTargets.clear();
        npcForgeModes=npcs.forgeServices(npc.script);
        npcDialogScript=npc.script;
        npcDialogName=npc.name;
        npcDialogOptions.clear();
        npcDialogPage=0;
        npcTutorialOpen=false;
        npcDialogVendor=false;
        npcForgeOrigin=npc;
        npcForgeMap=mapName;
        if(npc.script=="TOP_TEST_CLASS") {
            dialog="TEST MODE: choose a profession. The change is immediate and saved; no historical quest, level, gold, or race checks are applied.";
            fillClassTestPage(1);
            npcTutorialOpen=true;
        }
        if(mapName=="garner" && npc.script=="r_talk09" &&
           npc.name.find("Marriam")!=std::string::npos) {
            auto cookOption=[&](const char* label,int trigger) {
                top::NpcLuaData::DialogOption option;
                option.label=label;
                option.triggerId=trigger;
                npcDialogOptions.push_back(std::move(option));
            };
            cookOption("Cook Turkey (10 Turkey Meat + 10000G)",510);
            if(play.level()<2)
                cookOption("PK Test (Level 65 + 1000000G)",514);
            if(!play.hasHistoricalRecord(815))
                cookOption("Cook Christmas Meal (first time, 10000G)",511);
            else if(!play.hasHistoricalRecord(816))
                cookOption("Cook Christmas Meal (second time, 1000000G)",512);
            else if(!play.hasHistoricalRecord(817))
                cookOption("Cook Christmas Meal (third time, 10000000G)",513);
            npcTutorialOpen=true;
            dialog="Marriam: cooking recipes from the historical Garner script.";
        }
        if(mapName=="garner" && npc.script=="leo_talkaa" &&
           npc.name.find("Wood Processing Merchant")!=std::string::npos) {
            std::vector<top::NpcLuaData::ExchangeRule> exchanges;
            if(npcs.exchangeRules(npc.script,exchanges)) {
                for(std::size_t i=0;i<exchanges.size() && i<1000;++i) {
                    const auto& rule=exchanges[i];
                    top::NpcLuaData::DialogOption option;
                    option.label="Exchange " + std::to_string(rule.inputId) + " x" +
                        std::to_string(rule.inputCount) + " for " + std::to_string(rule.outputId) +
                        " x" + std::to_string(rule.outputCount);
                    option.triggerId=7000+static_cast<int>(i);
                    npcDialogOptions.push_back(std::move(option));
                }
                npcTutorialOpen=true;
                dialog="Wood Processing Merchant: historical exchanges. The offer list comes from the Garner Lua table.";
            }
        }
        if(npc.script=="TeleportIsland" && mapName=="garner" && npc.serial==122) {
            historicalIslandDestinations.clear();
            static const char* islandCities[]={"Zephyr Isle","Glacier Isle","Outlaw Isle","Isle of Chill",
                                               "Canary Isle","Cupid Isle","Isle of Fortune"};
            bool completeIslandRouteData=true;
            for(const char* city:islandCities) {
                const auto birth=std::find_if(historicalBirthPoints.begin(),historicalBirthPoints.end(),
                    [city](const top::BirthPoint& point){return point.city==city;});
                if(birth==historicalBirthPoints.end()) {
                    completeIslandRouteData=false;
                    break;
                }
                top::Portal destination;
                destination.name=city;destination.targetMap=birth->map;
                destination.targetX=static_cast<float>(birth->x);
                destination.targetY=static_cast<float>(birth->y);
                historicalIslandDestinations.push_back(std::move(destination));
            }
            if(!completeIslandRouteData) historicalIslandDestinations.clear();
            npcTeleportTargets.clear();
            dialog="Silvius: Hi, I am the Island Teleporter! How can I help you?";
            if(historicalIslandDestinations.empty()) {
                dialog+=" The historical island routes are unavailable because birth-point data is incomplete.";
            } else {
                dialog+=" Teleport is free through level 40; above level 40 it costs 500G.";
                fillIslandTeleportPage(1);
                npcTutorialOpen=true;
            }
        } else if (npc.name.find("Teleporter") != std::string::npos || npc.name.find("Jovial") != std::string::npos ||
            npc.script.find("Teleport") != std::string::npos) {
            // Jovial é o teleporter de Argent; Senna permanece apenas como
            // guia inicial. Esses
            // destinos não são portais físicos no chão, portanto entram
            // primeiro na lista local.
            const bool jovial = npc.name.find("Jovial") != std::string::npos;
            // Jovial usa as opções históricas de TeleportCity, interpretadas
            // abaixo a partir dos pontos de nascimento. Os botões genéricos
            // de destino duplicavam o diálogo e não incluíam os Haven.
            if (!jovial) {
                for (const top::Portal& candidate : portals.portals()) {
                    if (npcTeleportTargets.size() >= 4) break;
                    if (candidate.targetMap == mapName) continue;
                    bool duplicate = false;
                    for (const top::Portal& selected : npcTeleportTargets)
                        duplicate = duplicate || selected.targetMap == candidate.targetMap;
                    if (!duplicate) npcTeleportTargets.push_back(candidate);
                    if (npcTeleportTargets.size() == 4) break;
                }
            }
            if (!jovial && npcTeleportTargets.size() < 4) {
                std::vector<std::string> preferred = {"magicsea", "darkblue", "eastgoaf", "jialebi", "garner"};
                preferred.insert(preferred.end(), availableMaps.begin(), availableMaps.end());
                for (const std::string& destination : preferred) {
                    if (destination == mapName) continue;
                    bool duplicate = false;
                    for (const top::Portal& selected : npcTeleportTargets)
                        duplicate = duplicate || selected.targetMap == destination;
                    if (duplicate) continue;
                    top::Portal fallback;
                    fallback.targetMap = destination;
                    fallback.name = destination == "magicsea" ? "Shaitan City" :
                                    destination == "darkblue" ? "Icicle Castle" :
                                    destination == "eastgoaf" ? "Thundoria Castle" :
                                    destination == "garner" ? "Argent City" : destination;
                    npcTeleportTargets.push_back(fallback);
                    if (npcTeleportTargets.size() == 4) break;
                }
            }
            if (!npcTeleportTargets.empty()) {
                if (!dialog.empty()) dialog += "  ";
                dialog += "Teleporte: ";
                for (std::size_t i = 0; i < npcTeleportTargets.size(); ++i) {
                    if (i) dialog += " | ";
                    dialog += npcTeleportTargets[i].name;
                }
            }
        }
        // 05-AK: apresentar toda a lista historica. O Play valida nivel,
        // progresso e elegibilidade antes de permitir aceitar cada missao.
        std::vector<int> linkedMissions;
        std::vector<top::HistoricalMission> npcMissionList;
        if(npcs.missionIds(npc.script,linkedMissions)) {
            for(int id:linkedMissions) {
                top::HistoricalMission m;
                if(npcs.missionByDisplay(id,m))npcMissionList.push_back(std::move(m));
                if(npcMissionList.size()>=128)break;
            }
        }
        play.setNpcMissionList(npcMissionList);
        const bool missionOffered=!testVendor && (!npcMissionList.empty() || play.hasWelcomeMission());
        std::vector<int> stock;
        const bool vendor = npcs.catalog(npc.script, stock) && !stock.empty();
        npcDialogVendor=vendor;
        npcDialogHasMissions=missionOffered;
        top::NpcLuaData::DialogPage firstDialogPage;
        if(npcs.dialogPage(npc.script,1,firstDialogPage) && !firstDialogPage.options.empty()) {
            npcDialogPage=1;
            npcDialogOptions=firstDialogPage.options;
            npcTutorialOpen=!missionOffered;
        }
        // The historical welcome trigger is opt-in (NoRecord -> MultiTrigger),
        // but the only visible path to its page-7 button is commented out in
        // NpcScript01.lua. Expose that same one-time action explicitly here;
        // interacting with Salvier alone must not award/save the gift.
        if(salvierWelcomeNpc && !play.hasHistoricalRecord(500)) {
            top::NpcLuaData::DialogOption gift;
            gift.label="Claim welcome gift (100G + 100-199 EXP)";
            gift.triggerId=503;
            npcDialogOptions.push_back(std::move(gift));
            npcTutorialOpen=true;
        }
        if(mapName=="garner" && npc.script=="r_talk234" &&
           npc.name.find("Cartel")!=std::string::npos && !historicalHairStyles.empty()) {
            top::NpcLuaData::DialogOption hair;
            hair.label="I want to change my hairstyle";
            hair.triggerId=900;
            npcDialogOptions.push_back(std::move(hair));
            npcTutorialOpen=true;
            dialog="Cartel: choose a hairstyle from hairs.txt. The historical materials and gold are charged on selection.";
        }
        // Historical r_talk05 Start(GetMultiTrigger(), 2): Donna offers the
        // special wine only after record 255, while the player lacks item
        // 4072 and has not set record 242. Keep the ordinary shop and fame
        // options intact while exposing this gated local purchase.
        if(mapName=="garner" && npc.script=="r_talk05" &&
           npc.name.find("Donna")!=std::string::npos &&
           play.hasHistoricalRecord(255) && !play.hasBagItem(4072) &&
           !play.hasHistoricalRecord(242)) {
            top::NpcLuaData::DialogOption wine;
            wine.label="Eight Treasures Wine (100000G)";
            wine.triggerId=500;
            npcDialogOptions.push_back(std::move(wine));
        }
        // Historical William trigger 2: offer a replacement token while
        // mission 248 is active, or after record 249 and before record 250;
        // never offer a duplicate already in the bag.
        if(mapName=="garner" && npc.script=="r_talk14" &&
           npc.name.find("William")!=std::string::npos &&
           (play.hasActiveHistoricalMission(248) ||
            (play.hasHistoricalRecord(249) && !play.hasHistoricalRecord(250))) &&
           !play.hasBagItem(4082)) {
            top::NpcLuaData::DialogOption token;
            token.label="Ancient Bounty Token (20000G)";
            token.triggerId=501;
            npcDialogOptions.push_back(std::move(token));
            npcTutorialOpen=true;
            dialog="William: substituo o token de recompensa perdido por 20000G.";
        }
        if(mapName=="garner" && npc.script=="r_talk30" &&
           npc.name.find("Furlington")!=std::string::npos) {
            top::NpcLuaData::DialogOption refine;
            refine.label="Refine Clarion Fragment (10 Clarion Sand + 200G)";
            refine.triggerId=502;
            npcDialogOptions.push_back(std::move(refine));
            npcTutorialOpen=true;
            dialog="Furlington: troco 10 Clarion Sand por um Clarion Fragment mediante 200G.";
        }
        if(mapName=="garner" && npc.script=="r_talk151" && npc.name.find("Sinbad")!=std::string::npos) {
            fillSinbadBoatPage();
            npcTutorialOpen=true;
            dialog="Sinbad: escolhe o casco. O custo e as restricoes usam shipinfo e as pecas historicas.";
        } else if(mapName=="garner" && npc.script=="r_talk152" && npc.name.find("Shirley")!=std::string::npos) {
            fillShirleyBoatPage();
            npcTutorialOpen=true;
            dialog=play.ownedBoatIds().empty()
                ? "Shirley: ainda nao ha barcos seus registrados neste atracadouro. Construa um com Sinbad."
                : "Shirley: selecione um barco para zarpar, reparar ou reabastecer em Argent.";
        }
        if (vendor) play.setVendor(npc.x, npc.y, npc.name, stock);
        if (missionOffered || vendor || !dialog.empty() || npcForgeModes)
            play.talkNpc(npc.name, dialog, vendor, npcForgeModes!=0);
        // Vendor dialogue opens first. Trade opens the shop on demand.

    };
    std::unordered_map<std::string, unsigned> sceneTextures;
    auto sceneTextureFor = [&](const std::string& path) -> unsigned {
        if (path.empty()) return 0;
        const auto it = sceneTextures.find(path);
        if (it != sceneTextures.end()) return it->second;
        unsigned handle = 0;
        top::Image image;
        if (image.load(path)) handle = device.createTexture(image.rgba.data(), image.width, image.height);
        sceneTextures[path] = handle;
        return handle;
    };

    top::TerrainMesh mesh;
    top::SceneMesh sceneMesh;
    top::SceneMesh animatedSceneMesh;
    top::StaticBatchCache cachedTerrain;
    top::StaticBatchCache cachedScene;
    top::StaticBatchCache cachedAnimatedScene;
    top::ActorGpuCache cachedActors;
    auto releaseStatic = [&](unsigned handle) { device.deleteStaticMesh(handle); };
    auto uploadActor = [&](unsigned handle, const top::MeshVertex* vertices, int count) {
        return device.uploadActorMesh(handle, vertices, count);
    };
    auto uploadStatic = [&](unsigned handle, const top::MeshVertex* vertices, int count) {
        return device.uploadStaticMesh(handle, vertices, count);
    };
    top::WorldStreamSchedule worldSchedule;
    float sceneAnimationTime = 0.f;
    constexpr int kSpan = 96;
    float standZ = map.heightAt(actor.x, actor.y);
    auto sceneBlockedAt = [&](float x, float y) {
        if (actors.boatMode()) return false;
        const float terrain = map.heightAt(x, y);
        const float floor = sceneMesh.surfaceAt(x, y, standZ, terrain);
        return sceneMesh.solidAt(x, y, floor) ||
               (floor > terrain + 0.2f && floor > standZ + 0.78f);
    };
    auto actorFootprintBlocked = [&](float x, float y) {
        constexpr float radius = 0.20f;
        auto blocked = [&](float px, float py) {
            return actors.boatMode() ? !map.sailableAt(px, py) :
                   (map.blockedAt(px, py) || sceneBlockedAt(px, py));
        };
        return blocked(x, y) || blocked(x + radius, y) || blocked(x - radius, y) ||
               blocked(x, y + radius) || blocked(x, y - radius);
    };
    bool haveTarget = false;
    std::vector<top::PathPoint> walkPath;
    std::size_t walkAt = 0;
    float walkGoalX = actor.x;
    float walkGoalY = actor.y;
    auto planWalk = [&](float x, float y) {
        walkAt = 0;
        walkGoalX = x;
        walkGoalY = y;
        haveTarget = top::PathFinder::find(map, actor.x, actor.y, x, y, actors.boatMode(), walkPath,
                                           actorFootprintBlocked);
        return haveTarget;
    };

    std::vector<top::MonsterSlot> gmSpawns;
    unsigned gmSpawnCounter=0;
    auto makeRoster=[&]() {
        visualRoster=actorRoster(localMonsters,visualNpcs,actor.x,actor.y);
        visualRoster.insert(visualRoster.end(),gmSpawns.begin(),gmSpawns.end());
    };
    const std::vector<std::string>& travelMaps = availableMaps;
    float transitionFade = 0.f;
    float populationX = actor.x;
    float populationY = actor.y;
    bool devSuppressTravelSave = false;  // profile switch must not overwrite its own location.
    auto travel = [&](const std::string& nextName, float destinationX = -1.f,
                      float destinationY = -1.f, bool fromDevLab = false) {
        const std::string nextPath = findMap(nextName);
        if (nextPath.empty()) return false;
        // Preparar a chegada num mapa separado. Portal/ticket/barco nao
        // podem desmontar o mapa atual se o destino nao tiver terreno valido.
        top::MapFile nextMap;
        if (!nextMap.open(nextPath)) return false;
        // Somente F9 preserva o yaw ao mudar de mapa. Portais e tickets
        // mantem a inicializacao historica existente do destino.
        Actor nextActor = top::dev_sea::arrivalActor(actor, fromDevLab);
        if (destinationX >= 0.f && destinationY >= 0.f) {
            nextActor.x = destinationX;
            nextActor.y = destinationY;
        } else if (!mapSpawn(clientRootFrom(nextPath), nextName, nextActor.x, nextActor.y)) {
            mapSpawn(root, nextName, nextActor.x, nextActor.y);
        }
        const bool boatArrival = actors.boatMode();
        if (boatArrival) {
            if (!nextMap.sailableAt(nextActor.x, nextActor.y) &&
                !placeNearTerrain(nextMap, nextActor, true, 60)) {
                nextMap.close();
                return false;
            }
        } else if (nextMap.blockedAt(nextActor.x,nextActor.y) &&
                   !placeOnLand(nextMap, nextActor)) {
            nextMap.close();
            return false;
        }
        // A posicao de chegada sera gravada DEPOIS do commit no novo mapa.
        map.swap(nextMap);
        nextMap.close(); // libera o antigo mapa somente apos commit
        play.setWorldMap(nextName);
        mapName = nextName;
        gmSpawns.clear();
        mapPath = nextPath;
        mapRoot = clientRootFrom(mapPath);
        actor = nextActor;
        standZ = boatArrival ? actors.boatSeaHeight() : map.heightAt(actor.x, actor.y);
        scene.open(root, mapName, mapRoot);
        monsterSpawns.load(mapName);
        localMonsters = monsterSpawns.nearest(actor.x, actor.y, kMaxLocalMonsters, kLocalMonsterRadius);
        visualNpcs.load(mapName);
        makeRoster();
        reopenActorRoster(playerId);
        if (!actors.minimapReady()) actors.buildMinimapFallback(map);
        actors.setPlayer(actor.x, actor.y, actor.yaw, map.heightAt(actor.x, actor.y), false, 0.f);
        actors.ensurePlaced(map);
        play.bind(actorRoot, actors, profileId, profileName);
        effects.open(root);
        audio.setMap(mapName);
        const top::Tile arrivalAudioTile = map.at(static_cast<int>(actor.x), static_cast<int>(actor.y));
        if (arrivalAudioTile.valid) audio.setArea(arrivalAudioTile.island);
        portals.load(mapName);
        npcs.load(mapName);
        refreshQuestNpcMarkers();
        loadShops();
        placeVendor();
        haveTarget = false;
        walkPath.clear();
        walkAt = 0;
        worldSchedule.invalidate();
        cachedTerrain.clear(releaseStatic);
        cachedScene.clear(releaseStatic);
        cachedAnimatedScene.clear(releaseStatic);
        cachedActors.clear(releaseStatic);
        mesh = top::TerrainMesh{};
        sceneMesh = top::SceneMesh{};
        animatedSceneMesh = top::SceneMesh{};
        transitionFade = 1.f;
        populationX = actor.x;
        populationY = actor.y;
        top::MonsterSpawnRegion closest;
        float distanceToSpawn = 0.f;
        if (monsterSpawns.closestRegion(actor.x, actor.y, closest, distanceToSpawn))
            std::printf("spawns %s: %zu regioes ChaSpn, %zu Lua (%zu scripts), %zu monstros perto; origem #%d a %.0f m\n",
                        mapName.c_str(), monsterSpawns.regions().size(), monsterSpawns.scripted().size(),
                        monsterSpawns.scriptFileCount(), localMonsters.size(), closest.serial, distanceToSpawn);
        else std::printf("spawns %s: sem nascimentos ChaSpn/Lua no pacote\n", mapName.c_str());
        std::printf("transicao offline: %s  %dx%d  %.1f, %.1f\n", mapName.c_str(), map.width(), map.height(), actor.x,
                    actor.y);
        if (!characterSelect && !devSuppressTravelSave && !saveCurrentLocation(false))
            std::fprintf(stderr,"SEA-03: nao foi possivel salvar destino do mapa\n");
        return true;
    };

    auto clock = std::chrono::steady_clock::now();
    float waterAnimation = 0.f;
    float footstepTimer = 0.f;
    int frames = 0;
    top::performance::Meter frameMeter;
    double profileStreamMs = 0.0, profileActorsMs = 0.0, profileRenderMs = 0.0, profileSwapMs = 0.0;
    double profileTerrainMs = 0.0, profileStaticSceneMs = 0.0, profileAnimatedSceneMs = 0.0, profilePlayMs = 0.0;
    std::size_t profileDynamicBytes = 0, profileStaticBytes = 0, profileDraws = 0;
    std::size_t profileActorUploaded = 0, profileActorReused = 0;
    unsigned profileFrames = 0;
    const auto milliseconds = [](std::chrono::steady_clock::time_point a,
                                 std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    bool heldPerfToggle = false;
    top::Input input;
    // A camera nasce na frente do corpo (yaw pi). O corpo olha para +Y.
    // O botao direito e as setas giram so a camera. O clique e o WASD
    // viram o corpo para o passo, com a pose de corrida.
    // pi olha a cara. O desvio tira a palmeira do rosto. Distancia curta e
    // lift baixo mostram a cara, nao so o topo da cabeca.
    float camYaw = kStartCamYaw;
    float camDist = loadedSettings.cameraDistance;
    float camLift = loadedSettings.cameraLift;
    audio.setVolumes(loadedSettings.audioVolume, loadedSettings.audioVolume);
    bool heldI = false;
    bool heldC = false;
    bool heldK = false;
    bool heldAltK = false;
    bool heldAltE = false;
    bool heldAltA = false;
    bool heldAltS = false;
    bool heldAltD = false;
    bool heldAltB = false;
    bool heldAltV = false;
    bool heldAltY = false;
    bool heldSkill[8] = {};
    bool heldE = false;
    bool heldInsert = false;
    bool heldTravel = false;
    bool heldBoat = false;
    bool heldBoatWindow = false;
    bool heldEscape = false;
    bool heldGmExp = false;
    bool heldGmHeal = false;
    bool heldGmSpawn = false;
    bool heldGmItem = false;
    bool heldForge=false,heldSockets=false,heldGold=false,heldCombine=false,heldMilling=false;
    bool confirmForge=false,confirmCombine=false,confirmMilling=false,confirmProduction=false,
         confirmRepair=false,confirmExtractGem=false,confirmTiChun=false,confirmUpgrade=false;
    int confirmStabilizerUid=0, confirmCatalystUid=0;
    int confirmAuxUidA=0, confirmAuxUidB=0;
    bool confirmFromWorkbench=false;
    bool npcClickPending = false;
    top::Npc npcClickTarget{};
    std::string npcClickMap;
    int inventoryPress = -1;
    int skillPressId = 0;
    int skillPressSlot = -1;
    int skillPressX = 0, skillPressY = 0;
    int confirmBagIndex = -1;
    int confirmBagId = 0;
    int confirmBagUid = 0;
    bool confirmSell = false;
    int inventoryPressX = 0;
    int inventoryPressY = 0;
    float portalLock = 1.5f;
    startup.mark("estado do jogo pronto; entrando no loop");
    std::printf("janela aberta. Clique no chao anda, clique no monstro ataca, 1-4 usam skills. E conversa com NPC (Trade abre loja), M viaja. Botoes: bolsa, status e skill (I, C, K). Insert senta. Alt+K abre/fecha o catalogo de skills gratuito para teste; clique + para subir sem comprar livro nem gastar pontos. Arraste item para loja/vazio (confirme); + na skill evolui; arraste skill para F1-F8. Alt+Y escolhe qualquer pet para teste visual sem inventario. Alt+G XP, Alt+H cura, Alt+J invoca monstro, Alt+U concede --gm-item. Alt+F forja; Alt+T combina; Alt+M perfura encaixe (890+891, confirma); Alt+O encaixes GM; Alt+L ouro GM. Alt+B bancada de forja visual; fale com o ferreiro e clique Forge. Passe o mouse sobre itens para ver os atributos.\n");

    auto aspectOf = [&]() {
        return static_cast<float>(window.width()) / static_cast<float>(std::max(window.height(), 1));
    };

    if(petSelectorRequested)openPetSelector();
    auto nextPopulationPreload = std::chrono::steady_clock::now();
    bool populationPreloadNotice = false;
    while (!input.quit) {
        device.resetFrameStats();
        window.pump(input);
        if (input.quit) break;  // no GL draw/swap after WM_DELETE_WINDOW
        if (top::platform::saveRequested())
            top::platform::saveCompleted(characterSelect || saveCurrentLocation(false));
        if (characterSelect && actors.characterCreationStage() == 2 &&
            (!input.textInput.empty() || input.backspacePressed || input.replaceText)) {
            if (input.replaceText)
                for (int i=0;i<24;++i) actors.editCharacterCreateName("",true);
            actors.editCharacterCreateName(input.textInput, input.backspacePressed);
        }
        device.resize(window.width(), window.height());
        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - clock).count();
        clock = now;
        if (dt > 0.05f) dt = 0.05f;
        transitionFade = std::max(0.f, transitionFade - dt * 2.4f);
        portalLock = std::max(0.f, portalLock - dt);
        // Historical controllers advance on individual frames. A single dt
        // jump is not a valid preview of their births/key transitions.
        if (!shotPath.empty()) dt = visualDiagnostic ? (frames==0?0.f:1.f/60.f) : 1.f/30.f;
        waterAnimation += dt * 30.f;
        while (waterAnimation >= 30.f) waterAnimation -= 30.f;
        sceneAnimationTime += dt;
        scene.setAnimationTime(sceneAnimationTime);

        // F9 toggles a developer-only location panel. No game-state toggle
        // is required for normal play; the panel never modifies skill FX.
        const bool f9 = input.key[136];
        if (f9 && !heldF9 && !characterSelect) {
            devPanelOpen = !devPanelOpen;
            if (!devPanelOpen) devCommandEditing = false;
        }
        heldF9 = f9;
        // Editar destino nao deve mover o jogador nem abrir bolsa/skills.
        if (devPanelOpen && devCommandEditing) {
            if (input.key[27]) { devCommandEditing = false; input.key[27] = false; }
            for (int k=1;k<128;++k) if (k!=13) input.key[k]=false;
        }
        const bool alt = input.key[6];
        const bool perfToggle = alt && input.key['p'];
        if (perfToggle && !heldPerfToggle) showPerf = !showPerf;
        heldPerfToggle = perfToggle;
        const bool pressEscape = input.key[27] && !heldEscape;
        heldEscape = input.key[27];
        if (pressEscape && !characterSelect) actors.toggleSystemWindow();
        const bool pressI = input.key['i'] && !heldI;
        const bool pressC = input.key['c'] && !heldC;
        const bool pressK = input.key['k'] && !alt && !heldK;
        const bool pressAltK = input.key['k'] && alt && !heldAltK;
        const bool pressAltE = alt && input.key['e'] && !heldAltE;
        const bool pressAltA = alt && input.key['a'] && !heldAltA;
        const bool pressAltS = alt && input.key['s'] && !heldAltS;
        const bool pressAltD = alt && input.key['d'] && !heldAltD;
        const bool pressAltB = alt && input.key['b'] && !heldAltB;
        const bool pressAltV = alt && input.key['v'] && !heldAltV;
        const bool pressAltY=alt && input.key['y'] && !heldAltY;
        heldAltY=alt && input.key['y'];
        heldI = input.key['i'];
        heldC = input.key['c'];
        heldK = input.key['k'];
        heldAltK = alt && input.key['k'];
        heldAltE = alt && input.key['e'];
        heldAltA = alt && input.key['a'];
        heldAltS = alt && input.key['s'];
        heldAltD = alt && input.key['d'];
        heldAltB = alt && input.key['b'];
        heldAltV = alt && input.key['v'];
        if (pressI || pressAltE) {
            if (play.bankOpen()) {
                play.closeBank();
                if (actors.tempOpen()) actors.togglePanel(4);
            }
            actors.togglePanel(0);
        }
        if (pressC && actors.boatMode()) play.fireBoatCannon(actor.x, actor.y);
        else if (pressC || pressAltA) actors.togglePanel(1);
        if (pressK || pressAltS) actors.togglePanel(2);
        if (pressAltK && !characterSelect) {
            const bool enabled = !play.skillTestMode();
            play.setSkillTestMode(enabled);
            if (enabled) actors.showSkillPanel();
            else actors.hideSkillPanel();
            actors.setLiveLine(6, enabled
                ? "Teste de skills ativo: todas as skills estao liberadas; clique + para subir sem custo."
                : "Teste de skills desativado.");
        }
        if (pressAltD) actors.togglePanel(4);
        if (pressAltB && !characterSelect) actors.toggleForgeWorkbench(window.width(),window.height());
        if(pressAltY && !characterSelect)openPetSelector();
        if (pressAltV && !characterSelect) actors.toggleProductionWorkbench(window.width(),window.height());
        for (int slot = 0; slot < 8; ++slot) {
            const unsigned char key = static_cast<unsigned char>('1' + slot);
            const bool pressed = input.key[key] || input.key[128+slot]; // 1..8 or F1..F8
            if (pressed && !heldSkill[slot]) play.castSkill(slot);
            heldSkill[slot] = pressed;
        }
        const bool gmExp = alt && input.key['g'];
        const bool gmHeal = alt && input.key['h'];
        if(gmExp && !heldGmExp) play.gmAddExperience(gmExperience);
        if(gmHeal && !heldGmHeal) play.gmHeal();
        heldGmExp=gmExp;
        heldGmHeal=gmHeal;
        const bool gmItem=alt && input.key['u'];
        if(gmItem && !heldGmItem && gmItemId>0) play.gmGiveItem(gmItemId);
        heldGmItem=gmItem;
        const bool gmSpawn=alt && input.key['j'];
        if(gmSpawn && !heldGmSpawn && gmSpawns.size()<6 && !actors.boatMode()) {
            const int id=gmMonsterId;
            ++gmSpawnCounter;
            const float x=actor.x+3.f,y=actor.y+3.f;
            if(!map.blockedAt(x,y)) {
                gmSpawns.push_back({id,x,y,false,0.f});
                makeRoster();
                if(reopenActorRoster(playerId)) {
                    actors.setPlayer(actor.x,actor.y,actor.yaw,map.heightAt(actor.x,actor.y),false,0.f);
                    actors.ensurePlaced(map);
                    play.rebindWorld(actors);
                    std::printf("GM offline: invocou ID %d em %.1f,%.1f\n",id,x,y);
                }
            } else std::printf("GM offline: local de invocacao bloqueado\n");
        }
        heldGmSpawn=gmSpawn;
        const bool pressedForge=alt && input.key['f'];
        if(pressedForge && !heldForge && confirmBagIndex<0 &&
           play.forgeCost(forgeEquipIndex)>0 && play.bagItemId(forgeGemIndex)>0 &&
           play.bagItemId(forgeRefinerIndex)>0){
            confirmBagIndex=forgeEquipIndex;
            confirmBagId=play.bagItemId(forgeEquipIndex);
            confirmBagUid=play.bagItemUid(forgeEquipIndex);
            confirmForge=true;
            confirmAuxUidA=play.bagItemUid(forgeGemIndex);
            confirmAuxUidB=play.bagItemUid(forgeRefinerIndex);
        }
        heldForge=pressedForge;
        const bool pressedCombine=alt && input.key['t'];
        if(pressedCombine && !heldCombine && confirmBagIndex<0 &&
           play.bagItemId(combineScrollIndex)>0 && play.bagItemId(combineLeftIndex)>0 &&
           play.bagItemId(combineRightIndex)>0){
            confirmBagIndex=combineScrollIndex;
            confirmBagId=play.bagItemId(combineScrollIndex);
            confirmBagUid=play.bagItemUid(combineScrollIndex);
            confirmCombine=true;
            confirmAuxUidA=play.bagItemUid(combineLeftIndex);
            confirmAuxUidB=play.bagItemUid(combineRightIndex);
        }
        heldCombine=pressedCombine;
        const bool pressedMilling=alt && input.key['m'];
        if(pressedMilling && !heldMilling && confirmBagIndex<0 &&
           play.socketCost(socketEquipIndex)>0 &&
           top::socket_rules::materials(play.bagItemId(socketStabilizerIndex),
                                        play.bagItemId(socketCatalystIndex)) &&
           play.bagItemUid(socketEquipIndex)>0 &&
           play.bagItemUid(socketStabilizerIndex)>0 &&
           play.bagItemUid(socketCatalystIndex)>0){
            confirmBagIndex=socketEquipIndex;
            confirmBagId=play.bagItemId(socketEquipIndex);
            confirmBagUid=play.bagItemUid(socketEquipIndex);
            confirmStabilizerUid=play.bagItemUid(socketStabilizerIndex);
            confirmCatalystUid=play.bagItemUid(socketCatalystIndex);
            confirmMilling=true;
            confirmAuxUidA=confirmStabilizerUid;
            confirmAuxUidB=confirmCatalystUid;
        }
        heldMilling=pressedMilling;
        const bool pressedSockets=alt && input.key['o'];
        if(pressedSockets && !heldSockets)play.gmOpenSockets(forgeEquipIndex,gmSocketCount);
        heldSockets=pressedSockets;
        const bool pressedGold=alt && input.key['l'];
        if(pressedGold && !heldGold)play.gmAddGold(gmGoldAmount);
        heldGold=pressedGold;
        const bool pressE = input.key['e'] && !alt && !heldE;
        heldE = input.key['e'];
        if (input.key[9] && !heldInsert) play.toggleSit();
        heldInsert = input.key[9];
        if (input.key['b'] && !alt && !heldBoat) {
            Actor berth = actor;
            if (!actors.boatMode()) {
                if (placeNearTerrain(map, berth, true) && actors.setBoatMode(true)) {
                    actor = berth;
                    haveTarget = false;
                    play.clearTarget();
                    std::printf("embarcou no Guppy em %.1f, %.1f\n", actor.x, actor.y);
                }
            } else if (placeNearTerrain(map, berth, false) && actors.setBoatMode(false)) {
                actor = berth;
                haveTarget = false;
                play.clearTarget();
                std::printf("desembarcou em %.1f, %.1f\n", actor.x, actor.y);
            }
        }
        heldBoat = input.key['b'];
        if (input.key['n'] && !heldBoatWindow && !actors.boatMode()) actors.toggleBoatWindow();
        heldBoatWindow = input.key['n'];
        if (input.key['m'] && !alt && !heldTravel) {
            auto it = std::find(travelMaps.begin(), travelMaps.end(), mapName);
            std::size_t index = it == travelMaps.end() ? 0 : static_cast<std::size_t>(it - travelMaps.begin() + 1);
            bool moved = false;
            for (std::size_t attempt = 0; attempt < travelMaps.size() && !moved; ++attempt) {
                moved = travel(travelMaps[(index + attempt) % travelMaps.size()]);
            }
            if (moved) portalLock = 1.5f;
        }
        heldTravel = input.key['m'];

        if (input.rightDown) {
            // Rotacao horizontal invertida: arrastar o mouse para a direita
            // gira a camera no sentido contrario, como voce prefere no estilo ToP.
            camYaw -= input.mouseDx * 0.008f;
            camLift += input.mouseDy * 0.002f;
        }
        if (input.key[1]) camYaw -= dt * 1.6f;
        if (input.key[2]) camYaw += dt * 1.6f;
        if (input.key[3]) camLift -= dt * 0.6f;
        if (input.key[4]) camLift += dt * 0.6f;
        // Wheel events belong to the window under the cursor. In the old flow,
        // opening a store intercepted ALL wheel events and the bag could not scroll.
        if (input.wheel != 0 && actors.bagWheel(input.mouseX, input.mouseY, window.width(), window.height()))
            actors.pageBag(-input.wheel);
        else if (input.wheel != 0 && actors.skillWheel(input.mouseX, input.mouseY, window.width(), -input.wheel)) {}
        else if (play.shopOpen() && input.wheel != 0 &&
                 actors.shopWindowHit(input.mouseX, input.mouseY, window.width()) != -1)
            play.scrollShop(-input.wheel);
        else camDist -= static_cast<float>(input.wheel) * 1.4f;
        if (input.key[7]) camDist -= dt * 12.f;
        if (input.key[8]) camDist += dt * 12.f;
        camDist = top::classic_camera::distance(camDist);
        camLift = top::classic_camera::lift(camLift);

        // Um so par para a camera e para o WASD. camYaw 0 olha para +Y.
        // O olho fica atras desse olhar: no yaw pi ele esta no +Y, o lado da face.
        // W anda no olhar. D anda na direita da tela (no yaw pi, isso e +X).
        const float camFwdX = std::sin(camYaw);
        const float camFwdY = std::cos(camYaw);
        const float camRightX = -camFwdY;
        const float camRightY = camFwdX;
        const float feetNow = map.heightAt(actor.x, actor.y);
        const float eyeX0 = actor.x - camFwdX * camDist;
        const float eyeZ0 = actor.y - camFwdY * camDist;
        const float eyeY0 = feetNow + camDist * camLift;
        float view0[16];
        float proj0[16];
        float mvp0[16];
        float inv[16];
        lookAt(eyeX0, eyeY0, eyeZ0, actor.x, feetNow + 1.15f, actor.y, view0);
        perspective(0.9f, aspectOf(), 0.3f, 800.f, proj0);
        multiply(proj0, view0, mvp0);
        if (!shotPath.empty() && !visualDiagnostic && frames == 1) {
            // Para o lado aberto da praca, longe da palmeira.
            planWalk(actor.x - 5.f, actor.y);
        }

        // Um unico tratamento de clique na interface. Antes havia dois blocos
        // (um aqui e outro depois do HUD): o painel abria e fechava no mesmo
        // quadro e a bolsa era clicada duas vezes.
        auto devWarp = [&](Actor desired, bool water) {
                    if (water) {
                        if (!map.sailableAt(desired.x, desired.y)) return false;
                        if (!actors.boatMode() && !actors.setBoatMode(true)) return false;
                    } else {
                        if (map.blockedAt(desired.x,desired.y) &&
                            !placeOnLand(map, desired)) return false;
                        if (actors.boatMode() && !actors.setBoatMode(false)) return false;
                    }
                    actor = desired;
                    haveTarget = false;
                    walkPath.clear();
                    walkAt = 0;
                    play.clearTarget();
                    actors.setPlayer(actor.x, actor.y, actor.yaw,
                                     water ? actors.boatSeaHeight() : map.heightAt(actor.x,actor.y),
                                     false, 0.f);
                    // Rebuild nearby actors from historical map records.
                    localMonsters = monsterSpawns.nearest(actor.x, actor.y,
                                                         kMaxLocalMonsters, kLocalMonsterRadius);
                    makeRoster();
                    reopenActorRoster(playerId);
                    actors.setPlayer(actor.x, actor.y, actor.yaw,
                                     water ? actors.boatSeaHeight() : map.heightAt(actor.x,actor.y), false, 0.f);
                    actors.ensurePlaced(map);
                    play.rebindWorld(actors);
                    worldSchedule.invalidate();
                    populationX=actor.x; populationY=actor.y;
                    saveCurrentLocation(false);
                    return true;
                };
        // SEA-03: so a bancada offline consome esta entrada.
        if (devPanelOpen && devCommandEditing && !characterSelect) {
            if (input.key[27]) {
                devCommandEditing = false;
                input.key[27] = false;
            } else if (input.key[13]) {
                top::dev_sea::Destination command;
                if (!top::dev_sea::parse(devCommand, command)) {
                    actors.setLiveLine(6,"Formato: mapa X Y [terra|barco]");
                } else {
                    const std::string targetPath = findMap(command.location.map);
                    top::MapFile targetMap;
                    if (targetPath.empty() || !targetMap.open(targetPath) ||
                        !top::dev_sea::resolve(targetMap, command)) {
                        actors.setLiveLine(6,"Terreno ou coordenadas invalidos para destino.");
                    } else {
                        const bool boatBefore = actors.boatMode();
                        const bool boatAfter = command.location.boat;
                        bool success = false;
                        if (command.location.map == mapName) {
                            success = devWarp({command.location.x,command.location.y,actor.yaw},boatAfter);
                        } else if (boatAfter == boatBefore || actors.setBoatMode(boatAfter)) {
                            success = travel(command.location.map,command.location.x,command.location.y,
                                             true); // F9: preservar orientacao entre mapas.
                            if (!success && boatBefore != boatAfter) actors.setBoatMode(boatBefore);
                        }
                        actors.setLiveLine(6,success ? "SEA-03: destino alcancado." :
                                                      "SEA-03: falha na viagem.");
                        if (success) devCommandEditing = false;
                    }
                }
            } else {
                if (input.backspacePressed && !devCommand.empty()) devCommand.pop_back();
                for (char c : input.textInput)
                    if (devCommand.size() < 90 &&
                        ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '_' || c == ' ' ||
                         c == '.' || c == '-')) devCommand.push_back(c);
            }
            // Impede que WASD, teclas de inventario e skills consumam os caracteres.
            for (int k=1;k<128;++k) input.key[k]=false;
            input.wheel=0;
        }
        bool clickedUi = false;
        if (input.mousePressed && !input.rightDown) {
            const int sw = window.width();
            const int sh = window.height();
            if (devPanelOpen && input.mouseX >= kDevPanelX &&
                input.mouseX < kDevPanelX + kDevPanelWidth &&
                input.mouseY >= kDevPanelY && input.mouseY < kDevRowY + kDevRows * kDevRowHeight) {
                // This overlay captures its own clicks before any inventory,
                // skill or world picking. Never fabricate a monster/effect.
                clickedUi = true;
                const int row = (input.mouseY - kDevRowY) / kDevRowHeight;
                if (row == 0) {
                    actors.setLiveLine(6, saveCurrentLocation(true) ?
                        "Ponto de retorno registrado (F9)." : "Falha ao registrar ponto.");
                } else if (row == 1) {
                    top::dev_world::Location checkpoint;
                    if (!top::dev_world::read(top::dev_world::path(root, positionIdentity(), true), checkpoint)) {
                        actors.setLiveLine(6, "Nenhum ponto de retorno salvo.");
                    } else if ([&]() {
                        const std::string checkpointPath=findMap(checkpoint.map);
                        top::MapFile checkpointMap;
                        return checkpointPath.empty() || !checkpointMap.open(checkpointPath) ||
                            checkpoint.x>=checkpointMap.width() || checkpoint.y>=checkpointMap.height() ||
                            (checkpoint.boat ? !checkpointMap.sailableAt(checkpoint.x,checkpoint.y) :
                                               checkpointMap.blockedAt(checkpoint.x,checkpoint.y));
                    }()) {
                        actors.setLiveLine(6,"Checkpoint fora de terreno valido.");
                    } else if (checkpoint.map != mapName) {
                        const bool wasBoat = actors.boatMode();
                        if (checkpoint.boat != wasBoat && !actors.setBoatMode(checkpoint.boat)) {
                            actors.setLiveLine(6, "Nao foi possivel alterar o modo naval.");
                        } else if (!travel(checkpoint.map, checkpoint.x, checkpoint.y)) {
                            if (actors.boatMode() != wasBoat) actors.setBoatMode(wasBoat);
                            actors.setLiveLine(6, "Mapa do ponto salvo indisponivel.");
                        } else {
                            actor.yaw = checkpoint.yaw;
                            actors.setPlayer(actor.x,actor.y,actor.yaw,
                                checkpoint.boat ? actors.boatSeaHeight() : map.heightAt(actor.x,actor.y),false,0.f);
                            saveCurrentLocation(false);
                            actors.setLiveLine(6,"Retornou ao mapa do ponto salvo.");
                        }
                    } else {
                        actors.setLiveLine(6, devWarp({checkpoint.x, checkpoint.y, checkpoint.yaw}, checkpoint.boat) ?
                            "Retornou ao ponto salvo." : "Ponto salvo inacessivel.");
                    }
                } else if (row == 2) {
                    Actor found=actor;
                    actors.setLiveLine(6,
                        top::dev_sea::nearestWater(map,actor.x,actor.y,found.x,found.y) &&
                        devWarp(found,true) ?
                        "Area de agua encontrada; barco ativado." :
                        "Nao encontrei agua navegavel perto deste ponto.");
                } else if (row == 3) {
                    top::MonsterSpawnRegion spawn;
                    float distance = 0.f;
                    if (monsterSpawns.closestRegion(actor.x,actor.y,spawn,distance)) {
                        const Actor destination{(spawn.x0+spawn.x1)*.5f,
                                                (spawn.y0+spawn.y1)*.5f,actor.yaw};
                        const bool sea = map.sailableAt(destination.x,destination.y) &&
                                         map.blockedAt(destination.x,destination.y);
                        actors.setLiveLine(6, devWarp(destination, sea) ?
                            "Regiao historica de monstros localizada." : "Falha ao acessar spawn.");
                    } else actors.setLiveLine(6,"Mapa sem regiao de monstros.");
                } else if (row == 4) {
                    const top::Npc* nearest=nullptr;
                    float best=1e30f;
                    for (const auto& npc:npcs.npcs()) {
                        const float d2=(npc.x-actor.x)*(npc.x-actor.x)+(npc.y-actor.y)*(npc.y-actor.y);
                        if(d2<best) {best=d2;nearest=&npc;}
                    }
                    if (nearest) {
                        Actor destination{nearest->x + 1.5f, nearest->y + 1.5f, actor.yaw};
                        actors.setLiveLine(6, devWarp(destination,false) ?
                            "NPC historico mais proximo localizado." : "Falha ao acessar NPC.");
                    } else actors.setLiveLine(6,"Mapa sem NPCs.");
                } else if (row == 5) {
                    Actor destination=actor;
                    if (!mapSpawn(mapRoot,mapName,destination.x,destination.y))
                        mapSpawn(root,mapName,destination.x,destination.y);
                    actors.setLiveLine(6,devWarp(destination,false) ?
                        "Voltou ao ponto inicial deste mapa." : "Nao localizei ponto inicial.");
                } else if (row == 6) {
                    actors.setLiveLine(6,saveCurrentLocation(false) ?
                        "Posicao atual salva para proxima execucao." : "Falha ao salvar posicao.");
                } else if (row == 7) {
                    devCommand = mapName + " ";
                    devCommandEditing = true;
                } else if (row == 8) {
                    if (!travelMaps.empty()) {
                        auto it=std::find(travelMaps.begin(),travelMaps.end(),mapName);
                        const std::size_t next=it==travelMaps.end()?0:
                            (static_cast<std::size_t>(it-travelMaps.begin())+1)%travelMaps.size();
                        actors.setLiveLine(6,travel(travelMaps[next])?
                            "Proximo mapa historico carregado.":"Mapa indisponivel.");
                    }
                } else if (row == 9) {
                    float best=1e30f;
                    Actor found=actor;
                    bool valid=false;
                    for (const auto& region:monsterSpawns.regions()) {
                        const float x=(region.x0+region.x1)*.5f;
                        const float y=(region.y0+region.y1)*.5f;
                        if (!map.sailableAt(x,y)) continue;
                        const float d2=(x-actor.x)*(x-actor.x)+(y-actor.y)*(y-actor.y);
                        if (d2<best) { best=d2;found.x=x;found.y=y;valid=true; }
                    }
                    for (const auto& spawn:monsterSpawns.scripted()) {
                        if (!map.sailableAt(spawn.x,spawn.y)) continue;
                        const float d2=(spawn.x-actor.x)*(spawn.x-actor.x)+
                                       (spawn.y-actor.y)*(spawn.y-actor.y);
                        if (d2<best) { best=d2;found.x=spawn.x;found.y=spawn.y;valid=true; }
                    }
                    actors.setLiveLine(6,valid && devWarp(found,true)?
                        "Spawn marinho real localizado.":"Sem spawn navegavel neste mapa.");
                } else if (row == 10) {
                    const auto p=currentDevLocation();
                    std::printf("SEA-03: %s %.3f %.3f yaw %.3f barco=%d regiao=%u navegavel=%d\n",
                        p.map.c_str(),p.x,p.y,p.yaw,p.boat,
                        static_cast<unsigned>(map.regionAt(p.x,p.y)),map.sailableAt(p.x,p.y));
                    actors.setLiveLine(6,"Dados do terreno impressos no terminal.");
                } else if (row == 11) {
                    devPanelOpen=false;
                    devCommandEditing=false;
                }
            }
            if (confirmBagIndex >= 0) {
                clickedUi = true; // a confirmation dialog blocks ALL world/UI clicks
                if (top::inventory_confirm::hit(input.mouseX,input.mouseY,sw,sh) ==
                        top::inventory_confirm::Click::Confirm &&
                    play.bagItemId(confirmBagIndex) == confirmBagId &&
                    ((!confirmForge && !confirmCombine && !confirmMilling && !confirmProduction && !confirmRepair && !confirmExtractGem && !confirmTiChun && !confirmUpgrade) ||
                     play.bagItemUid(confirmBagIndex)==confirmBagUid) &&
                    (!confirmMilling || (play.bagItemUid(socketStabilizerIndex)==confirmStabilizerUid &&
                                        play.bagItemUid(socketCatalystIndex)==confirmCatalystUid)) &&
                    (!(confirmForge||confirmCombine||confirmMilling||confirmProduction||confirmExtractGem||confirmTiChun||confirmUpgrade) ||
                     (play.bagItemUid(confirmUpgrade?forgeEquipIndex:confirmForge||confirmProduction||confirmExtractGem||confirmTiChun?forgeGemIndex:confirmCombine?combineLeftIndex:socketStabilizerIndex)==confirmAuxUidA &&
                      (confirmAuxUidB==0 || play.bagItemUid(confirmUpgrade?forgeRefinerIndex:confirmForge||confirmProduction?forgeRefinerIndex:confirmCombine?combineRightIndex:socketCatalystIndex)==confirmAuxUidB))) &&
                    (!confirmFromWorkbench || (confirmProduction ?
                         play.previewProduction(confirmBagIndex,forgeGemIndex,forgeRefinerIndex).valid :
                         play.previewWorkbench(confirmUpgrade?7:confirmTiChun?6:confirmExtractGem?5:confirmRepair?4:confirmForge?0:confirmCombine?1:2,
                         confirmBagIndex,
                         confirmUpgrade?forgeEquipIndex:confirmForge||confirmExtractGem||confirmTiChun?forgeGemIndex:confirmCombine?combineLeftIndex:socketStabilizerIndex,
                         confirmUpgrade?forgeRefinerIndex:confirmForge?forgeRefinerIndex:confirmCombine?combineRightIndex:socketCatalystIndex).valid))) {
                    if (confirmMilling) play.socketEquipment(socketEquipIndex,socketStabilizerIndex,socketCatalystIndex);
                    else if (confirmForge) play.forgeBag(forgeEquipIndex,forgeGemIndex,forgeRefinerIndex);
                    else if (confirmCombine) play.combineGems(combineScrollIndex,combineLeftIndex,combineRightIndex);
                    else if (confirmProduction) play.processProduction(confirmBagIndex,forgeGemIndex,forgeRefinerIndex);
                    else if (confirmRepair) play.repairBagItem(confirmBagIndex);
                    else if (confirmExtractGem) play.extractSocketGem(forgeEquipIndex,forgeGemIndex);
                    else if (confirmTiChun) play.upgradeEquipmentTiChun(forgeEquipIndex,forgeGemIndex);
                    else if (confirmUpgrade) play.upgradeEquipmentEffectiveness(confirmBagIndex,forgeEquipIndex,forgeRefinerIndex);
                    else if (confirmSell && play.shopOpen()) play.sellBag(confirmBagIndex);
                    else if (!confirmSell) play.discardBag(confirmBagIndex);
                    if (confirmFromWorkbench) actors.forgeWorkbenchReset();
                }
                // Clicking Cancel or outside the Yes button always cancels.
                confirmBagIndex = -1;
                confirmBagId = 0;
                confirmBagUid=0;
                confirmForge=false;
                confirmCombine=false;
                confirmMilling=false;
                confirmProduction=false;
                confirmRepair=false;
                confirmExtractGem=false;
                confirmTiChun=false;
                confirmUpgrade=false;
                confirmStabilizerUid=confirmCatalystUid=0;
                confirmAuxUidA=confirmAuxUidB=0;
                confirmFromWorkbench=false;
            }
            if (!clickedUi) { // block other panels while confirmation is active
            // 05-AC: bancada sempre recebe cliques antes de outros painéis.
            if (!clickedUi && actors.forgeWorkbenchOpen()) {
                const int action=actors.forgeWorkbenchHit(input.mouseX,input.mouseY,sw,sh);
                if (action>=0) {
                    clickedUi=true;
                    if(action==1)actors.toggleForgeWorkbench(sw,sh);
                    else if(action>=2 && action<=4)actors.forgeWorkbenchMode(action-2);
                    else if(action==6)actors.forgeWorkbenchMode(4);
                    else if(action==7)actors.forgeWorkbenchMode(5);
                    else if(action==8)actors.forgeWorkbenchMode(6);
                    else if(action==9)actors.forgeWorkbenchMode(7);
                    else if(action>=10 && action<=12)actors.forgeWorkbenchClearSlot(action-10);
                    else if(action==5 && confirmBagIndex<0) {
                        const auto indices=actors.forgeWorkbenchIndices();
                        const int mode=actors.forgeWorkbenchModeIndex();
                        const bool production=mode==3;
                        const bool repair=mode==4;
                        const bool extractGem=mode==5;
                        const bool tiChun=mode==6;
                        const bool upgrade=mode==7;
                        const bool valid=production ?
                            play.previewProduction(indices[0],indices[1],indices[2]).valid :
                            play.previewWorkbench(mode,indices[0],indices[1],indices[2]).valid;
                        if(valid){
                            confirmBagIndex=indices[0];
                            confirmBagId=play.bagItemId(indices[0]);
                            confirmBagUid=play.bagItemUid(indices[0]);
                            confirmAuxUidA=play.bagItemUid(indices[1]);
                            confirmAuxUidB=indices[2]>=0?play.bagItemUid(indices[2]):0;
                            confirmForge=mode==0;confirmCombine=mode==1;confirmMilling=mode==2;
                            confirmProduction=production;
                            confirmRepair=repair;
                            confirmExtractGem=extractGem;
                            confirmTiChun=tiChun;
                            confirmUpgrade=upgrade;
                            confirmFromWorkbench=true;
                            if(confirmForge){forgeEquipIndex=indices[0];forgeGemIndex=indices[1];forgeRefinerIndex=indices[2];}
                            if(confirmCombine){combineScrollIndex=indices[0];combineLeftIndex=indices[1];combineRightIndex=indices[2];}
                            if(confirmProduction){forgeEquipIndex=indices[0];forgeGemIndex=indices[1];forgeRefinerIndex=indices[2];}
                            if(confirmExtractGem){forgeEquipIndex=indices[0];forgeGemIndex=indices[1];}
                            if(confirmTiChun){forgeEquipIndex=indices[0];forgeGemIndex=indices[1];}
                            if(confirmUpgrade){forgeEquipIndex=indices[1];forgeRefinerIndex=indices[2];}
                            if(confirmMilling){socketEquipIndex=indices[0];socketStabilizerIndex=indices[1];socketCatalystIndex=indices[2];
                                confirmStabilizerUid=confirmAuxUidA;confirmCatalystUid=confirmAuxUidB;}
                        }
                    }
                }
            }
            const int boatChoice = actors.boatHit(input.mouseX, input.mouseY, sw, sh);
            if (!clickedUi && boatChoice >= 0) {
                clickedUi = true;
                if (boatChoice == 0) actors.closeBoatWindow();
                else {
                    actors.setBoatMode(false, boatChoice);
                    saveSettings(root, {camDist, camLift, audio.musicVolume(), actors.boatId()});
                    actors.setLiveLine(6, "Barco selecionado: " + std::to_string(boatChoice));
                }
            }
            const int systemAction = actors.systemHit(input.mouseX, input.mouseY, sw, sh);
            if (!clickedUi && systemAction != 0) {
                clickedUi = true;
                if (systemAction == -1) actors.closeSystemWindow();
                else if (systemAction == 1) {
                    camDist = camDist < 13.f ? 15.f : (camDist < 18.f ? 22.f : 9.f);
                    saveSettings(root, {camDist, camLift, audio.musicVolume(), actors.boatId()});
                    actors.setLiveLine(6, "Video: distancia da camera alterada");
                } else if (systemAction == 2) {
                    const int next = audio.musicVolume() >= 128 ? 0 : audio.musicVolume() + 32;
                    audio.setVolumes(next, next);
                    saveSettings(root, {camDist, camLift, next, actors.boatId()});
                    actors.setLiveLine(6, "Audio: musica e efeitos " + std::to_string(next * 100 / 128) + "%");
                } else if (systemAction == 3) {
                    camLift = camLift < 1.6f ? 1.85f : (camLift < 2.1f ? 2.4f : 1.35f);
                    saveSettings(root, {camDist, camLift, audio.musicVolume(), actors.boatId()});
                    actors.setLiveLine(6, "Jogo: inclinacao da camera alterada e salva");
                } else if (systemAction == 4 || systemAction == 5) {
                    characterSelect = true;
                    haveTarget = false;
                    play.clearTarget();
                    actors.closeSystemWindow();
                    actors.setCharacterWindow(true);
                } else if (systemAction == 6) {
                    input.quit = true;
                }
            }
            // A grade de teleportes offline tem sua geometria na NpcDialogUI,
            // mas a regra de viagem/mapa continua aqui no loop do jogo.
            if (!clickedUi && !npcTeleportTargets.empty() && play.npcDialogOpen()) {
                const int option = actors.teleportHit(input.mouseX, input.mouseY, sw, sh);
                if (option >= 0) {
                    if (option < static_cast<int>(npcTeleportTargets.size())) {
                        const top::Portal destination = npcTeleportTargets[static_cast<std::size_t>(option)];
                        if (travel(destination.targetMap, destination.targetX, destination.targetY)) {
                            npcTeleportTargets.clear();
                            play.closeDialog();
                            portalLock = 2.f;
                        }
                    }
                    clickedUi = true;
                }
            }
            // Barras de titulo de Skills e Quests: fechar/arrastar primeiro,
            // sem enviar clique ao mundo nem selecionar uma skill.
            if (!clickedUi) {
                const int hit = actors.skillWindowHit(input.mouseX, input.mouseY, sw);
                if (hit == 1) { actors.togglePanel(2); clickedUi = true; }
                else if (hit == 2) { actors.beginSkillDrag(input.mouseX, input.mouseY, sw); clickedUi = true; }
            }
            if (!clickedUi) {
                const int questAction=actors.questListActionAt(input.mouseX,input.mouseY);
                if(questAction>=0){play.selectJournalAction(questAction);clickedUi=true;}
            }
            const int skillPageDelta = clickedUi ? 0 : actors.skillPageHit(input.mouseX, input.mouseY, sw);
            if (!clickedUi && skillPageDelta != 0) {
                actors.changeSkillPage(skillPageDelta);
                clickedUi = true;
            }
            if (!clickedUi && actors.skillTestModeHit(input.mouseX, input.mouseY, sw)) {
                const bool enabled = !play.skillTestMode();
                play.setSkillTestMode(enabled);
                actors.setLiveLine(6, enabled
                    ? "Teste de skills ativo: todas estao listadas; clique + para aprender/subir gratis."
                    : "Teste de skills desativado; evolucao normal restaurada.");
                clickedUi = true;
            }
            if (!clickedUi && actors.questAbandonHit(input.mouseX,input.mouseY)) {
                clickedUi=true;
                play.abandonHistoricalMission();
            }
            if (!clickedUi) {
                const int hit = actors.questWindowHit(input.mouseX, input.mouseY);
                if (hit == 1) { actors.togglePanel(3); clickedUi = true; }
                else if (hit == 2) { actors.beginQuestDrag(input.mouseX, input.mouseY); clickedUi = true; }
            }
            const int upgradeSkill = clickedUi ? -1 : actors.skillUpgradeHit(input.mouseX, input.mouseY, sw);
            if (!clickedUi && upgradeSkill >= 0) {
                clickedUi = true;
                if(play.skillTestMode())play.testIncreaseSkill(upgradeSkill);
                else play.increaseSkill(upgradeSkill);
            }
            const int selectedSkill = clickedUi ? -1 : actors.skillHit(input.mouseX, input.mouseY, sw);
            if (!clickedUi && selectedSkill >= 0) {
                clickedUi = true;
                skillPressId = selectedSkill; // never auto-fill a hotkey on click
                skillPressSlot = -1;
                skillPressX = input.mouseX; skillPressY = input.mouseY;
            }
            if (!clickedUi) {
            const int characterAction = actors.characterHit(input.mouseX, input.mouseY, sw, sh);
            if (characterAction != 0) {
                clickedUi = true;
                if (characterAction >= 1 && characterAction <= 4) {
                    if (characterAction <= static_cast<int>(characterCatalog.profiles().size())) {
                        const auto& profile = characterCatalog.profiles()[static_cast<std::size_t>(characterAction - 1)];
                        selectedProfileId = profile.id;
                        refreshCharacterRoster();
                        if (!showCharacterPreview(profile.raceId)) {
                            restoreCharacterPreview();
                            actors.setLiveLine(6, "Nao foi possivel carregar o modelo 3D desta raca.");
                        }
                    }
                } else if (characterAction == -1) {
                    if (!characterCatalogReady)
                        actors.setLiveLine(6, "Catalogo invalido: preservado sem alteracoes; restaure o arquivo manualmente.");
                    else if (characterCatalog.profiles().size() >= top::OfflineCharacterCatalog::kMaxCharacters)
                        actors.setLiveLine(6, "Todos os quatro slots de personagem estao ocupados.");
                    else if (!birthPointsReady)
                        actors.setLiveLine(6, "Dados historicos de nascimento nao encontrados.");
                    else actors.beginCharacterCreation();
                } else if (characterAction == -2) {
                    if (!characterCatalogReady)
                        actors.setLiveLine(6, "Catalogo invalido: exclusao desativada para preservar os dados.");
                    else if (selectedProfileId && selectedProfileId == profileId)
                        actors.setLiveLine(6, "Nao e possivel excluir o personagem que esta em uso.");
                    else if (selectedProfileId) actors.beginCharacterDeletion();
                } else if (characterAction == -5) {
                    const int deleting = selectedProfileId;
                    if (deleting && characterCatalog.erase(deleting)) {
                        if (selectedProfileId == deleting) selectedProfileId = characterCatalog.activeId();
                        characterCatalog.save();
                        refreshCharacterRoster();
                        actors.cancelCharacterCreation();
                        const auto* selected = characterCatalog.find(selectedProfileId);
                        if (selected && !showCharacterPreview(selected->raceId)) {
                            restoreCharacterPreview();
                            actors.setLiveLine(6, "Nao foi possivel carregar a previa 3D do personagem.");
                        }
                    }
                } else if (characterAction == -6) {
                    refreshCharacterRoster();
                    actors.cancelCharacterCreation();
                    const auto* selected = characterCatalog.find(selectedProfileId);
                    if (!showCharacterPreview(selected ? selected->raceId : playerId)) {
                        restoreCharacterPreview();
                        actors.setLiveLine(6, "Nao foi possivel restaurar a previa 3D do personagem.");
                    }
                } else if (characterAction == -3) {
                    const auto* active = characterCatalog.find(selectedProfileId);
                    if (active) {
                        // Preserve a posicao do personagem de saida. A previa 3D
                        // do seletor recria ActorSkin e desliga temporariamente
                        // o barco; nao deixar esse estado temporario no save.
                        if (profileId > 0) {
                            top::dev_world::Location outgoing=currentDevLocation();
                            if (characterPreviewSession) outgoing.boat=characterPreviewRestoreBoat;
                            if (!top::dev_world::write(
                                    top::dev_world::path(root,profileId),outgoing))
                                std::fprintf(stderr,"SEA-03: erro salvando personagem anterior\n");
                        }
                        const int previousProfileId = profileId;
                        const int previousPlayerId = playerId;
                        const std::string previousProfileName = profileName;
                        characterCatalog.select(active->id);
                        characterCatalog.save();
                        selectedProfileId = active->id;
                        profileId = active->id;
                        profileName = active->name;
                        playerId = active->raceId;
                        characterSelect = false;
                        actors.setCharacterWindow(false);
                        // SEA-03: restaurar posicao propria do personagem, se valida.
                        top::dev_world::Location savedProfile;
                        bool hasSaved = false;
                        if (top::dev_world::read(top::dev_world::path(root,active->id),savedProfile)) {
                            const std::string savedPath=findMap(savedProfile.map);
                            top::MapFile savedMap;
                            hasSaved=!savedPath.empty() && savedMap.open(savedPath) &&
                                savedProfile.x<savedMap.width() && savedProfile.y<savedMap.height() &&
                                (savedProfile.boat ? savedMap.sailableAt(savedProfile.x,savedProfile.y) :
                                                     !savedMap.blockedAt(savedProfile.x,savedProfile.y));
                        }
                        const bool priorBoat=actors.boatMode();
                        const bool boatReady=!hasSaved || priorBoat==savedProfile.boat ||
                                             actors.setBoatMode(savedProfile.boat);
                        devSuppressTravelSave=true;
                        const bool profileTravelOkay=boatReady && (hasSaved ?
                            travel(savedProfile.map,savedProfile.x,savedProfile.y) :
                            travel(active->birthMap,static_cast<float>(active->birthX),
                                                    static_cast<float>(active->birthY)));
                        devSuppressTravelSave=false;
                        if (!profileTravelOkay && actors.boatMode()!=priorBoat)
                            actors.setBoatMode(priorBoat);
                        if (profileTravelOkay && hasSaved) {
                            actor.yaw=savedProfile.yaw;
                            actors.setPlayer(actor.x,actor.y,actor.yaw,
                                savedProfile.boat ? actors.boatSeaHeight() : map.heightAt(actor.x,actor.y),false,0.f);
                        }
                        if (profileTravelOkay && !saveCurrentLocation(false))
                            std::fprintf(stderr,"SEA-03: erro gravando posicao do personagem\n");
                        if (!profileTravelOkay) {
                            profileId = previousProfileId;
                            playerId = previousPlayerId;
                            profileName = previousProfileName;
                            characterCatalog.select(previousProfileId);
                            characterCatalog.save();
                            selectedProfileId = previousProfileId ? previousProfileId : active->id;
                            characterSelect = true;
                            actors.setCharacterWindow(true);
                            actors.setLiveLine(6, "Nao foi possivel carregar o local de nascimento.");
                            restoreCharacterPreview();
                        } else {
                            characterPreviewSession = false;
                        }
                    }
                } else if (characterAction == -4) {
                    if (actors.characterCreationStage() != 0) {
                        actors.cancelCharacterCreation();
                        const auto* selected = characterCatalog.find(selectedProfileId);
                        if (!showCharacterPreview(selected ? selected->raceId : playerId))
                            actors.setLiveLine(6, "Nao foi possivel carregar a previa 3D do personagem.");
                    } else {
                        if (restoreCharacterPreview()) {
                            characterSelect = false;
                            actors.setCharacterWindow(false);
                        } else {
                            actors.setLiveLine(6, "Nao foi possivel restaurar o modelo antes de sair da selecao.");
                        }
                    }
                } else if (characterAction >= 101 && characterAction <= 104) {
                    const int raceId = characterAction - 100;
                    actors.chooseCharacterRace(raceId);
                    if (!showCharacterPreview(raceId)) {
                        restoreCharacterPreview();
                        actors.setLiveLine(6, "Nao foi possivel carregar o modelo 3D desta raca.");
                    }
                } else if (characterAction >= 201 && characterAction <= 203) {
                    const int raceId = actors.characterCreateRace();
                    if (raceId >= 1 && raceId <= 4) {
                        const std::string name = actors.characterCreateName();
                        int createdId = 0;
                        if (!name.empty() && characterCatalog.create(raceId, name, birthCities[characterAction - 201],
                                                    historicalBirthPoints, createdId) &&
                            characterCatalog.save()) {
                            selectedProfileId = createdId;
                            refreshCharacterRoster();
                            actors.cancelCharacterCreation();
                            if (!showCharacterPreview(raceId)) {
                                restoreCharacterPreview();
                                actors.setLiveLine(6, "Personagem criado, mas a previa 3D nao carregou.");
                            }
                        } else actors.setLiveLine(6, "Nao foi possivel criar o personagem neste slot.");
                    }
                }
            } else {
            int dialogAction = play.dialogActionAt(input.mouseX, input.mouseY, sw, sh);
            if (actors.npcTutorialGuideHit(input.mouseX,input.mouseY,sw,sh)>=0) dialogAction=-100;
            else if (npcTutorialOpen && play.npcDialogOpen()) {
                const int pageDelta=actors.npcDialogOptionPageHit(input.mouseX,input.mouseY,sw,sh);
                const int option=actors.npcDialogOptionHit(input.mouseX,input.mouseY,sw,sh);
                const int footer=actors.npcTutorialFooterHit(input.mouseX,input.mouseY,sw,sh);
                if(pageDelta!=0)dialogAction=pageDelta<0?-130:-131;
                else if(option>=0)dialogAction=-110-option;
                else if(footer==1)dialogAction=-120;
                else if(footer==0)dialogAction=-121;
            }
            if (dialogAction <= -100) {
                clickedUi=true;
                if(dialogAction==-100) {
                    npcTutorialOpen=true;
                } else if(dialogAction==-130 || dialogAction==-131) {
                    actors.changeNpcDialogOptionPage(dialogAction==-130?-1:1);
                } else if(dialogAction<=-110 && dialogAction>-120) {
                    const int optionIndex=-110-dialogAction;
                    if(optionIndex>=0 && optionIndex<int(npcDialogOptions.size())) {
                        const auto option=npcDialogOptions[static_cast<std::size_t>(optionIndex)];
                        if(npcDialogScript=="r_talk13" && npcDialogName.find("Monica")!=std::string::npos &&
                           mapName=="garner" && option.label=="Vault (200G Fee)") {
                            // Both historical Argent and Thundoria bank scripts
                            // charge 200G before opening the same offline vault.
                            if(play.openBank()) {
                                actors.openBankPanel();
                                npcTutorialOpen=false;
                                play.closeDialog();
                            }
                        } else if(npcDialogScript=="r_talk29" && npcDialogName.find("Macurdo")!=std::string::npos &&
                           mapName=="garner" && option.label=="Vault (200G Fee)") {
                            if(play.openBank()) {
                                actors.openBankPanel();
                                npcTutorialOpen=false;
                                play.closeDialog();
                            }
                        } else if(npcDialogScript=="leo_talkaa" && npcDialogName.find("Wood Processing Merchant")!=std::string::npos &&
                           mapName=="garner" && option.triggerId>=7000) {
                            std::vector<top::NpcLuaData::ExchangeRule> exchanges;
                            const std::size_t index=static_cast<std::size_t>(option.triggerId-7000);
                            if(npcs.exchangeRules(npcDialogScript,exchanges) && index<exchanges.size()) {
                                const auto& rule=exchanges[index];
                                play.processNpcRecipe({{rule.inputId,rule.inputCount}},0,
                                    {{rule.outputId,rule.outputCount}},"Wood Merchant exchange",false);
                            }
                        } else if(npcDialogScript=="r_talk151" && npcDialogName.find("Sinbad")!=std::string::npos &&
                                  mapName=="garner" && option.triggerId>=5001 && option.triggerId<=5006) {
                            const int shipId=option.triggerId-5000;
                            if(play.buildHistoricalBoat(shipId)) {
                                npcTutorialOpen=false;
                                play.closeDialog();
                            }
                        } else if(npcDialogScript=="r_talk152" && npcDialogName.find("Shirley")!=std::string::npos &&
                                  mapName=="garner" && option.triggerId>=6000 &&
                                  option.triggerId-6000<static_cast<int>(play.ownedBoatIds().size())) {
                            const int shipId=play.ownedBoatIds()[static_cast<std::size_t>(option.triggerId-6000)];
                            if(actors.boatMode()) {
                                actors.setLiveLine(6,"Voce ja esta navegando.");
                            } else {
                                Actor berth=actor;
                                berth.x=22.60f;berth.y=28.29f;berth.yaw=177.f;
                                if(!placeNearTerrain(map,berth,true,12)) {
                                    actors.setLiveLine(6,"Nao ha agua navegavel no atracadouro historico de Argent.");
                                } else if(!actors.setBoatMode(true,shipId)) {
                                    actors.setLiveLine(6,"Nao foi possivel carregar o casco historico.");
                                } else {
                                    actor=berth;
                                    standZ=actors.boatSeaHeight();
                                    haveTarget=false;walkPath.clear();play.clearTarget();
                                    actors.setPlayer(actor.x,actor.y,actor.yaw,0.f,false,0.f);
                                    saveSettings(root,{camDist,camLift,audio.musicVolume(),actors.boatId()});
                                    if(!saveCurrentLocation(false))
                                        actors.setLiveLine(6,"Zarpe concluido, mas a localizacao nao foi salva.");
                                    else
                                        portalLock=2.f;
                                    npcTutorialOpen=false;
                                    play.closeDialog();
                                }
                            }
                        } else if(npcDialogScript=="r_talk152" && npcDialogName.find("Shirley")!=std::string::npos &&
                                  mapName=="garner" && option.triggerId>=6100 && option.triggerId<6200 &&
                                  option.triggerId-6100<static_cast<int>(play.ownedBoatIds().size())) {
                            if(play.repairOwnedBoat(play.ownedBoatIds()[static_cast<std::size_t>(option.triggerId-6100)])) {
                                npcTutorialOpen=false;
                                play.closeDialog();
                            }
                        } else if(npcDialogScript=="r_talk152" && npcDialogName.find("Shirley")!=std::string::npos &&
                                  mapName=="garner" && option.triggerId>=6200 && option.triggerId<6300 &&
                                  option.triggerId-6200<static_cast<int>(play.ownedBoatIds().size())) {
                            if(play.refuelOwnedBoat(play.ownedBoatIds()[static_cast<std::size_t>(option.triggerId-6200)])) {
                                npcTutorialOpen=false;
                                play.closeDialog();
                            }
                        } else if((option.directAction=="OpenForge" || option.directAction=="OpenUnite" ||
                                   option.directAction=="OpenMilling" || option.directAction=="OpenRepair" ||
                                   option.directAction=="OpenGetStone" || option.directAction=="OpenItemTiChun" ||
                                   option.directAction=="OpenUpgrade") &&
                                  npcForgeModes!=0 && mapName=="garner") {
                            actors.openForgeWorkbench(sw,sh,npcForgeModes);
                            npcForgeModes=0;
                            npcTeleportTargets.clear();
                            npcTutorialOpen=false;
                            play.closeDialog();
                        } else if(option.directAction=="Teleport" && !option.actionTarget.empty() &&
                                  npcDialogScript=="TeleportCity" && npcDialogName.find("Jovial")!=std::string::npos &&
                                  mapName=="garner") {
                            bool arrived=false;
                            for(const top::BirthPoint& destination:historicalBirthPoints) {
                                if(destination.city!=option.actionTarget || findMap(destination.map).empty())continue;
                                if(travel(destination.map,static_cast<float>(destination.x),
                                          static_cast<float>(destination.y))) {
                                    arrived=true;
                                    break;
                                }
                            }
                            if(arrived) {
                                npcTeleportTargets.clear();
                                npcTutorialOpen=false;
                                play.closeDialog();
                                portalLock=2.f;
                            } else {
                                actors.setLiveLine(6,"Rota historica indisponivel ou bloqueada; o teleporte nao foi concluido.");
                            }
                        } else if(option.directAction=="SetSpawnPos" && !option.actionTarget.empty() &&
                                  ((npcDialogScript=="TeleportCity" && npcDialogName.find("Jovial")!=std::string::npos) ||
                                   (npcDialogScript=="r_talk87" && npcDialogName.find("Arena Administrator")!=std::string::npos)) &&
                                  mapName=="garner") {
                            const auto destination=std::find_if(historicalBirthPoints.begin(),historicalBirthPoints.end(),
                                [&](const top::BirthPoint& point){return point.city==option.actionTarget;});
                            top::dev_world::Location spawn;
                            if(destination==historicalBirthPoints.end() || findMap(destination->map).empty()) {
                                actors.setLiveLine(6,"Ponto de retorno historico indisponivel neste cliente.");
                            } else {
                                spawn.map=destination->map;
                                spawn.x=static_cast<float>(destination->x);
                                spawn.y=static_cast<float>(destination->y);
                                spawn.yaw=actor.yaw;
                                if(top::dev_world::write(historicalSpawnPath(),spawn)) {
                                    actors.setLiveLine(6,"Ponto de retorno registrado: "+option.actionTarget+".");
                                    npcTutorialOpen=false;
                                    play.closeDialog();
                                } else {
                                    actors.setLiveLine(6,"Nao foi possivel salvar o ponto de retorno.");
                                }
                            }
                        } else if(option.directAction=="GetChaName_4" && option.actionParameter==1 &&
                           npcDialogScript=="r_talk242" && npcDialogName.find("Bingo")!=std::string::npos &&
                           mapName=="garner") {
                            // Historical GetChaName_4: consume the first
                            // available voucher in script order and grant ten
                            // items 3077, matching the original GiveItem call.
                            play.processNpcRecipeAnyInput({2242,2243,2244,2245},0,{{3077,10}},
                                                          "Bingo - voucher por 10 Honey Chocolate");
                        } else if(option.directAction=="TransferDiamond" &&
                                  npcDialogScript=="r_talk242" && npcDialogName.find("Bingo")!=std::string::npos &&
                                  mapName=="garner") {
                            play.redeemNpcDiamondVoucher(option.actionParameter);
                        } else if(option.directAction=="GetChaName_5" && option.actionParameter==1 &&
                           npcDialogScript=="r_talk242" && npcDialogName.find("Bingo")!=std::string::npos &&
                           mapName=="garner") {
                            // Historical GetChaName_5: exchange one Pirate
                            // Voucher 6 for one Morph Runestone, requiring an
                            // unlocked bag and one free slot.
                            play.processNpcRecipe({{2240,1}},0,{{1028,1}},"Bingo - Morph Runestone",true);
                        } else if(option.triggerId>0) {
                            if(npcDialogScript=="r_talk234" && npcDialogName.find("Cartel")!=std::string::npos &&
                               mapName=="garner" && option.triggerId==900) {
                                fillHairStylePage(1);
                                play.talkNpc(npcDialogName,"Select a hairstyle. The listed materials and gold come from hairs.txt.",false,false);
                            } else if(npcDialogScript=="r_talk234" && npcDialogName.find("Cartel")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>=1001 && option.triggerId<=2000) {
                                const int serial=option.triggerId-1000;
                                const auto style=std::find_if(historicalHairStyles.begin(),historicalHairStyles.end(),
                                    [&](const HistoricalHairStyle& row){return row.serial==serial;});
                                if(style!=historicalHairStyles.end() && playerId>=1 && playerId<=4 &&
                                   style->usable[static_cast<std::size_t>(playerId-1)]!=0 &&
                                   play.applyHistoricalHairStyle(style->itemId,style->materials,style->gold)) {
                                    actors.setLiveLine(6,"Penteado historico aplicado. A chance de falha original nao consta nos dados disponiveis.");
                                    npcTutorialOpen=false;
                                    play.closeDialog();
                                }
                            } else if(npcDialogScript=="r_talk05" && npcDialogName.find("Donna")!=std::string::npos &&
                               mapName=="garner" && option.triggerId==500) {
                                if(option.label=="Eight Treasures Wine (100000G)" &&
                                   play.hasHistoricalRecord(255) && !play.hasBagItem(4072) &&
                                   !play.hasHistoricalRecord(242) &&
                                   play.processNpcRecipe({},100000,{{4072,1}},"Donna - Eight Treasures Wine",true)) {
                                    npcTutorialOpen=false;
                                    play.closeDialog();
                                }
                            } else if(npcDialogScript=="r_talk14" && npcDialogName.find("William")!=std::string::npos &&
                               mapName=="garner" && option.triggerId==501) {
                                const bool eligible=play.hasActiveHistoricalMission(248) ||
                                    (play.hasHistoricalRecord(249) && !play.hasHistoricalRecord(250));
                                if(eligible && !play.hasBagItem(4082) &&
                                   play.processNpcRecipe({},20000,{{4082,1}},
                                       "William - Ancient Bounty Token",true)) {
                                    npcTutorialOpen=false;
                                    play.closeDialog();
                                }
                            } else if(npcDialogScript=="r_talk30" && npcDialogName.find("Furlington")!=std::string::npos &&
                               mapName=="garner" && option.triggerId==502) {
                                if(play.processNpcRecipe({{1784,10}},200,{{1785,1}},
                                   "Furlington - Clarion Crystal",false)) {
                                    npcTutorialOpen=false;
                                    play.closeDialog();
                                }
                            } else if(npcDialogScript=="r_talk05" && npcDialogName.find("Donna")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>=8100 && option.triggerId<=8108) {
                                struct Reward { int fame; int item; int count; const char* name; };
                                static constexpr Reward rewards[]={{20,2602,1,"Novice Endeavor Emblem"},
                                    {200,2603,1,"Standard Endeavor Emblem"},
                                    {2000,2604,1,"Expert Endeavor Emblem"},
                                    {500,855,5,"Fairy Coin x5"},
                                    {5000,3094,3,"Amplifier of Strive x3"},
                                    {20000,610,1,"Novice Self Destruct"},
                                    {50000,885,1,"Level 1 Refining Gem"},
                                    {200000,862,1,"Gem of Colossus"},
                                    {600000,1012,1,"Gem of Soul"}};
                                const Reward& reward=rewards[static_cast<std::size_t>(option.triggerId-8100)];
                                play.redeemNpcFameReward(reward.fame,reward.item,reward.count,reward.name);
                            } else if(npcDialogScript=="TeleportIsland" && option.triggerId>=3000 &&
                               option.triggerId-3000<static_cast<int>(historicalIslandDestinations.size())) {
                                const auto& destination=historicalIslandDestinations[
                                    static_cast<std::size_t>(option.triggerId-3000)];
                                if(!play.historicalNpcTeleportFareReady()) {
                                    actors.setLiveLine(6,"You need at least 500G to teleport.");
                                } else if(travel(destination.targetMap,destination.targetX,destination.targetY)) {
                                    if(play.payHistoricalNpcTeleportFare()) {
                                        historicalIslandDestinations.clear();
                                        npcTutorialOpen=false;
                                        play.closeDialog();
                                        portalLock=2.f;
                                    }
                                } else {
                                    actors.setLiveLine(6,"O destino historico nao pode ser carregado; nenhuma tarifa foi cobrada.");
                                }
                            } else if(npcDialogScript=="r_talk01" && npcDialogName.find("Salvier")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId==503) {
                                if(play.claimArgentWelcomeGift()) {
                                    npcTutorialOpen=false;
                                    play.closeDialog();
                                } else {
                                    actors.setLiveLine(6,"Presente de boas-vindas indisponivel ou save falhou; nenhum premio novo foi concedido.");
                                }
                            } else if(npcDialogScript=="TOP_TEST_PET") {
                                const auto selected=std::find_if(historicalPets.begin(),historicalPets.end(),[&](const auto& p){return p.itemId==option.triggerId-100000;});
                                if(option.triggerId>=100000 && (selected==historicalPets.end() || !selected->definitionAvailable)) {
                                    npcTutorialOpen=false;
                                    play.talkNpc("Pet sem imagem","Este pet esta na lista de itens, mas a definicao visual dele nao consta nos arquivos deste cliente. Nenhum item foi adicionado. Alt+Y reabre a lista.",false,false);
                                } else {
                                if(option.triggerId==1)previewPetItem=-1;
                                else if(option.triggerId==2)previewPetItem=0;
                                else if(option.triggerId>=100000)previewPetItem=option.triggerId-100000;
                                npcTutorialOpen=false;play.closeDialog();
                                }
                            } else if(npcDialogScript=="TOP_TEST_CLASS" && option.triggerId==300) {
                                openPetSelector();
                            } else if(npcDialogScript=="TOP_TEST_CLASS" && option.triggerId>=101 && option.triggerId<=118) {
                                if(!play.testSetJob(option.triggerId-100))
                                    actors.setLiveLine(6,"Nao foi possivel alterar a classe neste momento.");
                                else {
                                    npcTutorialOpen=false;
                                    play.closeDialog();
                                }
                            } else if(npcDialogScript=="TOP_TEST_CLASS" && option.triggerId==200) {
                                const bool enabled=!play.skillTestMode();
                                play.setSkillTestMode(enabled);
                                if(enabled)actors.showSkillPanel();else actors.hideSkillPanel();
                                npcTutorialOpen=false;
                                play.closeDialog();
                            } else if(npcDialogScript=="r_talk03" && npcDialogName.find("Ditto")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                // Historical r_talk03: Ditto's four recovery
                                // decoctions each consume one bottle, ten
                                // recipe-specific herbs and 50G for one item.
                                int herb=0, product=0;
                                if(option.label=="Decoct Liquorice Potion") {herb=3129;product=3133;}
                                else if(option.label=="Brew Energetic Tea") {herb=3130;product=3134;}
                                else if(option.label=="Decoct Special Ointment") {herb=3131;product=3135;}
                                else if(option.label=="Make Snowy Soft Bud") {herb=3132;product=3136;}
                                if(herb>0) {
                                    play.processNpcRecipe({{1779,1},{herb,10}},50,{{product,1}},option.label);
                                } else {
                                    actors.setLiveLine(6,"Receita historica de Ditto nao reconhecida; nenhum item foi consumido.");
                                }
                            } else if(npcDialogScript=="r_talk07" && npcDialogName.find("Jimberry")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                int material=0, fee=10000;
                                if(option.label.find("Crystal Eolith to change a Reality Mask")!=std::string::npos) material=4031;
                                else if(option.label.find("Teak Wood Log to change a Reality Mask")!=std::string::npos) material=3991;
                                else if(option.label.find("Bubble Fish to change a Reality Mask")!=std::string::npos) {
                                    material=4011;
                                    // Historical Lua checks HasMoney(10000), then calls TakeMoney(50000).
                                    // Require the explicit debit before consuming any of the inputs.
                                    fee=50000;
                                }
                                if(material>0) {
                                    play.processNpcRecipe({{2588,1},{material,10}},fee,
                                                          {{2326,1}},"Jimberry - Reality Mask",true);
                                } else {
                                    actors.setLiveLine(6,"Troca de Reality Mask nao reconhecida; nenhum recurso foi consumido.");
                                }
                            } else if(npcDialogScript=="r_talk02" && npcDialogName.find("Beldi")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                // Historical r_talk02 lantern exchange: 15 Red
                                // Paper, 15 Bamboo Sticks and one Strange
                                // Candle for one Lantern, with a free slot
                                // required before any material is consumed.
                                if(option.label=="Confirm trade") {
                                    play.processNpcRecipe({{3904,15},{3911,15},{4450,1}},0,
                                                          {{3342,1}},"Beldi - Lantern",true);
                                } else {
                                    actors.setLiveLine(6,"Troca historica de Beldi nao reconhecida; nenhum recurso foi consumido.");
                                }
                            } else if(npcDialogScript=="r_talk09" && npcDialogName.find("Marriam")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                // Historical r_talk09 cooking and hair-colorant
                                // recipes. SetRecord is committed with the meal
                                // so a failed save cannot charge twice or lose inputs.
                                if(option.triggerId==510) {
                                    play.processNpcRecipe({{2886,10}},10000,{{2895,1}},
                                        "Marriam - Turkey",false);
                                } else if(option.triggerId>=511 && option.triggerId<=513) {
                                    const int tier=option.triggerId-511;
                                    const int priorRecord=tier==0?0:(tier==1?815:816);
                                    const int record=815+tier;
                                    const int fee=tier==0?10000:(tier==1?1000000:10000000);
                                    const bool gate=(tier==0?!play.hasHistoricalRecord(815):
                                        play.hasHistoricalRecord(priorRecord) && !play.hasHistoricalRecord(record));
                                    if(gate)
                                        play.processNpcRecipe({{2895,10},{2894,1}},fee,{{2898,1}},
                                            "Marriam - Christmas Meal",false,0,record);
                                    else
                                        actors.setLiveLine(6,"Etapa de Christmas Meal ja concluida ou fora de ordem; nenhum recurso foi consumido.");
                                } else if(option.triggerId==514 && play.level()<2) {
                                    if(play.grantMarriamPkTest()) {
                                        npcTutorialOpen=false;
                                        play.closeDialog();
                                    }
                                } else {
                                    int dye=0, product=0;
                                    if(option.label=="Decoct Red Colorant") {dye=1787;product=1797;}
                                    else if(option.label=="Decoct Orange Colorant") {dye=1788;product=1798;}
                                    else if(option.label=="Decoct Black Colorant") {dye=4606;product=4607;}
                                    else if(option.label=="Decoct Brown Colorant") {dye=4608;product=4609;}
                                    if(dye>0) {
                                        play.processNpcRecipe({{dye,5},{1657,1},{1778,1}},200,{{product,1}},option.label);
                                    } else {
                                        actors.setLiveLine(6,"Receita historica de Marriam nao reconhecida; nenhum item foi consumido.");
                                    }
                                }
                            } else if(npcDialogScript=="r_talk04" && npcDialogName.find("Gina")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                const int greaterHealCost=play.hasNewcomerHealingEntitlement()?0:200;
                                const int healCost=play.hasNewcomerHealingEntitlement()?0:50;
                                if(option.label=="Greater Heal") play.receiveNpcHealing(greaterHealCost,0,true);
                                else if(option.label=="Heal") play.receiveNpcHealing(healCost,50,false);
                                else actors.setLiveLine(6,"Servico historico de Gina nao reconhecido; nenhum recurso foi consumido.");
                            } else if(npcDialogScript=="r_talk08" && npcDialogName.find("Margaret")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                // Historical r_talk08: five food/drink recipes,
                                // each using one Glass (1777) and ten matching
                                // ingredients. The historical handler charges
                                // no gold for these recipes.
                                int ingredient=0, product=0;
                                if(option.label=="Mix Elven Fruit Juice") {ingredient=3116;product=3122;}
                                else if(option.label=="Brew Red Date Tea") {ingredient=3117;product=3123;}
                                else if(option.label=="Make Mushroom Soup") {ingredient=3118;product=3124;}
                                else if(option.label=="Mix Stramonium Juice") {ingredient=3119;product=3125;}
                                else if(option.label=="Make Ice Cream") {ingredient=3120;product=3126;}
                                if(ingredient>0) {
                                    play.processNpcRecipe({{1777,1},{ingredient,10}},0,{{product,1}},option.label);
                                } else {
                                    actors.setLiveLine(6,"Receita historica de Margaret nao reconhecida; nenhum item foi consumido.");
                                }
                            } else if(npcDialogScript=="r_talk252" && npcDialogName.find("Cleaner")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                if(option.label=="Clean Mud") play.cleanWithNpcWasher();
                                else actors.setLiveLine(6,"Opcao historica do Cleaner nao reconhecida; nenhum recurso foi consumido.");
                            } else if(npcDialogScript=="r_talk06" && npcDialogName.find("Little Daniel")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                // Historical r_talk06: Ancient Generator
                                // exchange. Its map destination is outside
                                // this change; the Argent transaction itself
                                // is fully described by the Lua trigger.
                                if(option.label=="Exchange") {
                                    play.processNpcRecipe({{3933,10}},5000,{{1812,1}},"Ancient Generator",true);
                                } else {
                                    actors.setLiveLine(6,"Troca historica de Little Daniel nao reconhecida; nenhum recurso foi consumido.");
                                }
                            } else if(npcDialogScript=="r_talk242" && npcDialogName.find("Bingo")!=std::string::npos &&
                                      mapName=="garner" && option.triggerId>0) {
                                // r_talk242 has several independent Argent
                                // exchanges with fully specified item inputs
                                // and outputs. Each source trigger requires an
                                // unlocked bag and one free slot; processNpcRecipe
                                // keeps both materials and output atomic.
                                bool recognized = true;
                                if(option.label=="Confirm to combine") {
                                    play.processNpcRecipe({{3905,5},{3907,1}},0,{{3906,1}},"Bingo - Wishing Stone",true);
                                } else if(option.label=="Confirm trade") {
                                    play.processNpcRecipe({{3342,2},{3337,1}},0,{{3335,1}},"Bingo - Fairy March",true);
                                } else if(option.label=="Exchange 10 Locks of Mystic") {
                                    play.processNpcRecipe({{3915,99}},0,{{2440,10}},"Bingo - Locks of Mystic",true);
                                } else if(option.label=="Exchange one Treasure Map") {
                                    play.processNpcRecipe({{3915,200}},0,{{1092,1}},"Bingo - Treasure Map",true);
                                } else if(option.label=="I have Pirate Voucher 8") {
                                    play.processNpcRecipe({{2238,1}},0,{{2306,1}},"Bingo - Pirate Voucher 8",true);
                                } else if(option.label=="I have Pirate Voucher 9") {
                                    play.processNpcRecipe({{2237,1}},0,{{855,99}},"Bingo - Pirate Voucher 9",true);
                                } else if(option.label=="I have Pirate Voucher 7") {
                                    play.processNpcRecipe({{2239,1}},0,{{1001,1}},"Bingo - Pirate Voucher 7",true);
                                } else if(option.label=="I have Pirate Voucher 5") {
                                    play.processNpcRecipe({{2241,1}},0,{{578,1}},"Bingo - Pirate Voucher 5",true);
                                } else {
                                    recognized = false;
                                }
                                if(!recognized)
                                    actors.setLiveLine(6,"Esta troca do Bingo ainda nao tem regra local; nenhum item foi consumido.");
                            } else if(npcDialogScript=="BT_NewUser001" && npcDialogPage==31 && option.triggerId==1) {
                                const auto result=play.grantSennaUnityChest();
                                if(result==top::Play::StarActionResult::Ineligible) {
                                    top::NpcLuaData::DialogPage failurePage;
                                    if(npcs.dialogPage(npcDialogScript,6,failurePage)) {
                                        npcDialogPage=6;
                                        npcDialogOptions=failurePage.options;
                                        play.talkNpc(npcDialogName,failurePage.text,npcDialogVendor,npcForgeModes!=0);
                                    }
                                }
                            } else {
                                actors.setLiveLine(6,"Acao condicional deste dialogo ainda nao esta disponivel offline.");
                            }
                        } else if(option.teamStarTransfer) {
                            if(npcDialogScript=="BT_NewUser001" && npcDialogPage==32)
                                play.redeemTeamStar();
                            else actors.setLiveLine(6,"Resgate Star of Unity indisponivel neste NPC.");
                        } else if(option.close) {
                            npcTutorialOpen=false;
                            if(!npcDialogHasMissions)play.closeDialog();
                        } else {
                            if(npcDialogScript=="r_talk05" && npcDialogName.find("Donna")!=std::string::npos &&
                               mapName=="garner" && (option.nextPage==5 || option.nextPage==9007 || option.nextPage==9008)) {
                                fillDonnaRewardPage(option.nextPage);
                                play.talkNpc(npcDialogName,"Donna: current reputation is "+
                                    std::to_string(play.fame())+". Redeeming a reward deducts its listed points.",
                                    npcDialogVendor,npcForgeModes!=0);
                            } else if(npcDialogScript=="r_talk234" && npcDialogName.find("Cartel")!=std::string::npos &&
                               option.nextPage>=101 && option.nextPage<200) {
                                fillHairStylePage(option.nextPage-100);
                                play.talkNpc(npcDialogName,"Select a hairstyle. The listed materials and gold come from hairs.txt.",false,false);
                            } else if(npcDialogScript=="TOP_TEST_CLASS" && option.nextPage>0) {
                                fillClassTestPage(option.nextPage);
                                const char* text="Choose a profession. Selecting one changes and saves the current character class immediately.";
                                play.talkNpc(npcDialogName,text,false,false);
                            } else if(npcDialogScript=="TeleportIsland" && option.nextPage>0) {
                                fillIslandTeleportPage(option.nextPage);
                                play.talkNpc(npcDialogName,
                                    "Silvius: choose an island destination. Teleport is free through level 40; above that it costs 500G.",
                                    false,false);
                            } else {
                            top::NpcLuaData::DialogPage page;
                            if(npcs.dialogPage(npcDialogScript,option.nextPage,page)) {
                                npcDialogPage=option.nextPage;
                                npcDialogOptions=page.options;
                                play.talkNpc(npcDialogName,page.text,npcDialogVendor,npcForgeModes!=0);
                            } else actors.setLiveLine(6,"Esta pagina de dialogo nao existe no roteiro local.");
                            }
                        }
                    }
                } else if(dialogAction==-120) {
                    npcTutorialOpen=false;
                } else if(dialogAction==-121) {
                    npcTutorialOpen=false;
                    play.closeDialog();
                }
            } else if (dialogAction >= 0) {
                clickedUi = true;
                if(dialogAction==4 && npcForgeModes && npcForgeMap==mapName &&
                    std::hypot(actor.x-npcForgeOrigin.x, actor.y-npcForgeOrigin.y)<=2.4f){
                    // Item operations remain in Play and still need Confirm.
                    play.clickDialog(dialogAction);
                    actors.openForgeWorkbench(sw,sh,npcForgeModes);
                    npcForgeModes=0;
                    npcTeleportTargets.clear();
                } else {
                    play.clickDialog(dialogAction);
                    // Mission acceptance and explicit hand-in can affect
                    // world-space markers and eligibility immediately.
                    if(dialogAction==1 || dialogAction==5)refreshQuestNpcMarkers();
                }
                if (dialogAction == 3 && play.shopOpen())
                    actors.showShopInventory(sw);
                // Fechar a conversa também fecha as opções temporárias do
                // teleporter; elas não podem capturar as teclas 1..4 depois.
                if (!play.npcDialogOpen() || dialogAction == 4) {
                    npcTeleportTargets.clear();npcForgeModes=0;
                    if(!play.npcDialogOpen()) {
                        npcTutorialOpen=false;
                        npcDialogOptions.clear();
                        npcDialogPage=0;
                        npcDialogHasMissions=false;
                    }
                }
            } else {
            const int mapQuestAction = play.npcDialogOpen() ? -1 :
                actors.mapQuestHit(input.mouseX,input.mouseY,sw,sh);
            if(mapQuestAction>=0) {
                actors.mapQuestAction(mapQuestAction);
                clickedUi=true;
            } else {
            const int shop = actors.shopWindowHit(input.mouseX, input.mouseY, sw);
            if (shop == -3) {
                clickedUi = true;
                actors.beginShopDrag(input.mouseX, input.mouseY, sw);
            } else if (shop == -2) {
                clickedUi = true;
                play.closeShop();
            } else if (shop >= 0) {
                clickedUi = true;
                play.clickShop(shop);
            } else {
            const int state = actors.stateHit(input.mouseX, input.mouseY);
            if (state >= 0) {
                clickedUi = true;
                if (state == 1) actors.closeState();
                else if (state >= 2 && state <= 6) play.spendAttr(state - 2);
                else if (state == 7) play.setJob(1);
                else if (state == 8) play.setJob(2);
                else if (state == 9) play.setJob(4);
                else if (state == 10) play.setJob(5);
                else if (state == 11) actors.beginStateDrag(input.mouseX, input.mouseY);
            } else {
            const int shortcut = actors.shortcutHit(input.mouseX, input.mouseY, sw, sh);
            if (shortcut >= 0) {
                clickedUi = true;
                if (shortcut <= 3) actors.togglePanel(shortcut);  // bolsa, status, skills, missoes
                else if (shortcut >= 10 && shortcut <= 17) {
                    skillPressId = play.skillAtSlot(shortcut-10);
                    skillPressSlot = shortcut-10;
                    skillPressX = input.mouseX; skillPressY = input.mouseY;
                    // A short click casts on mouse release; moving starts drag.
                }
            } else {
                const int bag = actors.invHit(input.mouseX, input.mouseY, sw, sh);
                if (bag >= 0) {
                    clickedUi = true;
                    if (bag == 1) {
                        if (play.bankOpen()) {
                            play.closeBank();
                            if (actors.tempOpen()) actors.togglePanel(4);
                        }
                        actors.closeInv();
                    }
                    else if (bag == 2) actors.beginInvDrag(input.mouseX, input.mouseY);
                    else if (bag == 3) actors.pageBag(-1);
                    else if (bag == 4) actors.pageBag(1);
                    else if (bag == 5) { if (play.bankOpen()) play.closeBank(); actors.togglePanel(4); }
                    else if (bag >= 100 && bag < 400) {
                        inventoryPress = bag;
                        inventoryPressX = input.mouseX;
                        inventoryPressY = input.mouseY;
                    }
                }
            }
            }
            }
            }
            }
            }
            } // mapa de missoes / lojas / inventario
            } // !clickedUi (modal blocker)
        }
        if (inventoryPress >= 0) {
            const int dx = input.mouseX - inventoryPressX;
            const int dy = input.mouseY - inventoryPressY;
            const bool movedItem = dx * dx + dy * dy > 25;
            actors.setItemDrag(inventoryPress, input.mouseX, input.mouseY, input.mouseDown && movedItem);
            if (input.mouseReleased) {
                if (actors.forgeWorkbenchOpen()) {
                    // Selecionar nao equipa, vende ou descarta itens durante o trabalho.
                    if(movedItem){
                        const int slot=actors.forgeWorkbenchDropSlot(input.mouseX,input.mouseY,window.width(),window.height());
                        if(slot>=0)actors.forgeWorkbenchSelect(inventoryPress,slot);
                    }else actors.forgeWorkbenchSelect(inventoryPress);
                } else if (movedItem) {
                    const int destination=actors.invDropHit(input.mouseX,input.mouseY,window.width(),window.height());
                    const bool fromBag=inventoryPress>=200 && inventoryPress<300;
                    if (fromBag && play.shopOpen() && actors.shopContains(input.mouseX,input.mouseY,window.width())) {
                        confirmBagIndex=inventoryPress-200;
                        confirmBagId=play.bagItemId(confirmBagIndex);
                        confirmSell=true;
                    } else if(fromBag && destination == -1 &&
                              !actors.uiBlocks(input.mouseX,input.mouseY,window.width(),window.height())) {
                        confirmBagIndex=inventoryPress-200;
                        confirmBagId=play.bagItemId(confirmBagIndex);
                        confirmSell=false;
                    } else play.dragInventory(inventoryPress,destination);
                } else if (inventoryPress >= 100 && inventoryPress < 200) {
                    play.clickEquip(inventoryPress - 100);
                } else if (inventoryPress >= 200 && inventoryPress < 300) {
                    play.setTicketBoatMode(actors.boatMode());
                    play.clickBag(inventoryPress - 200, &actors);
                    if (const auto ticket = play.takeTicketTravelRequest()) {
                        // Travel pre-validates map files; a ticket is removed
                        // only after a successful transition and rebind.
                        if (travel(ticket->map,ticket->x,ticket->y)) {
                            if (!play.consumeTicketAfterTravel(ticket->uid,ticket->itemId))
                                std::fprintf(stderr,"ticket: viagem efetuada, falha ao localizar UID %d no save\n",ticket->uid);
                        } else play.reportTicketTravelFailed();
                    }
                } else if (inventoryPress >= 300 && inventoryPress < 400) {
                    play.takeTemp(inventoryPress - 300);
                }
                inventoryPress = -1;
                actors.setItemDrag(-1, input.mouseX, input.mouseY, false);
            }
        }
        if (skillPressId > 0 || skillPressSlot >= 0) {
            const int dx=input.mouseX-skillPressX, dy=input.mouseY-skillPressY;
            const bool moved=dx*dx+dy*dy>36;
            actors.setSkillDragIcon(play.skillIcon(skillPressId),input.mouseX,input.mouseY,
                                    input.mouseDown && moved && skillPressId>0);
            if (input.mouseReleased) {
                const int slot=actors.shortcutHit(input.mouseX,input.mouseY,window.width(),window.height())-10;
                if (moved && slot>=0 && slot<8) {
                    if (skillPressSlot>=0) play.moveSkillSlot(skillPressSlot,slot);
                    else play.assignSkillToSlot(skillPressId,slot);
                } else if (!moved && skillPressSlot>=0 && skillPressId>0) play.castSkill(skillPressSlot);
                skillPressId=0; skillPressSlot=-1;
                actors.setSkillDragIcon({},input.mouseX,input.mouseY,false);
            }
        }
        actors.dragShop(input.mouseX, input.mouseY, input.mouseDown && !input.rightDown, window.width(), window.height());
        actors.dragState(input.mouseX, input.mouseY, input.mouseDown && !input.rightDown, window.width(), window.height());
        actors.dragInv(input.mouseX, input.mouseY, input.mouseDown && !input.rightDown, window.width(), window.height());
        actors.dragSkill(input.mouseX, input.mouseY, input.mouseDown && !input.rightDown, window.width(), window.height());
        actors.dragQuest(input.mouseX, input.mouseY, input.mouseDown && !input.rightDown, window.width(), window.height());
        if (input.mousePressed && !input.rightDown && !clickedUi &&
            !actors.uiBlocks(input.mouseX, input.mouseY, window.width(), window.height()) && invert4(mvp0, inv)) {
            const float ndcX = static_cast<float>(input.mouseX) / static_cast<float>(window.width()) * 2.f - 1.f;
            const float ndcY = 1.f - static_cast<float>(input.mouseY) / static_cast<float>(window.height()) * 2.f;
            float x0=0.f,y0=0.f,z0=0.f,x1=0.f,y1=0.f,z1=0.f;
            unproject(inv,ndcX,ndcY,-1.f,x0,y0,z0);
            unproject(inv,ndcX,ndcY,1.f,x1,y1,z1);
            const top::screen_pick::Ray ray{x0,y0,z0,x1-x0,y1-y0,z1-z0};
            float hitX=0.f,hitY=0.f;
            const bool ground=rayTerrain(map,x0,y0,z0,x1,y1,z1,hitX,hitY);
            float groundT=std::numeric_limits<float>::infinity();
            if(ground) {
                if(std::fabs(x1-x0)>.001f) groundT=(hitX-x0)/(x1-x0);
                else if(std::fabs(z1-z0)>.001f) groundT=(hitY-z0)/(z1-z0);
            }
            const auto picked=play.rayHit(ray,groundT,&map);
            float npcT=std::numeric_limits<float>::infinity();
            bool havePickedNpc=false;
            top::Npc pickedNpc{};
            for(const top::Npc& npc:npcs.visible(actor.x,actor.y,52.f)) {
                const float t=top::screen_pick::actor(ray,npc.x,map.heightAt(npc.x,npc.y),npc.y,.90f,2.35f);
                if(t<npcT && top::screen_pick::visible(t,groundT)) { npcT=t; pickedNpc=npc; havePickedNpc=true; }
            }
            if(havePickedNpc && npcT<picked.t) {
                play.clearTarget();
                npcClickTarget=pickedNpc;
                npcClickPending=true;
                npcClickMap=mapName;
                if(std::hypot(npcClickTarget.x-actor.x,npcClickTarget.y-actor.y)<2.35f) {
                    npcClickPending=false;
                    interactNpc(&npcClickTarget);
                } else {
                    const float dx=npcClickTarget.x-actor.x,dy=npcClickTarget.y-actor.y;
                    const float dist=std::max(0.01f,std::hypot(dx,dy));
                    if(!planWalk(npcClickTarget.x-dx/dist*1.7f,npcClickTarget.y-dy/dist*1.7f)) npcClickPending=false;
                }
            } else if(picked.kind==1) {
                npcClickPending=false;
                play.pickFoe(picked.index);
                float chaseX=0.f,chaseY=0.f;
                if(play.chasePoint(actor.x,actor.y,chaseX,chaseY)) { haveTarget=false; walkPath.clear(); }
                else { haveTarget=false; walkPath.clear(); }
            } else if(picked.kind==2) {
                npcClickPending=false;
                float lx=0.f,ly=0.f;
                if(play.requestLoot(picked.index,actor.x,actor.y,lx,ly) &&
                   std::hypot(lx-actor.x,ly-actor.y)>1.6f)
                    planWalk(lx,ly);
            } else if(ground) {
                npcClickPending=false;
                play.pick(hitX,hitY);
                float chaseX=0.f,chaseY=0.f;
                if(play.chasePoint(actor.x,actor.y,chaseX,chaseY)) { haveTarget=false; walkPath.clear(); }
                else planWalk(hitX,hitY);
            }
        }

        // Alt+A, Alt+S e Alt+E sao atalhos de painel: nao andam com o WASD.
        float wishX = 0.f;
        float wishY = 0.f;
        if (!characterSelect && !alt) {
            if (input.key['w']) {
                wishX += camFwdX;
                wishY += camFwdY;
            }
            if (input.key['s']) {
                wishX -= camFwdX;
                wishY -= camFwdY;
            }
            if (input.key['d']) {
                wishX += camRightX;
                wishY += camRightY;
            }
            if (input.key['a']) {
                wishX -= camRightX;
                wishY -= camRightY;
            }
        }
        if(previewWalk && visualDiagnostic && !characterSelect){wishX=1.f;wishY=0.f;}
        const float wishLen = std::sqrt(wishX * wishX + wishY * wishY);
        // lMSpd usa centimetros por segundo no cliente antigo (480 = 4,8 m/s).
        // Shift serve a comandos modificados; nao e um multiplicador de corrida.
        const float speed = actors.boatMode() ?
            actors.boatMoveSpeed() * play.boatItemSpeedMultiplier() : play.moveSpeed();
        bool moving = false;
        // Anda por eixo (desliza na parede) e diz se o corpo saiu do lugar.
        // "moving" so vale se houve deslocamento de verdade, para nao ficar
        // com a pose de corrida parado contra um obstaculo.
        auto tryStep = [&](float stepX, float stepY) {
            const float bx = actor.x;
            const float by = actor.y;
            auto passable = [&](float x, float y) {
                if (actors.boatMode()) return map.sailableAt(x, y);
                return !map.blockedAt(x, y) && !sceneBlockedAt(x, y);
            };
            // A malha de colisao historica tem celulas de 50 cm. Subpassos
            // menores que meia celula impedem corrida/lag de atravessar uma
            // faixa bloqueada entre o inicio e o destino do quadro.
            const int steps = std::max(1, static_cast<int>(std::ceil(
                std::max(std::fabs(stepX), std::fabs(stepY)) / 0.20f)));
            const float dx = stepX / static_cast<float>(steps);
            const float dy = stepY / static_cast<float>(steps);
            for (int sub = 0; sub < steps; ++sub) {
                const float radius = actors.boatMode() ? 0.42f : 0.20f;
                auto canMove = [&](float x, float y) {
                    // O cliente antigo usa uma cápsula aproximada no plano;
                    // quatro amostras preservam isso sem exigir física 3D.
                    return passable(x, y) && passable(x + radius, y) && passable(x - radius, y) &&
                           passable(x, y + radius) && passable(x, y - radius);
                };
                if (canMove(actor.x + dx, actor.y)) actor.x += dx;
                if (canMove(actor.x, actor.y + dy)) actor.y += dy;
            }
            return actor.x != bx || actor.y != by;
        };
        if(wishLen>0.01f) npcClickPending=false;
        if(npcClickPending && npcClickMap!=mapName) npcClickPending=false;
        if (play.sitting() && wishLen > 0.01f) play.toggleSit();
        if (play.lockMove()) {
            haveTarget = false;
        } else if (play.sitting()) {
            haveTarget = false;
        } else if (wishLen > 0.01f) {
            haveTarget = false;
            walkPath.clear();
            const float dirX = wishX / wishLen;
            const float dirY = wishY / wishLen;
            actor.yaw = std::atan2(dirX, dirY);
            moving = tryStep(dirX * speed * dt, dirY * speed * dt);
        } else {
            float chaseX = 0.f;
            float chaseY = 0.f;
            if (play.chasePoint(actor.x, actor.y, chaseX, chaseY)) {
                const float dx = chaseX - actor.x;
                const float dy = chaseY - actor.y;
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < 0.35f) {
                    float face = actor.yaw;
                    if (play.aim(actor.x, actor.y, face)) actor.yaw = face;
                } else {
                    actor.yaw = std::atan2(dx, dy);
                    const float step = std::min(speed * dt, dist);
                    moving = tryStep(dx / dist * step, dy / dist * step);
                }
            } else if (haveTarget) {
                if (walkAt >= walkPath.size()) {
                    haveTarget = false;
                    walkPath.clear();
                }
                const float waypointX = haveTarget ? walkPath[walkAt].x : actor.x;
                const float waypointY = haveTarget ? walkPath[walkAt].y : actor.y;
                const float dx = waypointX - actor.x;
                const float dy = waypointY - actor.y;
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < .18f) {
                    ++walkAt;
                    if (walkAt >= walkPath.size()) {
                        haveTarget = false;
                        walkPath.clear();
                    }
                } else {
                    actor.yaw = std::atan2(dx, dy);
                    const float step = std::min(speed * dt, dist);
                    moving = tryStep(dx / dist * step, dy / dist * step);
                    if (!moving) {
                        // O caminho em grade considera o bloqueio do terreno,
                        // mas não todos os sólidos da cena. Quando um objeto
                        // impede o waypoint, recalcula a rota até o destino
                        // original em vez de cancelar o comando de caminhada.
                        std::vector<top::PathPoint> rerouted;
                        if (top::PathFinder::find(map, actor.x, actor.y, walkGoalX, walkGoalY,
                                                  actors.boatMode(), rerouted, actorFootprintBlocked) &&
                            !rerouted.empty()) {
                            walkPath = std::move(rerouted);
                            walkAt = 0;
                            haveTarget = true;
                        } else {
                            haveTarget = false;
                            walkPath.clear();
                        }
                    }
                }
            }
        }
        if(npcClickPending && std::hypot(npcClickTarget.x-actor.x,npcClickTarget.y-actor.y)<2.35f) {
            npcClickPending=false;
            haveTarget=false;
            walkPath.clear();
            interactNpc(&npcClickTarget);
        }
        const LocalBerth* nearbyGarnerBerth = nullptr;
        float nearbyBerthDistance = 6.f;
        if (mapName == "garner") {
            for (const LocalBerth& berth : kGarnerBerths) {
                const float distance = std::hypot(actor.x - berth.dockX, actor.y - berth.dockY);
                if (distance < nearbyBerthDistance) {
                    nearbyGarnerBerth = &berth;
                    nearbyBerthDistance = distance;
                }
            }
        }
        if (nearbyGarnerBerth) {
            actors.setLiveLine(7, std::string("Atracadouro ") +
                std::to_string(nearbyGarnerBerth->id) + "  [E: " +
                (actors.boatMode() ? "desembarcar" : "embarcar") + "]");
        }
        if (pressE) {
            npcClickPending = false;
            if (nearbyGarnerBerth && nearbyBerthDistance <= 3.5f) {
                Actor berthPosition = actor;
                bool transitioned = false;
                if (actors.boatMode()) {
                    berthPosition.x = nearbyGarnerBerth->dockX;
                    berthPosition.y = nearbyGarnerBerth->dockY;
                    if (map.blockedAt(berthPosition.x, berthPosition.y) &&
                        !placeNearTerrain(map, berthPosition, false, 12)) {
                        actors.setLiveLine(6, "Nao ha terreno seguro neste atracadouro.");
                    } else if (actors.setBoatMode(false)) {
                        actor = berthPosition;
                        transitioned = true;
                    } else {
                        actors.setLiveLine(6, "Nao foi possivel preparar o personagem para desembarcar.");
                    }
                } else {
                    berthPosition.x = nearbyGarnerBerth->shipX;
                    berthPosition.y = nearbyGarnerBerth->shipY;
                    berthPosition.yaw = nearbyGarnerBerth->shipYaw;
                    if (!map.sailableAt(berthPosition.x, berthPosition.y) &&
                        !placeNearTerrain(map, berthPosition, true, 12)) {
                        actors.setLiveLine(6, "Nao ha agua navegavel neste atracadouro.");
                    } else if (actors.setBoatMode(true)) {
                        actor = berthPosition;
                        transitioned = true;
                    } else {
                        actors.setLiveLine(6, "Nao foi possivel carregar o barco para este atracadouro.");
                    }
                }
                if (transitioned) {
                    haveTarget = false;
                    walkPath.clear();
                    play.clearTarget();
                    standZ = actors.boatMode() ? actors.boatSeaHeight() : map.heightAt(actor.x, actor.y);
                    actors.setPlayer(actor.x, actor.y, actor.yaw, 0.f, false, 0.f);
                    if (!saveCurrentLocation(false)) {
                        actors.setLiveLine(6, "Mudanca concluida, mas a localizacao nao foi salva.");
                    } else {
                        actors.setLiveLine(6, actors.boatMode()
                            ? "Embarcou pelo atracadouro historico de Garner."
                            : "Desembarcou pelo atracadouro historico de Garner.");
                    }
                }
            } else {
                interactNpc();
            }
        }
        if (!play.npcDialogOpen()) { npcTeleportTargets.clear(); npcForgeModes=0; }
        if (portalLock <= 0.f) {
            top::Portal portal;
            if (portals.near(actor.x, actor.y, 1.25f, portal)) {
                actors.setLiveLine(7, "Portal: " + portal.name + " -> " + portal.targetMap + "  [entrando]");
                if (travel(portal.targetMap, portal.targetX, portal.targetY)) {
                    portalLock = 2.f;
                    std::printf("portal %s -> %s %.1f, %.1f\n", portal.name.c_str(), portal.targetMap.c_str(),
                                portal.targetX, portal.targetY);
                }
            }
        }

        // PERF-02: look ahead along the player's *actual intended route*.
        // Start CPU-only model decoding on a worker before the 32m boundary.
        // Only ActorSkin::pollPopulationPrefetch() moves parsed data to the
        // live renderer; no OpenGL calls are made by the worker.
        if (actors.pollPopulationPrefetch() && profilePerf) {
            const auto& pre = actors.lastPrefetchStats();
            std::printf("[perf-prefetch] complete requested=%d loaded=%d failed=%d elapsed=%.2f ms\n",
                        pre.requested, pre.loaded, pre.failed, pre.elapsedMs);
        }
        const bool legacyPopulation = std::getenv("TOP_PERF01_LEGACY_ROSTER") != nullptr;
        const bool disablePreload = std::getenv("TOP_PERF02_DISABLE") != nullptr;
        const auto preloadNow = std::chrono::steady_clock::now();
        if (!legacyPopulation && !disablePreload && !characterSelect && haveTarget &&
            preloadNow >= nextPopulationPreload) {
            nextPopulationPreload = preloadNow + std::chrono::milliseconds(400);
            const float dx = walkGoalX - actor.x;
            const float dy = walkGoalY - actor.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > 1.f) {
                std::vector<top::MonsterSlot> candidates;
                // A small horizon reduces total memory and avoids loading an
                // entire map. The spawn source is the same as live monsters.
                for (float ahead : {45.f, 85.f, 120.f}) {
                    const auto sample = monsterSpawns.nearest(
                        actor.x + ahead * dx / distance, actor.y + ahead * dy / distance,
                        kMaxLocalMonsters, kLocalMonsterRadius);
                    candidates.insert(candidates.end(), sample.begin(), sample.end());
                }
                const int queued = actors.beginPopulationPrefetch(candidates, 3);
                if (queued && profilePerf)
                    std::printf("[perf-prefetch] queued=%d pos=%.1f,%.1f cache=%d\n",
                                queued, actor.x, actor.y, actors.lastPopulationUpdate().cachedTypes);
            }
        }

        // So entram monstros e NPCs da volta. O conjunto troca ao caminhar
        // uma faixa, sem trazer o mapa inteiro para a memoria.
        const float populationDx = actor.x - populationX;
        const float populationDy = actor.y - populationY;
        if (!characterSelect && !actors.systemWindow() &&
            populationDx * populationDx + populationDy * populationDy >= 32.f * 32.f) {
            const bool resumeWalk = haveTarget;
            const float resumeGoalX = walkGoalX;
            const float resumeGoalY = walkGoalY;
            const auto perfPopulationBegin = std::chrono::steady_clock::now();
            localMonsters = monsterSpawns.nearest(actor.x, actor.y, kMaxLocalMonsters, kLocalMonsterRadius);
            const auto perfSpawnMs = milliseconds(perfPopulationBegin, std::chrono::steady_clock::now());
            makeRoster();
            const auto perfRosterBegin = std::chrono::steady_clock::now();
            // Optional A/B diagnostic: run the original full reopen path.
            // Defer ONLY a streaming roster change when a missing species is
            // still decoding. The existing world remains valid meanwhile, and
            // the next frame retries without blocking the render thread.
            const bool canIncrement = !legacyPopulation && !play.hasTransientSummons();
            const auto preloadState = canIncrement && !disablePreload ?
                actors.ensurePopulationPrefetched(visualRoster) : top::ActorSkin::PrefetchState::Fallback;
            const bool deferred = canIncrement && !disablePreload &&
                preloadState == top::ActorSkin::PrefetchState::Pending;
            const bool incremented = !deferred && canIncrement &&
                (disablePreload || preloadState != top::ActorSkin::PrefetchState::Fallback) &&
                actors.updatePopulation(visualRoster);
            const bool updated = !deferred && (incremented || reopenActorRoster(playerId));
            const auto perfModelsMs = milliseconds(perfRosterBegin, std::chrono::steady_clock::now());
            if (deferred && profilePerf && !populationPreloadNotice) {
                std::printf("[perf-prefetch] boundary deferred roster=%zu pos=%.1f,%.1f\n",
                            visualRoster.size(), actor.x, actor.y);
                populationPreloadNotice = true;
            }
            if (updated) {
                populationPreloadNotice = false;
                actors.setPlayer(actor.x, actor.y, actor.yaw, 0.f, moving, 0.f);
                actors.ensurePlaced(map);
                const auto perfPlayBegin = std::chrono::steady_clock::now();
                if (!incremented || !play.refreshPopulation(actors)) play.rebindWorld(actors);
                const auto perfPlayMs = milliseconds(perfPlayBegin, std::chrono::steady_clock::now());
                play.pushActors(actors);
                if (profilePerf) {
                    const auto& stats = actors.lastPopulationUpdate();
                    std::printf("[perf-population] mode=%s spawn=%.2f ms models=%.2f ms play=%.2f ms total=%.2f ms roster=%zu retained=%d created=%d new_types=%d cached_types=%d\n",
                                incremented ? "incremental" : "fallback", perfSpawnMs,
                                perfModelsMs, perfPlayMs,
                                milliseconds(perfPopulationBegin, std::chrono::steady_clock::now()),
                                visualRoster.size(), incremented ? stats.retained : 0,
                                incremented ? stats.created : 0, incremented ? stats.newTypes : 0,
                                incremented ? stats.cachedTypes : 0);
                }
                populationX = actor.x;
                populationY = actor.y;
                // O refresh do roster nao deve cancelar uma caminhada por
                // clique. O caminho pode atravessar obstaculos que acabaram
                // de entrar no conjunto local; replana ate o mesmo destino.
                if (resumeWalk) planWalk(resumeGoalX, resumeGoalY);
                std::printf("populacao %s atualizada em %.1f, %.1f: %zu modelos\n", mapName.c_str(), actor.x,
                            actor.y, visualRoster.size());
            }
        }

        if (frames == 0) startup.mark("primeiro quadro: eventos, input e logica");
        const auto streamStart = std::chrono::steady_clock::now();
        const int originX = static_cast<int>(actor.x) - kSpan / 2;
        const int originY = static_cast<int>(actor.y) - kSpan / 2;
        const int currentSceneAnimationTick = static_cast<int>(sceneAnimationTime * 30.f);
        // 05-X1: o ultimo setor do terreno DEVE ser registrado somente quando
        // buildTerrainMesh() for executado. Na 05-X a animacao de cenarios (~15Hz)
        // atualizava meshX/meshY sem refazer o terreno, impedindo o streaming
        // do chao durante a caminhada (o "mapa azul" visto no Linux).
        const top::WorldStreamSchedule::Update refresh = worldSchedule.advance(
            originX, originY, currentSceneAnimationTick, sceneMesh.animatedObjects > 0);
        if (refresh.terrain) {
            const auto terrainStart = std::chrono::steady_clock::now();
            top::buildTerrainMesh(map, originX, originY, kSpan, mesh);
            if (useGpuCache) cachedTerrain.update(mesh.batches, uploadStatic);
            if (profilePerf) profileTerrainMs += milliseconds(terrainStart, std::chrono::steady_clock::now());
        }
        // 05-X3: os ~1100 objetos imoveis nao sao reconstruidos a 15 Hz.
        // O pass estatico preserva pisos, escadas, superficies e colisoes,
        // inclusive o snapshot de objetos animados quando a area muda.
        if (refresh.terrain) {
            const auto sceneStart = std::chrono::steady_clock::now();
            scene.build(map, actor.x, actor.y, 64.f, sceneMesh, top::SceneWorld::Pass::Static);
            effects.setMapSceneEffects(sceneMesh.mapEffects);
            if (useGpuCache) cachedScene.update(sceneMesh.batches, uploadStatic);
            if (profilePerf) profileStaticSceneMs += milliseconds(sceneStart, std::chrono::steady_clock::now());
        }
        if (refresh.scene) {
            const auto sceneStart = std::chrono::steady_clock::now();
            scene.build(map, actor.x, actor.y, 64.f, animatedSceneMesh, top::SceneWorld::Pass::Animated);
            if (useGpuCache) cachedAnimatedScene.update(animatedSceneMesh.batches, uploadStatic);
            if (profilePerf) profileAnimatedSceneMs += milliseconds(sceneStart, std::chrono::steady_clock::now());
            if (frames == 0) {
                std::printf("cena objetos %d  animados %d/%d  modelos falhos %d  triangulos %d\n", sceneMesh.placed,
                            sceneMesh.animatedObjects, sceneMesh.animatedParts, sceneMesh.modelFail,
                            sceneMesh.triangles + animatedSceneMesh.triangles);
                std::fflush(stdout);
            }
        }
        const float terrainFeet = map.heightAt(actor.x, actor.y);
        // CCharacter::_UpdateHeight does not sample the seabed for SEA actors:
        // it uses the boat character record's SeaHeight/100 offset instead.
        // The map height below a ship is the seabed, not its waterline.
        const float feet = actors.boatMode() ? actors.boatSeaHeight() :
                           sceneMesh.surfaceAt(actor.x, actor.y, standZ, terrainFeet);
        standZ = feet;
        const auto playStart = std::chrono::steady_clock::now();
        if (!actors.boatMode()) {
            play.setPlayerFacing(actor.yaw);
            play.step(dt, &map, actor.x, actor.y);
        }
        if (play.respawnSerial() != handledRespawnSerial) {
            handledRespawnSerial = play.respawnSerial();
            top::dev_world::Location spawn;
            if (top::dev_world::read(historicalSpawnPath(), spawn)) {
                if (travel(spawn.map, spawn.x, spawn.y))
                    actors.setLiveLine(6, "Voce reapareceu no ponto registrado.");
                else
                    actors.setLiveLine(6, "Ponto registrado indisponivel; voce reapareceu onde caiu.");
            }
        }
        if (moving && !actors.boatMode()) {
            footstepTimer -= dt;
            if (footstepTimer <= 0.f) {
                const top::Tile stepTile = map.at(static_cast<int>(actor.x), static_cast<int>(actor.y));
                audio.emitFootstep(stepTile.valid ? static_cast<int>(stepTile.layer[0].tex) : 0);
                footstepTimer = 0.42f;
            }
        } else if (!moving) {
            footstepTimer = 0.f;
        }
        for (const top::EffectEvent& event : play.takeEffects()) {
            // EffDelay adds the water channel only outside LAND. An unknown
            // or invalid tile stays Unknown rather than assuming open sea.
            top::EffectEvent visualEvent = event;
            if (event.kind == top::EffectEvent::SkillHit) {
                const top::Tile impactTile = map.at(
                    static_cast<int>(event.toX), static_cast<int>(event.toY));
                if (impactTile.valid) {
                    // EffDelay.cpp checks LAND only (including bridge-over-water).
                    constexpr std::uint16_t kLand = 0x0001u;
                    visualEvent.surface = (impactTile.region & kLand)
                        ? top::SkillEffectSurface::Land : top::SkillEffectSurface::Water;
                }
            }
            if (event.kind == top::EffectEvent::Flight) {
                // Preserve the existing height anchor until dummy matrices are
                // wired here; the path itself now retains 3D world positions.
                visualEvent.fromHeight=map.heightAt(event.fromX,event.fromY);
                visualEvent.toHeight=map.heightAt(event.toX,event.toY);
            }
            frameVisualEvents.push_back(visualEvent);
            audio.emit(event);
        }
        if (profilePerf) {
            profilePlayMs += milliseconds(playStart, std::chrono::steady_clock::now());
            profileStreamMs += milliseconds(streamStart, std::chrono::steady_clock::now());
        }
        if (frames == 0) startup.mark("primeiro quadro: streaming de terreno e cenario");
        const auto actorsStart = std::chrono::steady_clock::now();
        actors.setPlayer(actor.x, actor.y, actor.yaw, feet, moving, dt);
        if(previewSkillId<=0 || frames==0)play.pushActors(actors);
        if(previewSkillId>0) {
            // Capture a specific authored action, including the safe-area
            // weapon policy. Does not learn/cast the skill or consume SP.
            const auto* visual=effects.skillVisuals().find(previewSkillId);
            actors.setPlayerPose(visual->firstAction>0?5:0);actors.setPlayerActionOnce(true);
            actors.setPlayerActionBase(visual->firstAction>0?visual->firstAction:1);
            actors.setPlayerActionPlaybackRate(1.f);actors.setPlayerActionCycle(1);
            actors.setPlayerWeaponsDrawn(false);
        }
        std::vector<top::GroundMark> portalMarks;
        portalMarks.reserve(portals.portals().size());
        for (const top::Portal& portal : portals.portals())
            portalMarks.push_back({portal.x, portal.y, 2});
        actors.addMarks(portalMarks);
        // 05-X6: calculate the SAME matrices used by rendering before skinning.
        // Never cull the simulation; ActorSkin only skips visual geometry.
        const float eyeX = actor.x - camFwdX * camDist;
        const float eyeZ = actor.y - camFwdY * camDist;
        const float eyeY = feet + camDist * camLift;
        const float lookY = feet + 1.15f;
        float view[16];
        float proj[16];
        float mvp[16];
        lookAt(eyeX, eyeY, eyeZ, actor.x, lookY, actor.y, view);
        perspective(0.9f, aspectOf(), 0.3f, 800.f, proj);
        multiply(proj, view, mvp);
        actors.setFrustum(mvp, useFrustumCulling);
        actors.build(map, &sceneMesh);
        // Actor-owned auras follow the player's root even without optional
        // full dummy matrices. This does not move independent ground hits.
        effects.setPlayerCastAnchor(actor.x, actor.y, feet);
        // Direct CMPStrip uses its own dummy endpoint history, independent of
        // the optional single-point actor aura binding.
        auto stripBodyDummies=actors.playerDummyWorldMatrices();
        effects.setPlayerStripBodyMatrices(stripBodyDummies);
        effects.setPlayerStripItemMatrices(actors.playerItemDummyWorldMatrices());
        std::unordered_map<int, std::unordered_map<int, std::array<float, 16>>> targetDummies;
        std::unordered_map<int, std::array<float, 3>> targetRoots;
        std::unordered_map<int, std::unordered_map<int,std::array<float,16>>> itemDummies;
        targetDummies[-1]=actors.playerDummyWorldMatrices();
        targetRoots[-1]={actor.x,feet,actor.y};
        for (int monster=0;monster<actors.monsterCount();++monster) {
            targetDummies[monster]=actors.monsterDummyWorldMatrices(monster);
            targetRoots[monster]=actors.monsterWorldRoot(monster);
            itemDummies[monster]=actors.actorItemDummyWorldMatrices(monster);
        }
        effects.setTargetDummyMatrices(std::move(targetDummies));
        effects.setTargetActorRoots(std::move(targetRoots));
        effects.setActorItemDummyMatrices(std::move(itemDummies));
        // Sample the same .lab frame just used by actors.build.
        effects.setPlayerDummyMatrices(actors.playerDummyWorldMatrices());
        // Dispatch after sampling this frame's pose, including projectile
        // snapshots. Sampling here avoids launching from the previous frame.
        const int petItem=actors.boatMode()?0:(previewPetItem>=0?previewPetItem:play.equippedFairyItemId());
        const auto pet=std::find_if(historicalPets.begin(),historicalPets.end(),[&](const auto& p){return p.itemId==petItem;});
        const bool petReady=effects.setPlayerAttachedEffect(16,pet==historicalPets.end()?0:pet->effectId,
                                      pet==historicalPets.end()?-1:pet->dummy);
        if(visualDiagnostic && petItem!=lastLoggedPet) {
            std::fprintf(stderr,"[pet-preview] item=%d effect=%d dummy=%d resource_ready=%d\n",petItem,
                pet==historicalPets.end()?0:pet->effectId,pet==historicalPets.end()?-1:pet->dummy,int(petReady));
            lastLoggedPet=petItem;
        }
        for(const auto& event:frameVisualEvents)effects.emit(event);
        frameVisualEvents.clear();
        if (profilePerf) profileActorsMs += milliseconds(actorsStart, std::chrono::steady_clock::now());
        if (frames == 0) startup.mark("primeiro quadro: animacao dos atores");
        effects.step(dt,&map);
        if (actors.boatMode() && moving) {
            effects.emit({top::EffectEvent::Wake, actor.x, actor.y,
                          actor.x - std::sin(actor.yaw) * 0.9f,
                          actor.y - std::cos(actor.yaw) * 0.9f});
            // Casco deixa duas linhas de espuma, como no rastro do cliente
            // antigo; a segunda fica deslocada pelo través da embarcação.
            const float sideX = std::cos(actor.yaw) * 0.34f;
            const float sideY = -std::sin(actor.yaw) * 0.34f;
            effects.emit({top::EffectEvent::Wake, actor.x + sideX, actor.y + sideY,
                          actor.x + sideX - std::sin(actor.yaw) * 0.75f,
                          actor.y + sideY - std::cos(actor.yaw) * 0.75f});
        }
        const auto renderStart = std::chrono::steady_clock::now();



        const top::Tile lightTile = map.at(static_cast<int>(actor.x), static_cast<int>(actor.y));
        if (lightTile.valid) audio.setArea(lightTile.island);
        const top::AreaLight& areaLight = areaLighting.at(lightTile.island);
        // O DX9 combinava a cor ambiente da AreaSet com a luz direcional e a
        // cor do tile. GLSL recebe a mesma direcao, ja no eixo Y-up do OpenGL.
        const float ambientR = .42f + areaLight.environment[0] * .32f;
        const float ambientG = .42f + areaLight.environment[1] * .32f;
        const float ambientB = .42f + areaLight.environment[2] * .32f;
        device.setLighting(areaLight.direction[0], areaLight.direction[2], areaLight.direction[1],
                           areaLight.diffuse[0] * .42f, areaLight.diffuse[1] * .42f, areaLight.diffuse[2] * .42f,
                           ambientR, ambientG, ambientB);
        device.beginFrame(.53f * .65f + areaLight.environment[0] * .35f,
                          .70f * .65f + areaLight.environment[1] * .35f,
                          .86f * .65f + areaLight.environment[2] * .35f);
        for (const top::TerrainBatch& batch : mesh.batches) {
            if (batch.verts.empty()) continue;
            int mode = batch.mode;
            unsigned texture = 0;
            if (mode == 1 || mode == 2) {
                texture = textureFor(batch.texId);
                if (texture == 0) mode = 0;
            }
            if (mode == 7) {
                texture = waterTextures[static_cast<std::size_t>(static_cast<int>(waterAnimation) % 30)];
                if (texture == 0) mode = 0;
            }
            if (mode == 2 && atlas == 0) continue;
            const auto slot = cachedTerrain.at(static_cast<std::size_t>(&batch - mesh.batches.data()));
            if (useGpuCache && slot.buffer && slot.vertices == static_cast<int>(batch.verts.size()))
                device.drawStaticMesh(slot.buffer, slot.vertices, mvp, mode, texture, atlas);
            else device.draw(batch.verts.data(), static_cast<int>(batch.verts.size()), mvp, mode, texture, atlas);
        }
        // Desenhe ambos os passes na mesma ordem de blend do codigo anterior:
        // sombras, opacos e, por ultimo, a geometria translucida (mode 4).
        // Um pass separado evita que a parte animada invalide os VBOs estaticos.
        auto drawSceneLayer = [&](const top::SceneMesh& source, const top::StaticBatchCache& cache, int layer) {
            for (std::size_t i = 0; i < source.batches.size(); ++i) {
                const top::SceneBatch& batch = source.batches[i];
                const int currentLayer = batch.mode == 6 ? 0 : batch.mode == 4 ? 2 : 1;
                if (currentLayer != layer || batch.verts.empty()) continue;
                int mode = batch.mode;
                unsigned texture = 0;
                if (mode == 3 || mode == 4 || mode == 8) {
                    texture = sceneTextureFor(batch.texture);
                    if (texture == 0) mode = 0;
                }
                const auto slot = cache.at(i);
                if (useGpuCache && slot.buffer && slot.vertices == static_cast<int>(batch.verts.size()))
                    device.drawStaticMesh(slot.buffer, slot.vertices, mvp, mode, texture, 0);
                else device.draw(batch.verts.data(), static_cast<int>(batch.verts.size()), mvp, mode, texture, 0);
            }
        };
        for (int layer = 0; layer != 3; ++layer) {
            drawSceneLayer(sceneMesh, cachedScene, layer);
            drawSceneLayer(animatedSceneMesh, cachedAnimatedScene, layer);
        }
        if (useActorGpuCache) cachedActors.update(actors.batches(), uploadActor, releaseStatic);
        bool drewActor = false;
        for (const top::ActorBatch& batch : actors.batches()) {
            if (batch.verts.empty()) continue;
            int mode = batch.mode;
            unsigned texture = 0;
            if (mode == 3 || mode == 4) {
                texture = sceneTextureFor(batch.texture);
                if (texture == 0 && !batch.texture.empty()) mode = 0;
            }
            const std::size_t batchIndex = static_cast<std::size_t>(&batch - actors.batches().data());
            const auto gpu = useActorGpuCache ? cachedActors.at(batchIndex) : top::ActorGpuCache::Slot{};
            if (gpu.buffer && gpu.vertices == static_cast<int>(batch.verts.size()))
                device.drawStaticMesh(gpu.buffer, gpu.vertices, mvp, mode, texture, 0);
            else device.draw(batch.verts.data(), static_cast<int>(batch.verts.size()), mvp, mode, texture, 0);
            drewActor = true;
        }
        // Alvo de combate: circulo azul sob o monstro selecionado, sem alterar a malha.
        float selectedX=0.f, selectedY=0.f, selectedRadius=0.f;
        if (play.selectedMonster(selectedX, selectedY, selectedRadius)) {
            const auto ring=top::selection_ring::build(selectedX,selectedY,
                                                       map.heightAt(selectedX,selectedY),selectedRadius);
            device.draw(ring.data(),static_cast<int>(ring.size()),mvp,9,0,0);
        }
        effects.build(map, top::EffectViewBasis::fromView(view));
        // SK2.6.1: opt-in, once-per-texture diagnostic of actual effect
        // batches submitted to GfxDevice (not merely effects in the catalog).
        // No GL state or effect geometry is changed by this trace.
        const char* blendDrawTrace = std::getenv("TOP_TRACE_BLEND_DRAWS");
        const bool traceBlendDraw = blendDrawTrace && blendDrawTrace[0] == '1';
        static std::set<std::string> loggedEffectBlends;
        for (const top::EffectBatch& batch : effects.batches()) {
            if (batch.verts.empty()) continue;
            const unsigned texture = sceneTextureFor(batch.texture);
            if (traceBlendDraw && loggedEffectBlends.size() < 80) {
                const std::string key = batch.texture + "|" + std::to_string(batch.mode) +
                    "|" + std::to_string(batch.sourceBlend) + "|" + std::to_string(batch.destBlend);
                if (loggedEffectBlends.insert(key).second) {
                    float minA = 1.f, maxA = 0.f;
                    for (const auto& vertex : batch.verts) {
                        minA = std::min(minA, vertex.a);
                        maxA = std::max(maxA, vertex.a);
                    }
                    std::fprintf(stderr,
                        "[blend-batch] texture=%s id=%u mode=%d dx9=(%d,%d) "
                        "triangles=%zu vertex_alpha=%.3f..%.3f\n",
                        batch.texture.c_str(), texture, batch.mode,
                        batch.sourceBlend, batch.destBlend, batch.verts.size() / 3,
                        minA, maxA);
                }
            }
            // Incomplete texture zero samples as opaque black in the effect
            // shader. A failed image decode/upload must not draw a black quad.
            if (texture == 0) continue;
            device.draw(batch.verts.data(), static_cast<int>(batch.verts.size()), mvp,
                        batch.mode, texture, 0, batch.sourceBlend, batch.destBlend,
                        batch.minFilter, batch.magFilter);
        }
        if (!drewActor) {
            const float s = 0.45f;
            const top::MeshVertex marker[3] = {
                markerVertex(actor.x, feet + 1.6f, actor.y, 0.95f, 0.85f, 0.2f),
                markerVertex(actor.x - s, feet + 0.2f, actor.y - s, 0.9f, 0.4f, 0.15f),
                markerVertex(actor.x + s, feet + 0.2f, actor.y + s, 0.9f, 0.4f, 0.15f),
            };
            device.draw(marker, 3, mvp, 0, 0, 0);
        }
        actors.setItemConfirm(confirmBagIndex >= 0 ?
            (confirmProduction ? "Processar blueprint com a fada e os materiais?" : confirmUpgrade ?
                "Aprimorar efetividade de " + play.bagItemName(forgeEquipIndex) + "? Chance indicada na bancada; custo " +
                    std::to_string(play.equipmentUpgradeCost(forgeEquipIndex)) + "G. Em falha, scroll e crystal sao consumidos." : confirmTiChun ?
                "Aprimorar " + play.bagItemName(confirmBagIndex) + " com " +
                    play.bagItemName(forgeGemIndex) + " por " +
                    std::to_string(play.tiChunCost(confirmBagIndex,forgeGemIndex)) + "G? Os atributos seguem o evento historico." : confirmExtractGem ?
                "Extrair a gema de " + play.bagItemName(confirmBagIndex) + " usando " +
                    play.bagItemName(forgeGemIndex) + " por " +
                    std::to_string(play.gemExtractionCost(confirmBagIndex,forgeGemIndex)) + "G?" : confirmRepair ?
                "Reparar " + play.bagItemName(confirmBagIndex) + " por " +
                    std::to_string(play.repairItemCost(confirmBagIndex)) + "G?" : confirmMilling ? "Abrir encaixe em " + play.bagItemName(confirmBagIndex) +
                " (" + std::to_string(play.socketCost(confirmBagIndex)) + " ouro + 890 + 891)?" :
             confirmCombine ? "Combinar: falha consome gemas e ouro?" : confirmForge ?
                "Forjar: falha consome gemas e ouro?" :
                std::string(confirmSell ? "Vender " : "Jogar fora ") +
                play.bagItemName(confirmBagIndex) + "?") : "");
        play.pushHud(actors, mvp, window.width(), window.height());
        std::vector<std::string> npcDialogOptionLabels;
        if (play.npcDialogOpen() && npcDialogPage > 0) {
            npcDialogOptionLabels.reserve(npcDialogOptions.size());
            for (const auto& option : npcDialogOptions) npcDialogOptionLabels.push_back(option.label);
            if (npcTutorialOpen) actors.setNpcQuestViews({}, -1, 0);
        } else {
            npcTutorialOpen=false;
            npcDialogOptions.clear();
            npcDialogPage=0;
        }
        actors.setNpcDialogOptions(std::move(npcDialogOptionLabels),
                                   npcTutorialOpen && play.npcDialogOpen(), npcDialogHasMissions);
        std::vector<std::string> teleportLabels;
        teleportLabels.reserve(npcTeleportTargets.size());
        for (const top::Portal& destination : npcTeleportTargets)
            teleportLabels.push_back(destination.name.empty() ? destination.targetMap : destination.name);
        actors.setTeleportChoices(play.npcDialogOpen() ? std::move(teleportLabels) : std::vector<std::string>{});
        if(actors.forgeWorkbenchOpen()){
            const auto slots=actors.forgeWorkbenchIndices();
            if(actors.forgeWorkbenchModeIndex()==3){
                const auto preview=play.previewProduction(slots[0],slots[1],slots[2]);
                actors.forgeWorkbenchPreview(preview.valid,preview.fee,preview.successChance,preview.message);
            }else{
                const auto preview=play.previewWorkbench(actors.forgeWorkbenchModeIndex(),slots[0],slots[1],slots[2]);
                actors.forgeWorkbenchPreview(preview.valid,preview.cost,preview.chance,preview.message);
            }
        }
        actors.setPointer(input.mouseX, input.mouseY, input.mouseDown);
        actors.buildHud(window.width(), window.height());
        float hud[16];
        std::memset(hud, 0, sizeof hud);
        hud[0] = hud[5] = hud[10] = hud[15] = 1.f;
        for (const top::ActorBatch& batch : actors.hud()) {
            if (batch.verts.empty()) continue;
            unsigned texture = whiteTex;
            if (batch.texture == "font") texture = fontTex;
            else if (batch.texture == "minimap") {
                const std::uint8_t* pixels = actors.minimapPixels();
                const int side = actors.minimapSize();
                if (pixels && side > 0) {
                    if (miniTex == 0) miniTex = device.createTexture(pixels, side, side);
                    else device.replaceTexture(miniTex, pixels, side, side);
                    texture = miniTex;
                }
            } else if (batch.texture != "white") {
                // Se a textura da UI nao abrir (ex.: new4.tga), desenha branco
                // em vez de passar o handle 0 para o device.
                const unsigned handle = sceneTextureFor(batch.texture);
                texture = handle ? handle : whiteTex;
            }
            device.draw(batch.verts.data(), static_cast<int>(batch.verts.size()), hud, 5, texture, 0);
        }
        if (devPanelOpen && devFont.ready()) {
            const int sw = window.width(), sh=window.height();
            top::ui::UIRenderer boxes(sw,sh);
            boxes.rect(kDevPanelX,kDevPanelY,kDevPanelWidth,488.f,{.075f,.12f,.19f,.96f});
            boxes.border(kDevPanelX,kDevPanelY,kDevPanelWidth,488.f,1.5f,{.37f,.65f,.84f,1.f});
            for (int i=0;i<kDevRows;++i)
                boxes.rect(kDevPanelX+9.f,kDevRowY+i*kDevRowHeight,
                           kDevPanelWidth-18.f,kDevRowHeight-2.f,
                           input.mouseX>=kDevPanelX && input.mouseX<kDevPanelX+kDevPanelWidth &&
                           input.mouseY>=kDevRowY+i*kDevRowHeight &&
                           input.mouseY<kDevRowY+(i+1)*kDevRowHeight ?
                           top::ui::Color{.19f,.32f,.40f,1.f}:
                           top::ui::Color{.12f,.19f,.27f,1.f});
            std::vector<top::ActorBatch> devBatches;
            top::ui::HudComposer composer(devBatches,devFont,root,sw,sh);
            top::ui::HudDrawList emptyDraw;
            composer.layer(boxes,emptyDraw,{0.f,0.f,float(sw),float(sh)});
            composer.text("F9  LABORATORIO DO MUNDO",kDevPanelX+14.f,kDevPanelY+9.f,.76f,.91f,1.f);
            char posLabel[180];
            std::snprintf(posLabel,sizeof(posLabel),"Mapa: %s  X: %.1f  Y: %.1f  Alt: %.2f  %s",
                          mapName.c_str(),actor.x,actor.y,map.heightAt(actor.x,actor.y),
                          actors.boatMode()?"BARCO":"TERRA");
            composer.text(posLabel,kDevPanelX+14.f,kDevPanelY+37.f,.88f,.88f,.88f);
            const top::Tile tile=map.at(static_cast<int>(actor.x),static_cast<int>(actor.y));
            char tileLabel[180];
            std::snprintf(tileLabel,sizeof(tileLabel),
                "Terreno: mascara=%u ilha=%u bloqueado=%d navegavel=%d",
                static_cast<unsigned>(tile.region),static_cast<unsigned>(tile.island),
                map.blockedAt(actor.x,actor.y)?1:0,map.sailableAt(actor.x,actor.y)?1:0);
            composer.text(tileLabel,kDevPanelX+14.f,kDevPanelY+56.f,.78f,.89f,.93f);
            composer.text(devCommandEditing ? "Destino: "+devCommand+"_ [Enter / Esc]" :
                "Escolha uma acao abaixo. Save: separado do inventario.",
                kDevPanelX+14.f,kDevPanelY+80.f,1.f,.87f,.64f);
            const char* actions[] = {
                "Marcar ponto de retorno", "Voltar ao ponto marcado",
                "Ir para agua navegavel (barco)", "Ir a spawn historico de monstros",
                "Ir ao NPC mais proximo", "Ir ao inicio deste mapa",
                "Salvar posicao agora", "Teleporte manual (mapa X Y [terra|barco])",
                "Proximo mapa historico", "Buscar spawn de monstro marinho",
                "Inspecionar terreno no terminal", "Fechar painel (F9)"};
            for (int i=0;i<kDevRows;++i)
                composer.text(actions[i], kDevPanelX+19.f,
                              float(kDevRowY+i*kDevRowHeight+6),.93f,.94f,.96f);
            for (const auto& batch:devBatches) {
                const unsigned tex=batch.texture=="font"?fontTex:whiteTex;
                if(!batch.verts.empty() && tex)
                    device.draw(batch.verts.data(),static_cast<int>(batch.verts.size()),hud,5,tex,0);
            }
        }
        if (transitionFade > 0.f) {
            const float alpha = std::min(1.f, transitionFade);
            top::MeshVertex fade[6]{};
            const float coords[6][2] = {{-1.f, -1.f}, {1.f, -1.f}, {-1.f, 1.f},
                                        {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}};
            for (int i = 0; i < 6; ++i) {
                fade[i].x = coords[i][0]; fade[i].y = coords[i][1]; fade[i].z = 0.f;
                fade[i].r = fade[i].g = fade[i].b = 0.f; fade[i].a = alpha;
                fade[i].u = (coords[i][0] + 1.f) * .5f; fade[i].v = (coords[i][1] + 1.f) * .5f;
            }
            device.draw(fade, 6, hud, 5, whiteTex, 0);
        }
        if (cursorReady) {
            int pointerX = input.mouseX;
            int pointerY = input.mouseY;
            if (!shotPath.empty()) {
                pointerX = 860;
                pointerY = 150;
            }
            int cursorState = top::CurNormal;
            if (shotPath.empty()) {
                if (input.rightDown) cursorState = top::CurCamera;
                else if (inventoryPress >= 0 || skillPressId > 0) cursorState = top::CurDrag;
                else if (actors.uiBlocks(pointerX, pointerY, window.width(), window.height())) cursorState = top::CurActive;
                else if (input.mouseDown) cursorState = top::CurClick;
                else if (play.skillArmed()) cursorState = top::CurSkill;
                else {
                    float invCursor[16];
                    if (invert4(mvp, invCursor)) {
                        const float ndcX = (static_cast<float>(pointerX) / static_cast<float>(window.width())) * 2.f - 1.f;
                        const float ndcY = 1.f - (static_cast<float>(pointerY) / static_cast<float>(window.height())) * 2.f;
                        float x0 = 0.f;
                        float y0 = 0.f;
                        float z0 = 0.f;
                        float x1 = 0.f;
                        float y1 = 0.f;
                        float z1 = 0.f;
                        unproject(invCursor, ndcX, ndcY, -1.f, x0, y0, z0);
                        unproject(invCursor, ndcX, ndcY, 1.f, x1, y1, z1);
                        float hitX = 0.f;
                        float hitY = 0.f;
                        const bool hasGround=rayTerrain(map,x0,y0,z0,x1,y1,z1,hitX,hitY);
                        float groundT=std::numeric_limits<float>::infinity();
                        if(hasGround) {
                            if(std::fabs(x1-x0)>.001f) groundT=(hitX-x0)/(x1-x0);
                            else if(std::fabs(z1-z0)>.001f) groundT=(hitY-z0)/(z1-z0);
                        }
                        const top::screen_pick::Ray ray{x0,y0,z0,x1-x0,y1-y0,z1-z0};
                        const auto picked=play.rayHit(ray,groundT,&map);
                        if(picked.kind==2) cursorState=top::CurPick;
                        else if(picked.kind==1) cursorState=top::CurAttack;
                        else {
                            bool hoverNpc=false;
                            for(const top::Npc& npc:npcs.visible(actor.x,actor.y,52.f)) {
                                const float t=top::screen_pick::actor(ray,npc.x,map.heightAt(npc.x,npc.y),npc.y,.90f,2.35f);
                                if(top::screen_pick::visible(t,groundT)) {hoverNpc=true;break;}
                            }
                            if(hoverNpc) cursorState=top::CurChat;
                            else if(hasGround && map.blockedAt(hitX,hitY)) cursorState=top::CurBlock;
                        }
                    }
                }
            }
            gameCursor.setState(cursorState);
            gameCursor.advance(dt);
            const int slot = gameCursor.frameSlot();
            const top::CursorFrame* frame = gameCursor.frame();
            if (frame && slot >= 0 && slot < static_cast<int>(cursorTex.size()) && cursorTex[static_cast<std::size_t>(slot)] != 0) {
                const float sw = static_cast<float>(window.width());
                const float sh = static_cast<float>(window.height());
                const float x = static_cast<float>(pointerX - frame->hotX);
                const float y = static_cast<float>(pointerY - frame->hotY);
                const float w = static_cast<float>(frame->w);
                const float h = static_cast<float>(frame->h);
                auto corner = [&](float px, float py, float u, float v) {
                    top::MeshVertex vertex{};
                    vertex.x = (px / sw) * 2.f - 1.f;
                    vertex.y = 1.f - (py / sh) * 2.f;
                    vertex.a = 1.f;
                    vertex.r = 1.f;
                    vertex.g = 1.f;
                    vertex.b = 1.f;
                    vertex.u = u;
                    vertex.v = v;
                    return vertex;
                };
                const top::MeshVertex quad[6] = {
                    corner(x, y, 0.f, 0.f),     corner(x + w, y, 1.f, 0.f), corner(x, y + h, 0.f, 1.f),
                    corner(x + w, y, 1.f, 0.f), corner(x + w, y + h, 1.f, 1.f), corner(x, y + h, 0.f, 1.f),
                };
                device.draw(quad, 6, hud, 5, cursorTex[static_cast<std::size_t>(slot)], 0);
            }
        }
        if (frames == 0) {
            const top::ActorReport& rep = actors.report();
            std::printf("atores %d  pecas %d  pecas falhas %d  ossos %d  triangulos %d  anima %d  z %.2f..%.2f\n", rep.actors,
                        rep.parts, rep.partFail, rep.bones, rep.triangles, rep.animMoves ? 1 : 0, rep.bodyMinZ, rep.bodyMaxZ);
            std::fflush(stdout);
        }
        auto saveShot = [&](const std::string& path) {
            const int w = window.width();
            const int h = window.height();
            std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w * h * 3));
            device.readPixels(rgb.data(), w, h);
            int other = 0;
            for (int i = 0; i < w * h; ++i) {
                const int dr = std::abs(rgb[i * 3] - 135);
                const int dg = std::abs(rgb[i * 3 + 1] - 178);
                const int db = std::abs(rgb[i * 3 + 2] - 219);
                if (dr + dg + db > 40) ++other;
            }
            if (!writePpm(path, rgb.data(), w, h)) {
                std::fprintf(stderr, "nao gravei %s\n", path.c_str());
                return false;
            }
            std::printf("quadro %s %dx%d  chao %d%%\n", path.c_str(), w, h, other * 100 / (w * h));
            std::fflush(stdout);
            return true;
        };
        const int previewCaptureFrame=std::max(0,static_cast<int>(std::lround(previewSkillTime*60.f)));
        if (!shotPath.empty() && frames == (visualDiagnostic?previewCaptureFrame:0)) {
            if (!saveShot(shotPath)) return 1;
            if(visualDiagnostic) {
                if(!statsPreview.empty())std::fprintf(stderr,"[stats-preview-final] %s\n",play.statDiagnostic().c_str());
                if(previewWalk)std::fprintf(stderr,"[movement-preview] elapsed=%.6f displacement=%.6f from=%.3f,%.3f to=%.3f,%.3f\n",
                    frames/60.f,std::hypot(actor.x-diagnosticStartX,actor.y-diagnosticStartY),
                    diagnosticStartX,diagnosticStartY,actor.x,actor.y);
                std::fprintf(stderr,"[skill-preview-clock] steps=%d dt=1/60 elapsed=%.6f\n",
                    frames,frames/60.f);
                input.quit=true;
            }
        }
        if (!shotPath.empty() && !visualDiagnostic && frames == 18) {
            std::string walkPath = shotPath;
            const auto dot = walkPath.find_last_of('.');
            if (dot == std::string::npos) walkPath += "-walk.ppm";
            else walkPath.insert(dot, "-walk");
            std::printf("andou para %.2f, %.2f  yaw %.3f\n", actor.x, actor.y, actor.yaw);
            std::fflush(stdout);
            if (!saveShot(walkPath)) return 1;
            input.quit = true;
        }
        if (profilePerf) profileRenderMs += milliseconds(renderStart, std::chrono::steady_clock::now());
        if (frames == 0) startup.mark("primeiro quadro: comandos de desenho OpenGL");
        const auto swapStart = std::chrono::steady_clock::now();
        const auto webStats = device.frameStats();
        top::platform::frameInfo(webStats.draws, static_cast<unsigned>(webStats.dynamicBytes),
                                webStats.attributeSetups, webStats.uniformUpdates,
                                characterSelect && actors.characterCreationStage() == 2, !characterSelect);
        window.swap();
        if (profilePerf) {
            profileSwapMs += milliseconds(swapStart, std::chrono::steady_clock::now());
            const auto stats = device.frameStats();
            if (useActorGpuCache) {
                profileActorUploaded += cachedActors.stats().uploadedBytes;
                profileActorReused += cachedActors.stats().reusedBytes;
            }
            profileDynamicBytes += stats.dynamicBytes;
            profileStaticBytes += stats.staticBytes;
            profileDraws += stats.draws;
            ++profileFrames;
        }

        if (shotPath.empty() && !characterSelect) {
            devAutoSaveElapsed += dt;
            if (devAutoSaveElapsed >= 5.f) {
                devAutoSaveElapsed = 0.f;
                saveCurrentLocation(false);
            }
        }
        ++frames;
        if (frames == 1) {
            startup.mark("primeiro quadro: swap / apresentacao");
            startup.finish();
            std::printf("primeiro quadro desenhado\n");
        }
        top::performance::Sample sample;
        if (frameMeter.frame(std::chrono::steady_clock::now(),sample)) {
            char title[256];
            if (showPerf) {
                std::snprintf(title,sizeof title,"ToP OpenGL | %.0f FPS | %.1f ms/quadro | RSS %ld MiB | %.0f,%.0f",
                              sample.fps,sample.ms,sample.ramMiB,actor.x,actor.y);
                std::printf("perf FPS %.1f  frame %.2f ms  RAM RSS %ld MiB (VRAM nao medida)\n",
                            sample.fps,sample.ms,sample.ramMiB);
                if (profilePerf && profileFrames) {
                    const double div = static_cast<double>(profileFrames);
                    std::printf("profile medio: stream %.2f ms | atores %.2f ms | render %.2f ms | swap %.2f ms | "
                                "draw %.0f | upload dinamico %.2f MiB | cache %.2f MiB | %s\n",
                                profileStreamMs / div, profileActorsMs / div, profileRenderMs / div,
                                profileSwapMs / div, static_cast<double>(profileDraws) / div,
                                static_cast<double>(profileDynamicBytes) / div / 1048576.0,
                                static_cast<double>(profileStaticBytes) / div / 1048576.0,
                                useGpuCache ? "cache GPU ON" : "cache GPU OFF");
                    std::printf("profile cache atores: enviado %.2f MiB | reutilizado %.2f MiB | %s\n",
                                static_cast<double>(profileActorUploaded) / div / 1048576.0,
                                static_cast<double>(profileActorReused) / div / 1048576.0,
                                useActorGpuCache ? "ON" : "OFF");
                    const top::ActorReport& ar = actors.report();
                    std::printf("profile frustum atores: testados %d | visiveis %d | descartados %d | %s\n",
                                actors.report().frustumTested, actors.report().frustumVisible,
                                actors.report().frustumCulled, useFrustumCulling ? "ON" : "OFF");
                    std::printf("profile atores detalhado: esqueletos novos %d | poses reutilizadas %d | "
                                "vertices deformados %d | vertices no mundo %d | indices emitidos %d\n",
                                ar.skeletonUpdates, ar.skeletonReuses,
                                ar.skinnedUniqueVertices, ar.transformedUniqueVertices,
                                ar.triangles * 3);
                    std::printf("profile stream detalhado: terreno %.2f ms | cena estatica %.2f ms | "
                                "cena animada %.2f ms | logica/efeitos %.2f ms\n",
                                profileTerrainMs / div, profileStaticSceneMs / div,
                                profileAnimatedSceneMs / div, profilePlayMs / div);
                    profileStreamMs = profileActorsMs = profileRenderMs = profileSwapMs = 0.0;
                    profileTerrainMs = profileStaticSceneMs = profileAnimatedSceneMs = profilePlayMs = 0.0;
                    profileDynamicBytes = profileStaticBytes = profileDraws = 0;
                    profileActorUploaded = profileActorReused = 0;
                    profileFrames = 0;
                }
            } else std::snprintf(title,sizeof title,"Tales of Pirates - OpenGL  %.0f, %.0f",actor.x,actor.y);
            window.setTitle(title);
        }
    }

    if (shotPath.empty() && !characterSelect) saveCurrentLocation(false);
    cachedTerrain.clear(releaseStatic);
    cachedScene.clear(releaseStatic);
    cachedAnimatedScene.clear(releaseStatic);
    cachedActors.clear(releaseStatic);
    device.shutdown();
    window.close();
    map.close();
    return 0;
}
