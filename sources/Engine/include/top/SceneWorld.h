#pragma once

// Objetos de map/<nome>.obj no mesmo lugar que CSceneObjFile::ReadSectionObjInfo
// e CSceneObj::_UpdatePos / _UpdateYaw. O modelo sai de sceneobjinfo.txt.

#include "top/GfxDevice.h"
#include "top/HistoricalMapSceneEffects.h"
#include "top/MapFile.h"
#include "top/ModelFile.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace top {

struct SceneBatch {
    int mode = 3;
    std::string texture;
    std::vector<MeshVertex> verts;
};

// Retangulo do sceneobjinfo tipo 2 (Passable / Impassable). Nao e desenhado.
struct SceneSlab {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float yawDeg = 0.f;
    float minX = 0.f;
    float maxX = 0.f;
    float minY = 0.f;
    float maxY = 0.f;
    float top = 0.f;
    bool solid = false;
};

struct SceneMesh {
    std::vector<SceneBatch> batches;
    std::vector<std::string> missingTextures;
    std::vector<SceneSlab> slabs;
    std::vector<MapSceneEffectPlacement> mapEffects;
    // Triangulos de piso dos predios, 9 floats (x, y, altura) por triangulo.
    std::vector<float> walk;
    std::unordered_map<std::uint64_t, std::vector<int>> walkCell;
    int placed = 0;
    int modelFail = 0;
    int triangles = 0;
    int untexturedTriangles = 0;
    int animatedObjects = 0;
    int animatedParts = 0;
    int animatedUv = 0;
    int animatedOpacity = 0;
    int animatedImages = 0;
    int staticShadows = 0;
    // Maior piso em (x, y) que nao passa de hint + um degrau. O terreno entra sempre.
    float surfaceAt(float x, float y, float hint, float terrain) const;
    bool solidAt(float x, float y, float feet) const;
};

class SceneWorld {
public:
    // 05-X3: separate static and animated objects without rescanning the
    // static geometry at every animation tick. All retains legacy behavior.
    enum class Pass { All, Static, Animated };
    bool open(const std::string& clientRoot, const std::string& mapName = "garner", const std::string& mapRoot = {});
    void setAnimationTime(float seconds) { animationTime_ = seconds; }
    void build(MapFile& map, float centerX, float centerY, float radius, SceneMesh& out,
               Pass pass = Pass::All);
    // Confere o giro de lwMatrix44RotateZ(-yaw + pi) contra numeros fixos.
    static bool selfTest();

private:
    std::string root_;
    std::vector<std::uint8_t> obj_;
    int sectionsX_ = 0;
    int sectionsY_ = 0;
    int sectionW_ = 8;
    int sectionH_ = 8;
    struct CachedModel {
        bool tried = false;
        bool ok = false;
        bool boundsReady = false;
        bool animationChecked = false;
        bool containsAnimation = false;
        float minX = 0.f;
        float minY = 0.f;
        float minZ = 0.f;
        float maxX = 0.f;
        float maxY = 0.f;
        float maxZ = 0.f;
        Model model;
    };
    struct SceneRecord {
        std::string file;
        int kind = 0;
        bool passable = false;
        bool solid = false;
    };

    std::string texturePath(const std::string& fileName);
    const Model* modelFor(const std::string& fileName, bool& failed);

    std::vector<SceneRecord> records_;
    std::unordered_map<std::string, CachedModel> models_;
    std::unordered_map<std::string, std::string> texByLower_;
    bool texScanned_ = false;
    float animationTime_ = 0.f;
};

}  // namespace top
