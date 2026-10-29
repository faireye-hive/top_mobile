// Integration regression: lifetime policy actually reaches EffectSystem batches.
#include "top/EffectSystem.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace top;
#define REQUIRE(x) do { if(!(x))throw std::runtime_error(#x); } while(0)
template<class T> void put(std::ofstream& out,T x){out.write((const char*)&x,sizeof x);}
void name(std::ofstream& out,const char* s) {char b[32]{};for(unsigned i=0;s[i]&&i<31;++i)b[i]=s[i];out.write(b,32);}
void emitPar(const fs::path& p,int type) {
    std::ofstream o(p,std::ios::binary);
    put<unsigned>(o,2);name(o,"pool");put<int>(o,1);
    put<int>(o,type);name(o,"sample");put<int>(o,4);
    name(o,"glow.tga");name(o,"");
    for(int i=0;i<3;++i)put<float>(o,1.f); // range
    put<std::uint16_t>(o,2);put<float>(o,1.f);put<float>(o,2.f);
    for(int i=0;i<6;++i)put<float>(o,i==0?.4f:0.f);
    for(int i=0;i<2;++i)for(int j=0;j<4;++j)put<float>(o,1.f);
    put<std::uint8_t>(o,1);
    for(int v : {5,6,1,1})put<int>(o,v);
    put<float>(o,.5f);put<float>(o,1.f);
    // Historical DX9 X direction, no acceleration.
    for(int i=0;i<6;++i)put<float>(o,i==0?1.f:0.f);
    put<float>(o,.2f);
}
void effFile(const fs::path& path){
    std::ofstream f(path,std::ios::binary);
    put<unsigned>(f,6);put<int>(f,0);put<unsigned char>(f,0);name(f,"");
    put<unsigned char>(f,0);name(f,"");put<unsigned char>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0.f);put<int>(f,1);
    name(f,"layer");for(int v:{1,5,7})put<int>(f,v);
    put<float>(f,5.f);put<unsigned short>(f,1);put<float>(f,5.f);
    for(int i=0;i<3;++i)put<float>(f,2.f);
    for(int i=0;i<6;++i)put<float>(f,0.f);
    for(int i=0;i<4;++i)put<float>(f,1.f);
    put<unsigned short>(f,4);put<unsigned short>(f,1);put<float>(f,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(f,uv);
    put<unsigned short>(f,0);put<float>(f,0.f);name(f,"glow.tga");name(f,"RectPlane");
    put<unsigned char>(f,1);put<int>(f,1);put<int>(f,8);
    put<float>(f,3.f);put<float>(f,1.f);put<float>(f,2.f);
    put<unsigned short>(f,0);put<float>(f,0.f);put<float>(f,0.f);put<int>(f,0);
    put<unsigned char>(f,0);for(int i=0;i<4;++i)put<float>(f,0.f);put<unsigned char>(f,0);
}
struct Drawn {std::size_t verts=0; float centroidX=0,centroidY=0;};
Drawn draw(const fs::path& root,int skill,float seconds,bool pool,bool motion,bool extended) {
    if(pool)setenv("TOP_HISTORICAL_PARTICLE_POOL","1",1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_POOL");
    if(motion)setenv("TOP_HISTORICAL_PARTICLE_MOTION","1",1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_MOTION");
    if(extended)setenv("TOP_HISTORICAL_PARTICLE_EXTENDED","1",1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_EXTENDED");
    EffectSystem system;REQUIRE(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=skill;
    event.sourceIsPlayer=true;
    system.emit(event);system.step(seconds);
    MapFile map;system.build(map,1.f,0.f);
    Drawn drawn;float sum=0.f,sumY=0.f;
    for(const auto& batch:system.batches())
        if(batch.texture.find("glow.tga.png")!=std::string::npos)
            for(const auto& v:batch.verts){++drawn.verts;sum+=v.x;sumY+=v.y;}
    if(drawn.verts){drawn.centroidX=sum/float(drawn.verts);drawn.centroidY=sumY/float(drawn.verts);}
    return drawn;
}
int main(){
    const fs::path tmp=fs::temp_directory_path()/("sk217c-"+std::to_string(getpid()));
    fs::create_directories(tmp/"effect");fs::create_directories(tmp/"scripts/table");
    fs::create_directories(tmp/"texture/effect");
    {std::ofstream f(tmp/"texture/effect/glow.tga.png");f<<"fixture";}
    effFile(tmp/"effect/glow.eff");
    {std::ofstream f(tmp/"scripts/table/magicsingleinfo.txt");
        f<<"38\tdefault\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(tmp/"scripts/table/sceneffectinfo.txt");
        f<<"382\tblast2.par\tBlast2\n";}
    {std::ofstream f(tmp/"scripts/table/skillinfo.txt");
        std::vector<std::string> cols(74,"0");cols[0]="532";cols[51]="382";
        for(std::size_t c=0;c<cols.size();++c)f<<(c?"\t":"")<<cols[c];f<<'\n';}
    emitPar(tmp/"effect/blast2.par",10);
    // Frame count=2, life=.5. BLAST2 stops before generic particle
    // lifetime reaches 0.5; the second frame has lateral state changes.
    const auto legacy=draw(tmp,532,.12f,false,false,false);
    const auto historical=draw(tmp,532,.12f,false,false,true);
    REQUIRE(legacy.verts==24 && historical.verts==24);
    REQUIRE(std::abs(legacy.centroidX-historical.centroidX)<1e-6f);
    const auto oldLater=draw(tmp,532,.45f,false,false,false);
    const auto newLater=draw(tmp,532,.45f,false,false,true);
    REQUIRE(oldLater.verts==24 && newLater.verts==0);
    unsetenv("TOP_HISTORICAL_PARTICLE_EXTENDED");
    fs::remove_all(tmp);
    std::cout<<"SK2.17-C integration BLAST2 geometry and terminal stop OK\n";
}
