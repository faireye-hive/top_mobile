#include "top/EffectSystem.h"

#include <array>
#include <cassert>
#include <cmath>
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
void defaultEff(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<std::uint32_t>(out, 6); put<std::int32_t>(out, 0);
    put<std::uint8_t>(out, 0); name(out, "");
    put<std::uint8_t>(out, 0); name(out, ""); put<std::uint8_t>(out, 0);
    for (int i = 0; i < 4; ++i) put<float>(out, 0.f);
    put<std::int32_t>(out, 1); name(out, "base");
    for (int value : {1, 5, 7}) put<std::int32_t>(out, value);
    put<float>(out, 5.f); put<std::uint16_t>(out, 1); put<float>(out, 5.f);
    for (int i = 0; i < 3; ++i) put<float>(out, 1.f);
    for (int i = 0; i < 6; ++i) put<float>(out, 0.f);
    for (int i = 0; i < 4; ++i) put<float>(out, 1.f);
    put<std::uint16_t>(out, 4); put<std::uint16_t>(out, 1); put<float>(out, 1.f);
    for (float uv : {0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 1.f, 1.f}) put<float>(out, uv);
    put<std::uint16_t>(out, 0); put<float>(out, 0.f); name(out, "glow.tga");
    name(out, "RectPlane"); put<std::uint8_t>(out, 1);
    put<std::int32_t>(out, 1); put<std::int32_t>(out, 8);
    put<float>(out, 1.f); put<float>(out, 1.f); put<float>(out, 1.f);
    put<std::uint16_t>(out, 0); put<float>(out, 0.f); put<float>(out, 0.f);
    put<std::int32_t>(out, 0); put<std::uint8_t>(out, 0);
    for (int i = 0; i < 4; ++i) put<float>(out, 0.f);
    put<std::uint8_t>(out, 0);
}
void firePar(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<std::uint32_t>(out, 2); name(out, "fire"); put<std::int32_t>(out, 1);
    put<std::int32_t>(out, 2); name(out, "fire-system"); put<std::int32_t>(out, 2);
    name(out, "glow.tga"); name(out, "");
    for (int i = 0; i < 3; ++i) put<float>(out, 1.f); // range
    put<std::uint16_t>(out, 2); put<float>(out, 1.f); put<float>(out, 1.f);
    for (int i = 0; i < 6; ++i) put<float>(out, 0.f); // angle keys
    for (int i = 0; i < 8; ++i) put<float>(out, 1.f); // RGBA keys
    put<std::uint8_t>(out, 1); // billboard
    for (int value : {5, 6, 1, 1}) put<std::int32_t>(out, value);
    put<float>(out, 1.f); put<float>(out, 1.f); // life, authored velocity
    for (int i = 0; i < 3; ++i) put<float>(out, 0.f); // direction
    for (int i = 0; i < 3; ++i) put<float>(out, 0.f); // acceleration
    put<float>(out, .025f); // shared emitter step
}
std::array<float, 2> centroid(const fs::path& root, bool sharedTimer) {
    setenv("TOP_HISTORICAL_PARTICLE_POOL", "1", 1);
    if (sharedTimer) setenv("TOP_HISTORICAL_PARTICLE_EMITTER_TIMER", "1", 1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_EMITTER_TIMER");
    EffectSystem system;
    assert(system.open(root.string()));
    EffectEvent event; event.kind = EffectEvent::Cast; event.variant = 532; event.sourceIsPlayer = true;
    system.emit(event); system.step(.016f);
    MapFile map; system.build(map, 1.f, 0.f);
    std::array<float, 2> sum{{0.f, 0.f}};
    std::size_t count = 0;
    for (const auto& batch : system.batches()) {
        if (batch.texture.find("glow.tga.png") == std::string::npos) continue;
        for (const auto& vertex : batch.verts) { sum[0] += vertex.x; sum[1] += vertex.z; ++count; }
    }
    assert(count > 0);
    for (float& coordinate : sum) coordinate /= static_cast<float>(count);
    return sum;
}

int main() {
    const fs::path root = fs::temp_directory_path() / ("sk217q-fire-timer-" + std::to_string(getpid()));
    fs::create_directories(root / "effect"); fs::create_directories(root / "scripts/table");
    fs::create_directories(root / "texture/effect");
    { std::ofstream(root / "texture/effect/glow.tga.png") << "fixture"; }
    defaultEff(root / "effect/default.eff"); firePar(root / "effect/fire.par");
    { std::ofstream table(root / "scripts/table/magicsingleinfo.txt");
      table << "38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n"; }
    { std::ofstream table(root / "scripts/table/sceneffectinfo.txt"); table << "382\tfire.par\tFire\n"; }
    { std::ofstream table(root / "scripts/table/skillinfo.txt");
      std::array<std::string, 74> columns; columns.fill("0"); columns[0] = "532"; columns[51] = "382";
      for (std::size_t i = 0; i < columns.size(); ++i) table << (i ? "\t" : "") << columns[i];
      table << '\n'; }

    const auto actualDt = centroid(root, true);
    const auto nominalSnapshot = centroid(root, false);
    assert(std::abs(actualDt[0] - nominalSnapshot[0]) < 1e-5f &&
           std::abs(actualDt[1] - nominalSnapshot[1]) < 1e-5f);

    unsetenv("TOP_HISTORICAL_PARTICLE_POOL");
    unsetenv("TOP_HISTORICAL_PARTICLE_EMITTER_TIMER");
    fs::remove_all(root);
}
