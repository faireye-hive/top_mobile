#include "top/EffectSystem.h"
#include "top/LegacyEffectInspector.h"
#include "top/LegacyParticleCurves.h"

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
template<class T> void put(std::ofstream& f, T v) { f.write(reinterpret_cast<const char*>(&v), sizeof v); }
void name32(std::ofstream& f, const char* s) {
    char bytes[32]{};
    for (int i = 0; s[i] && i < 31; ++i) bytes[i] = s[i];
    f.write(bytes, 32);
}
void eff(const fs::path& file, bool valid) {
    std::ofstream f(file, std::ios::binary);
    put<std::uint32_t>(f, valid ? 3 : 999);
    f << "base_tex" << '\0' << "../../unsafe" << '\0' << "glow" << '\0';
}
void part(const fs::path& file, bool truncate, bool invalidCurve) {
    std::ofstream f(file, std::ios::binary);
    put<std::uint32_t>(f, 2); name32(f, "controller");
    put<std::int32_t>(f, truncate ? 2 : 1);
    put<std::int32_t>(f, 2); name32(f, "animated");
    put<std::int32_t>(f, 1); name32(f, "spark"); name32(f, "");
    for (int i=0; i<3; ++i) put<float>(f, 0.f);
    put<std::uint16_t>(f, 3);
    for (float v : {1.f, 4.f, 1.f}) put<float>(f, v);
    // Tres vetores XYZ para cada quadro. Z e a rotacao do billboard.
    for (float v : {0.f,0.f,0.f, 0.f,0.f,0.785398f, 0.f,0.f,1.570796f}) put<float>(f, v);
    for (int i=0; i<3; ++i) {
        put<float>(f, (i==0 ? 1.f : (i==1 ? 0.1f : 0.9f)));
        put<float>(f, 0.2f); put<float>(f, 0.3f);
        put<float>(f, (i==0 ? 1.f : (i==1 ? .8f : .1f)));
    }
    put<std::uint8_t>(f, 0);
    for (int i=0;i<4;++i) put<std::int32_t>(f,0);
    put<float>(f, invalidCurve ? std::numeric_limits<float>::quiet_NaN() : .7f);
    put<float>(f, 0.f);
    for (int i=0; i<6; ++i) put<float>(f, 0.f);
    put<float>(f, 0.f);
}
void skill(std::ofstream& f) {
    std::vector<std::string> fields(74,"0"); fields[0]="5";
    fields[51]="42"; fields[59]="42"; fields[63]="43";
    for (std::size_t i=0;i<fields.size();++i) f << (i?"\t":"") << fields[i];
    f << '\n';
}
const top::EffectBatch* findTexture(const top::EffectSystem& fx, const char* name) {
    for (const auto& batch: fx.batches())
        if (batch.texture.find(name)!=std::string::npos && !batch.verts.empty()) return &batch;
    return nullptr;
}
void file(const fs::path& path) { std::ofstream f(path); f << "texture"; }
bool near(float a, float b, float tol=.0001f) { return std::fabs(a-b)<tol; }
}
int main() {
    using top::legacyfx::sample;
    // Tres intervalos: a chave central ocorre em 1/3 da vida; ultimo 1/3 segura a ultima.
    const auto mid = sample({1.f, 4.f, 1.f},
                            {1.f,.2f,.3f,1.f, .1f,.2f,.3f,.8f, .9f,.2f,.3f,.1f},
                            {0.f,0.f,0.f, 0.f,0.f,.785398f, 0.f,0.f,1.570796f}, 1.f/3.f);
    assert(near(mid.size, 4.f));
    assert(near(mid.color[0], .1f));
    assert(near(mid.color[3], .8f));
    assert(near(mid.rotation, .785398f));
    assert(near(sample({1.f,4.f,1.f},{},{},1.f/6.f).size, 2.5f));
    assert(std::isfinite(sample({1.f}, {}, {}, std::numeric_limits<float>::quiet_NaN()).size));
    assert(near(sample({}, {}, {}, 2.f).size, 1.f));

    const fs::path root = fs::temp_directory_path() / ("top_05h_" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "scripts/table");
    fs::create_directories(root / "effect");
    fs::create_directories(root / "texture/effect");
    eff(root / "effect/base.eff", true);
    eff(root / "effect/bad.eff", false);
    top::LegacyEffectSummary inspect;
    assert(top::inspectLegacyEff((root / "effect/base.eff").string(),inspect));
    assert(inspect.version==3 && inspect.bytes > 8);
    assert(inspect.candidates.size()==2);
    assert(inspect.candidates[0].offset==4 && inspect.candidates[0].candidate=="base_tex");
    assert(!top::inspectLegacyEff((root / "effect/bad.eff").string(),inspect));
    assert(inspect.version==3); // falha sem estado parcial
    {
        std::ofstream f(root / "scripts/table/magicsingleinfo.txt");
        f << "38\tdefault\t1\tbase.eff\t0\t0\t0\t0\t0\t0\t0\n";
        f << "42\tanimated\t1\tbase.eff\t0\t1\tgood\t0\t0\t0\t0\n";
        f << "43\tbroken\t1\tbad.eff\t0\t1\tbad\t0\t0\t0\t0\n";
    }
    { std::ofstream f(root / "scripts/table/skillinfo.txt"); skill(f); }
    part(root / "effect/good.par", false, false);
    part(root / "effect/bad.par", true, true);
    file(root / "texture/effect/spark.tga.png");
    file(root / "texture/effect/base_tex.tga.png");

    top::EffectSystem fx;
    assert(fx.open(root.string()));
    fx.emit({top::EffectEvent::Flight, 0.f,0.f,1.f,0.f,5,.75f});
    top::MapFile map;
    fx.step(0.f); // FrameMove creates the zero-step FIRE slot before rendering.
    fx.build(map,1.f,0.f);
    auto batch = findTexture(fx, "spark.tga.png");
    assert(batch && batch->verts.size()>=6);
    const float startR = batch->verts[0].r;
    const float startAlpha = batch->verts[0].a;
    assert(near(startR,1.f));
    assert(near(startAlpha,1.f));
    const float startX = batch->verts[0].x;
    fx.step(.7f/3.f); // m_fFrameTime = life / 3, not life / 2
    fx.build(map,1.f,0.f);
    batch = findTexture(fx,"spark.tga.png");
    assert(batch && batch->verts.size()>=6);
    assert(near(batch->verts[0].r,.1f));
    assert(near(batch->verts[0].a,.8f));
    assert(batch->verts[0].x != startX); // tamanho e rotacao realmente animados
    // UV mantem todo o intervalo 0..1; 3 quadros de curva nao sao 3 celulas de atlas.
    assert(near(batch->verts[0].u,0.f) && near(batch->verts[1].u,1.f));
    fx.emit({top::EffectEvent::SkillHit, 0.f,0.f,1.f,0.f,5}); // controlador truncado
    fx.build(map,1.f,0.f);
    assert(!fx.batches().empty()); // fallback preservado, sem particulas parciais
    assert(fx.cachedHistoricalEffects()==2);
    // Duas aberturas nao mantem ponteiros/texturas cacheadas da primeira.
    assert(fx.open(root.string()));
    assert(fx.cachedHistoricalEffects()==0);
    fs::remove_all(root);
    std::cout << "05-H: .eff header/offsets, curvas .par, RGBA/rotacao e fallback OK\n";
}
