#include "top/EffectSystem.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace top;
#define REQUIRE(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
template<class T> void put(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof value);}
void name(std::ofstream& out,const char* value){char b[32]{};for(unsigned i=0;value[i]&&i<31;++i)b[i]=value[i];out.write(b,32);}
void writePar(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,2);name(out,"blast-group");put<std::int32_t>(out,1);
    put<std::int32_t>(out,3);name(out,"group");put<std::int32_t>(out,1);
    name(out,"glow.tga");name(out,"");
    for(int i=0;i<3;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,2);put<float>(out,1.f);put<float>(out,.5f);
    for(int i=0;i<6;++i)put<float>(out,i==0?1.f:0.f);
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,1);
    for(int v:{5,6,1,1})put<std::int32_t>(out,v);
    put<float>(out,.5f);put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,i==0?1.f:0.f);
    put<float>(out,.2f);
}
void writeEff(const fs::path& path){
    std::ofstream f(path,std::ios::binary);
    put<std::uint32_t>(f,6);put<std::int32_t>(f,0);put<std::uint8_t>(f,0);name(f,"");
    put<std::uint8_t>(f,0);name(f,"");put<std::uint8_t>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0.f);put<std::int32_t>(f,1);name(f,"layer");
    for(int v:{1,5,7})put<std::int32_t>(f,v);
    put<float>(f,5.f);put<std::uint16_t>(f,1);put<float>(f,5.f);
    for(int i=0;i<3;++i)put<float>(f,2.f);for(int i=0;i<6;++i)put<float>(f,0.f);
    for(int i=0;i<4;++i)put<float>(f,1.f);
    put<std::uint16_t>(f,4);put<std::uint16_t>(f,1);put<float>(f,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(f,uv);
    put<std::uint16_t>(f,0);put<float>(f,0.f);name(f,"glow.tga");name(f,"RectPlane");
    put<std::uint8_t>(f,1);put<std::int32_t>(f,1);put<std::int32_t>(f,8);
    for(int i=0;i<3;++i)put<float>(f,1.f);
    put<std::uint16_t>(f,0);put<float>(f,0.f);put<float>(f,0.f);
    put<std::int32_t>(f,0);put<std::uint8_t>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0.f);put<std::uint8_t>(f,0);
}
float centroid(const fs::path& root,bool groupCycle){
    setenv("TOP_HISTORICAL_PARTICLE_POOL","1",1);
    setenv("TOP_HISTORICAL_PARTICLE_MOTION","1",1);
    if(groupCycle)setenv("TOP_HISTORICAL_BLAST_GROUP_CYCLE","1",1);
    else unsetenv("TOP_HISTORICAL_BLAST_GROUP_CYCLE");
    EffectSystem system;REQUIRE(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=532;event.sourceIsPlayer=true;
    system.emit(event);
    for(int i=0;i<7;++i)system.step(.1f);
    MapFile map;system.build(map,1.f,0.f);
    float sum=0.f;std::size_t n=0;
    for(const auto& batch:system.batches())if(batch.texture.find("glow.tga.png")!=std::string::npos)
        for(const auto& vertex:batch.verts){sum+=vertex.x;++n;}
    if(n!=6)throw std::runtime_error("unexpected BLAST vertices="+std::to_string(n)+
        " batches="+std::to_string(system.batches().size()));
    return sum/static_cast<float>(n);
}
int main(){
    const fs::path root=fs::temp_directory_path()/("sk217ak-"+std::to_string(getpid()));
    fs::create_directories(root/"effect");fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    {std::ofstream tex(root/"texture/effect/glow.tga.png");tex<<"fixture";}
    writeEff(root/"effect/default.eff");
    writePar(root/"effect/blast.par");
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
        f<<"38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"382\tblast.par\tBlast\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
        std::vector<std::string> fields(74,"0");fields[0]="532";fields[51]="382";
        for(std::size_t i=0;i<fields.size();++i)f<<(i?"\t":"")<<fields[i];f<<'\n';}
    const float analytic=centroid(root,false),group=centroid(root,true);
    REQUIRE(std::fabs(analytic-group)<1e-5f);
    REQUIRE(std::isfinite(group));
    unsetenv("TOP_HISTORICAL_PARTICLE_POOL");unsetenv("TOP_HISTORICAL_PARTICLE_MOTION");
    unsetenv("TOP_HISTORICAL_BLAST_GROUP_CYCLE");
    fs::remove_all(root);
}
