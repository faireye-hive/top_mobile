#include "top/EffectSystem.h"
#include "top/HistoricalShrink.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

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
void shrinkPar(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<std::uint32_t>(out, 2); name(out, "shrink"); put<std::int32_t>(out, 1);
    put<std::int32_t>(out, 12); name(out, "shrink-system"); put<std::int32_t>(out, 1);
    name(out, "glow.tga"); name(out, "");
    put<float>(out, 10.f); put<float>(out, 10.f); put<float>(out, 10.f); // spawn range
    put<std::uint16_t>(out, 2); put<float>(out, 1.f); put<float>(out, 1.f);
    for (int i = 0; i < 6; ++i) put<float>(out, 0.f); // angle keys
    for (int i = 0; i < 8; ++i) put<float>(out, 1.f); // RGBA keys
    put<std::uint8_t>(out, 1); // billboard
    for (int value : {5, 6, 1, 1}) put<std::int32_t>(out, value);
    put<float>(out, 5.f); put<float>(out, .05f); // life, authored velocity
    for (int i = 0; i < 3; ++i) put<float>(out, 0.f); // direction
    for (int i = 0; i < 3; ++i) put<float>(out, 0.f); // acceleration
    put<float>(out, .05f); // emission step
}
std::array<float, 3> centroid(const fs::path& root, bool historical) {
    if (historical) setenv("TOP_HISTORICAL_SHRINK", "1", 1);
    else unsetenv("TOP_HISTORICAL_SHRINK");
    EffectSystem system;
    assert(system.open(root.string()));
    EffectEvent event; event.kind = EffectEvent::Cast; event.variant = 532; event.sourceIsPlayer = true;
    system.emit(event); system.step(.1f);
    MapFile map; system.build(map, 1.f, 0.f);
    std::array<float, 3> sum{{0.f, 0.f, 0.f}};
    std::size_t count = 0;
    for (const auto& batch : system.batches()) {
        if (batch.texture.find("glow.tga.png") == std::string::npos) continue;
        for (const auto& vertex : batch.verts) {
            sum[0] += vertex.x; sum[1] += vertex.y; sum[2] += vertex.z; ++count;
        }
    }
    assert(count != 0);
    for (float& coordinate : sum) coordinate /= static_cast<float>(count);
    return sum;
}

std::array<float, 3> timelineCentroid(const fs::path& root) {
    setenv("TOP_HISTORICAL_SHRINK_TIMELINE","1",1);
    EffectSystem system;
    assert(system.open(root.string()));
    EffectEvent event; event.kind=EffectEvent::Cast; event.variant=532; event.sourceIsPlayer=true;
    system.emit(event);
    system.step(.04f); system.step(.04f); system.step(.04f);
    MapFile map; system.build(map,1.f,0.f);
    std::array<float,3> sum{{0.f,0.f,0.f}}; std::size_t count=0;
    for(const auto& batch:system.batches()) {
        if(batch.texture.find("glow.tga.png")==std::string::npos) continue;
        for(const auto& vertex:batch.verts) {
            sum[0]+=vertex.x;sum[1]+=vertex.y;sum[2]+=vertex.z;++count;
        }
    }
    assert(count!=0);
    for(float& value:sum)value/=static_cast<float>(count);
    unsetenv("TOP_HISTORICAL_SHRINK_TIMELINE");
    return sum;
}

int main() {
    const fs::path root = fs::temp_directory_path() / ("sk217p-shrink-" + std::to_string(getpid()));
    fs::create_directories(root / "effect"); fs::create_directories(root / "scripts/table");
    fs::create_directories(root / "texture/effect");
    { std::ofstream(root / "texture/effect/glow.tga.png") << "fixture"; }
    defaultEff(root / "effect/default.eff"); shrinkPar(root / "effect/shrink.par");
    { std::ofstream table(root / "scripts/table/magicsingleinfo.txt");
      table << "38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n"; }
    { std::ofstream table(root / "scripts/table/sceneffectinfo.txt"); table << "382\tshrink.par\tShrink\n"; }
    { std::ofstream table(root / "scripts/table/skillinfo.txt");
      std::array<std::string, 74> columns; columns.fill("0"); columns[0] = "532"; columns[51] = "382";
      for (std::size_t i = 0; i < columns.size(); ++i) table << (i ? "\t" : "") << columns[i];
      table << '\n'; }

    const auto actual = centroid(root, true);
    historical_shrink::SystemTimeline firstBirth;
    firstBirth.advance(1,.1f,{{10.f,10.f,10.f}},.05f,5.f,.05f,2,2,2654435761u);
    const auto expected=firstBirth.sample(0,2);
    assert(expected.alive);
    assert(std::abs(actual[0] - expected.position[0]) < 1e-4f);
    assert(std::abs(actual[2] - expected.position[1]) < 1e-4f);
    assert(std::abs(actual[1] - (expected.position[2] + 0.9f)) < 1e-4f);
    const auto legacy = centroid(root, false);
    assert(std::abs(legacy[0] - actual[0]) < 1e-5f &&
           std::abs(legacy[2] - actual[2]) < 1e-5f);

    const auto timelinePosition=timelineCentroid(root);
    historical_shrink::SystemTimeline expectedTimeline;
    for(float dt:{.04f,.04f,.04f})
        expectedTimeline.advance(1,dt,{{10.f,10.f,10.f}},.05f,5.f,.05f,2,2,
                                 2654435761u);
    const auto timelineSample=expectedTimeline.sample(0,2);
    assert(timelineSample.alive && std::abs(timelineSample.age-.04f)<1e-6f);
    assert(std::abs(timelinePosition[0]-timelineSample.position[0])<1e-4f);
    assert(std::abs(timelinePosition[2]-timelineSample.position[1])<1e-4f);
    assert(std::abs(timelinePosition[1]-(timelineSample.position[2]+.9f))<1e-4f);

    unsetenv("TOP_HISTORICAL_SHRINK"); fs::remove_all(root);
}
