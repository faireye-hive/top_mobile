#include "top/EffectSystem.h"
#include "top/MagicEffectCatalog.h"
#include "top/SceneEffectCatalog.h"

#include <algorithm>
#include <cassert>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {
template <typename T> void write(std::ofstream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}
void name32(std::ofstream& out, const char* name) {
    char str[32]{};
    for (std::size_t i = 0; name[i] && i < sizeof(str) - 1; ++i) str[i] = name[i];
    out.write(str, 32);
}
void eff(const fs::path& file, const char* textureName) {
    std::ofstream out(file, std::ios::binary);
    write<std::uint32_t>(out, 1);
    out.write(textureName, std::char_traits<char>::length(textureName) + 1);
    out.write("data", 4);
}
// Minimal structured version-6 I_Effect using a native RectPlane.
// Its texture must override the alpha-only controller texture.
void nativeEff(const fs::path& file, const char* texture = "nested_tex.tga", int destBlend = 7) {
    std::ofstream out(file, std::ios::binary);
    write<std::uint32_t>(out, 6); write<std::int32_t>(out, 0);
    write<std::uint8_t>(out, 0); name32(out, "");
    write<std::uint8_t>(out, 0); name32(out, "");
    write<std::uint8_t>(out, 0);
    for (int i=0; i<4; ++i) write<float>(out, 0.f);
    write<std::int32_t>(out, 1); name32(out, "nested particle mesh");
    for (int i : {1,5,destBlend}) write<std::int32_t>(out, i);
    write<float>(out, 1.f); write<std::uint16_t>(out, 2);
    write<float>(out, 0.f); write<float>(out, 1.f);
    for (int i=0; i<6; ++i) write<float>(out, 1.f);
    for (int i=0; i<6; ++i) write<float>(out, 0.f);
    for (int i=0; i<6; ++i) write<float>(out, 0.f);
    for (int i=0; i<8; ++i) write<float>(out, 1.f);
    write<std::uint16_t>(out, 0); write<std::uint16_t>(out, 0);
    write<float>(out, 0.f);
    write<std::uint16_t>(out, 0); write<float>(out, 0.f);
    name32(out, texture); name32(out, "RectPlane");
    write<std::uint8_t>(out, 0); write<std::int32_t>(out, 1);
    write<std::int32_t>(out, 8); write<float>(out, 3.f);
    write<float>(out, 1.f); write<float>(out, 2.f);
    write<std::uint16_t>(out, 0); write<float>(out, 0.f);
    write<float>(out, 0.f); write<std::int32_t>(out, 0);
    write<std::uint8_t>(out, 0);
    for (int i=0; i<4; ++i) write<float>(out, 0.f);
    write<std::uint8_t>(out, 0);
}
void par(const fs::path& file, int type = 2, const char* model = "",
         int sourceBlend = 0, int destBlend = 0) {
    // Minimal v15 CMPPartCtrl-compatible fixture; values mimic the original
    // binary record layout, not an invented JSON representation.
    std::ofstream out(file, std::ios::binary);
    write<std::uint32_t>(out, 15); name32(out, "controller");
    write<std::int32_t>(out, 1);             // systems
    write<float>(out, 1.2f);                 // controller life
    write<std::int32_t>(out, type); name32(out, type == 12 ? "shrinking spark" : "flying spark");
    write<std::int32_t>(out, 2); name32(out, type == 12 ? "shrink" : "spark"); name32(out, model);
    for (int i = 0; i < 3; ++i) write<float>(out, type == 12 ? 4.f : 0.25f);
    write<std::uint16_t>(out, 1); write<float>(out, 1.f);
    for (int i = 0; i < 3; ++i) write<float>(out, 0.f);  // per-frame rotation
    for (int i = 0; i < 4; ++i) write<float>(out, 1.f);  // RGBA
    write<std::uint8_t>(out, 0);
    write<std::int32_t>(out, sourceBlend);
    write<std::int32_t>(out, destBlend);
    write<std::int32_t>(out, 0); write<std::int32_t>(out, 0); // filters
    write<float>(out, 0.7f); write<float>(out, 1.f);   // life / velocity
    for (int i = 0; i < 6; ++i) write<float>(out, 0.f); // dir / acceleration
    write<float>(out, 0.01f); // step
    write<std::uint8_t>(out, 0); name32(out, ""); // model-range flag / name
    write<float>(out, 2.f); write<float>(out, 0.5f); write<float>(out, 1.f); // emitter offset XYZ
    write<float>(out, type == 12 ? 0.f : 0.25f); write<float>(out, type == 12 ? 0.f : 0.5f); // delay / play time
    write<std::uint8_t>(out, 1); // authored CEffPath is present
    write<std::int32_t>(out, 3); write<float>(out, 4.f); // points / path velocity
    for (const auto& point : {std::array<float,3>{0.f,0.f,0.f},
                              std::array<float,3>{1.f,0.f,0.f},
                              std::array<float,3>{1.f,1.f,0.f}})
        for (float coordinate : point) write<float>(out, coordinate);
    for (const auto& segment : {std::pair<std::array<float,3>,std::array<float,3>>{
                                    {1.f,0.f,0.f},{1.f,0.f,0.f}},
                                {{0.f,1.f,0.f},{1.f,0.f,0.f}}}) {
        for (float coordinate : segment.first) write<float>(out, coordinate);
        for (float distanceData : segment.second) write<float>(out, distanceData);
    }
    write<std::uint8_t>(out, 0); // shade
    name32(out, ""); // hit effect
    write<std::int32_t>(out, 0); // random range
    write<std::uint8_t>(out, 0); write<std::uint8_t>(out, 0); // model direction / media
}
void touch(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    out.write("placeholder", 11);  // Only the manifest/file path is tested here.
}
void skill(std::ofstream& out, int id, int castId, int flightId, int hitId) {
    std::vector<std::string> cols(74, "0");
    cols[0] = std::to_string(id); cols[51] = std::to_string(castId);
    cols[59] = std::to_string(flightId); cols[63] = std::to_string(hitId);
    for (std::size_t i = 0; i < cols.size(); ++i) out << (i ? "\t" : "") << cols[i];
    out << '\n';
}
}

