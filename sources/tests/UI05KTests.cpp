#include "top/LegacyEffectGeometry.h"
#include "top/LegacyEffectInspector.h"
#include "top/EffectSystem.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
namespace fs = std::filesystem;
namespace {
template<class T> void put(std::ofstream& o, T v){ o.write(reinterpret_cast<const char*>(&v), sizeof v); }
void name32(std::ofstream& o,const std::string& s){char n[32]{}; assert(s.size()<32);s.copy(n,s.size());o.write(n,32);}
void save(const fs::path& path,const std::string& model,bool morph=false,int destBlend=6){
    std::ofstream f(path,std::ios::binary);
    put<std::uint32_t>(f,6);put<std::int32_t>(f,0);
    put<std::uint8_t>(f,0);name32(f,"");put<std::uint8_t>(f,0);name32(f,"");
    put<std::uint8_t>(f,0);for(int i=0;i<4;++i)put<float>(f,0.f);
    put<std::int32_t>(f,1);name32(f,"Native Mesh");
    for(int n : {1,5,destBlend})put<std::int32_t>(f,n);
    put<float>(f,1.f);put<std::uint16_t>(f,2);
    put<float>(f,1.f);put<float>(f,1.f);
    for(int i=0;i<6;++i)put<float>(f,1.f); // size
    for(int i=0;i<6;++i)put<float>(f,0.f); // angle
    for(float v : {0.f,0.f,0.f, 1.f,2.f,3.f})put<float>(f,v); // position
    for(int i=0;i<8;++i)put<float>(f,1.f); // rgba
    // Quad UV animation authored directly by original tool.
    const bool rect = model=="Rect" || model=="RectPlane" || model=="RectZ";
    const bool triangle = model=="Triangle" || model=="TrianglePlane";
    const std::uint16_t count=rect?4:triangle?3:0;
    put<std::uint16_t>(f,count);put<std::uint16_t>(f,count?2:0);put<float>(f,.5f);
    for(int t=0;t<(count?2:0);++t)
        for(int i=0;i<count;++i) {put<float>(f, t ? .25f : 0.f);put<float>(f, .5f);}
    put<std::uint16_t>(f,0);put<float>(f,0.f);name32(f,"alpha.tga");
    name32(f,model);put<std::uint8_t>(f,0);put<std::int32_t>(f,1);
    put<std::int32_t>(f,8);put<float>(f,3.f);put<float>(f,1.f);put<float>(f,2.f);
    put<std::uint16_t>(f,0);put<float>(f,0.f);put<float>(f,0.f);
    put<std::int32_t>(f,morph?1:0);
    if(morph)for(int i=0;i<2;++i){put<std::int32_t>(f,8);put<float>(f,3.f+i);put<float>(f,1.f);put<float>(f,2.f);}
    put<std::uint8_t>(f,0);for(int i=0;i<4;++i)put<float>(f,0.f);
    put<std::uint8_t>(f,0);
}
// Minimal valid binary v15 particle controller. It deliberately mimics the
// global hpushf.par fallback so the test detects unrequested particles.
void fallbackPar(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    put<std::uint32_t>(out,15); name32(out,"fallback");
    put<std::int32_t>(out,1); put<float>(out,1.2f);
    put<std::int32_t>(out,4); name32(out,"generic ring");
    put<std::int32_t>(out,2); name32(out,"generic_ring"); name32(out,"");
    for(int i=0;i<3;++i)put<float>(out,0.25f);
    put<std::uint16_t>(out,1); put<float>(out,1.f);
    for(int i=0;i<3;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<std::int32_t>(out,0);
    put<float>(out,0.7f); put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    put<float>(out,0.01f);
    put<std::uint8_t>(out,0); name32(out,"");
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<float>(out,0.f); put<float>(out,0.3f);
    put<std::uint8_t>(out,0);
    put<std::uint8_t>(out,0); name32(out,"");
    put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0); put<std::uint8_t>(out,0);
}
void table(std::ofstream& f){
    std::vector<std::string> cols(74,"0");
    cols[0]="5";cols[51]="42";
    for(std::size_t i=0;i<cols.size();++i)f<<(i?"\t":"")<<cols[i];f<<'\n';
    // Illusion Slash (81) -> magic single 18 via its 1018 flight ID.
    cols.assign(74,"0"); cols[0]="81"; cols[59]="1018"; cols[63]="195";
    for(std::size_t i=0;i<cols.size();++i)f<<(i?"\t":"")<<cols[i];f<<'\n';
}
}
int main(){
    using namespace top;
    using namespace top::legacy_mesh;
    Primitive mesh;
    legacy_eff::Subeffect::ModelParams p; p.segments=8;p.height=3;p.topRadius=1;p.bottomRadius=2;
    for(const auto& n: {"Triangle","TrianglePlane","Rect","RectPlane","RectZ","Cone","Cylinder"}){
        assert(recognized(n));assert(make(n,p,mesh));
        const int expected = (std::string(n)=="Triangle" || std::string(n)=="TrianglePlane") ? 1 :
                             (std::string(n)=="Cone" || std::string(n)=="Cylinder") ? 16 : 2;
        assert(int(mesh.triangles.size())==expected);
        legacy_eff::Key key;key.position={2.f,1.f,3.f};key.size={1.f,1.f,1.f};
        std::vector<MeshVertex> output;
        emit(mesh,key,{},1.f,2.f,3.f,output);
        assert(output.size()==std::size_t(expected)*3);
        for(const auto& v:output)assert(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z));
    }
    // Illusion Slash: a geometria de voo precisa seguir a direcao do alvo.
    // No DX, RotatingXZ(-90 graus) alinha +Y local a +X do mundo.
    assert(make("RectPlane",p,mesh));
    legacy_eff::Key planeKey;
    planeKey.size = {2.f,2.f,2.f};
    std::vector<MeshVertex> north, east;
    emit(mesh,planeKey,{},0.f,0.f,0.f,north,1.f,0.f);
    emit(mesh,planeKey,{},0.f,0.f,0.f,east,1.f,-1.57079632679f);
    assert(north.size()==6 && east.size()==6);
    assert(std::fabs(north[0].x + 1.f)<.0001f && std::fabs(north[0].z + 1.f)<.0001f);
    assert(std::fabs(east[0].x + 1.f)<.0001f && std::fabs(east[0].z - 1.f)<.0001f);
    assert(std::fabs(east[0].y - north[0].y)<.0001f);
    assert(std::fabs(east[0].u - north[0].u)<.0001f);
    // Uma rota diagonal deve preservar as distancias ao centro.
    std::vector<MeshVertex> diagonal;
    emit(mesh,planeKey,{},0.f,0.f,0.f,diagonal,1.f,-.78539816339f);
    assert(std::fabs(std::hypot(diagonal[0].x, diagonal[0].z) - std::sqrt(2.f)) < .0001f);
    assert(!recognized("untrusted.lgo") && !make("untrusted.lgo",p,mesh));
    auto invalid=p;invalid.segments=0;assert(!make("Cylinder",invalid,mesh));
    invalid=p;invalid.bottomRadius=std::nanf("");assert(!make("Cone",invalid,mesh));
    const auto root=fs::temp_directory_path()/("top_05k_"+std::to_string(::getpid()));
    fs::remove_all(root);fs::create_directories(root/"effect");
    fs::create_directories(root/"texture/effect");fs::create_directories(root/"scripts/table");
    save(root/"effect/native.eff","Rect",false);
    save(root/"effect/cone.eff","Cone",true);
    save(root/"effect/unknown.eff","unknown.lgo",false);
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tbase\t1\tnative.eff\t0\t0\t0\t0\t0\t0\t0\n";
     f<<"42\tnative\t1\tnative.eff\t0\t0\t0\t0\t0\t0\t0\n";
     f<<"18\tIllusion Slash flight\t1\tnative.eff\t15\t0\t0\t-1\t3\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");table(f);}
    {std::ofstream f(root/"texture/effect/alpha.tga.png");f<<"fake texture file";}
    {std::ofstream f(root/"texture/effect/generic_ring.tga.png");f<<"fake texture file";}
    fallbackPar(root/"effect/hpushf.par");
    // Synthetic equivalent of 195's .par file, separate from the 1018
    // geometry. Validates that target impact goes through its own channel.
    fallbackPar(root/"effect/hit195.par");
    { std::ofstream f(root/"scripts/table/sceneffectinfo.txt");
      f<<"195\thit195.par\tIllusion Slash Hit\tSea Wave\t0\t0\t-1\t-1\t0\t1\t0\t-1\n"; }

    LegacyEffectSummary report;assert(inspectLegacyEff((root/"effect/native.eff").string(),report));
    assert(report.structured && report.document.parts.size()==1);
    auto& part=report.document.parts[0];
    assert(!part.billboard && part.model=="Rect" && part.uvVertexCount==4);
    const auto sampled=legacy_eff::sample(part,.5f);
    assert(std::fabs(sampled.position[0]-.5f)<1e-5f);
    assert(part.coordFrames.size()==2 && !part.billboard);
    assert(make(part.model,part.modelParams,mesh));
    std::vector<legacy_mesh::UV> defaultUV;
    for(const auto& pt:mesh.points)defaultUV.push_back(pt.uv);
    auto animated=legacy_eff::sampleMeshUV(part,.25f,defaultUV);
    assert(animated.size()==4 && std::fabs(animated[0][0]-.125f)<1e-5f);
    std::vector<MeshVertex> generated;
    emit(mesh,sampled,animated,0.f,0.f,0.f,generated);
    assert(generated.size()==6 && std::fabs(generated[0].u-.125f)<1e-5f);
    // The versioned ModelParam track for dynamic primitives must be retained.
    LegacyEffectSummary cone;assert(inspectLegacyEff((root/"effect/cone.eff").string(),cone));
    assert(cone.structured && cone.document.parts[0].animatedParams.size()==2);
    assert(std::fabs(paramsAt(cone.document.parts[0],.5f).height-3.5f)<1e-5f);
    assert(make("Cone",paramsAt(cone.document.parts[0],.5f),mesh));
    // Entire file must validate before a partial model reaches the renderer.
    fs::copy_file(root/"effect/cone.eff",root/"effect/truncated.eff");
    fs::resize_file(root/"effect/truncated.eff",fs::file_size(root/"effect/truncated.eff")-8);
    LegacyEffectSummary broken;assert(inspectLegacyEff((root/"effect/truncated.eff").string(),broken));
    assert(!broken.structured && broken.document.parts.empty());
    EffectSystem fx;assert(fx.open(root.string()));
    fx.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    MapFile map;
    fx.build(map,1.f,0.f);
    auto vertexCount=[](const EffectSystem& system){
        std::size_t count=0;
        for(const auto& b:system.batches())
            if(b.texture.find("alpha.tga.png")!=std::string::npos) count+=b.verts.size();
        return count;
    };
    const std::size_t rectVertices=vertexCount(fx);
    // 05-BG: Illusion Slash has an authored native .eff mesh but no .par.
    // The universal hpushf.par fallback must NOT add an unrelated ring.
    EffectSystem slash; assert(slash.open(root.string()));
    slash.emit({EffectEvent::Flight,0.f,0.f,4.f,0.f,81,0.25f});
    assert(slash.activeCount()==1);
    slash.build(map,1.f,0.f);
    bool slashMesh=false, unwantedGenericRing=false;
    for (const auto& b:slash.batches()) {
        if (b.verts.empty()) continue;
        slashMesh |= b.texture.find("alpha.tga.png")!=std::string::npos;
        unwantedGenericRing |= b.texture.find("generic_ring.tga.png")!=std::string::npos;
    }
    assert(slashMesh && !unwantedGenericRing);
    // SK2.8: Part_fshade first renders at the original scale and then
    // switches to absolute Scaling(3). No permanent guessed 2.75x boost.
    auto planarSpan=[](const EffectSystem& sys) {
        float xlo=1e9f,xhi=-1e9f,zlo=1e9f,zhi=-1e9f;
        for(const auto& b:sys.batches()) {
            if(b.texture.find("alpha.tga.png")==std::string::npos) continue;
            for(const auto& v:b.verts) {
                xlo=std::min(xlo,v.x);xhi=std::max(xhi,v.x);
                zlo=std::min(zlo,v.z);zhi=std::max(zhi,v.z);
            }
        }
        return std::hypot(xhi-xlo,zhi-zlo);
    };
    const float firstFrameSpan=planarSpan(slash);
    slash.step(.015f);
    slash.build(map,1.f,0.f);
    assert(firstFrameSpan>.001f && planarSpan(slash)>firstFrameSpan*2.5f);
    // Optional factor remains available for side-by-side calibration.
    assert(::setenv("TOP_ILLUSION_FX_SCALE", "1.25", 1) == 0);
    EffectSystem tuned; assert(tuned.open(root.string()));
    tuned.emit({EffectEvent::Flight,0.f,0.f,4.f,0.f,81,0.25f});
    tuned.step(.015f);
    tuned.build(map,1.f,0.f);
    assert(::unsetenv("TOP_ILLUSION_FX_SCALE") == 0);
    assert(planarSpan(tuned)>planarSpan(slash)*1.15f);
    EffectSystem impact; assert(impact.open(root.string()));
    impact.emit({EffectEvent::SkillHit,0.f,0.f,4.f,0.f,81});
    impact.step(.01f); // shared FIRE emitter reaches its authored interval
    impact.build(map,1.f,0.f);
    bool sawHit195 = false;
    for (const auto& b: impact.batches())
        if (!b.verts.empty() && b.texture.find("generic_ring.tga.png")!=std::string::npos)
            sawHit195 = true;
    assert(sawHit195);

    // DX9 Part_fshade discards timer overshoot and adds at most one trail
    // copy per call. A long update must therefore differ from age-derived
    // copy counting when the opt-in historical state is enabled.
    auto fshadeVertices=[&](EffectSystem& system) {
        std::size_t count=0;
        for(const auto& batch:system.batches())
            if(batch.texture.find("alpha.tga.png")!=std::string::npos)
                count+=batch.verts.size();
        return count;
    };
    assert(::setenv("TOP_HISTORICAL_FSHADE_FRAME_COUNT","1",1)==0);
    EffectSystem fshadeHistorical; assert(fshadeHistorical.open(root.string()));
    fshadeHistorical.emit({EffectEvent::Flight,0.f,0.f,100.f,0.f,81,8.f});
    fshadeHistorical.step(1.21f);
    fshadeHistorical.build(map,1.f,0.f);
    const auto historicalFshadeVertices=fshadeVertices(fshadeHistorical);
    assert(::unsetenv("TOP_HISTORICAL_FSHADE_FRAME_COUNT")==0);
    EffectSystem fshadeAgeBased; assert(fshadeAgeBased.open(root.string()));
    fshadeAgeBased.emit({EffectEvent::Flight,0.f,0.f,100.f,0.f,81,8.f});
    fshadeAgeBased.step(1.21f);
    fshadeAgeBased.build(map,1.f,0.f);
    assert(historicalFshadeVertices>0 &&
           fshadeVertices(fshadeAgeBased)>historicalFshadeVertices);

    auto useModel=[&](const std::string& file){
        std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
        f<<"38\tbase\t1\t"<<file<<"\t0\t0\t0\t0\t0\t0\t0\n";
        f<<"42\tmodel\t1\t"<<file<<"\t0\t0\t0\t0\t0\t0\t0\n";
    };
    // Historical D3DBLEND_SRCALPHA / D3DBLEND_INVSRCALPHA must render with
    // GL straight-alpha mode, not the additive fallback. A different blend
    // state must not be merged with the additive batch for the same texture.
    save(root/"effect/normal_alpha.eff", "RectPlane", false, 7);
    useModel("normal_alpha.eff");
    EffectSystem straight; assert(straight.open(root.string()));
    straight.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    straight.build(map,1.f,0.f);
    bool sawNormalAlpha = false;
    for (const auto& b:straight.batches())
        if (b.texture.find("alpha.tga.png")!=std::string::npos && b.verts.size()==6) {
            assert(b.mode == 8);
            sawNormalAlpha = true;
        }
    assert(sawNormalAlpha);
    // A travelling native RectPlane with a 1.0s animation track must be
    // destroyed when its 0.25s trajectory reaches the target, as in DX9.
    // Otherwise the slash remains parked at the monster for a full second
    // (5 seconds for the historical 04000009.eff), appearing as a lingering
    // spiral when repeated. Non-flight cast keeps its authored lifetime.
    EffectSystem projectile; assert(projectile.open(root.string()));
    projectile.emit({EffectEvent::Flight,0.f,0.f,4.f,0.f,5,0.25f});
    assert(projectile.activeCount()==1);
    projectile.step(0.24f); assert(projectile.activeCount()==1);
    projectile.step(0.02f); assert(projectile.activeCount()==0);
    EffectSystem castLifetime; assert(castLifetime.open(root.string()));
    castLifetime.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    castLifetime.step(0.26f); assert(castLifetime.activeCount()==1);
    useModel("unknown.eff");
    EffectSystem unknown; assert(unknown.open(root.string()));
    unknown.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    unknown.build(map,1.f,0.f);
    assert(rectVertices == 6 && vertexCount(unknown) == 0); // unsupported documented effect stays silent, not a fake tornado
    useModel("cone.eff");
    EffectSystem dynamicCone; assert(dynamicCone.open(root.string()));
    dynamicCone.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    dynamicCone.build(map,1.f,0.f);
    assert(vertexCount(dynamicCone) == 48); // authored primitive replaces the procedural ring
    fs::remove_all(root);
    std::cout<<"05-K: 7 primitivas, UV, transformacao, morfologia, sem efeitos artificiais OK\n";
}
