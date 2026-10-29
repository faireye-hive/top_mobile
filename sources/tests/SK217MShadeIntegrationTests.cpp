#include "top/EffectSystem.h"

#include <array>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace top;

template <class T> void put(std::ofstream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void name(std::ofstream& out, const char* value) {
    char bytes[32]{};
    for (unsigned i = 0; value[i] && i < 31; ++i) bytes[i] = value[i];
    out.write(bytes, sizeof(bytes));
}
void shadeMap(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<std::uint32_t>(out, 780627); put<std::uint32_t>(out, 16);
    put<std::uint32_t>(out, 16); put<std::uint32_t>(out, 8); put<std::uint32_t>(out, 8);
    put<std::uint32_t>(out, 36); put<std::uint32_t>(out, 0);
    put<std::uint32_t>(out, 0); put<std::uint32_t>(out, 0);
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
        std::array<std::uint8_t, 15> tile{};
        tile[7] = 10; // cHeight +1 m
        tile[11] = tile[12] = tile[13] = tile[14] = 30; // subgrid +1.5 m
        out.write(reinterpret_cast<const char*>(tile.data()), tile.size());
    }
}
void shadePar(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<std::uint32_t>(out, 2); name(out, "shade"); put<std::int32_t>(out, 1);
    put<std::int32_t>(out, 13); name(out, "shade"); put<std::int32_t>(out, 4);
    name(out, "shade.tga"); name(out, "");
    for (int i = 0; i < 3; ++i) put<float>(out, 0.f);
    put<std::uint16_t>(out, 2);
    put<float>(out, 2.f); put<float>(out, 3.f);
    for (int i = 0; i < 6; ++i) put<float>(out, 0.f);
    for (int frame = 0; frame < 2; ++frame)
        for (int channel = 0; channel < 4; ++channel) put<float>(out, 1.f);
    put<std::uint8_t>(out, 1);
    for (int value : {5, 6, 1, 1}) put<std::int32_t>(out, value);
    put<float>(out, 1.f); put<float>(out, 2.f);
    for (int i = 0; i < 6; ++i) put<float>(out, 0.f);
    put<float>(out, 0.f);
}
void defaultEff(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<std::uint32_t>(out, 6); put<std::int32_t>(out, 0);
    put<std::uint8_t>(out, 0); name(out, "");
    put<std::uint8_t>(out, 0); name(out, ""); put<std::uint8_t>(out, 0);
    for (int i = 0; i < 4; ++i) put<float>(out, 0.f);
    put<std::int32_t>(out, 1); name(out, "base");
    for (int value : {1, 5, 7}) put<std::int32_t>(out, value);
    put<float>(out, 5.f); put<std::uint16_t>(out, 1); put<float>(out, 5.f);
    for (int i = 0; i < 3; ++i) put<float>(out, 2.f);
    for (int i = 0; i < 6; ++i) put<float>(out, 0.f);
    for (int i = 0; i < 4; ++i) put<float>(out, 1.f);
    put<std::uint16_t>(out, 4); put<std::uint16_t>(out, 1); put<float>(out, 1.f);
    for (float uv : {0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 1.f, 1.f}) put<float>(out, uv);
    put<std::uint16_t>(out, 0); put<float>(out, 0.f); name(out, "shade.tga");
    name(out, "RectPlane"); put<std::uint8_t>(out, 1);
    put<std::int32_t>(out, 1); put<std::int32_t>(out, 8);
    put<float>(out, 3.f); put<float>(out, 1.f); put<float>(out, 2.f);
    put<std::uint16_t>(out, 0); put<float>(out, 0.f); put<float>(out, 0.f);
    put<std::int32_t>(out, 0); put<std::uint8_t>(out, 0);
    for (int i = 0; i < 4; ++i) put<float>(out, 0.f);
    put<std::uint8_t>(out, 0);
}
std::size_t draw(const fs::path& root, bool enabled, bool terrainGrid,
                 float& firstY, bool& foundExpectedY) {
    if (enabled) setenv("TOP_HISTORICAL_SHADE", "1", 1);
    else unsetenv("TOP_HISTORICAL_SHADE");
    if (terrainGrid) setenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID", "1", 1);
    else unsetenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID");
    EffectSystem system;
    assert(system.open(root.string()));
    EffectEvent event;
    event.kind = EffectEvent::Cast;
    event.variant = 532;
    event.sourceIsPlayer = true;
    system.emit(event);
    system.step(.25f);
    MapFile map;
    assert(map.open((root / "terrain.map").string()));
    system.build(map, 1.f, 0.f);
    std::size_t vertices = 0;
    for (const auto& batch : system.batches()) {
        if (batch.texture.find("shade.tga.png") == std::string::npos) continue;
        for (const auto& vertex : batch.verts) {
            if (vertices == 0) firstY = vertex.y;
            const float expectedY = 1.515f;
            if (std::abs(vertex.y - expectedY) < 1e-5f)
                foundExpectedY = true;
            ++vertices;
        }
    }
    return vertices;
}

int main() {
    const fs::path root = fs::temp_directory_path() /
        ("sk217m-shade-" + std::to_string(getpid()));
    fs::create_directories(root / "effect");
    fs::create_directories(root / "scripts/table");
    fs::create_directories(root / "texture/effect");
    { std::ofstream(root / "texture/effect/shade.tga.png") << "fixture"; }
    shadeMap(root / "terrain.map");
    defaultEff(root / "effect/default.eff");
    { std::ofstream table(root / "scripts/table/magicsingleinfo.txt");
      table << "38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n"; }
    shadePar(root / "effect/shade.par");
    { std::ofstream table(root / "scripts/table/sceneffectinfo.txt");
      table << "382\tshade.par\tShade\n"; }
    { std::ofstream table(root / "scripts/table/skillinfo.txt");
      std::array<std::string, 74> columns;
      columns.fill("0"); columns[0] = "532"; columns[51] = "382";
      for (std::size_t i = 0; i < columns.size(); ++i)
          table << (i ? "\t" : "") << columns[i];
      table << '\n'; }

    float legacyY = 0.f, linearY = 0.f, gridY = 0.f;
    bool legacyHeight = false, linearHeight = false, gridHeight = false;
    const auto legacyVertices = draw(root, false, false, legacyY, legacyHeight);
    const auto linearVertices = draw(root, true, false, linearY, linearHeight);
    const auto shadeVertices = draw(root, true, true, gridY, gridHeight);
    assert(legacyVertices == 54u); // four generic GL quads
    assert(linearVertices == 54u);
    assert(shadeVertices == 54u);  // 3x3 projected cells, two triangles each
    assert(linearHeight); // tile cHeight + DX9 bias
    assert(gridHeight); // object subgrid + DX9 bias
    assert(std::abs(legacyY - linearY) < 1e-5f);

    unsetenv("TOP_HISTORICAL_SHADE");
    unsetenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID");
    fs::remove_all(root);
}
