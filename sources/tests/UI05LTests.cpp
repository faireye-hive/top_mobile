#include "top/EffectSystem.h"
#include "top/LegacyExternalEffectMesh.h"
#include "top/LegacyEffectInspector.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
namespace fs = std::filesystem;
namespace {
template<class T> void put(std::ofstream& f,T value) {f.write(reinterpret_cast<const char*>(&value),sizeof(value));}
void name32(std::ofstream& f,const std::string& name){char data[32]{};assert(name.size()<32);name.copy(data,name.size());f.write(data,32);}
void eff(const fs::path& file, const std::string& mesh){
    std::ofstream f(file,std::ios::binary);
    put<std::uint32_t>(f,6);put<std::int32_t>(f,0);
    put<std::uint8_t>(f,0);name32(f,"");put<std::uint8_t>(f,0);name32(f,"");
    put<std::uint8_t>(f,0);for(int i=0;i<4;++i)put<float>(f,0.f);
    put<std::int32_t>(f,1);name32(f,"Mesh");
    for (int n : {1,5,6})put<std::int32_t>(f,n);
    put<float>(f,1.f);put<std::uint16_t>(f,2);
    put<float>(f,0.f);put<float>(f,1.f);
    for(int i=0;i<6;++i)put<float>(f,1.f);
    for(int i=0;i<6;++i)put<float>(f,0.f);
    for(int i=0;i<6;++i)put<float>(f,0.f);
    for(int i=0;i<8;++i)put<float>(f,1.f);
    put<std::uint16_t>(f,0);put<std::uint16_t>(f,0);put<float>(f,0.f);
    put<std::uint16_t>(f,0);put<float>(f,0.f);name32(f,"alpha.tga");
    name32(f,mesh);put<std::uint8_t>(f,0);put<std::int32_t>(f,1);
    put<std::int32_t>(f,8);put<float>(f,3.f);put<float>(f,1.f);put<float>(f,2.f);
    put<std::uint16_t>(f,0);put<float>(f,0.f);put<float>(f,0.f);
    put<std::int32_t>(f,0);
    put<std::uint8_t>(f,0);for(int i=0;i<4;++i)put<float>(f,0.f);
    put<std::uint8_t>(f,0);
}
// Synthetic version 0x1003 lwGeomObjInfo fixture. Used to exercise the
// existing Model::loadGeom() binary parser, not a guessed parser in 05-L.
void lgo(const fs::path& file, bool invalidIndex=false){
    std::ofstream f(file,std::ios::binary);
    put<std::uint32_t>(f,0x1003);
    char geom[116]{};
    // Parent sentinel and 4x4 local identity at offset 12.
    std::uint32_t parent=0xffffffffu;
    std::memcpy(geom+4,&parent,4);
    float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    identity[12]=2.f; // local displacement must be preserved
    std::memcpy(geom+12,identity,sizeof(identity));
    // mesh size: 6 u32 +96 header +16 subset +9 float pos +6 float uv +3 u32 indices
    const std::uint32_t meshBytes=24+96+16+36+24+12;
    std::memcpy(geom+104,&meshBytes,4);
    f.write(geom,sizeof(geom));
    for(auto val:{std::uint32_t(0x100),std::uint32_t(4),std::uint32_t(3),
                  std::uint32_t(3),std::uint32_t(1),std::uint32_t(0)})put(f,val);
    char reserved[96]{};f.write(reserved,sizeof(reserved));
    for(auto val:{std::uint32_t(1),std::uint32_t(0),std::uint32_t(0),std::uint32_t(0)})put(f,val);
    for(float val:{0.f,0.f,0.f, 1.f,0.f,0.f, 0.f,1.f,0.f})put(f,val);
    for(float val:{0.f,0.f, 1.f,0.f, 0.f,1.f})put(f,val);
    for(auto val:{std::uint32_t(0),std::uint32_t(1),invalidIndex?std::uint32_t(7):std::uint32_t(2)})put(f,val);
}
void parNested(const fs::path& path, const std::string& modelName) {
    // Version-6 historical CMPPartCtrl with one type-6 STRIP system.
    std::ofstream f(path,std::ios::binary);
    put<std::uint32_t>(f,6);name32(f,"nested model ctrl");
    put<std::int32_t>(f,1);put<float>(f,2.f);
    put<std::int32_t>(f,6);name32(f,"external effect");
    put<std::int32_t>(f,1);name32(f,"0");name32(f,modelName);
    for(int i=0;i<3;++i)put<float>(f,0.f);
    put<std::uint16_t>(f,1);put<float>(f,1.f);
    for(int i=0;i<3;++i)put<float>(f,0.f);
    for(int i=0;i<4;++i)put<float>(f,1.f);
    put<std::uint8_t>(f,0);
    for(int i=0;i<4;++i)put<std::int32_t>(f,0);
    put<float>(f,4.5f);put<float>(f,1.f);
    for(int i=0;i<7;++i)put<float>(f,0.f);
    put<std::uint8_t>(f,0);name32(f,"");
    for(int i=0;i<3;++i)put<float>(f,0.f);
    put<float>(f,0.f);put<float>(f,.5f);
}
void skill(std::ofstream& f,int id=5,int cast=42){std::vector<std::string> cols(74,"0");cols[0]=std::to_string(id);cols[51]=std::to_string(cast);
    for(std::size_t i=0;i<cols.size();++i)f<<(i?"\t":"")<<cols[i];f<<'\n';}
std::size_t count(const top::EffectSystem& fx){std::size_t n=0;for(const auto& batch:fx.batches())
    if(batch.texture.find("alpha.tga.png")!=std::string::npos)n+=batch.verts.size();return n;}
}
int main(){
    using namespace top;
    assert(legacy_external::filename("mesh.lgo")=="mesh.lgo");
    assert(legacy_external::filename("mesh")=="mesh.lgo");
    for(const auto& bad:{"../bad.lgo","bad.txt","/tmp/evil.lgo","x\\y.lgo","..",""})
        assert(legacy_external::filename(bad).empty());
    const auto root=fs::temp_directory_path()/("top_05l_"+std::to_string(::getpid()));
    fs::remove_all(root);fs::create_directories(root/"model/effect");
    fs::create_directories(root/"effect");fs::create_directories(root/"texture/effect");
    fs::create_directories(root/"scripts/table");
    lgo(root/"model/effect/mesh.lgo");
    lgo(root/"model/effect/bad.lgo",true);
    Model model;
    assert(legacy_external::load(root.string(),"mesh.lgo",model));
    assert(!legacy_external::load(root.string(),"bad.lgo",model));
    assert(!legacy_external::load(root.string(),"nope.lgo",model));
    assert(legacy_external::load(root.string(),"mesh",model));
    assert(model.geoms.size()==1);
    legacy_mesh::Primitive primitive;
    assert(legacy_external::subsetPrimitive(model.geoms.front(),model.geoms.front().subsets.front(),0.f,primitive));
    assert(primitive.triangles.size()==1 && primitive.points.size()==3);
    assert(std::fabs(primitive.points[0].xyz[0]-2.f)<.0001f);
    assert(std::fabs(primitive.points[1].uv[0]-1.f)<.0001f);
    legacy_eff::Key key;
    std::vector<MeshVertex> output;
    legacy_mesh::emit(primitive,key,{},0.f,0.f,0.f,output);
    assert(output.size()==3);
    for(const auto& vertex:output)assert(std::isfinite(vertex.x) && std::isfinite(vertex.y));
    eff(root/"effect/real.eff","mesh.lgo");
    eff(root/"effect/missing.eff","missing.lgo");
    // .par -> nested .eff -> external .lgo; parent texture name "0" is
    // intentionally absent and must not suppress the embedded model.
    parNested(root/"effect/nested.par","real.eff");
    parNested(root/"effect/nested_missing.par","missing.eff");
    {
        std::ofstream f(root/"scripts/table/sceneffectinfo.txt");
        f<<"77\tnested.par\tExternal nested\n";
        f<<"78\tnested_missing.par\tMissing external nested\n";
    }
    {std::ofstream f(root/"scripts/table/skillinfo.txt");skill(f);skill(f,7,77);skill(f,8,78);}
    {std::ofstream f(root/"texture/effect/alpha.tga.png");f<<"texture fixture";}
    auto table=[&](const char* file){std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
        f<<"38\tbase\t1\t"<<file<<"\t0\t0\t0\t0\t0\t0\t0\n";
        f<<"42\teffect\t1\t"<<file<<"\t0\t0\t0\t0\t0\t0\t0\n";};
    table("real.eff");
    LegacyEffectSummary parse;
    assert(inspectLegacyEff((root/"effect/real.eff").string(),parse));
    assert(parse.structured && parse.document.parts.front().model=="mesh.lgo");
    EffectSystem real;assert(real.open(root.string()));
    real.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,5});
    MapFile map;real.build(map,1.f,0.f);
    const auto actual=count(real);
    table("missing.eff");
    EffectSystem missing;assert(missing.open(root.string()));
    missing.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,5});missing.build(map,1.f,0.f);
    assert(actual==3 && count(missing)==0); // missing authored mesh is silent, never a fake visual
    // SK2.5: nested external .lgo must reach the same renderer; the model
    // track is in a type-6 .par, not a standalone .eff from magicsingleinfo.
    EffectSystem nested;
    assert(nested.open(root.string()));
    nested.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,7});
    nested.step(.12f);
    nested.build(map,1.f,0.f);
    assert(count(nested)==3); // before this fix nested_tracks=0 -> 0 verts
    EffectSystem noInventedMesh;
    assert(noInventedMesh.open(root.string()));
    noInventedMesh.emit({EffectEvent::Cast,0.f,0.f,0.f,0.f,8});
    noInventedMesh.step(.12f);
    noInventedMesh.build(map,1.f,0.f);
    assert(count(noInventedMesh)==0); // never fabricate missing .lgo
    fs::remove_all(root);
    std::cout<<"05-L: .lgo real, transform, UV, subset, traversal, no fake fallback OK\n";
}
