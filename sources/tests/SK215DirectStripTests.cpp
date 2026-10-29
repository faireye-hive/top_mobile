// SK2.15-A: direct CMPStrip metadata, time-stamped two-dummy ribbon and
// EffectSystem integration, without GL context or client assets.
#include "top/EffectSystem.h"
#include "top/LegacyParAudit.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace top;
#define REQUIRE(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
template<class T> void put(std::ostream& f,T v){f.write(reinterpret_cast<const char*>(&v),sizeof v);}
void name(std::ostream& f,const char* s){char n[32]{};for(std::size_t i=0;s[i]&&i<31;++i)n[i]=s[i];f.write(n,32);}
void stripFile(const fs::path& p,bool truncated=false){
    std::ofstream f(p,std::ios::binary);
    put<std::uint32_t>(f,13);name(f,"swordlight02.par");put<std::int32_t>(f,0);put<float>(f,0.f);
    put<int>(f,1); // CMPPartCtrl::_iStripNum, NOT system count
    put<int>(f,100);put<int>(f,0);put<int>(f,1);
    for(int i=0;i<4;++i)put<float>(f,1.f);
    put<float>(f,.5f);put<float>(f,.03f);name(f,"swordlight02.tga");
    put<int>(f,5);if(!truncated)put<int>(f,2);
    if(!truncated)put<int>(f,0); // v8+ trailing CChaModel count
}
void effFile(const fs::path& path,float clipSeconds=5.f){
    std::ofstream f(path,std::ios::binary);
    put<unsigned>(f,6);put<int>(f,0);put<unsigned char>(f,0);name(f,"");
    put<unsigned char>(f,0);name(f,"");put<unsigned char>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0);put<int>(f,1);
    name(f,"layer");for(int v:{1,5,7})put<int>(f,v);
    put<float>(f,clipSeconds);put<unsigned short>(f,1);put<float>(f,clipSeconds);
    for(int i=0;i<3;++i)put<float>(f,2);
    for(int i=0;i<3;++i)put<float>(f,0);
    for(int i=0;i<3;++i)put<float>(f,0);
    for(int i=0;i<4;++i)put<float>(f,1);
    put<unsigned short>(f,4);put<unsigned short>(f,1);put<float>(f,1);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(f,uv);
    put<unsigned short>(f,0);put<float>(f,0);name(f,"glow.tga");name(f,"RectPlane");
    put<unsigned char>(f,1);put<int>(f,1);put<int>(f,8);
    put<float>(f,3);put<float>(f,1);put<float>(f,2);
    put<unsigned short>(f,0);put<float>(f,0);put<float>(f,0);put<int>(f,0);
    put<unsigned char>(f,0);for(int i=0;i<4;++i)put<float>(f,0);put<unsigned char>(f,0);
}
// CMPPartSys type=6 fixture, distinct from direct CMPStrip controller tails.
// DX9 sets exactly one live particle and runs its nested .eff controller.
void nestedStripFile(const fs::path& path,int particleType,int minFilter=1,int magFilter=1){
    std::ofstream f(path,std::ios::binary);
    put<std::uint32_t>(f,2);name(f,"nested_strip");put<std::int32_t>(f,1);
    put<std::int32_t>(f,particleType);name(f,"child_effect");put<std::int32_t>(f,4);
    name(f,"1.tga");name(f,"repeat.eff");
    for(int i=0;i<3;++i)put<float>(f,0.f); // range
    put<std::uint16_t>(f,2);for(int i=0;i<2;++i)put<float>(f,1.f);
    for(int i=0;i<6;++i)put<float>(f,0.f); // angles
    for(int i=0;i<8;++i)put<float>(f,1.f); // colours
    put<std::uint8_t>(f,0); // billboard
    for(int v:{5,6,minFilter,magFilter})put<std::int32_t>(f,v); // blend and filters
    put<float>(f,3.5f);put<float>(f,1.f); // life and velocity
    for(int i=0;i<6;++i)put<float>(f,0.f); // direction and acceleration
    put<float>(f,0.f); // emission step
}
std::array<float,16> transform(float x,float y,float z){
    std::array<float,16> m{};m[0]=m[5]=m[10]=m[15]=1.f;
    m[12]=x;m[13]=y;m[14]=z;return m;
}
std::size_t countVerts(const EffectSystem& sys,const std::string& texture){
    std::size_t total=0;
    for(const auto& b:sys.batches())if(b.texture.find(texture)!=std::string::npos)total+=b.verts.size();
    return total;
}
int main(){
    unsetenv("TOP_HISTORICAL_CMPSTRIP_LIFECYCLE");
    REQUIRE(legacy_direct_strip::lifecycleEnabled());
    const auto tmp=fs::temp_directory_path()/("sk215_"+std::to_string(getpid()));
    fs::create_directories(tmp/"effect");fs::create_directories(tmp/"scripts/table");
    fs::create_directories(tmp/"texture/effect");
    stripFile(tmp/"effect/swordlight02.par");stripFile(tmp/"effect/bad.par",true);
    auto raw=[&](const char* f){std::ifstream in(tmp/"effect"/f,std::ios::binary);
        return std::vector<unsigned char>{std::istreambuf_iterator<char>(in),{}};};
    const auto r=par_audit::parse(raw("swordlight02.par"));
    REQUIRE(r.ok&&r.document.systems.empty());
    std::vector<legacy_direct_strip::Definition> strips;
    REQUIRE(legacy_direct_strip::decodeTail(raw("swordlight02.par"),r.document.consumed,r.document.version,strips));
    REQUIRE(strips.size()==1&&strips[0].maxLength==100&&strips[0].dummyA==0&&strips[0].dummyB==1);
    REQUIRE(strips[0].srcBlend==5&&strips[0].dstBlend==2&&std::abs(strips[0].life-.5f)<1e-5f);
    // DX9 CMPStrip::FrameMove samples only when its timer is strictly greater
    // than _fStep, including when the former option is absent.
    unsetenv("TOP_HISTORICAL_CMPSTRIP_STRICT_STEP");
    legacy_direct_strip::Trail defaultBoundary;
    REQUIRE(legacy_direct_strip::append(defaultBoundary,strips[0],0.f,transform(0,0,0),transform(0,1,0)));
    REQUIRE(!legacy_direct_strip::append(defaultBoundary,strips[0],strips[0].step,
        transform(1,0,0),transform(1,1,0)));
    setenv("TOP_HISTORICAL_CMPSTRIP_STRICT_STEP","1",1);
    legacy_direct_strip::Trail dx9Boundary;
    REQUIRE(legacy_direct_strip::append(dx9Boundary,strips[0],0.f,transform(0,0,0),transform(0,1,0)));
    REQUIRE(!legacy_direct_strip::append(dx9Boundary,strips[0],strips[0].step,
        transform(1,0,0),transform(1,1,0)));
    REQUIRE(legacy_direct_strip::append(dx9Boundary,strips[0],strips[0].step+.0001f,
        transform(2,0,0),transform(2,1,0)));
    unsetenv("TOP_HISTORICAL_CMPSTRIP_STRICT_STEP");
    auto bad=par_audit::parse(raw("bad.par"));REQUIRE(bad.ok);
    REQUIRE(!legacy_direct_strip::decodeTail(raw("bad.par"),bad.document.consumed,bad.document.version,strips));
    // World endpoints are DX row-vector XYZ: z is the vertical component.
    legacy_direct_strip::Trail trail;
    const auto d=legacy_direct_strip::Definition{100,0,1,{{1,1,1,1}},.5f,.03f,"glow.tga",5,2};
    REQUIRE(legacy_direct_strip::append(trail,d,0.f,transform(1,2,3),transform(1,4,3)));
    REQUIRE(!legacy_direct_strip::append(trail,d,.01f,transform(2,2,3),transform(2,4,3)));
    REQUIRE(legacy_direct_strip::append(trail,d,.04f,transform(2,2,3),transform(2,4,3)));
    std::vector<MeshVertex> mesh;
    legacy_direct_strip::emit(trail,d,.04f,mesh);
    REQUIRE(mesh.size()==6&&mesh[0].y==3&&mesh[1].z==4&&mesh[2].x==2);
    REQUIRE(mesh[0].u==0&&mesh[1].v==0&&mesh[2].u==1&&mesh[0].a<mesh[2].a);
    REQUIRE(legacy_direct_strip::append(trail,d,.6f,transform(3,2,3),transform(3,4,3)));
    mesh.clear();legacy_direct_strip::emit(trail,d,.6f,mesh);REQUIRE(mesh.empty());
    // DX9 keeps expired pairs in the vertex buffer until maxLength is reached;
    // a non-loop strip then disables rendering instead of sliding the history.
    setenv("TOP_HISTORICAL_CMPSTRIP_LIFECYCLE","1",1);
    legacy_direct_strip::Definition bounded{4,0,1,{{1,1,1,1}},.1f,.01f,"glow.tga",5,2};
    legacy_direct_strip::Trail boundedTrail;
    REQUIRE(legacy_direct_strip::append(boundedTrail,bounded,0.f,
        transform(0,0,0),transform(0,1,0)));
    REQUIRE(legacy_direct_strip::append(boundedTrail,bounded,.02f,
        transform(1,0,0),transform(1,1,0)));
    REQUIRE(boundedTrail.pairs.size()==2&&boundedTrail.capacityVertices==4);
    REQUIRE(!legacy_direct_strip::append(boundedTrail,bounded,.04f,
        transform(2,0,0),transform(2,1,0)));
    REQUIRE(boundedTrail.stopped&&boundedTrail.pairs.size()==2);
    mesh.clear();legacy_direct_strip::emit(boundedTrail,bounded,.04f,mesh);
    REQUIRE(mesh.empty());
    // The original loop path expands capacity in increments of 100 vertices.
    legacy_direct_strip::Definition looping{2,0,1,{{1,1,1,1}},.1f,.01f,"glow.tga",5,2};
    legacy_direct_strip::Trail loopingTrail;
    REQUIRE(legacy_direct_strip::append(loopingTrail,looping,0.f,
        transform(0,0,0),transform(0,1,0),true));
    REQUIRE(legacy_direct_strip::append(loopingTrail,looping,.02f,
        transform(1,0,0),transform(1,1,0),true));
    REQUIRE(legacy_direct_strip::append(loopingTrail,looping,.04f,
        transform(2,0,0),transform(2,1,0),true));
    REQUIRE(!loopingTrail.stopped&&loopingTrail.capacityVertices==102&&
            loopingTrail.pairs.size()==3);
    legacy_direct_strip::Definition aging{20,0,1,{{1,1,1,1}},.05f,.01f,"glow.tga",5,2};
    legacy_direct_strip::Trail agingTrail;
    REQUIRE(legacy_direct_strip::append(agingTrail,aging,0.f,
        transform(0,0,0),transform(0,1,0)));
    REQUIRE(legacy_direct_strip::append(agingTrail,aging,.02f,
        transform(1,0,0),transform(1,1,0)));
    REQUIRE(legacy_direct_strip::append(agingTrail,aging,.10f,
        transform(2,0,0),transform(2,1,0)));
    REQUIRE(agingTrail.pairs.size()==3); // faded DX9 vertices still consume capacity
    mesh.clear();legacy_direct_strip::emit(agingTrail,aging,.10f,mesh);
    REQUIRE(mesh.size()==12&&mesh.front().a==0.f); // DX9 still submits transparent old geometry
    unsetenv("TOP_HISTORICAL_CMPSTRIP_LIFECYCLE");
    {std::ofstream f(tmp/"texture/effect/swordlight02.tga.png");f<<"fixture";}
    {std::ofstream f(tmp/"texture/effect/glow.tga.png");f<<"fixture";}
    effFile(tmp/"effect/glow.eff");
    effFile(tmp/"effect/repeat.eff",.2f);
    nestedStripFile(tmp/"effect/type6.par",6);
    nestedStripFile(tmp/"effect/type5.par",5);
    nestedStripFile(tmp/"effect/type6_linear.par",6,2,2);
    {std::ofstream f(tmp/"texture/effect/1.tga.png");f<<"controller-mask";}
    {std::ofstream f(tmp/"scripts/table/magicsingleinfo.txt");f<<"38\tdefault\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n";}

    {std::ofstream f(tmp/"scripts/table/sceneffectinfo.txt");
      f<<"218\tswordlight02.par\tSwordLight\n";
      f<<"336\ttype6.par\tNestedStrip\n";
      f<<"337\ttype5.par\tNestedModel\n";
      f<<"338\ttype6_linear.par\tNestedStripLinear\n";}
    {std::ofstream f(tmp/"scripts/table/skillinfo.txt");
      for(int skill:{501,502,503,504,505}){std::vector<std::string> cols(74,"0");cols[0]=std::to_string(skill);
        if(skill==501){cols[51]="218";cols[50]="0";}
        else if(skill==502) cols[54]="218,0";
        else cols[51]=skill==503?"336":skill==504?"337":"338";
        for(std::size_t i=0;i<cols.size();++i)f<<(i?"\t":"")<<cols[i];f<<'\n';}}
    MapFile map;
    auto run=[&](int skill,bool provideBody,bool provideItem){
        EffectSystem sys;REQUIRE(sys.open(tmp.string()));
        EffectEvent event;event.kind=EffectEvent::Cast;event.variant=skill;event.sourceIsPlayer=true;
        sys.emit(event);
        if(provideBody)sys.setPlayerStripBodyMatrices({{0,transform(0,0,1)},{1,transform(0,1,1)}});
        if(provideItem)sys.setPlayerStripItemMatrices({{0,transform(0,0,1)},{1,transform(0,1,1)}});
        sys.step(.01f);sys.build(map,1.f,0.f);REQUIRE(countVerts(sys,"swordlight02")==0);
        if(provideBody)sys.setPlayerStripBodyMatrices({{0,transform(1,0,1)},{1,transform(1,1,1)}});
        if(provideItem)sys.setPlayerStripItemMatrices({{0,transform(1,0,1)},{1,transform(1,1,1)}});
        sys.step(.04f);sys.build(map,1.f,0.f);
        return countVerts(sys,"swordlight02");
    };
    REQUIRE(run(501,true,false)==6); // actor dummies
    REQUIRE(run(502,false,true)==6); // actual item dummies supplied
    REQUIRE(run(502,true,false)==0); // body bones never replace item bones
    REQUIRE(run(501,false,true)==0);
    // SK2.15-B: the child .eff lasts only 0.2s while the parent STRIP
    // remains active for 3.5s. A cast rendered at 0.27s must show its SECOND
    // nested clip cycle, not silently drop the track after the first cycle.
    auto nestedAt=[&](int skill,float seconds){
        EffectSystem sys;REQUIRE(sys.open(tmp.string()));
        EffectEvent event;event.kind=EffectEvent::Cast;
        event.variant=skill;event.sourceIsPlayer=true;
        sys.emit(event);sys.step(seconds);sys.build(map,1.f,0.f);
        return countVerts(sys,"glow.tga.png");
    };
    REQUIRE(nestedAt(503,.05f)==6);
    REQUIRE(nestedAt(503,.27f)==6);
    REQUIRE(nestedAt(503,.47f)==6);
    // Preserve existing MODEL (type=5) one-shot policy for cast channels.
    REQUIRE(nestedAt(504,.27f)==0);
    // SK2.15-C: same .eff/texture + blend from two .par systems with
    // different min/mag filters must NOT be merged into one GL draw.
    if (const char* setting = std::getenv("TOP_HISTORICAL_FILTERS");
        setting && setting[0]=='1' && setting[1]=='\0') {
        EffectSystem sys;REQUIRE(sys.open(tmp.string()));
        for (int skill : {503,505}) {
            EffectEvent event;event.kind=EffectEvent::Cast;
            event.variant=skill;event.sourceIsPlayer=true;sys.emit(event);
        }
        sys.step(.05f);sys.build(map,1.f,0.f);
        int point=0,linear=0;
        for (const auto& batch:sys.batches()) {
            if(batch.texture.find("glow.tga.png")==std::string::npos || batch.verts.empty())continue;
            if(batch.minFilter==1 && batch.magFilter==1){++point;REQUIRE(batch.verts.size()==6);}
            else if(batch.minFilter==2 && batch.magFilter==2){++linear;REQUIRE(batch.verts.size()==6);}
            else REQUIRE(false);
        }
        REQUIRE(point==1&&linear==1);
    }
    fs::remove_all(tmp);
    std::cout<<"SK2.15-A/B: direct CMPStrip + conventional type-6 nested model replay OK\n";
}
