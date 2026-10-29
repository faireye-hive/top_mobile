#include "top/EffectSystem.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace top;
#define REQUIRE(x) do { if (!(x)) throw std::runtime_error(#x); } while (0)

template<class T> void put(std::ofstream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof value);
}
void name(std::ofstream& out, const char* value) {
    char bytes[32]{};
    for (int i = 0; value[i] && i < 31; ++i) bytes[i] = value[i];
    out.write(bytes, sizeof bytes);
}
void defaultEff(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<std::uint32_t>(out, 6); put<int>(out, 0); put<std::uint8_t>(out, 0); name(out, "");
    put<std::uint8_t>(out, 0); name(out, ""); put<std::uint8_t>(out, 0);
    for (int i = 0; i < 4; ++i) put<float>(out, 0.f);
    put<int>(out, 1); name(out, "layer");
    for (int value : {1, 5, 7}) put<int>(out, value);
    put<float>(out, 5.f); put<std::uint16_t>(out, 1); put<float>(out, 5.f);
    for (int i = 0; i < 3; ++i) put<float>(out, 0.f);
    for (int i = 0; i < 6; ++i) put<float>(out, 0.f);
    for (int i = 0; i < 4; ++i) put<float>(out, 1.f);
    put<std::uint16_t>(out, 4); put<std::uint16_t>(out, 1); put<float>(out, 1.f);
    for (float value : {0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f}) put<float>(out, value);
    put<std::uint16_t>(out, 0); put<float>(out, 0.f); name(out, "glow.tga");
    name(out, "RectPlane"); put<std::uint8_t>(out, 1); put<int>(out, 1); put<int>(out, 8);
    for (float value : {3.f,1.f,2.f}) put<float>(out, value);
    put<std::uint16_t>(out, 0); put<float>(out, 0.f); put<float>(out, 0.f); put<int>(out, 0);
    put<std::uint8_t>(out, 0); for (int i = 0; i < 4; ++i) put<float>(out, 0.f);
    put<std::uint8_t>(out, 0);
}
void firePar(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<unsigned>(out, 2); name(out, "map-scene"); put<int>(out, 1);
    put<int>(out, 2); name(out, "fire"); put<int>(out, 4);
    name(out, "glow.tga"); name(out, "");
    for (int i = 0; i < 3; ++i) put<float>(out, 0.f);
    put<std::uint16_t>(out, 2); put<float>(out, .5f); put<float>(out, 1.f);
    for (int i = 0; i < 6; ++i) put<float>(out, i == 0 ? 1.f : 0.f);
    for (int i = 0; i < 8; ++i) put<float>(out, 1.f);
    put<std::uint8_t>(out, 1);
    for (int value : {5,6,1,1}) put<int>(out, value);
    put<float>(out, .5f); put<float>(out, 1.f);
    for (int i = 0; i < 6; ++i) put<float>(out, i == 0 ? 1.f : 0.f);
    put<float>(out, .2f);
}

int main() {
    const fs::path root = fs::temp_directory_path() /
        ("sk217ba-map-scene-" + std::to_string(static_cast<long long>(getpid())));
    fs::create_directories(root / "effect");
    fs::create_directories(root / "scripts/table");
    fs::create_directories(root / "texture/effect");
    { std::ofstream texture(root / "texture/effect/glow.tga.png"); texture << "fixture"; }
    defaultEff(root / "effect/glow.eff");
    firePar(root / "effect/map-scene.par");
    { std::ofstream table(root / "scripts/table/magicsingleinfo.txt");
      table << "38\tdefault\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n"; }
    { std::ofstream table(root / "scripts/table/sceneffectinfo.txt");
      table << "7\tmap-scene.par\tMap scene fixture\n"; }
    { std::ofstream table(root / "scripts/table/skillinfo.txt"); }

    setenv("TOP_HISTORICAL_MAP_SCENE_EFFECTS", "1", 1);
    EffectSystem effects;
    REQUIRE(effects.open(root.string()));
    MapSceneEffectPlacement placement;
    placement.key = 0x123400005678ull;
    placement.effectId = 7;
    placement.x = 12.f; placement.y = 18.f; placement.height = 2.f;
    placement.yaw = 1.57079632679f;
    float yawX = 0.f, yawZ = 0.f;
    historical_map_scene_effects::rotateHorizontal(1.f, 0.f, placement.yaw, yawX, yawZ);
    REQUIRE(std::fabs(yawX) < .00001f && std::fabs(yawZ-1.f) < .00001f);
    effects.setMapSceneEffects({placement});
    REQUIRE(effects.activeCount() == 1);
    effects.step(3.f); // Looped scene controller must outlive its authored particle life.
    REQUIRE(effects.activeCount() == 1);
    MapFile map;
    effects.build(map, 1.f, 0.f);
    std::size_t vertices = 0;
    for (const auto& batch : effects.batches()) vertices += batch.verts.size();
    REQUIRE(vertices > 0);
    effects.setMapSceneEffects({});
    REQUIRE(effects.activeCount() == 0);
    unsetenv("TOP_HISTORICAL_MAP_SCENE_EFFECTS");
    EffectSystem disabled;
    REQUIRE(disabled.open(root.string()));
    disabled.setMapSceneEffects({placement});
    REQUIRE(disabled.activeCount() == 1);
    fs::remove_all(root);
}
