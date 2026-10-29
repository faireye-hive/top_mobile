#include "top/EffectSystem.h"
#include "top/HistoricalModelPosition.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
void modelPar(const fs::path& path,float delay=0.f) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);name(out,"model");put<std::int32_t>(out,1);
    put<float>(out,2.f);
    put<std::int32_t>(out,5);name(out,"model-system");put<std::int32_t>(out,1);
    name(out,"controller.tga");name(out,"child.eff");
    for(float value:{4.f,6.f,8.f})put<float>(out,value);
    put<std::uint16_t>(out,2);
    put<float>(out,1.f);put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,1);
    for(int value:{5,6,1,1})put<std::int32_t>(out,value);
    put<float>(out,.4f);put<float>(out,0.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    put<float>(out,0.f);
    put<std::uint8_t>(out,0);name(out,"");
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<float>(out,delay);put<float>(out,0.f);
}
float childCentroid(bool historical, bool modelPosition=false,float delay=0.f,
                    float nextStep=0.f) {
    if(historical) {
        setenv("TOP_HISTORICAL_MODEL_FRAME_STEP","1",1);
        setenv("TOP_HISTORICAL_MODEL_EFF_TIMELINE","1",1);
    } else {
        unsetenv("TOP_HISTORICAL_MODEL_FRAME_STEP");
        unsetenv("TOP_HISTORICAL_MODEL_EFF_TIMELINE");
    }
    if(modelPosition)setenv("TOP_HISTORICAL_MODEL_POSITION","1",1);
    else unsetenv("TOP_HISTORICAL_MODEL_POSITION");
    const fs::path root=fs::temp_directory_path()/
        (std::string("sk217aj-model-eff-")+std::to_string(getpid())+
         (historical?"-historical":"-analytic"));
    fs::create_directories(root/"effect");
    fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    {std::ofstream(root/"texture/effect/base.tga.png")<<"base";}
    {std::ofstream(root/"texture/effect/controller.tga.png")<<"controller";}
    {std::ofstream(root/"texture/effect/child.tga.png")<<"child";}
    defaultEff(root/"effect/default.eff");
    movingChildEff(root/"effect/child.eff");
    modelPar(root/"effect/model.par",delay);
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");
     f<<"600\tmodel.par\tMODEL timeline fixture\n";}
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     std::array<std::string,74> columns;columns.fill("0");
     columns[0]="600";columns[51]="600";
     for(std::size_t i=0;i<columns.size();++i)f<<(i?"\t":"")<<columns[i];
     f<<'\n';}
    EffectSystem system;
    assert(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=600;
    event.sourceIsPlayer=true;
    system.emit(event);system.step(.15f);
    if(nextStep>0.f)system.step(nextStep);
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
    unsetenv("TOP_HISTORICAL_MODEL_POSITION");
    assert(historical_model_position::enabled());
    const auto midpoint=historical_model_position::midpointOffset({{4.f,6.f,8.f}});
    assert(midpoint[0]==2.f && midpoint[1]==3.f && midpoint[2]==4.f);
    const float analytic=childCentroid(false,false);
    const float stepped=childCentroid(true,false);
    const float positioned=childCentroid(false,true);
    const float delayedCrossing=childCentroid(true,false,.1f);
    const float delayedNext=childCentroid(true,false,.1f,.1f);
    assert(std::fabs(analytic-stepped)<1e-5f);
    assert(std::fabs(analytic-positioned)<1e-5f);
    assert(std::fabs(delayedCrossing-2.f)<1e-5f);
    assert(std::fabs(delayedNext-12.f)<1e-5f);
    unsetenv("TOP_HISTORICAL_MODEL_FRAME_STEP");
    unsetenv("TOP_HISTORICAL_MODEL_EFF_TIMELINE");
    unsetenv("TOP_HISTORICAL_MODEL_POSITION");
}
