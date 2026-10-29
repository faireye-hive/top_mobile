#include "top/LegacyEffFormat.h"
#include "top/LegacyEffectInspector.h"
#include "top/EffectSystem.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;
namespace {
template <typename T> void put(std::ofstream& f, T x) {
    f.write(reinterpret_cast<const char*>(&x), sizeof x);
}
void name32(std::ofstream& f, const std::string& name) {
    char buffer[32]{};
    assert(name.size() < 32);
    name.copy(buffer, name.size());
    f.write(buffer, sizeof buffer);
}
void writeUV(std::ofstream& f, float minU, float maxU, float minV, float maxV) {
    for (auto [u,v] : std::array<std::array<float,2>,4>{{
        {{minU,maxV}},{{minU,minV}},{{maxU,minV}},{{maxU,maxV}}}}) {
        put<float>(f,u); put<float>(f,v);
    }
}
void eff(const fs::path& path, bool nonexistentSecondTexture = false) {
    std::ofstream f(path, std::ios::binary);
    put<std::uint32_t>(f,6); put<std::int32_t>(f,2);
    put<std::uint8_t>(f,0); name32(f, "");
    put<std::uint8_t>(f,0); name32(f, "");
    put<std::uint8_t>(f,0);
    for(int i=0;i<4;i++)put<float>(f,0.f);
    put<std::int32_t>(f,1);
    name32(f,"animated_uv");
    for(int v:{1,5,6})put<std::int32_t>(f,v);
    put<float>(f,1.f); put<std::uint16_t>(f,2);
    put<float>(f,0.f);put<float>(f,1.f);
    for(int i=0;i<6;i++)put<float>(f,1.f); // 2 sizes
    for(int i=0;i<6;i++)put<float>(f,0.f); // 2 angles
    for(int i=0;i<6;i++)put<float>(f,0.f); // 2 positions
    for(int i=0;i<8;i++)put<float>(f,1.f); // 2 colors
    // CTexCoordList: 4 vertices, two coordinates curves, interpolate
    put<std::uint16_t>(f,4);put<std::uint16_t>(f,2);put<float>(f,.5f);
    writeUV(f,0.f,1.f,0.f,1.f);
    writeUV(f,.1f,.9f,.1f,.9f);
    // CTexList: 2 discrete regions (takes precedence over coord interpolation)
    put<std::uint16_t>(f,2);put<float>(f,.5f);name32(f,"alpha.tga");
    writeUV(f,0.f,1.f,0.f,1.f);
    writeUV(f,.25f,.75f,.25f,.75f);
    name32(f,"RectPlane");put<std::uint8_t>(f,1);put<std::int32_t>(f,2);
    put<std::int32_t>(f,4);
    for(int i=0;i<3;i++)put<float>(f,1.f);
    put<std::uint16_t>(f,2);put<float>(f,.5f);
    name32(f,"alpha.tga");
    name32(f,nonexistentSecondTexture? "missing.tga" : "beta.tga");
    put<float>(f,.5f); // original stores frame time twice
    put<std::int32_t>(f,0);
    put<std::uint8_t>(f,0);
    for(int i=0;i<4;i++)put<float>(f,0.f);
    put<std::uint8_t>(f,1);
}
std::vector<const top::EffectBatch*> findTexture(const top::EffectSystem& fx,const std::string& part){
    std::vector<const top::EffectBatch*> out;
    for (const auto& batch : fx.batches()) if (batch.texture.find(part) != std::string::npos) out.push_back(&batch);
    return out;
}
}
int main(){
    const auto temp = fs::temp_directory_path()/ ("top_05j_"+std::to_string(::getpid()));
    fs::remove_all(temp);
    fs::create_directories(temp/"effect");
    fs::create_directories(temp/"texture/effect");
    fs::create_directories(temp/"scripts/table");
    eff(temp/"effect/base.eff");
    eff(temp/"effect/fallback.eff",true);
    { std::ofstream f(temp/"scripts/table/magicsingleinfo.txt");
      f<<"38\tbase\t1\tbase.eff\t0\t0\t0\t0\t0\t0\t0\n";
      f<<"42\tcustom\t1\tbase.eff\t0\t0\t0\t0\t0\t0\t0\n";
      f<<"43\tbroken\t1\tfallback.eff\t0\t0\t0\t0\t0\t0\t0\n"; }
    { std::ofstream f(temp/"scripts/table/skillinfo.txt");
      std::vector<std::string> fields(74,"0"); fields[0]="5";fields[51]="42";
      for(size_t i=0;i<fields.size();++i)f<<(i?"\t":"")<<fields[i];f<<'\n';
    }
    {std::ofstream f(temp/"texture/effect/alpha.tga.png");f<<"placeholder";}
    {std::ofstream f(temp/"texture/effect/beta.tga.png");f<<"placeholder";}
    top::LegacyEffectSummary summary;
    assert(top::inspectLegacyEff((temp/"effect/base.eff").string(),summary));
    assert(summary.structured && summary.document.parts.size()==1);
    const auto& part=summary.document.parts[0];
    assert(part.coordFrames.size()==2 && part.regions.size()==2 && part.frames.size()==2);
    assert(std::fabs(part.textureFrameTime-.5f)<1e-6f);
    assert(top::legacy_eff::textureFrameIndex(.75f,.5f,2)==1);
    assert(top::legacy_eff::textureFrameIndex(1.25f,.5f,2)==0);
    assert(top::legacy_eff::textureFrameIndex(1.25f,0.f,2)==0);
    auto uv=top::legacy_eff::sampleUV(part,.75f);
    assert(std::fabs(uv[0][0]-.25f)<1e-5f);
    assert(std::fabs(uv[2][0]-.75f)<1e-5f);
    auto interpolated=part;
    interpolated.regions.clear();
    uv=top::legacy_eff::sampleUV(interpolated,.25f);
    assert(std::fabs(uv[0][0]-.05f)<1e-5f);
    { auto bad=summary.document.parts[0]; bad.coordFrameTime=0.f;
      assert(top::legacy_eff::textureFrameIndex(1e30f,.5f,2)<2); }
    top::EffectSystem fx;
    assert(fx.open(temp.string()));
    fx.emit({top::EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    top::MapFile map;
    fx.build(map,1.f,0.f);
    auto start=findTexture(fx,"alpha.tga.png");
    assert(!start.empty());
    fx.step(.5f);fx.build(map,1.f,0.f);
    auto changed=findTexture(fx,"beta.tga.png");
    assert(!changed.empty());
    bool foundNativeUV=false;
    for (const auto* batch : changed) {
        for (const auto& v : batch->verts)
            if (std::fabs(v.u-.25f)<1e-5f || std::fabs(v.u-.75f)<1e-5f)
                foundNativeUV=true;
    }
    assert(foundNativeUV);
    // Reject a truncated UV payload without publishing a structured document.
    const auto chopped=temp/"effect/truncated.eff";
    fs::copy_file(temp/"effect/base.eff",chopped);
    fs::resize_file(chopped,fs::file_size(chopped)-10);
    top::LegacyEffectSummary truncated;
    assert(top::inspectLegacyEff(chopped.string(),truncated));
    assert(!truncated.structured && truncated.document.parts.empty());
    // Missing texture frame: retain the base image rather than discarding the effect.
    top::EffectSystem fallback;
    // use ID 43 by altering a separate skill table
    {std::ofstream f(temp/"scripts/table/skillinfo.txt");std::vector<std::string> fields(74,"0");
     fields[0]="5";fields[51]="43";
     for(size_t i=0;i<fields.size();++i)f<<(i?"\t":"")<<fields[i];f<<'\n';}
    assert(fallback.open(temp.string()));
    fallback.emit({top::EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    fallback.step(.5f);fallback.build(map,1.f,0.f);
    assert(!findTexture(fallback,"alpha.tga.png").empty());
    fs::remove_all(temp);
    std::cout<<"05-J: UV interpolado, regioes, troca de textura e fallback OK\n";
}
