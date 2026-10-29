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
void emitPar(const fs::path& p,int type,unsigned version=2,int randomDivisor=2) {
    std::ofstream o(p,std::ios::binary);
    put<unsigned>(o,version);name(o,"pool");put<int>(o,1);
    if(version>=3)put<float>(o,5.f); // controller life
    put<int>(o,type);name(o,"sample");put<int>(o,4);
    name(o,"glow.tga");name(o,"");
    for(int i=0;i<3;++i)put<float>(o,1.f); // range
    put<std::uint16_t>(o,2);put<float>(o,1.f);put<float>(o,2.f);
    for(int i=0;i<6;++i)put<float>(o,0.f);
    for(int i=0;i<2;++i)for(int j=0;j<4;++j)put<float>(o,1.f);
    put<std::uint8_t>(o,1);
    for(int v : {5,6,1,1})put<int>(o,v);
    put<float>(o,.5f);put<float>(o,1.f);
    for(int i=0;i<6;++i)put<float>(o,0.f);
    put<float>(o,.2f);
    if(version>3) {put<std::uint8_t>(o,0);name(o,"");} // model range
    if(version>4)for(int i=0;i<3;++i)put<float>(o,0.f); // offset
    if(version>5) {put<float>(o,0.f);put<float>(o,0.f);} // delay/play
    if(version>8)put<std::uint8_t>(o,0); // no authored path
    if(version>9)put<std::uint8_t>(o,0); // shade
    if(version>10)name(o,""); // hit effect
    if(version>12)put<int>(o,randomDivisor);
    if(version>13)put<std::uint8_t>(o,0); // model direction
    if(version>14)put<std::uint8_t>(o,0); // media
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
std::size_t at(const fs::path& root,int skill,float seconds,bool historic) {
    if(historic)setenv("TOP_HISTORICAL_PARTICLE_POOL","1",1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_POOL");
    EffectSystem system;REQUIRE(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=skill;
    event.sourceIsPlayer=true;
    system.emit(event);system.step(seconds);
    MapFile map;system.build(map,1.f,0.f);
    std::size_t result=0;
    for (const auto& batch:system.batches())
        if(batch.texture.find("glow.tga.png")!=std::string::npos)
            result+=batch.verts.size();
    return result;
}
int main() {
    const fs::path tmp=fs::temp_directory_path()/("sk216a-"+std::to_string(getpid()));
    fs::create_directories(tmp/"effect");fs::create_directories(tmp/"scripts/table");
    fs::create_directories(tmp/"texture/effect");
    emitPar(tmp/"effect/snow.par",1);
    emitPar(tmp/"effect/fire.par",2);
    emitPar(tmp/"effect/blast.par",3);
    // v13+ stores _iRoadom explicitly: test divisors 1 and 4.
    emitPar(tmp/"effect/fire13_one.par",2,13,1);
    emitPar(tmp/"effect/fire13_four.par",2,13,4);
    effFile(tmp/"effect/glow.eff");
    {std::ofstream f(tmp/"scripts/table/magicsingleinfo.txt");
        f<<"38\tdefault\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(tmp/"texture/effect/glow.tga.png");f<<"fixture";}
    {std::ofstream f(tmp/"scripts/table/sceneffectinfo.txt");
        f<<"381\tsnow.par\tSnow\n382\tfire.par\tFire\n383\tblast.par\tBlast\n"
            "384\tfire13_one.par\tFire13One\n385\tfire13_four.par\tFire13Four\n";}
    {std::ofstream f(tmp/"scripts/table/skillinfo.txt");
        for (int i=0;i<5;++i) {
            std::vector<std::string> cols(74,"0");
            cols[0]=std::to_string(531+i);cols[51]=std::to_string(381+i);
            for(std::size_t c=0;c<cols.size();++c)f<<(c?"\t":"")<<cols[c];
            f<<'\n';
        }}
    for(int skill:{531,532}) {
        REQUIRE(at(tmp,skill,.05f,false)==24); // old: four quads immediately
        REQUIRE(at(tmp,skill,.05f,true)==6);  // historical: only slot zero born
        // v2 SNOW/FIRE actually default to _iRoadom=4, not 2: a
        // short-lived first slot has already expired at this snapshot.
        REQUIRE(at(tmp,skill,.25f,true)==6); // slot one is now born too
    }
    // Both v13 fixtures are otherwise identical; the serialized divisor
    // controls whether the first particle is still alive at t=.25 s.
    REQUIRE(at(tmp,534,.25f,true)==12); // _iRoadom=1, two living slots
    REQUIRE(at(tmp,535,.25f,true)==6);  // _iRoadom=4, one living slot
    REQUIRE(at(tmp,533,.05f,true)==24);   // BLAST: four slots together
    REQUIRE(at(tmp,533,.05f,false)==24);  // old draw count unchanged
    unsetenv("TOP_HISTORICAL_PARTICLE_POOL");
    fs::remove_all(tmp);
    std::cout<<"SK2.16-A integration: FIRE/SNOW staggered births, BLAST group and legacy compatibility OK\n";
}
