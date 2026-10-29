#include "top/EffectSystem.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs=std::filesystem;
using namespace top;

template<class T> void put(std::ofstream& out,T value) {
    out.write(reinterpret_cast<const char*>(&value),sizeof(value));
}
void name(std::ofstream& out,const char* value) {
    char bytes[32]{};
    for(unsigned i=0;value[i]&&i<31;++i)bytes[i]=value[i];
    out.write(bytes,sizeof(bytes));
}
void defaultEff(const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1);name(out,"base");
    for(int value:{1,5,7})put<std::int32_t>(out,value);
    put<float>(out,5.f);put<std::uint16_t>(out,1);put<float>(out,5.f);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,4);put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(out,uv);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name(out,"base.tga");
    name(out,"RectPlane");put<std::uint8_t>(out,1);
    put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);
    put<std::int32_t>(out,0);put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);
}
void movingChildEff(const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1);name(out,"step");
    for(int value:{1,5,7})put<std::int32_t>(out,value);
    put<float>(out,.3f);put<std::uint16_t>(out,3);
    put<float>(out,.1f);put<float>(out,.1f);put<float>(out,.1f);
    for(int i=0;i<9;++i)put<float>(out,1.f);
    for(int i=0;i<9;++i)put<float>(out,0.f);
    for(float value:{0.f,0.f,0.f,10.f,0.f,0.f,20.f,0.f,0.f})put<float>(out,value);
    for(int i=0;i<12;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,4);put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(out,uv);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name(out,"child.tga");
    name(out,"RectPlane");put<std::uint8_t>(out,0);
    put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);
    put<std::int32_t>(out,0);put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);
}
void snowPar(const fs::path& path,bool mediayEff=false) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,15);name(out,"snow");put<std::int32_t>(out,1);
    put<float>(out,1.f);
    put<std::int32_t>(out,1);name(out,"snow-system");put<std::int32_t>(out,2);
    name(out,"snow.tga");name(out,"child.eff");
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<std::uint16_t>(out,3);
    for(float size:{1.f,1.f,1.f})put<float>(out,size);
    for(int i=0;i<9;++i)put<float>(out,0.f);
    for(int i=0;i<12;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,1);
    for(int value:{5,6,1,1})put<std::int32_t>(out,value);
    put<float>(out,1.f);put<float>(out,1.f);
    put<float>(out,0.f);put<float>(out,0.f);put<float>(out,1.f);
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<float>(out,.1f);
    put<std::uint8_t>(out,0);name(out,""); // model range
    for(int i=0;i<3;++i)put<float>(out,0.f); // offset
    put<float>(out,0.f);put<float>(out,0.f); // delay and play time
    put<std::uint8_t>(out,0); // path
    put<std::uint8_t>(out,0); // shade
    name(out,""); // hit effect
    put<std::int32_t>(out,2); // random divisor
    put<std::uint8_t>(out,0); // model direction
    put<std::uint8_t>(out,mediayEff?1:0);
}
float childCentroidX(bool historical,bool mediayEff=false) {
    if(historical)setenv("TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE","1",1);
    else unsetenv("TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE");
    setenv("TOP_HISTORICAL_PARTICLE_POOL","1",1);
    setenv("TOP_HISTORICAL_PARTICLE_MOTION","1",1);
    const fs::path root=fs::temp_directory_path()/
        (std::string("sk217bp-snow-eff-")+std::to_string(getpid())+
         (historical?"-historical":"-legacy")+(mediayEff?"-per-particle":"-shared"));
    fs::create_directories(root/"effect");
    fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    {std::ofstream(root/"texture/effect/base.tga.png")<<"base";}
    {std::ofstream(root/"texture/effect/snow.tga.png")<<"controller";}
    {std::ofstream(root/"texture/effect/child.tga.png")<<"child";}
    defaultEff(root/"effect/default.eff");
    movingChildEff(root/"effect/child.eff");
    snowPar(root/"effect/snow.par",mediayEff);
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");
     f<<"600\tsnow.par\tSnow\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     std::array<std::string,74> columns;columns.fill("0");
     columns[0]="600";columns[51]="600";
     for(std::size_t i=0;i<columns.size();++i)f<<(i?"\t":"")<<columns[i];
     f<<'\n';}
    EffectSystem system;
    assert(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=600;
    event.sourceIsPlayer=true;
    system.emit(event);system.step(.65f);
    MapFile map;system.build(map,1.f,0.f);
    float sum=0.f;std::size_t count=0;
    for(const auto& batch:system.batches()) {
        if(batch.texture.find("child.tga.png")==std::string::npos)continue;
        for(const auto& vertex:batch.verts){sum+=vertex.x;++count;}
    }
    assert(count>0);
    fs::remove_all(root);
    return sum/static_cast<float>(count);
}
int main() {
    const float analytic=childCentroidX(false);
    const float shared=childCentroidX(true);
    const float perParticleFlag=childCentroidX(true,true);
    assert(std::fabs(analytic-shared)<1e-5f);
    assert(std::fabs(shared-10.f)<1e-5f);
    assert(std::fabs(perParticleFlag)<1e-5f); // unsupported per-particle child clock starts at newborn age zero
    unsetenv("TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE");
    unsetenv("TOP_HISTORICAL_PARTICLE_POOL");
    unsetenv("TOP_HISTORICAL_PARTICLE_MOTION");
}
