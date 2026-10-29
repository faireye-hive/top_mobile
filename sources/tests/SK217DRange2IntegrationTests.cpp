#include "top/EffectSystem.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fs=std::filesystem;
using namespace top;
#define REQUIRE(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)
template<class T> void put(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof value);}
void name(std::ofstream& out,const std::string& value){
    char bytes[32]{};for(std::size_t i=0;i<value.size()&&i<31;++i)bytes[i]=value[i];out.write(bytes,32);
}
void emitPar(const fs::path& path,int type,const char* texture,const char* hitEffect="",
             unsigned version=2,float life=2.f,float velocity=8.f,
             std::array<float,3> direction={{0.f,0.f,-1.f}},float step=.05f){
    std::ofstream out(path,std::ios::binary);
    put<unsigned>(out,version);name(out,"fixture");put<int>(out,1);
    if(version>=3)put<float>(out,life);
    put<int>(out,type);name(out,"range2");put<int>(out,1);name(out,texture);name(out,"");
    for(float value:std::array<float,3>{{0.f,0.f,0.f}})put<float>(out,value);
    put<std::uint16_t>(out,2);put<float>(out,1.f);put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<2;++i)for(int c=0;c<4;++c)put<float>(out,1.f);
    put<std::uint8_t>(out,1);
    for(int value:std::array<int,4>{{5,6,1,1}})put<int>(out,value);
    put<float>(out,life);put<float>(out,velocity);
    for(float value:direction)put<float>(out,value);
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<float>(out,step);
    if(version>3){put<std::uint8_t>(out,0);name(out,"");}
    if(version>4)for(int i=0;i<3;++i)put<float>(out,0.f);
    if(version>5){put<float>(out,0.f);put<float>(out,.7f);}
    if(version>8)put<std::uint8_t>(out,0);
    if(version>9)put<std::uint8_t>(out,0);
    if(version>10)name(out,hitEffect);
}
void effFile(const fs::path& path){
    std::ofstream f(path,std::ios::binary);
    put<unsigned>(f,6);put<int>(f,0);put<unsigned char>(f,0);name(f,"");
    put<unsigned char>(f,0);name(f,"");put<unsigned char>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0.f);put<int>(f,1);
    name(f,"layer");for(int v:std::array<int,3>{{1,5,7}})put<int>(f,v);
    put<float>(f,5.f);put<unsigned short>(f,1);put<float>(f,5.f);
    for(int i=0;i<3;++i)put<float>(f,2.f);
    for(int i=0;i<6;++i)put<float>(f,0.f);
    for(int i=0;i<4;++i)put<float>(f,1.f);
    put<unsigned short>(f,4);put<unsigned short>(f,1);put<float>(f,1.f);
    for(float uv:std::array<float,8>{{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f}})put<float>(f,uv);
    put<unsigned short>(f,0);put<float>(f,0.f);name(f,"base.tga");name(f,"RectPlane");
    put<unsigned char>(f,1);put<int>(f,1);put<int>(f,8);
    put<float>(f,3.f);put<float>(f,1.f);put<float>(f,2.f);
    put<unsigned short>(f,0);put<float>(f,0.f);put<float>(f,0.f);put<int>(f,0);
    put<unsigned char>(f,0);for(int i=0;i<4;++i)put<float>(f,0.f);put<unsigned char>(f,0);
}
std::size_t verticesFor(const EffectSystem& system,const std::string& name){
    std::size_t count=0;
    for(const auto& batch:system.batches())if(batch.texture.find(name)!=std::string::npos)count+=batch.verts.size();
    return count;
}
int main(){
    const fs::path root=fs::temp_directory_path()/("sk217d-range2-"+std::to_string(getpid()));
    fs::create_directories(root/"effect");fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    for(const char* tex:{"base.tga.png","range.tga.png","impact.tga.png"}){
        std::ofstream file(root/"texture/effect"/tex);file<<"fixture";
    }
    effFile(root/"effect/base.eff");
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");f<<"38\tdefault\t1\tbase.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"382\trange.par\tRange2\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
        std::vector<std::string> cols(74,"0");cols[0]="532";cols[51]="382";
        for(std::size_t i=0;i<cols.size();++i)f<<(i?"\t":"")<<cols[i];f<<'\n';}
    emitPar(root/"effect/range.par",15,"range.tga","impact.par",11,2.f,8.f);
    emitPar(root/"effect/impact.par",1,"impact.tga", "",2,1.f,0.f,{{0.f,0.f,0.f}},.1f);
    setenv("TOP_HISTORICAL_PARTICLE_EXTENDED","1",1);
    unsetenv("TOP_HISTORICAL_RANGE2_COLLISION");
    EffectSystem legacy;REQUIRE(legacy.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=532;event.sourceIsPlayer=true;
    legacy.emit(event);legacy.step(.35f);MapFile noMap;legacy.build(noMap,1.f,0.f);
    REQUIRE(verticesFor(legacy,"range.tga.png")>0);
    REQUIRE(verticesFor(legacy,"impact.tga.png")==0);

    setenv("TOP_HISTORICAL_RANGE2_COLLISION","1",1);
    EffectSystem historical;REQUIRE(historical.open(root.string()));
    historical.emit(event);historical.step(.35f);historical.build(noMap,1.f,0.f);
    REQUIRE(verticesFor(historical,"range.tga.png")==0);
    REQUIRE(verticesFor(historical,"impact.tga.png")>=20);

    // The actual-dt mode replays each EffectSystem step in order. A newly
    // emitted slot cannot move/collide in its birth update; the following
    // 16ms update crosses Z=0 and dispatches the authored hitEffect.
    setenv("TOP_HISTORICAL_RANGE2_ACTUAL_DT","1",1);
    EffectSystem actualDt;REQUIRE(actualDt.open(root.string()));
    actualDt.emit(event);
    std::size_t actualDtImpact=0;
    for(int frame=0;frame<12&&actualDtImpact==0;++frame) {
        actualDt.step(1.f/60.f);
        actualDt.build(noMap,1.f,0.f);
        actualDtImpact=verticesFor(actualDt,"impact.tga.png");
    }
    REQUIRE(actualDtImpact>=4);
    unsetenv("TOP_HISTORICAL_PARTICLE_EXTENDED");
    unsetenv("TOP_HISTORICAL_RANGE2_COLLISION");
    unsetenv("TOP_HISTORICAL_RANGE2_ACTUAL_DT");
    fs::remove_all(root);
    std::cout<<"SK2.17-D RANGE2 culling and serialized hitEffect integration OK\n";
}
