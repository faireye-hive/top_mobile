#include "top/HistoricalDummyController.h"
#include "top/EffectSystem.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fs=std::filesystem;
using namespace top;
#define REQUIRE(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
template<class T> void put(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof value);}
void name(std::ofstream& out,const char* value){char bytes[32]{};for(int i=0;value[i]&&i<31;++i)bytes[i]=value[i];out.write(bytes,32);}
void emitPar(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,2);name(out,"dummy");put<std::int32_t>(out,1);
    put<std::int32_t>(out,16);name(out,"sample");put<std::int32_t>(out,1);
    name(out,"glow.tga");name(out,"");
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<std::uint16_t>(out,2);put<float>(out,1.f);put<float>(out,2.f);
    for(int i=0;i<6;++i)put<float>(out,i==0?0.4f:0.f);
    for(int i=0;i<2;++i)for(int j=0;j<4;++j)put<float>(out,1.f);
    put<std::uint8_t>(out,1);for(int v:{5,6,1,1})put<std::int32_t>(out,v);
    put<float>(out,.2f);put<float>(out,2.8f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    put<float>(out,0.f);
}
void emitEff(const fs::path& path){
    std::ofstream f(path,std::ios::binary);
    put<std::uint32_t>(f,6);put<std::int32_t>(f,0);put<std::uint8_t>(f,0);name(f,"");
    put<std::uint8_t>(f,0);name(f,"");put<std::uint8_t>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0.f);put<std::int32_t>(f,1);
    name(f,"layer");for(int v:{1,5,7})put<std::int32_t>(f,v);
    put<float>(f,5.f);put<std::uint16_t>(f,1);put<float>(f,5.f);
    for(int i=0;i<3;++i)put<float>(f,2.f);
    for(int i=0;i<6;++i)put<float>(f,0.f);
    for(int i=0;i<4;++i)put<float>(f,1.f);
    put<std::uint16_t>(f,4);put<std::uint16_t>(f,1);put<float>(f,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(f,uv);
    put<std::uint16_t>(f,0);put<float>(f,0.f);name(f,"glow.tga");name(f,"RectPlane");
    put<std::uint8_t>(f,1);put<std::int32_t>(f,1);put<std::int32_t>(f,8);
    put<float>(f,3.f);put<float>(f,1.f);put<float>(f,2.f);
    put<std::uint16_t>(f,0);put<float>(f,0.f);put<float>(f,0.f);put<std::int32_t>(f,0);
    put<std::uint8_t>(f,0);for(int i=0;i<4;++i)put<float>(f,0.f);put<std::uint8_t>(f,0);
}
std::array<float,16> matrix(float x,float glY,float height){
    std::array<float,16> m{};m[0]=m[5]=m[10]=m[15]=1.f;
    m[12]=x;m[13]=glY;m[14]=height;return m;
}
int main(){
    using namespace historical_dummy;
    Timeline timeline;
    advance(timeline,.016f,{0.f,0.f,2.f},{0.f,0.f,0.f},2,.2f,0.f,4,2.8f,{},17u);
    REQUIRE(timeline.slots.size()==2 && timeline.slots[0].alive && timeline.slots[1].alive);
    REQUIRE(timeline.slots[0].direction.z==1.f);
    advance(timeline,.032f,{0.f,0.f,2.f},{0.f,0.f,0.f},2,.2f,0.f,4,2.8f,{},17u);
    REQUIRE(timeline.slots[0].position.z>2.f);
    const auto stable=timeline.slots[0].position.z;
    advance(timeline,.032f,{0.f,0.f,2.f},{0.f,0.f,0.f},2,.2f,0.f,4,2.8f,{},17u);
    REQUIRE(timeline.slots[0].position.z==stable);
    advance(timeline,.048f,{2.f,0.f,0.f},{0.f,0.f,0.f},2,.2f,0.f,4,2.8f,{},17u);
    REQUIRE(std::fabs(timeline.slots[0].position.x)<1e-6f);
    REQUIRE(timeline.slots[0].position.z>stable);
    Timeline stepped;
    advance(stepped,.016f,{0.f,0.f,2.f},{0.f,0.f,0.f},2,.2f,.04f,4,2.8f,{},31u);
    REQUIRE(!stepped.slots[0].alive && !stepped.slots[1].alive);
    advance(stepped,.032f,{0.f,0.f,2.f},{0.f,0.f,0.f},2,.2f,.04f,4,2.8f,{},31u);
    REQUIRE(stepped.slots[0].alive != stepped.slots[1].alive);

    const fs::path root=fs::temp_directory_path()/
        ("sk217ao-dummy-"+std::to_string(static_cast<long long>(getpid())));
    fs::create_directories(root/"effect");fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    {std::ofstream f(root/"texture/effect/glow.tga.png");f<<"fixture";}
    emitPar(root/"effect/dummy.par");
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
        f<<"38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    emitEff(root/"effect/default.eff");
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"100\tdummy.par\tfixture\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
        std::vector<std::string> cols(67,"0");cols[0]="500";cols[54]="100,0";
        for(std::size_t i=0;i<cols.size();++i)f<<(i?"\t":"")<<cols[i];f<<'\n';}
    setenv("TOP_HISTORICAL_DUMMY_CONTROLLER","1",1);
    EffectSystem effects;REQUIRE(effects.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=500;event.sourceIsPlayer=true;
    effects.emit(event);effects.step(.016f);
    effects.setPlayerStripItemMatrices({{1,matrix(0.f,0.f,2.f)},{2,matrix(0.f,0.f,0.f)}});
    MapFile map;effects.build(map,1.f,0.f);
    std::size_t vertices=0;float minY=1000.f,maxY=-1000.f;
    for(const auto& batch:effects.batches())for(const auto& v:batch.verts){
        ++vertices;minY=std::min(minY,v.y);maxY=std::max(maxY,v.y);
    }
    REQUIRE(vertices>0);
    REQUIRE(maxY>1.f && minY>0.f); // DX9 Z-up endpoints reached GL height Y.
    unsetenv("TOP_HISTORICAL_DUMMY_CONTROLLER");
    fs::remove_all(root);
    std::cout<<"SK2.17-AO DUMMY pool and item-dummy renderer mapping OK\n";
}
