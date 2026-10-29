#include "top/EffectSystem.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs=std::filesystem;
using namespace top;

template<class T> void put(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof value);}
void name(std::ofstream& out,const char* value){char data[32]{};for(int i=0;value[i]&&i<31;++i)data[i]=value[i];out.write(data,sizeof data);}

void roundPar(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<unsigned>(out,2);name(out,"round");put<int>(out,1);
    put<int>(out,9);name(out,"ring");put<int>(out,4);
    name(out,"glow.tga");name(out,"");
    put<float>(out,4.f);put<float>(out,2.f);put<float>(out,1.f);
    put<std::uint16_t>(out,4);
    for(float value:{1.f,2.f,3.f,4.f})put<float>(out,value);
    for(int i=0;i<12;++i)put<float>(out,0.f);
    for(int frame=0;frame<4;++frame)
        for(float value:{1.f,1.f,1.f,1.f})put<float>(out,value);
    put<std::uint8_t>(out,1);
    for(int value:{5,6,1,1})put<int>(out,value);
    put<float>(out,1.f);put<float>(out,2.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    put<float>(out,0.f);
}

void defaultEff(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<unsigned>(out,6);put<int>(out,0);put<std::uint8_t>(out,0);name(out,"");
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);put<int>(out,1);
    name(out,"layer");for(int value:{1,5,7})put<int>(out,value);
    put<float>(out,5.f);put<std::uint16_t>(out,1);put<float>(out,5.f);
    for(int i=0;i<3;++i)put<float>(out,0.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,4);put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(float value:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(out,value);
    put<std::uint16_t>(out,0);put<float>(out,.1f);name(out,"glow.tga");
    name(out,"RectPlane");put<std::uint8_t>(out,1);put<int>(out,1);put<int>(out,8);
    for(float value:{3.f,1.f,2.f})put<float>(out,value);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);put<int>(out,0);
    put<std::uint8_t>(out,0);for(int i=0;i<4;++i)put<float>(out,0.f);put<std::uint8_t>(out,0);
}

float averageQuadWidth(const fs::path& root,bool timeline){
    setenv("TOP_HISTORICAL_PARTICLE_EXTENDED","1",1);
    if(timeline)setenv("TOP_HISTORICAL_ROUND_TIMELINE","1",1);
    else unsetenv("TOP_HISTORICAL_ROUND_TIMELINE");
    EffectSystem system;assert(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=532;event.sourceIsPlayer=true;
    system.emit(event);
    system.step(.8f); // DX9 GetCurFrame advances only once and drops overshoot.
    MapFile map;system.build(map,1.f,0.f);
    float width=0.f;unsigned quads=0;
    for(const auto& batch:system.batches())
        for(std::size_t i=0;i+5<batch.verts.size();i+=6){
            const auto& a=batch.verts[i];const auto& b=batch.verts[i+1];
            const float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;
            width+=std::sqrt(dx*dx+dy*dy+dz*dz);
            ++quads;
        }
    assert(quads==4);
    return width/static_cast<float>(quads);
}

int main(){
    const fs::path root=fs::temp_directory_path()/
        ("sk217am-round-"+std::to_string(static_cast<long long>(getpid())));
    fs::create_directories(root/"effect");
    fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    {std::ofstream texture(root/"texture/effect/glow.tga.png");texture<<"fixture";}
    defaultEff(root/"effect/glow.eff");
    roundPar(root/"effect/round.par");
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
        f<<"38\tdefault\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"382\tround.par\tRound fixture\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
        std::vector<std::string> columns(74,"0");columns[0]="532";columns[51]="382";
        for(std::size_t i=0;i<columns.size();++i)f<<(i?"\t":"")<<columns[i];f<<'\n';}
    const float continuous=averageQuadWidth(root,false);
    const float frameStepped=averageQuadWidth(root,true);
    assert(std::fabs(frameStepped-continuous)<1e-5f);
    assert(std::fabs(frameStepped-.48f)<1e-5f); // key 1 size=2, quad width=2*.12*2
    unsetenv("TOP_HISTORICAL_ROUND_TIMELINE");
    unsetenv("TOP_HISTORICAL_PARTICLE_EXTENDED");
    fs::remove_all(root);
}