int main(int argc, char** argv) {
    const bool fixtureOnly = argc > 1 && std::string(argv[1]) == "--fixture-only";

    assert(!top::MagicEffectCatalog::safeName("../../steal"));
    assert(!top::MagicEffectCatalog::safeName("../a.par"));
    assert(!top::MagicEffectCatalog::safeName("a/b.par"));
    assert(top::MagicEffectCatalog::safeName("icearrowf(a).eff"));
    const fs::path root = fs::temp_directory_path() / ("top_05g_" + std::to_string(static_cast<long>(::getpid())));
    fs::remove_all(root);
    fs::create_directories(root / "scripts/table");
    fs::create_directories(root / "effect");
    fs::create_directories(root / "texture/effect");
    {
        std::ofstream table(root / "scripts/table/magicsingleinfo.txt");
        table << "//ID\tname\tmodel count\tmodels\tvel\tpart count\tparticles\tdummy\tmode\tlight\tresult\n";
        table << "38\tbase\t1\tbase_fx.eff\t10\t0\t0\t-1\t2\t0\t0\n";
        table << "42\tflight\t1\tflight_fx.eff\t10\t1\tflight_part\t-1\t2\t0\t0\n";
        table << "43\tno assets\t1\tmissing_fx.eff\t10\t1\tmissing_part\t-1\t2\t0\t0\n";
        table << "44\tunsafe\t1\t../../bad.eff\t10\t1\t../bad\t-1\t2\t0\t0\n";
        table << "45\tshrink\t1\tshrink_fx.eff\t10\t1\tshrink_part\t-1\t2\t0\t0\n";
        table << "18\tillusion flight\t1\tillusion_fx.eff\t10\t0\t0\t-1\t3\t0\t0\n";
        // Several authored .eff models share one MagicEffect ID.
        table << "46\tmulti model\t3\tmulti_first.eff,missing_middle.eff,multi_second.eff\t10\t0\t0\t-1\t2\t0\t0\n";
        table << "10\tgroup 1\t1\tgroup_one.eff\t10\t0\t0\t-1\t2\t0\t0\n";
        table << "25\tgroup 2\t1\tgroup_two.eff\t10\t0\t0\t-1\t2\t0\t1030006\n";
        table << "26\tgroup 3\t1\tgroup_three.eff\t10\t0\t0\t-1\t2\t0\t0\n";
        table << "20\tfrozen\t1\tfrozen_fx.eff\t10\t0\t0\t-1\t2\t0\ticearrowh\n";
    }
    {
        std::ofstream table(root / "scripts/table/magicgroupinfo.txt");
        table << "//ID\tname\ttypes\tmagic IDs\tcounts\tmotion\n";
        table << "2\tDual Shot\t3\t10,25,26\t1,1,1\t1\n";
        table << "1\tSpread Shot\t1\t10\t3\t0\n";
        table << "9\tbad counts\t2\t10,25\t0,1\t1\n";
    }
    {
        std::ofstream table(root / "scripts/table/sceneffectinfo.txt");
        table << "186\tcast_part.par\tIllusion Slash cast\tSea Wave\t0\t0\t-1\t-1\t0\t1\t0\t-1\n";
        table << "195\tcast_part.par\tIllusion Slash hit\tSea Wave\t0\t0\t-1\t-1\t0\t1\t0\t-1\n";
        table << "196\tnested_part.par\tnested strip\tSea Wave\t0\t0\t-1\t-1\t0\t1\t0\t-1\n";
        table << "197\tblend_part.par\tSK2.6 par blend\tSea Wave\t0\t0\t-1\t-1\t0\t1\t0\t-1\n";
    }
    {
        std::ofstream skills(root / "scripts/table/skillinfo.txt");
        skill(skills, 5, 38, 42, 43);
        skill(skills, 6, 38, 42, 45);
        skill(skills, 7, 186, 1018, 195);
        skill(skills, 10, 186, 1018, 196);
        skill(skills, 12, 0, 1046, 0);
        skill(skills, 90, 0, 2002, 0); // historical Dual Shot (group magic)
        skill(skills, 91, 0, 2001, 0); // fan/group spread
        skill(skills, 93, 0, 1020, 0); // historical Frozen Arrow
        skill(skills, 13, 197, 0, 0); // .par serializes its own blend factors
        std::vector<std::string> itemDummySkill(74, "0");
        itemDummySkill[0] = "14";
        itemDummySkill[53] = "0";
        itemDummySkill[54] = "42,0";
        for (std::size_t i = 0; i < itemDummySkill.size(); ++i)
            skills << (i ? "\t" : "") << itemDummySkill[i];
        skills << '\n';
        std::vector<std::string> itemWithoutDummySkill(74, "0");
        itemWithoutDummySkill[0] = "17";
        itemWithoutDummySkill[53] = "-1";
        itemWithoutDummySkill[54] = "42,0";
        itemWithoutDummySkill[55] = "45,0";
        for (std::size_t i = 0; i < itemWithoutDummySkill.size(); ++i)
            skills << (i ? "\t" : "") << itemWithoutDummySkill[i];
        skills << '\n';
        std::vector<std::string> targetDummySkill(74, "0");
        targetDummySkill[0] = "15";
        targetDummySkill[62] = "55";
        targetDummySkill[63] = "42";
        targetDummySkill[64] = "0";
        for (std::size_t i = 0; i < targetDummySkill.size(); ++i)
            skills << (i ? "\t" : "") << targetDummySkill[i];
        skills << '\n';
        std::vector<std::string> targetRootSkill(74, "0");
        targetRootSkill[0] = "16";
        targetRootSkill[62] = "-1";
        targetRootSkill[63] = "42";
        targetRootSkill[64] = "0";
        for (std::size_t i = 0; i < targetRootSkill.size(); ++i)
            skills << (i ? "\t" : "") << targetRootSkill[i];
        skills << '\n';
        // A partially converted historical skill must not create a generic
        // spinning tornado for missing item/flight/hit assets.
        std::vector<std::string> illusion(74, "0");
        illusion[0] = "81"; illusion[51] = "186";
        illusion[54] = "43,0"; // known ID, file deliberately absent
        illusion[59] = "1043"; // magic 43 also deliberately absent
        illusion[63] = "195";
        illusion[65] = "43"; // ground absent, valid target remains
        for (std::size_t i = 0; i < illusion.size(); ++i)
            skills << (i ? "\t" : "") << illusion[i];
        skills << '\n';
        // Two actor effects and one item effect, plus target/ground/water.
        std::vector<std::string> multi(74, "0");
        multi[0] = "8"; multi[48] = "0"; multi[51] = "186,195";
        multi[54] = "42,0"; multi[63] = "186"; multi[65] = "195";
        multi[66] = "43"; // deliberately absent => one procedural fallback
        for (std::size_t i = 0; i < multi.size(); ++i)
            skills << (i ? "\t" : "") << multi[i];
        skills << '\n';
        // Missing actor assets must not create overlapping generic rings.
        multi.assign(74, "0");
        multi[0] = "9"; multi[51] = "43,44";
        for (std::size_t i = 0; i < multi.size(); ++i)
            skills << (i ? "\t" : "") << multi[i];
        skills << '\n';
    }
    eff(root / "effect/base_fx.eff", "base_tex");
    nativeEff(root / "effect/flight_fx.eff", "flight_tex.tga");
    par(root / "effect/flight_part.par");
    eff(root / "effect/shrink_fx.eff", "shrink_tex");
    par(root / "effect/shrink_part.par", 12);
    nativeEff(root / "effect/illusion_fx.eff", "illusion_tex.tga");
    par(root / "effect/cast_part.par");
    par(root / "effect/blend_part.par", 2, "", 5, 6);
    par(root / "effect/nested_part.par", 6, "01010003.eff");
    nativeEff(root / "effect/01010003.eff");
    nativeEff(root / "effect/multi_first.eff", "multi_first.tga", 7);
    nativeEff(root / "effect/multi_second.eff", "multi_second.tga", 2);
    nativeEff(root / "effect/group_one.eff", "group_one.tga");
    nativeEff(root / "effect/group_two.eff", "group_two.tga");
    nativeEff(root / "effect/group_three.eff", "group_three.tga");
    par(root / "effect/01030006.par", 6, "group_result.eff");
    nativeEff(root / "effect/group_result.eff", "group_result_tex.tga");
    nativeEff(root / "effect/frozen_fx.eff", "frozen_tex.tga");
    // Separate historical Result controller, not an emitter on the missile.
    par(root / "effect/icearrowh.par", 6, "icearrowh.eff");
    nativeEff(root / "effect/icearrowh.eff", "icearrowh_tex.tga");
    touch(root / "texture/effect/base_tex.tga.png");
    touch(root / "texture/effect/flight_tex.tga.png");
    touch(root / "texture/effect/spark.tga.png");
    touch(root / "texture/effect/shrink.tga.png");
    touch(root / "texture/effect/shrink_tex.tga.png");
    touch(root / "texture/effect/illusion_tex.tga.png");
    touch(root / "texture/effect/nested_tex.tga.png");
    touch(root / "texture/effect/multi_first.tga.png");
    touch(root / "texture/effect/multi_second.tga.png");
    touch(root / "texture/effect/group_one.tga.png");
    touch(root / "texture/effect/group_two.tga.png");
    touch(root / "texture/effect/group_three.tga.png");
    touch(root / "texture/effect/group_result_tex.tga.png");
    touch(root / "texture/effect/frozen_tex.tga.png");
    touch(root / "texture/effect/icearrowh_tex.tga.png");
    touch(root / "texture/effect/spark.tga.png");

    top::MagicEffectCatalog catalog;
    assert(catalog.load((root / "scripts/table/magicsingleinfo.txt").string()));
    assert(catalog.size() == 11);
    assert(catalog.find(42)->models.front() == "flight_fx.eff");
    assert(catalog.find(18)->renderMode == 3);
    assert(catalog.find(42)->renderMode == 2);
    assert(catalog.find(42)->particles.front() == "flight_part");
    assert(catalog.find(44)->models.empty());
    assert(catalog.find(44)->particles.empty());
    top::SceneEffectCatalog sceneCatalog;
    assert(sceneCatalog.load((root / "scripts/table/sceneffectinfo.txt").string()));
    assert(sceneCatalog.size() == 4 && sceneCatalog.find(186));
    assert(sceneCatalog.find(186)->particle == "cast_part.par");

    top::EffectSystem effects;
    assert(effects.open(root.string()));
    assert(effects.report().magicEffectRecords == 11);
    assert(effects.report().magicGroupRecords == 2);
    assert(effects.report().sceneEffectRecords == 4);
    assert(effects.cachedHistoricalEffects() == 0); // lazy, not 411 eager reads
    effects.emit({top::EffectEvent::Flight, 0.f, 0.f, 5.f, 5.f, 5, 0.5f});
    assert(effects.cachedHistoricalEffects() == 1);
    top::MapFile map;
    effects.build(map, 1.f, 0.f);
    bool modelTexture = false, particleTexture = false;
    for (const auto& b : effects.batches()) {
        modelTexture |= b.texture.find("flight_tex.tga.png") != std::string::npos && !b.verts.empty();
        particleTexture |= b.texture.find("spark.tga.png") != std::string::npos && !b.verts.empty();
    }
    assert(modelTexture && !particleTexture); // delay authored in CMPPartSys
    effects.step(0.3f);
    effects.build(map, 1.f, 0.f);
    float particleMinX = 1e9f;
    for (const auto& b : effects.batches()) if (b.texture.find("spark.tga.png") != std::string::npos)
        for (const auto& v : b.verts) particleMinX = std::min(particleMinX, v.x);
    assert(particleMinX > 5.f); // flight +2m offset + 0.2m historical path progress
    effects.step(0.8f);
    assert(effects.activeCount() == 1); // particle tail outlives 0.5s flight trajectory
    effects.step(0.4f);
    assert(effects.activeCount() == 0);
    effects.emit({top::EffectEvent::SkillHit, 0.f, 0.f, 5.f, 5.f, 5});
    effects.build(map, 1.f, 0.f);
    bool fallback = false;
    for (const auto& b : effects.batches()) {
        fallback |= b.texture.find("base_tex.tga.png") != std::string::npos && !b.verts.empty();
    }
    // Missing documented historical resources must not create a fake sprite.
    assert(!fallback);
    assert(effects.cachedHistoricalEffects() == 2);
    effects.emit({top::EffectEvent::SkillHit, 0.f, 0.f, 5.f, 5.f, 5});
    assert(effects.cachedHistoricalEffects() == 2); // same missing resource cached
    effects.emit({top::EffectEvent::SkillHit, 0.f, 0.f, 0.f, 0.f, 6});
    auto shrinkSpread = [&]() {
        effects.build(map, 1.f, 0.f);
        std::vector<std::array<float, 3>> centers;
        for (const auto& batch : effects.batches()) {
            if (batch.texture.find("shrink.tga.png") == std::string::npos) continue;
            for (std::size_t i = 0; i + 5 < batch.verts.size(); i += 6) {
                std::array<float, 3> center{};
                for (std::size_t j = 0; j < 6; ++j) {
                    center[0] += batch.verts[i + j].x / 6.f;
                    center[1] += batch.verts[i + j].y / 6.f;
                    center[2] += batch.verts[i + j].z / 6.f;
                }
                centers.push_back(center);
            }
        }
        if (centers.empty()) return 0.f;
        std::array<float, 3> mean{};
        for (const auto& center : centers)
            for (int axis = 0; axis < 3; ++axis) mean[axis] += center[axis] / static_cast<float>(centers.size());
        float spread = 0.f;
        for (const auto& center : centers) {
            const float dx = center[0] - mean[0], dy = center[1] - mean[1], dz = center[2] - mean[2];
            spread += std::sqrt(dx * dx + dy * dy + dz * dz) / static_cast<float>(centers.size());
        }
        return spread;
    };
    effects.step(0.05f);
    const float initialShrinkSpread = shrinkSpread();
    effects.step(0.15f);
    const float contractedSpread = shrinkSpread();
    assert(initialShrinkSpread > 0.f && contractedSpread < initialShrinkSpread);

    top::EffectSystem originalCast;
    assert(originalCast.open(root.string()));
    originalCast.emit({top::EffectEvent::Cast, 1.f, 2.f, 1.f, 2.f, 7});
    originalCast.build(map, 1.f, 0.f);
    assert(originalCast.batches().empty()); // honors the original .par delay; no procedural ring
    originalCast.step(0.3f);
    originalCast.build(map, 1.f, 0.f);
    bool castParticle = false;
    for (const auto& batch : originalCast.batches())
        castParticle |= batch.texture.find("spark.tga.png") != std::string::npos && !batch.verts.empty();
    assert(castParticle);

    top::EffectSystem noTornado;
    assert(noTornado.open(root.string()));
    noTornado.emit({top::EffectEvent::Cast, 1.f, 2.f, 1.f, 2.f, 81});
    assert(noTornado.activeCount() == 1); // real 186, not fake missing item 43
    assert(noTornado.open(root.string()));
    noTornado.emit({top::EffectEvent::Flight, 1.f, 2.f, 5.f, 6.f, 81, .5f});
    assert(noTornado.activeCount() == 0); // no invented projectile for missing 1043
    assert(noTornado.open(root.string()));
    noTornado.emit({top::EffectEvent::SkillHit, 1.f, 2.f, 5.f, 6.f, 81});
    assert(noTornado.activeCount() == 1); // real 195, not fake ground 43

    // 05-BA: the renderer must use *all* historical channels, not only
    // castId() or hitId(). Each emit is checked independently.
    top::EffectSystem multiple;
    assert(multiple.open(root.string()));
    const auto* multivisual = multiple.skillVisuals().find(8);
    assert(multivisual && multivisual->channels.size() == 6);
    multiple.emit({top::EffectEvent::Cast, 1.f, 2.f, 1.f, 2.f, 8});
    assert(multiple.activeCount() == 3); // actor 186,195 plus documented item 42
    assert(multiple.open(root.string()));
    multiple.emit({top::EffectEvent::SkillHit, 1.f, 2.f, 1.f, 2.f, 8});
    assert(multiple.activeCount() == 2); // target+ground, not water on Unknown
    assert(multiple.open(root.string()));
    top::EffectEvent waterHit{top::EffectEvent::SkillHit, 1.f, 2.f, 1.f, 2.f, 8};
    waterHit.surface = top::SkillEffectSurface::Water;
    multiple.emit(waterHit);
    assert(multiple.activeCount() == 2); // target+ground; missing water stays silent
    assert(multiple.open(root.string()));
    top::EffectEvent landHit{top::EffectEvent::SkillHit, 1.f, 2.f, 1.f, 2.f, 8};
    landHit.surface = top::SkillEffectSurface::Land;
    multiple.emit(landHit);
    assert(multiple.activeCount() == 2);
    assert(multiple.open(root.string()));
    multiple.emit({top::EffectEvent::Cast, 1.f, 2.f, 1.f, 2.f, 9});
    assert(multiple.activeCount() == 0); // absent historical assets => no fake tornado

    // SK2.17-BU: a player item effect can follow its authored item dummy.
    // The switch is opt-in; without it the legacy GL event position remains.
    auto itemEffectCenterX = [&](const char* bind) {
        if (bind) assert(::setenv("TOP_HISTORICAL_ITEM_DUMMY_BIND",bind,1)==0);
        else assert(::unsetenv("TOP_HISTORICAL_ITEM_DUMMY_BIND")==0);
        top::EffectSystem itemEffect;
        assert(itemEffect.open(root.string()));
        itemEffect.setPlayerCastAnchor(1.f,2.f,0.f);
        std::array<float,16> itemMatrix{};
        itemMatrix[0]=itemMatrix[5]=itemMatrix[10]=itemMatrix[15]=1.f;
        itemMatrix[12]=10.f; itemMatrix[13]=30.f; itemMatrix[14]=20.f;
        itemEffect.setPlayerStripItemMatrices({{0,itemMatrix}});
        top::EffectEvent cast{top::EffectEvent::Cast,1.f,2.f,1.f,2.f,14};
        cast.sourceIsPlayer=true;
        itemEffect.emit(cast);
        itemEffect.build(map,1.f,0.f);
        float minX=1e9f,maxX=-1e9f;
        for(const auto& batch:itemEffect.batches())
            if(batch.texture.find("flight_tex.tga.png")!=std::string::npos)
                for(const auto& vertex:batch.verts){minX=std::min(minX,vertex.x);maxX=std::max(maxX,vertex.x);}
        assert(minX<maxX);
        return (minX+maxX)*.5f;
    };
    const float itemFallbackX=itemEffectCenterX("0");
    const float itemBoundX=itemEffectCenterX("1");
    assert(itemFallbackX>8.f && itemBoundX>8.f && std::fabs(itemBoundX-itemFallbackX)<1e-5f);
    assert(::unsetenv("TOP_HISTORICAL_ITEM_DUMMY_BIND")==0);

    // SK2.17-BW: DX9 ItemEffect rejects item channels whose configured dummy
    // is exactly -1. The compatibility path remains unchanged without the gate.
    assert(::unsetenv("TOP_HISTORICAL_ITEM_EFFECT_REQUIRE_DUMMY")==0);
    top::EffectSystem itemWithoutDummyLegacy;
    assert(itemWithoutDummyLegacy.open(root.string()));
    itemWithoutDummyLegacy.emit({top::EffectEvent::Cast,1.f,2.f,1.f,2.f,17});
    assert(itemWithoutDummyLegacy.activeCount()==0); // DX9 rejects dummy -1 by default
    assert(::setenv("TOP_HISTORICAL_ITEM_EFFECT_REQUIRE_DUMMY","1",1)==0);
    top::EffectSystem itemWithoutDummyHistorical;
    assert(itemWithoutDummyHistorical.open(root.string()));
    itemWithoutDummyHistorical.emit({top::EffectEvent::Cast,1.f,2.f,1.f,2.f,17});
    assert(itemWithoutDummyHistorical.activeCount()==0); // DX9 returns false for dummy -1
    top::EffectSystem itemWithDummyHistorical;
    assert(itemWithDummyHistorical.open(root.string()));
    itemWithDummyHistorical.emit({top::EffectEvent::Cast,1.f,2.f,1.f,2.f,14});
    assert(itemWithDummyHistorical.activeCount()==1); // nonnegative item dummy remains eligible
    assert(::unsetenv("TOP_HISTORICAL_ITEM_EFFECT_REQUIRE_DUMMY")==0);

    // SK2.17-BV: target channels follow the addressed actor's authored dummy.
    auto targetEffectCenterX = [&](const char* bind) {
        if (bind) assert(::setenv("TOP_HISTORICAL_TARGET_DUMMY_BIND",bind,1)==0);
        else assert(::unsetenv("TOP_HISTORICAL_TARGET_DUMMY_BIND")==0);
        top::EffectSystem targetEffect;
        assert(targetEffect.open(root.string()));
        std::array<float,16> targetDummyMatrix{};
        targetDummyMatrix[0]=targetDummyMatrix[5]=targetDummyMatrix[10]=targetDummyMatrix[15]=1.f;
        targetDummyMatrix[12]=15.f; targetDummyMatrix[13]=30.f; targetDummyMatrix[14]=20.f;
        std::unordered_map<int,std::unordered_map<int,std::array<float,16>>> targetMatrices;
        targetMatrices[0][55]=targetDummyMatrix;
        targetEffect.setTargetDummyMatrices(std::move(targetMatrices));
        top::EffectEvent hit{top::EffectEvent::SkillHit,1.f,2.f,3.f,4.f,15};
        hit.targetActorIndex=0;
        targetEffect.emit(hit);
        targetEffect.build(map,1.f,0.f);
        float minX=1e9f,maxX=-1e9f;
        for(const auto& batch:targetEffect.batches())
            if(batch.texture.find("flight_tex.tga.png")!=std::string::npos)
                for(const auto& vertex:batch.verts){minX=std::min(minX,vertex.x);maxX=std::max(maxX,vertex.x);}
        assert(minX<maxX);
        return (minX+maxX)*.5f;
    };
    const float targetFallbackX=targetEffectCenterX("0");
    const float targetBoundX=targetEffectCenterX("1");
    assert(targetFallbackX>8.f && targetBoundX>8.f && std::fabs(targetBoundX-targetFallbackX)<1e-5f);
    assert(::unsetenv("TOP_HISTORICAL_TARGET_DUMMY_BIND")==0);
    ::setenv("TOP_HISTORICAL_TARGET_DUMMY_BIND","1",1);
    top::EffectSystem targetRootEffect;
    assert(targetRootEffect.open(root.string()));
    targetRootEffect.setTargetActorRoots({{0,{12.f,30.f,20.f}}});
    top::EffectEvent rootHit{top::EffectEvent::SkillHit,1.f,2.f,3.f,4.f,16};
    rootHit.targetActorIndex=0;
    targetRootEffect.emit(rootHit);
    targetRootEffect.build(map,1.f,0.f);
    float rootMinX=1e9f,rootMaxX=-1e9f;
    for(const auto& batch:targetRootEffect.batches())
        if(batch.texture.find("flight_tex.tga.png")!=std::string::npos)
            for(const auto& vertex:batch.verts){rootMinX=std::min(rootMinX,vertex.x);rootMaxX=std::max(rootMaxX,vertex.x);}
    assert(rootMinX<rootMaxX && (rootMinX+rootMaxX)*.5f>10.f);
    assert(::unsetenv("TOP_HISTORICAL_TARGET_DUMMY_BIND")==0);

    // A type-6 STRIP .par can wrap an effect model; render the model's
    // original texture/geometry, not a flat controller billboard.
    top::EffectSystem nestedStrip;
    assert(nestedStrip.open(root.string()));
    nestedStrip.emit({top::EffectEvent::SkillHit, 1.f, 2.f, 3.f, 4.f, 10});
    nestedStrip.step(.35f);
    nestedStrip.build(map, 1.f, 0.f);
    bool foundNested = false;
    for (const auto& batch : nestedStrip.batches())
        foundNested |= batch.texture.find("nested_tex.tga.png") != std::string::npos &&
                       !batch.verts.empty();
    assert(foundNested); // failing in 05-BI: modelName was silently discarded

    // SK2: all real model entries must be rendered, even if one middle file
    // is unavailable. Their own D3D material blend modes remain distinct.
    top::EffectSystem multiModel;
    assert(multiModel.open(root.string()));
    multiModel.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 12, .5f});
    multiModel.build(map, 1.f, 0.f);
    bool firstModel = false, secondModel = false;
    for (const auto& batch : multiModel.batches()) {
        if (batch.verts.empty()) continue;
        if (batch.texture.find("multi_first.tga.png") != std::string::npos)
            firstModel = batch.mode == 8; // SRC_ALPHA / INV_SRC_ALPHA
        if (batch.texture.find("multi_second.tga.png") != std::string::npos)
            secondModel = batch.mode == 4; // SRC_ALPHA / ONE
    }
    assert(firstModel && secondModel);

    // SK2.11: explicit legacy compatibility mode does not forward raw factors.
    ::setenv("TOP_HISTORICAL_BLEND","0",1); // explicit compatibility mode
    top::EffectSystem legacyBlend;
    assert(legacyBlend.open(root.string()));
    legacyBlend.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 12, .5f});
    legacyBlend.build(map, 1.f, 0.f);
    for (const auto& batch:legacyBlend.batches())
        assert(batch.sourceBlend == 0 && batch.destBlend == 0);

    // The two EFF models preserve independent authoring pairs, even when
    // rendered into the same frame. The GL backend uses these original pairs.
    assert(::setenv("TOP_HISTORICAL_BLEND","1",1)==0);
    top::EffectSystem authenticBlend;
    assert(authenticBlend.open(root.string()));
    authenticBlend.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 12, .5f});
    authenticBlend.build(map, 1.f, 0.f);
    bool saw57=false, saw52=false;
    for (const auto& batch: authenticBlend.batches()) {
        if (batch.verts.empty()) continue;
        if (batch.texture.find("multi_first.tga.png")!=std::string::npos)
            saw57 = batch.sourceBlend==5 && batch.destBlend==7;
        if (batch.texture.find("multi_second.tga.png")!=std::string::npos)
            saw52 = batch.sourceBlend==5 && batch.destBlend==2;
    }
    assert(saw57 && saw52);

    // A .par controller stores source/destination followed by two sampler
    // filter integers; emit its own pair, independently of EFF tracks.
    top::EffectSystem controllerBlend;
    assert(controllerBlend.open(root.string()));
    controllerBlend.emit({top::EffectEvent::Cast, 1.f, 2.f, 6.f, 7.f, 13, .5f});
    controllerBlend.step(.3f);
    controllerBlend.build(map, 1.f, 0.f);
    bool saw56=false;
    for (const auto& batch:controllerBlend.batches())
        if (!batch.verts.empty() && batch.texture.find("spark.tga.png")!=std::string::npos)
            saw56 = batch.sourceBlend==5 && batch.destBlend==6;
    assert(saw56);
    assert(::unsetenv("TOP_HISTORICAL_BLEND")==0);

    // SK2.4: a magic Result is stored separately from in-flight particles.
    // The original CMagicCtrl::Stop() spawns it ON arrival, regardless of
    // whether skillinfo contains target/ground/water channels.
    top::EffectSystem resultFx;
    assert(resultFx.open(root.string()));
    assert(resultFx.magicEffects().find(20)->result == "icearrowh");
    resultFx.emit({top::EffectEvent::Flight, 1.f, 2.f, 9.f, 7.f, 93, .5f});
    assert(resultFx.activeCount() == 1); // result has not started
    auto hasTexture = [&](const char* needle) {
        resultFx.build(map, 1.f, 0.f);
        for (const auto& batch: resultFx.batches())
            if (batch.texture.find(needle) != std::string::npos && !batch.verts.empty())
                return true;
        return false;
    };
    assert(hasTexture("frozen_tex.tga.png"));
    assert(!hasTexture("icearrowh_tex.tga.png"));
    resultFx.emit({top::EffectEvent::SkillHit, 1.f, 2.f, 9.f, 7.f, 93});
    assert(resultFx.activeCount() == 1); // skill 93 has no additional hit channel
    resultFx.step(.49f);
    assert(!hasTexture("icearrowh_tex.tga.png"));
    resultFx.step(.02f); // time crosses projectile arrival
    assert(resultFx.activeCount() == 1); // flight expired, result created
    assert(!hasTexture("icearrowh_tex.tga.png")); // Result .par delay 0.25
    resultFx.step(.27f);
    assert(hasTexture("icearrowh_tex.tga.png"));
    // Historical result must be based on the target, not the old caster.
    float leftmost=1e9f;
    for (const auto& batch: resultFx.batches())
        if (batch.texture.find("icearrowh_tex.tga.png") != std::string::npos)
            for (const auto& vertex: batch.verts)
                leftmost=std::min(leftmost, vertex.x);
    assert(leftmost>7.f);
    assert(!hasTexture("frozen_tex.tga.png"));
    const std::size_t singleResult = resultFx.activeCount();
    resultFx.step(.05f);
    assert(resultFx.activeCount() == singleResult); // no repeated spawn
    resultFx.step(10.f);
    assert(resultFx.activeCount() == 0);

    // DX9 starts the Result controller at age zero on Stop(), then calls its
    // first FrameMove with dt=0 during Render. Do not carry the flight frame's
    // overshoot into the result's authored delay when the historical switch is on.
    assert(::setenv("TOP_HISTORICAL_MAGIC_RESULT_FRAME_START","1",1)==0);
    top::EffectSystem resultFrameStart;
    assert(resultFrameStart.open(root.string()));
    resultFrameStart.emit({top::EffectEvent::Flight,1.f,2.f,9.f,7.f,93,.5f});
    resultFrameStart.step(.76f); // arrives with .26s left in this GL update
    auto resultFrameVisible = [&]() {
        resultFrameStart.build(map,1.f,0.f);
        for (const auto& batch:resultFrameStart.batches())
            if (batch.texture.find("icearrowh_tex.tga.png")!=std::string::npos &&
                !batch.verts.empty()) return true;
        return false;
    };
    assert(!resultFrameVisible()); // Result's authored 0.25s delay has just begun.
    resultFrameStart.step(.26f);
    assert(resultFrameVisible());
    assert(::unsetenv("TOP_HISTORICAL_MAGIC_RESULT_FRAME_START")==0);

    // SK2.2: 2002 references group #2, not a missing magic ID.
    // DX9 Part_sequence fires magicsingle 10/25/26 at 0.0 / 0.2 / 0.4s.
    top::EffectSystem grouped;
    assert(grouped.open(root.string()));
    grouped.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 90, .9f});
    assert(grouped.activeCount() == 3);
    auto visibleGroup = [&](const char* name) {
        grouped.build(map, 1.f, 0.f);
        for (const auto& batch : grouped.batches())
            if (batch.texture.find(name) != std::string::npos && !batch.verts.empty()) return true;
        return false;
    };
    assert(visibleGroup("group_one.tga.png"));
    assert(!visibleGroup("group_two.tga.png"));
    assert(!visibleGroup("group_three.tga.png"));
    assert(!visibleGroup("base_tex.tga.png"));
    grouped.step(.21f);
    assert(visibleGroup("group_two.tga.png"));
    assert(!visibleGroup("group_three.tga.png"));
    grouped.step(.21f);
    assert(visibleGroup("group_three.tga.png"));
    // The second member has Result, triggered at its OWN arrival (0.2s
    // later than member 10), never when the first member arrives.
    top::EffectSystem groupedArrival;
    assert(groupedArrival.open(root.string()));
    groupedArrival.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 90, .9f});
    auto hasGroupResult = [&]() {
        groupedArrival.build(map, 1.f, 0.f);
        for (const auto& b: groupedArrival.batches())
            if (b.texture.find("group_result_tex.tga.png") != std::string::npos &&
                !b.verts.empty()) return true;
        return false;
    };
    groupedArrival.step(1.0f); // first member landed, second not yet
    assert(!hasGroupResult());
    groupedArrival.step(.12f); // second member arrived, result delayed 0.25s
    assert(!hasGroupResult());
    groupedArrival.step(.27f);
    assert(hasGroupResult());


    top::EffectSystem fanGroup;
    assert(fanGroup.open(root.string()));
    fanGroup.emit({top::EffectEvent::Flight, 0.f, 0.f, 0.f, 10.f, 91, 1.f});
    assert(fanGroup.activeCount() == 3);
    fanGroup.step(.35f);
    fanGroup.build(map, 1.f, 0.f);
    std::vector<float> fanX;
    for (const auto& batch:fanGroup.batches())
        if (batch.texture.find("group_one.tga.png")!=std::string::npos)
            for(std::size_t i=0;i+5<batch.verts.size();i+=6){
                float x=0.f;
                for(std::size_t v=0;v<6;++v) x+=batch.verts[i+v].x/6.f;
                fanX.push_back(x);
            }
    assert(fanX.size()==3);
    const auto [lowest,highest]=std::minmax_element(fanX.begin(),fanX.end());
    assert(*highest-*lowest > .25f); // real spread, not three identical flights

    // Frozen Arrow visual adjustment: all authored vertices scale relative
    // to the same keyframe, without altering the flight's position.
    auto frozenRadius = [&](const char* raw) {
        if (raw) ::setenv("TOP_FROZEN_ARROW_FX_SCALE", raw, 1);
        else ::unsetenv("TOP_FROZEN_ARROW_FX_SCALE");
        top::EffectSystem frozen;
        assert(frozen.open(root.string()));
        frozen.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 93, .5f});
        frozen.build(map, 1.f, 0.f);
        float minX=1e9f, maxX=-1e9f;
        for (const auto& batch : frozen.batches())
            if (batch.texture.find("frozen_tex.tga.png")!=std::string::npos)
                for (const auto& v:batch.verts) {
                    minX=std::min(minX,v.x); maxX=std::max(maxX,v.x);
                }
        assert(minX<maxX);
        return maxX-minX;
    };
    const float baseWidth=frozenRadius("1");
    const float defaultWidth=frozenRadius(nullptr);
    assert(std::fabs(defaultWidth-baseWidth)<.0001f);
    const float biggerWidth=frozenRadius("2.5");
    ::unsetenv("TOP_FROZEN_ARROW_FX_SCALE");
    assert(biggerWidth > 2.f * baseWidth);

    top::EffectSystem prefixedFlight;
    assert(prefixedFlight.open(root.string()));
    prefixedFlight.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 7, 0.5f});
    prefixedFlight.build(map, 1.f, 0.f);
    bool prefixedModel = false;
    for (const auto& batch : prefixedFlight.batches())
        prefixedModel |= batch.texture.find("illusion_tex.tga.png") != std::string::npos && !batch.verts.empty();
    assert(prefixedModel); // 1018 in skillinfo resolves magicsingleinfo ID 18
    // SK2.8 integration: test the actual EffectSystem mesh emitter, not
    // only the CPU-only motion helper.
    top::EffectSystem shade;
    assert(shade.open(root.string()));
    shade.emit({top::EffectEvent::Flight, 1.f, 2.f, 10.f, 2.f, 7, 1.2f});
    const auto countTexture = [&](const top::EffectSystem& system,
                                  const std::string& texture) {
        std::size_t total=0;
        for(const auto& b:system.batches()) if(b.texture.find(texture)!=std::string::npos)
            total+=b.verts.size();
        return total;
    };
    shade.build(map, 1.f, 0.f);
    assert(countTexture(shade,"illusion_tex.tga.png")==6);
    shade.step(.3f);
    shade.build(map, 1.f, 0.f);
    assert(countTexture(shade,"illusion_tex.tga.png")==6);
    shade.step(.35f);
    shade.build(map, 1.f, 0.f);
    assert(countTexture(shade,"illusion_tex.tga.png")==12);
    // Other modes keep a single rendered model.
    top::EffectSystem regularFlight;
    assert(regularFlight.open(root.string()));
    regularFlight.emit({top::EffectEvent::Flight, 1.f, 2.f, 10.f, 2.f, 5, 1.2f});
    regularFlight.step(.65f);
    regularFlight.build(map,1.f,0.f);
    assert(countTexture(regularFlight,"flight_tex.tga.png")==6);


    if (!fixtureOnly) {
    // Integration smoke against the real Illusion Slash assets shipped in
    // Client/, in addition to the synthetic format fixtures above.
    fs::path clientRoot = fs::current_path() / "../Client";
    if (!fs::exists(clientRoot / "scripts/table/skillinfo.txt"))
        clientRoot = fs::current_path() / "Client";
    assert(fs::exists(clientRoot / "effect/01000014.par"));
    assert(fs::exists(clientRoot / "effect/01020028.par"));
    assert(fs::exists(clientRoot / "effect/04000009.eff"));
    for (const auto kind : {top::EffectEvent::Cast, top::EffectEvent::Flight,
                            top::EffectEvent::SkillHit}) {
        top::EffectSystem historical;
        assert(historical.open(clientRoot.string()));
        historical.emit({kind, 1.f, 2.f, 6.f, 7.f, 81, .5f});
        historical.build(map, 1.f, 0.f);
        std::cout << "Illusion Slash event " << int(kind) << ": "
                  << historical.batches().size() << " batches";
        bool authoredTexture = false;
        for (const auto& batch : historical.batches())
        {
            std::cout << " [" << batch.texture << ", " << batch.verts.size() << " vertices]";
            authoredTexture |= !batch.verts.empty() && !batch.texture.empty() &&
                               batch.texture != historical.report().textureFile;
        }
        std::cout << '\n';
        assert(!historical.batches().empty());
        assert(authoredTexture); // actual skill resource, not the generic fallback texture
    }
    } // The separate fixture test does not require a mounted Client/ directory.
    fs::remove_all(root);
    std::cout << "05-G: separate scene/magic IDs, authored .par/.eff resources and fallback OK\n";
}
