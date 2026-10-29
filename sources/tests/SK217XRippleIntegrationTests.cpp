#include "top/EffectSystem.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs=std::filesystem;
using namespace top;

template<class T>void put(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof(value));}
void name(std::ofstream& out,const char* value){char bytes[32]{};for(unsigned i=0;value[i]&&i<31;++i)bytes[i]=value[i];out.write(bytes,sizeof(bytes));}

void defaultEff(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1);name(out,"base");
    for(int value:{1,5,7})put<std::int32_t>(out,value);
    put<float>(out,5.f);put<std::uint16_t>(out,1);put<float>(out,5.f);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,4);put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(out,uv);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name(out,"ripple.tga");
    name(out,"RectPlane");put<std::uint8_t>(out,1);
    put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    put<float>(out,1.f);put<float>(out,1.f);put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);
    put<std::int32_t>(out,0);put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);
}
void ripplePar(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,2);name(out,"ripple");put<std::int32_t>(out,1);
    put<std::int32_t>(out,4);name(out,"ripple-system");put<std::int32_t>(out,2);
    name(out,"ripple.tga");name(out,"");
    for(int i=0;i<3;++i)put<float>(out,i==0?2.f:0.f);
    put<std::uint16_t>(out,4);
    for(float size:{1.f,2.f,3.f,4.f})put<float>(out,size);
    for(int i=0;i<12;++i)put<float>(out,0.f);
    for(int i=0;i<16;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,1);
    for(int value:{5,6,1,1})put<std::int32_t>(out,value);
    put<float>(out,1.f);put<float>(out,0.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    put<float>(out,.1f);
}

std::size_t drawCount(const fs::path& root,bool historical){
    if(historical)setenv("TOP_HISTORICAL_RIPPLE_POOL","1",1);
    else unsetenv("TOP_HISTORICAL_RIPPLE_POOL");
    EffectSystem system;assert(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=532;event.sourceIsPlayer=true;
    system.emit(event);system.step(.01f);
    MapFile map;system.build(map,1.f,0.f);
    std::size_t vertices=0;
    for(const auto& batch:system.batches())
        if(batch.texture.find("ripple.tga.png")!=std::string::npos)vertices+=batch.verts.size();
    return vertices;
}

int main(){
    const fs::path root=fs::temp_directory_path()/("sk217x-ripple-"+std::to_string(getpid()));
    fs::create_directories(root/"effect");fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    {std::ofstream(root/"texture/effect/ripple.tga.png")<<"fixture";}
    defaultEff(root/"effect/default.eff");ripplePar(root/"effect/ripple.par");
    {std::ofstream t(root/"scripts/table/magicsingleinfo.txt");t<<"38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream t(root/"scripts/table/sceneffectinfo.txt");t<<"382\tripple.par\tRipple\n";}
    {std::ofstream t(root/"scripts/table/skillinfo.txt");std::array<std::string,74> c;c.fill("0");c[0]="532";c[51]="382";for(std::size_t i=0;i<c.size();++i)t<<(i?"\t":"")<<c[i];t<<'\n';}
    assert(drawCount(root,false)==6u); // normal execution preserves RIPPLE emission cadence
    assert(drawCount(root,true)==6u);  // DX9 primed emitter creates only the first dead slot
    unsetenv("TOP_HISTORICAL_RIPPLE_POOL");
    fs::remove_all(root);
}
