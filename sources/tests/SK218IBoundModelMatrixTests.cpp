#include "top/EffectSystem.h"
#include "top/EffectBindingAudit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>void put(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof value);}
void name(std::ofstream& out,const char* value){char b[32]{};for(int i=0;value[i]&&i<31;++i)b[i]=value[i];out.write(b,32);}
void eff(const fs::path& path,bool billboard=false,const char* mesh="RectPlane"){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);put<std::uint8_t>(out,0);name(out,"");
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1);name(out,"bound billboard");
    for(int v:{1,5,7})put<std::int32_t>(out,v);
    put<float>(out,1.f);put<std::uint16_t>(out,2);put<float>(out,0.f);put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,i%3==1?2.f:1.f); // child scale (1,2,1)
    for(int i=0;i<6;++i)put<float>(out,i%3==2?.4f:0.f); // child Z rotation
    for(int i=0;i<6;++i)put<float>(out,float(i%3+1)); // child XYZ=(1,2,3)
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<std::uint16_t>(out,0);put<float>(out,0.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name(out,"bound.tga");name(out,mesh);
    put<std::uint8_t>(out,billboard?1:0);put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    put<float>(out,3.f);put<float>(out,1.f);put<float>(out,2.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);for(int i=0;i<4;++i)put<float>(out,0.f);put<std::uint8_t>(out,0);
}
// Synthetic version-0x1003 lwGeomObjInfo, same format fixture as UI05LTests.
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
void par(const fs::path& path,bool modelDirection,int type=6,bool billboard=false){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,14);name(out,"controller");put<std::int32_t>(out,1);put<float>(out,4.f);
    put<std::int32_t>(out,type);name(out,"parent");put<std::int32_t>(out,1);name(out,"bound.tga");name(out,"child.eff");
    for(int i=0;i<3;++i)put<float>(out,float(4+i*2)); // nonzero range
    put<std::uint16_t>(out,1);put<float>(out,2.f); // parent scale
    for(int i=0;i<3;++i)put<float>(out,float(i+1)*.3f); // ignored in bound branch
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,billboard?1:0);for(int v:{5,7,1,1})put<std::int32_t>(out,v);
    put<float>(out,4.f);put<float>(out,0.f);for(int i=0;i<7;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);name(out,"");
    put<float>(out,0.f);put<float>(out,0.f);put<float>(out,1.f); // parent offset Z=1
    put<float>(out,0.f);put<float>(out,2.f);put<std::uint8_t>(out,0);put<std::uint8_t>(out,0);
    name(out,"");put<std::int32_t>(out,2);put<std::uint8_t>(out,modelDirection?1:0);
}
void skill(std::ofstream& out,int id,int actor,int target,int item){
    std::vector<std::string> c(74,"0");c[0]=std::to_string(id);c[50]="1";c[51]=std::to_string(actor);
    c[53]="1";c[54]=std::to_string(item)+",0";c[62]="1";c[63]=std::to_string(target);
    for(std::size_t i=0;i<c.size();++i)out<<(i?"\t":"")<<c[i];out<<'\n';
}
std::vector<top::MeshVertex> vertices(const fs::path& root,int id,bool enabled,bool bound,
                                     bool affine=true,bool itemBinding=true,bool targetBinding=true){
    if(enabled)setenv("TOP_HISTORICAL_BOUND_MODEL_MATRIX","1",1);
    else unsetenv("TOP_HISTORICAL_BOUND_MODEL_MATRIX");
    setenv("TOP_HISTORICAL_TARGET_DUMMY_BIND",targetBinding?"1":"0",1);
    setenv("TOP_HISTORICAL_ITEM_DUMMY_BIND",itemBinding?"1":"0",1);
    top::EffectSystem effect;require(effect.open(root.string()),"open synthetic client");
    std::array<float,16> m{};m[0]=m[5]=m[10]=m[15]=1.f;
    if(affine){m[0]=m[5]=0.f;m[1]=2.f;m[4]=-3.f;m[8]=.25f;m[10]=4.f;}
    m[12]=10.f;m[13]=20.f;m[14]=30.f;
    if(bound){effect.setPlayerDummyMatrices({{1,m}});effect.setTargetDummyMatrices({{0,{{1,m}}}});effect.setPlayerStripItemMatrices({{1,m}});}
    effect.setPlayerCastAnchor(10.f,20.f,30.f);
    top::EffectEvent event;event.kind=id==243?top::EffectEvent::SkillHit:top::EffectEvent::Cast;
    event.variant=id;event.sourceIsPlayer=true;event.targetActorIndex=0;
    effect.emit(event);effect.step(.1f);top::MapFile map;effect.build(map,1.f,0.f);
    std::vector<top::MeshVertex> out;
    for(const auto& batch:effect.batches())if(batch.texture.find("bound.tga.png")!=std::string::npos)
        out.insert(out.end(),batch.verts.begin(),batch.verts.end());
    require(out.size()==6||out.size()==3,"authored procedural/external triangles exist");return out;
}
void same(const std::vector<top::MeshVertex>& a,const std::vector<top::MeshVertex>& b){
    require(a.size()==b.size(),"same vertex count");
    for(std::size_t i=0;i<a.size();++i)
        require(std::fabs(a[i].x-b[i].x)<1e-5f&&std::fabs(a[i].y-b[i].y)<1e-5f&&
                std::fabs(a[i].z-b[i].z)<1e-5f,"excluded branch unchanged");
}
void expected(const std::vector<top::MeshVertex>& out,bool affine,bool external=false){
    // DX9 row vector: child S/R/T * parent scale * bone, then swap Y/Z.
    // Parent PAR angles/range are not in the _bUseBone .eff branch.
    const float xy[6][2]={{-.5f,-.5f},{-.5f,.5f},{.5f,.5f},{-.5f,-.5f},{.5f,.5f},{.5f,-.5f}};
    for(std::size_t i=0;i<out.size();++i){
        const float mx=external?(i==1?3.f:2.f):xy[i][0];
        const float my=external?(i==2?1.f:0.f):xy[i][1];
        const float cx=mx,cy=my*2.f;
        const float lx=(1.f+cx*std::cos(.4f)-cy*std::sin(.4f))*2.f;
        const float ly=(2.f+cx*std::sin(.4f)+cy*std::cos(.4f))*2.f;
        const float lz=3.f*2.f;
        const float x=10.f+(affine?-3.f*ly+.25f*lz:lx);
        const float y=31.f+(affine?4.f*lz:lz); // offset Z=1 is world, not bone rotated/scaled
        const float z=20.f+(affine?2.f*lx:ly);
        require(std::fabs(out[i].x-x)<1e-4f&&std::fabs(out[i].y-y)<1e-4f&&std::fabs(out[i].z-z)<1e-4f,
                "non-billboard child follows full DX9 bone matrix and world-space parent offset");
    }
}
}
int main(){
    const auto root=fs::temp_directory_path()/("top_sk218i_"+std::to_string(getpid()));
    try{
        fs::create_directories(root/"scripts/table");fs::create_directories(root/"effect");fs::create_directories(root/"texture/effect");
        eff(root/"effect/child.eff");par(root/"effect/bound.par",false);par(root/"effect/directed.par",true);
        par(root/"effect/model.par",false,5);par(root/"effect/board.par",false,6,true);
        std::ofstream(root/"texture/effect/bound.tga.png")<<"fixture";
        {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");f<<"38\tdefault\t1\tchild.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
        {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"336\tbound.par\tbound\tx\t0\t0\t-1\t-1\t0\t1\t0\t-1\n337\tdirected.par\tdir\tx\t0\t0\t-1\t-1\t0\t1\t0\t-1\n338\tmodel.par\tmodel\tx\t0\t0\t-1\t-1\t0\t1\t0\t-1\n339\tboard.par\tboard\tx\t0\t0\t-1\t-1\t0\t1\t0\t-1\n";}
        {std::ofstream f(root/"scripts/table/skillinfo.txt");skill(f,242,336,0,0);skill(f,243,0,336,0);skill(f,244,337,0,0);skill(f,245,0,0,336);skill(f,246,338,0,0);skill(f,247,339,0,0);}
        for(int id:{242,243,245,246})for(bool affine:{false,true}){
            expected(vertices(root,id,true,true,affine),affine);
            const auto old=vertices(root,id,false,true,affine);
            const auto corrected=vertices(root,id,true,true,affine);
            expected(old,affine);
            same(old,corrected);
        }
        same(vertices(root,244,false,true),vertices(root,244,true,true));
        same(vertices(root,247,false,true),vertices(root,247,true,true));
        same(vertices(root,242,false,false),vertices(root,242,true,false));
        same(vertices(root,245,false,true,true,false),vertices(root,245,true,true,true,false));
        same(vertices(root,243,false,true,true,true,false),vertices(root,243,true,true,true,true,false));
        const auto audit=fs::path(root.string()+"_audit");std::ostringstream auditLog;
        require(top::effect_binding_audit::run(root,audit,auditLog)==0,"static binding audit runs on synthetic format");
        require(auditLog.str().find("non_billboard_candidates=4 non_billboard_candidate_skills=4")!=std::string::npos,
                "audit matches non-billboard MODEL/STRIP actor/target/item candidates");
        fs::remove_all(audit);
        fs::create_directories(root/"model/effect");lgo(root/"model/effect/mesh.lgo");
        eff(root/"effect/child.eff",false,"mesh.lgo");
        for(int id:{242,243,245,246})for(bool affine:{false,true})
            expected(vertices(root,id,true,true,affine),affine,true);
        // A child billboard must retain its separate camera-facing rule.
        eff(root/"effect/child.eff",true);
        same(vertices(root,242,false,true),vertices(root,242,true,true));
        unsetenv("TOP_HISTORICAL_BOUND_MODEL_MATRIX");unsetenv("TOP_HISTORICAL_TARGET_DUMMY_BIND");unsetenv("TOP_HISTORICAL_ITEM_DUMMY_BIND");
        fs::remove_all(root);std::cout<<"SK2.18-I bound child model matrices OK\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';fs::remove_all(root);return 1;}
}
