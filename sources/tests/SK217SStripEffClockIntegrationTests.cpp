#include "top/EffectSystem.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace top;

template<class T> void put(std::ofstream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void name(std::ofstream& out, const char* value) {
    char bytes[32]{};
    for (unsigned i=0; value[i] && i<31; ++i) bytes[i]=value[i];
    out.write(bytes,sizeof(bytes));
}
void defaultEff(const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6); put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0); name(out,""); put<std::uint8_t>(out,0);
    name(out,""); put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1); name(out,"base");
    for(int value:{1,5,7})put<std::int32_t>(out,value);
    put<float>(out,5.f); put<std::uint16_t>(out,1); put<float>(out,5.f);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,4); put<std::uint16_t>(out,1); put<float>(out,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(out,uv);
    put<std::uint16_t>(out,0); put<float>(out,0.f); name(out,"glow.tga");
    name(out,"RectPlane"); put<std::uint8_t>(out,1);
    put<std::int32_t>(out,1); put<std::int32_t>(out,8);
    for(float v:{1.f,1.f,1.f})put<float>(out,v);
    put<std::uint16_t>(out,0); put<float>(out,0.f); put<float>(out,0.f);
    put<std::int32_t>(out,0); put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);
}
void twoFrameEff(const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6); put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0); name(out,""); put<std::uint8_t>(out,0);
    name(out,""); put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1); name(out,"step");
    for(int value:{1,5,7})put<std::int32_t>(out,value);
    put<float>(out,.2f); put<std::uint16_t>(out,2);
    put<float>(out,.1f); put<float>(out,.1f);
    for(float v:{1.f,1.f,1.f, 1.f,1.f,1.f})put<float>(out,v);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(float v:{0.f,0.f,0.f, 10.f,0.f,0.f})put<float>(out,v);
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,4); put<std::uint16_t>(out,1); put<float>(out,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(out,uv);
    put<std::uint16_t>(out,0); put<float>(out,0.f); name(out,"glow.tga");
    name(out,"RectPlane"); put<std::uint8_t>(out,1);
    put<std::int32_t>(out,1); put<std::int32_t>(out,8);
    for(float v:{1.f,1.f,1.f})put<float>(out,v);
    put<std::uint16_t>(out,2); put<float>(out,.1f);
    name(out,"frame0.tga"); name(out,"frame1.tga"); put<float>(out,0.f);
    put<std::int32_t>(out,0); put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);
}
void stripPar(const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,2); name(out,"strip"); put<std::int32_t>(out,1);
    put<std::int32_t>(out,6); name(out,"strip-system"); put<std::int32_t>(out,1);
    name(out,"1.tga"); name(out,"repeat.eff");
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<std::uint16_t>(out,2); put<float>(out,1.f); put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,0);
    for(int value:{5,6,1,1})put<std::int32_t>(out,value);
    put<float>(out,3.5f); put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    put<float>(out,0.f);
}
int selectedFrame(const fs::path& root,bool historicalClock) {
    if(historicalClock)setenv("TOP_HISTORICAL_STRIP_EFF_UV_CLOCK","1",1);
    else unsetenv("TOP_HISTORICAL_STRIP_EFF_UV_CLOCK");
    EffectSystem system;
    assert(system.open(root.string()));
    EffectEvent event; event.kind=EffectEvent::Cast; event.variant=600;
    event.sourceIsPlayer=true;
    system.emit(event); system.step(.25f);
    MapFile map; system.build(map,1.f,0.f);
    int selected=-1;
    for(const auto& batch:system.batches()) {
        if(batch.texture.find("frame0.tga.png")!=std::string::npos)selected=0;
        if(batch.texture.find("frame1.tga.png")!=std::string::npos)selected=1;
    }
    assert(selected>=0);
    return selected;
}
int main() {
    const fs::path root=fs::temp_directory_path()/
        ("sk217r-strip-eff-"+std::to_string(getpid()));
    fs::create_directories(root/"effect");
    fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    {std::ofstream(root/"texture/effect/1.tga.png")<<"mask";}
    {std::ofstream(root/"texture/effect/glow.tga.png")<<"glow";}
    {std::ofstream(root/"texture/effect/frame0.tga.png")<<"frame0";}
    {std::ofstream(root/"texture/effect/frame1.tga.png")<<"frame1";}
    defaultEff(root/"effect/default.eff");
    twoFrameEff(root/"effect/repeat.eff");
    stripPar(root/"effect/strip.par");
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"600\tstrip.par\tStrip\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     std::array<std::string,74> columns;columns.fill("0");columns[0]="600";columns[51]="600";
     for(std::size_t i=0;i<columns.size();++i)f<<(i?"\t":"")<<columns[i];f<<'\n';}

    const int accumulated=selectedFrame(root,false);
    const int controller=selectedFrame(root,true);
    assert(accumulated==1); // authored controller clock is now the default
    assert(controller==1);
    unsetenv("TOP_HISTORICAL_STRIP_EFF_UV_CLOCK");
    fs::remove_all(root);
}
