// Integration regression: lifetime policy actually reaches EffectSystem batches.
#include "top/EffectSystem.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace top;
#define REQUIRE(x) do { if(!(x))throw std::runtime_error(#x); } while(0)
template<class T> void put(std::ofstream& out,T x){out.write((const char*)&x,sizeof x);}
void name(std::ofstream& out,const char* s) {char b[32]{};for(unsigned i=0;s[i]&&i<31;++i)b[i]=s[i];out.write(b,32);}
void emitPar(const fs::path& p,int type,const char* model="",bool billboard=true) {
    std::ofstream o(p,std::ios::binary);
    put<unsigned>(o,2);name(o,"pool");put<int>(o,1);
    put<int>(o,type);name(o,"sample");put<int>(o,4);
    name(o,"glow.tga");name(o,model);
    for(int i=0;i<3;++i)put<float>(o,1.f); // range
    put<std::uint16_t>(o,2);put<float>(o,1.f);put<float>(o,2.f);
    for(int i=0;i<6;++i)put<float>(o,0.f);
    for(int i=0;i<2;++i)for(int j=0;j<4;++j)put<float>(o,1.f);
    put<std::uint8_t>(o,billboard?1:0);
    for(int v : {5,6,1,1})put<int>(o,v);
    put<float>(o,.5f);put<float>(o,1.f);
    // Historical DX9 X direction, no acceleration.
    for(int i=0;i<6;++i)put<float>(o,i==0?1.f:0.f);
    put<float>(o,.2f);
}
void effFile(const fs::path& path,int keyCount=1,int regionCount=0,int subeffectType=1){
    std::ofstream f(path,std::ios::binary);
    put<unsigned>(f,6);put<int>(f,0);put<unsigned char>(f,0);name(f,"");
    put<unsigned char>(f,0);name(f,"");put<unsigned char>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0.f);put<int>(f,1);
    name(f,"layer");for(int v:{subeffectType,5,7})put<int>(f,v);
    put<float>(f,5.f);put<unsigned short>(f,static_cast<unsigned short>(keyCount));
    for(int frame=0;frame<keyCount;++frame)put<float>(f,keyCount==1?5.f:(frame==0?.1f:.9f));
    for(int frame=0;frame<keyCount;++frame)for(int i=0;i<3;++i)put<float>(f,frame==0?1.f:2.f);
    for(int frame=0;frame<keyCount;++frame)for(int i=0;i<6;++i)put<float>(f,0.f);
    for(int frame=0;frame<keyCount;++frame)for(int i=0;i<4;++i)put<float>(f,1.f);
    put<unsigned short>(f,4);put<unsigned short>(f,1);put<float>(f,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(f,uv);
    put<unsigned short>(f,static_cast<unsigned short>(regionCount));put<float>(f,.1f);name(f,"glow.tga");
    for(int region=0;region<regionCount;++region){
        const float u=static_cast<float>(region)*.25f;
        for(float uv:{u,1.f,u,0.f,u+.25f,0.f,u+.25f,1.f})put<float>(f,uv);
    }
    name(f,"RectPlane");
    put<unsigned char>(f,1);put<int>(f,1);put<int>(f,8);
    put<float>(f,3.f);put<float>(f,1.f);put<float>(f,2.f);
    put<unsigned short>(f,0);put<float>(f,0.f);put<float>(f,0.f);put<int>(f,0);
    put<unsigned char>(f,0);for(int i=0;i<4;++i)put<float>(f,0.f);put<unsigned char>(f,0);
}
struct Drawn {std::size_t verts=0; float centroidX=0,centroidY=0,minX=0,maxX=0,sumU=0,spreadSquared=0;};
Drawn draw(const fs::path& root,int skill,float seconds,bool pool,bool motion,bool extended,
           bool blast3Timeline=false,bool windBillboardGeometry=false,
           bool windWorldGeometry=false) {
    if(pool)setenv("TOP_HISTORICAL_PARTICLE_POOL","1",1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_POOL");
    if(motion)setenv("TOP_HISTORICAL_PARTICLE_MOTION","1",1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_MOTION");
    if(extended)setenv("TOP_HISTORICAL_PARTICLE_EXTENDED","1",1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_EXTENDED");
    if(blast3Timeline)setenv("TOP_HISTORICAL_BLAST3_TIMELINE","1",1);
    else unsetenv("TOP_HISTORICAL_BLAST3_TIMELINE");
    if(windBillboardGeometry)setenv("TOP_HISTORICAL_WIND_BILLBOARD_GEOMETRY","1",1);
    else unsetenv("TOP_HISTORICAL_WIND_BILLBOARD_GEOMETRY");
    if(windWorldGeometry)setenv("TOP_HISTORICAL_WIND_WORLD_GEOMETRY","1",1);
    else unsetenv("TOP_HISTORICAL_WIND_WORLD_GEOMETRY");
    EffectSystem system;REQUIRE(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=skill;
    event.sourceIsPlayer=true;
    system.emit(event);
    if(blast3Timeline)for(int i=0;i<4;++i)system.step(seconds/4.f);
    else system.step(seconds);
    MapFile map;system.build(map,1.f,0.f);
    Drawn drawn;float sum=0.f,sumY=0.f,sumZ=0.f;std::vector<MeshVertex> vertices;
    drawn.minX=std::numeric_limits<float>::infinity();
    drawn.maxX=-std::numeric_limits<float>::infinity();
    for(const auto& batch:system.batches())
        if(batch.texture.find("glow.tga.png")!=std::string::npos ||
           batch.texture.find("glow.tga")!=std::string::npos)
            for(const auto& v:batch.verts){++drawn.verts;sum+=v.x;sumY+=v.y;sumZ+=v.z;drawn.sumU+=v.u;vertices.push_back(v);
                drawn.minX=std::min(drawn.minX,v.x);drawn.maxX=std::max(drawn.maxX,v.x);}
    if(drawn.verts){drawn.centroidX=sum/float(drawn.verts);drawn.centroidY=sumY/float(drawn.verts);
        const float centroidZ=sumZ/float(drawn.verts);
        for(const auto& v:vertices)drawn.spreadSquared+=(v.x-drawn.centroidX)*(v.x-drawn.centroidX)+
            (v.y-drawn.centroidY)*(v.y-drawn.centroidY)+(v.z-centroidZ)*(v.z-centroidZ);}
    return drawn;
}
int main(){
    const fs::path tmp=fs::temp_directory_path()/("sk217b-"+std::to_string(getpid()));
    fs::create_directories(tmp/"effect");fs::create_directories(tmp/"scripts/table");
    fs::create_directories(tmp/"texture/effect");
    {std::ofstream f(tmp/"texture/effect/glow.tga.png");f<<"fixture";}
    effFile(tmp/"effect/glow.eff");
    {std::ofstream f(tmp/"scripts/table/magicsingleinfo.txt");
        f<<"38\tdefault\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(tmp/"scripts/table/sceneffectinfo.txt");
        f<<"382\tfire.par\tFire\n";}
    {std::ofstream f(tmp/"scripts/table/skillinfo.txt");
        std::vector<std::string> cols(74,"0");cols[0]="532";cols[51]="382";
        for(std::size_t c=0;c<cols.size();++c)f<<(c?"\t":"")<<cols[c];f<<'\n';}
    // ROUND: four quads still render, but their centroid must follow the
    // authored orbit center instead of the generic velocity approximation.
    emitPar(tmp/"effect/fire.par",9);
    const auto oldRound=draw(tmp,532,.25f,false,false,false);
    const auto round=draw(tmp,532,.25f,false,false,true);
    REQUIRE(oldRound.verts==24 && round.verts==24);
    REQUIRE(std::abs(round.centroidX-oldRound.centroidX)<1e-5f);
    // WIND: _CreateWind emits one slot per step, unlike four legacy quads.
    emitPar(tmp/"effect/fire.par",7);
    const auto oldWind=draw(tmp,532,.25f,false,false,false);
    const auto wind=draw(tmp,532,.25f,false,false,true);
    REQUIRE(oldWind.verts==6 && wind.verts==6);
    emitPar(tmp/"effect/fire.par",7,"RectPlane",true);
    const auto windRibbon=draw(tmp,532,.25f,false,false,true);
    const auto windBillboard=draw(tmp,532,.25f,false,false,true,false,true);
    REQUIRE(windRibbon.verts==6 && windBillboard.verts==6);
    REQUIRE(std::abs(windRibbon.spreadSquared-windBillboard.spreadSquared)<1e-5f);
    emitPar(tmp/"effect/fire.par",7,"RectPlane",false);
    const auto windWorldRibbon=draw(tmp,532,.25f,false,false,true);
    const auto windWorldPlane=draw(tmp,532,.25f,false,false,true,false,false,true);
    REQUIRE(windWorldRibbon.verts==6 && windWorldPlane.verts==6);
    REQUIRE(std::abs(windWorldRibbon.spreadSquared-windWorldPlane.spreadSquared)<1e-5f);
    // BLAST3: group emits together and travels along the radial DX9 vectors.
    emitPar(tmp/"effect/fire.par",11,"child.eff");
    effFile(tmp/"effect/child.eff",1,4,3);
    const auto oldBlast=draw(tmp,532,.4f,false,false,false);
    const auto blast=draw(tmp,532,.4f,false,false,true);
    REQUIRE(oldBlast.verts==24 && blast.verts==24);
    REQUIRE(std::abs(oldBlast.centroidY-blast.centroidY)<1e-5f);
    const auto blast3Step=draw(tmp,532,.4f,false,false,true,true);
    REQUIRE(blast3Step.verts==blast.verts);
    REQUIRE(std::abs(blast3Step.sumU-blast.sumU)>.1f);
    // The opt-in mode must not change previously validated FIRE unless both
    // historical motion and pool are active.
    emitPar(tmp/"effect/fire.par",2);
    const auto fireBase=draw(tmp,532,.25f,false,false,false);
    const auto fireExt=draw(tmp,532,.25f,false,false,true);
    REQUIRE(fireBase.verts==fireExt.verts);
    REQUIRE(std::abs(fireBase.centroidX-fireExt.centroidX)<1e-5f);
    unsetenv("TOP_HISTORICAL_PARTICLE_EXTENDED");
    unsetenv("TOP_HISTORICAL_BLAST3_TIMELINE");
    fs::remove_all(tmp);
    std::cout<<"SK2.17-B integration: DX9 ROUND, WIND, BLAST3 batches and FIRE fallback OK\n";
}
