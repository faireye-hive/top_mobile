#include "top/LegacyEffectInspector.h"
#include "top/LegacyEffectGeometry.h"
#include "top/EffectSystem.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <unistd.h>
namespace fs = std::filesystem;
namespace {
template<typename T> void put(std::ofstream& f, T v) { f.write(reinterpret_cast<const char*>(&v), sizeof v); }
void name32(std::ofstream& f, const std::string& s) {
    char bytes[32]{};
    assert(s.size() < 32);
    s.copy(bytes, s.size()); f.write(bytes, 32);
}
void originalEff(const fs::path& dest, bool invalidModelParams=false,
                 bool invalidKeyTime=false, const std::string& model="square") {
    // Header follows the actual CMPResManger::LoadEffectFromFile version 6.
    // Child follows I_Effect::LoadFromFile, including its versioned suffix.
    std::ofstream f(dest, std::ios::binary);
    put<std::uint32_t>(f, 6); put<std::int32_t>(f, 2); // version, technique
    put<std::uint8_t>(f, 0); name32(f, ""); // path
    put<std::uint8_t>(f, 0); name32(f, ""); // sound
    put<std::uint8_t>(f, 0); // rotating
    for (int i=0;i<3;++i) put<float>(f, 0.f);
    put<float>(f, 0.f); put<std::int32_t>(f, 1); // speed, subeffects
    name32(f, "Sparks");
    for (int v : {1, 5, 6}) put<std::int32_t>(f, v); // type / blends
    put<float>(f, 1.f); put<std::uint16_t>(f, 3);
    for (int i=0;i<3;++i) put<float>(f, invalidKeyTime&&i==0?
        std::numeric_limits<float>::quiet_NaN():.5f);
    for (float v : {1.f,1.f,1.f, 4.f,4.f,4.f, 1.f,1.f,1.f}) put<float>(f,v);
    for (float v : {0.f,0.f,0.f, 0.f,0.f,.785398f, 0.f,0.f,1.570796f}) put<float>(f,v);
    for (float v : {0.f,0.f,0.f, 2.f,1.f,0.f, 0.f,0.f,0.f}) put<float>(f,v);
    for (float v : {1.f,0.f,0.f,1.f, 0.f,1.f,0.f,.8f, 0.f,0.f,1.f,0.f}) put<float>(f,v);
    put<std::uint16_t>(f, 0); put<std::uint16_t>(f, 0); put<float>(f, 0.f); // UV
    put<std::uint16_t>(f, 0); put<float>(f, 0.f); name32(f, "shine.tga");
    name32(f, model); put<std::uint8_t>(f, 1); put<std::int32_t>(f,0); // billboard
    put<std::int32_t>(f, 4); put<float>(f, invalidModelParams?
        std::numeric_limits<float>::quiet_NaN():1.f);
    put<float>(f, invalidModelParams?std::numeric_limits<float>::infinity():1.f);
    put<float>(f, invalidModelParams?-std::numeric_limits<float>::infinity():1.f); // mesh params, >v1
    put<std::uint16_t>(f, 1); put<float>(f,0.1f); name32(f,"shine.tga"); put<float>(f,0.1f); // >v2
    put<std::int32_t>(f, 0); // >v3 no cylinder params
    put<std::uint8_t>(f,0); for (int i=0;i<4;++i) put<float>(f, 0.f); // >v4
    put<std::uint8_t>(f,1); // >v5
}
void skill(std::ofstream& f) {
    std::vector<std::string> fields(74, "0");
    fields[0]="5"; fields[51]="42";
    for (std::size_t i=0;i<fields.size();++i) f << (i?"\t":"") << fields[i];
    f << '\n';
}
float peakSpriteX(const top::EffectSystem& fx) {
    float max = -1e10f;
    for (const auto& b : fx.batches()) if (b.texture.find("shine.tga.png")!=std::string::npos)
        for (const auto& v : b.verts) max = std::max(max, v.x);
    return max;
}
}
int main() {
    fs::path root = fs::temp_directory_path() / ("top_05i_" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "effect");
    fs::create_directories(root / "texture/effect");
    fs::create_directories(root / "scripts/table");
    const auto source = root / "effect" / "base.eff";
    originalEff(source);
    top::LegacyEffectSummary summary;
    assert(top::inspectLegacyEff(source.string(), summary));
    assert(summary.version == 6 && summary.structured && summary.document.parts.size() == 1);
    assert(summary.document.technique == 2 && !summary.document.useSound);
    assert(summary.document.parts[0].billboard && summary.document.parts[0].keys.size() == 3);
    assert(summary.document.parts[0].texture == "shine.tga");
    assert(summary.verifiedResources.size() >= 1);
    const auto mid = top::legacy_eff::sample(summary.document.parts[0], .5f);
    assert(std::fabs(mid.size[0] - 4.f) < .0001f);
    assert(std::fabs(mid.position[0] - 2.f) < .0001f);
    assert(std::fabs(mid.color[3] - .8f) < .0001f);
    assert(std::fabs(mid.angle[2] - .785398f) < .0001f);
    assert(std::fabs(top::legacy_eff::sample(summary.document.parts[0], .25f).size[0] - 2.5f) < .0001f);
    // The DX9 reads model parameters verbatim. RectPlane geometry ignores them,
    // while the GL Cone/Cylinder generator separately rejects unsafe values.
    const auto rawModelParams = root / "effect" / "raw_model_params.eff";
    originalEff(rawModelParams, true, false, "RectPlane");
    top::LegacyEffectSummary rawParams;
    assert(top::inspectLegacyEff(rawModelParams.string(), rawParams));
    assert(!rawParams.structured); // default is strict/off
    assert(top::inspectLegacyEff(rawModelParams.string(), rawParams, true));
    assert(rawParams.structured && rawParams.document.parts.size()==1);
    assert(std::isnan(rawParams.document.parts[0].modelParams.height));
    assert(std::isinf(rawParams.document.parts[0].modelParams.topRadius));
    top::legacy_mesh::Primitive rectangle;
    assert(top::legacy_mesh::make("RectPlane", rawParams.document.parts[0].modelParams, rectangle));
    // Non-finite animation values remain malformed rather than entering GL transforms.
    const auto rawKeyTime = root / "effect" / "raw_key_time.eff";
    originalEff(rawKeyTime, false, true);
    top::LegacyEffectSummary invalidKey;
    assert(top::inspectLegacyEff(rawKeyTime.string(), invalidKey));
    assert(!invalidKey.structured && invalidKey.document.parts.empty());
    // Truncation must NOT publish a half-decoded binary structure.
    fs::path truncated = root / "effect" / "partial.eff";
    fs::copy_file(source, truncated);
    fs::resize_file(truncated, fs::file_size(truncated)-2);
    top::LegacyEffectSummary bad;
    assert(top::inspectLegacyEff(truncated.string(), bad));
    assert(!bad.structured && bad.document.parts.empty());
    // A malformed name is not allowed to resolve outside Client/.
    { std::ofstream f(root / "scripts/table/magicsingleinfo.txt");
      f << "38\tbase\t1\tbase.eff\t0\t0\t0\t0\t0\t0\t0\n";
      f << "42\tcustom\t1\tbase.eff\t0\t0\t0\t0\t0\t0\t0\n"; }
    { std::ofstream f(root / "scripts/table/skillinfo.txt"); skill(f); }
    { std::ofstream f(root / "texture/effect/shine.tga.png"); f << "stub"; }
    top::EffectSystem system;
    assert(system.open(root.string()));
    system.emit({top::EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    top::MapFile map;
    system.build(map,1.f,0.f);
    const float before = peakSpriteX(system);
    assert(before > -100.f);
    system.step(.5f); system.build(map,1.f,0.f);
    const float after = peakSpriteX(system);
    assert(after > before + .5f); // authored position and scale keyframe
    assert(system.cachedHistoricalEffects() == 1);
    fs::remove_all(root);
    std::cout << "05-I: CMPResManger/I_Effect structured tracks, bounds, GL billboards OK\n";
}
