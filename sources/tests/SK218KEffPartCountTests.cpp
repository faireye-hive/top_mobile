#include "top/EffectSystem.h"
#include "top/LegacyEffectInspector.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>void put(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof value);}
void name(std::ofstream& out,const char* value){char b[32]{};for(int i=0;value[i]&&i<31;++i)b[i]=value[i];out.write(b,32);}
void eff(const fs::path& path,int parts,bool billboard=false,const char* mesh="RectPlane",bool skipFirst=false){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);put<std::uint8_t>(out,0);name(out,"");
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,parts);
    for(int part=0;part<parts;++part){name(out,"child");
    for(int v:{1,5,7})put<std::int32_t>(out,v);
    put<float>(out,1.f);put<std::uint16_t>(out,2);put<float>(out,0.f);put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,i%3==1?2.f:1.f); // child scale (1,2,1)
    for(int i=0;i<6;++i)put<float>(out,i%3==2?.4f:0.f); // child Z rotation
    for(int i=0;i<6;++i)put<float>(out,float(i%3+1)); // child XYZ=(1,2,3)
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<std::uint16_t>(out,0);put<float>(out,0.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name(out,"bound.tga");name(out,skipFirst&&part==0?"MissingMesh":mesh);
    put<std::uint8_t>(out,billboard?1:0);put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    put<float>(out,3.f);put<float>(out,1.f);put<float>(out,2.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);for(int i=0;i<4;++i)put<float>(out,0.f);put<std::uint8_t>(out,0);
    }
}
// Synthetic version-0x1003 lwGeomObjInfo, same format fixture as UI05LTests.
void lgo(const fs::path& file, bool invalidIndex=false){
    std::ofstream f(file,std::ios::binary);
    put<std::uint32_t>(f,0x1003);
    char geom[116]{};
    // Parent sentinel and 4x4 local identity at offset 12.
    std::uint32_t parent=0xffffffffu;
    std::memcpy(geom+4,&parent,4);
    float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    identity[12]=2.f; // local displacement must be preserved
    std::memcpy(geom+12,identity,sizeof(identity));
    // mesh size: 6 u32 +96 header +16 subset +9 float pos +6 float uv +3 u32 indices
    const std::uint32_t meshBytes=24+96+16+36+24+12;
    std::memcpy(geom+104,&meshBytes,4);
    f.write(geom,sizeof(geom));
    for(auto val:{std::uint32_t(0x100),std::uint32_t(4),std::uint32_t(3),
                  std::uint32_t(3),std::uint32_t(1),std::uint32_t(0)})put(f,val);
    char reserved[96]{};f.write(reserved,sizeof(reserved));
    for(auto val:{std::uint32_t(1),std::uint32_t(0),std::uint32_t(0),std::uint32_t(0)})put(f,val);
    for(float val:{0.f,0.f,0.f, 1.f,0.f,0.f, 0.f,1.f,0.f})put(f,val);
    for(float val:{0.f,0.f, 1.f,0.f, 0.f,1.f})put(f,val);
    for(auto val:{std::uint32_t(0),std::uint32_t(1),invalidIndex?std::uint32_t(7):std::uint32_t(2)})put(f,val);
}
void par(const fs::path& path,bool modelDirection,int type=6,bool billboard=false){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,14);name(out,"controller");put<std::int32_t>(out,1);put<float>(out,4.f);
    put<std::int32_t>(out,type);name(out,"parent");put<std::int32_t>(out,1);name(out,"bound.tga");name(out,"child.eff");
    for(int i=0;i<3;++i)put<float>(out,float(4+i*2)); // nonzero range
    put<std::uint16_t>(out,1);put<float>(out,2.f); // parent scale
    for(int i=0;i<3;++i)put<float>(out,float(i+1)*.3f); // ignored in bound branch
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,billboard?1:0);for(int v:{5,7,1,1})put<std::int32_t>(out,v);
    put<float>(out,4.f);put<float>(out,0.f);for(int i=0;i<7;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);name(out,"");
    put<float>(out,0.f);put<float>(out,0.f);put<float>(out,1.f); // parent offset Z=1
    put<float>(out,0.f);put<float>(out,2.f);put<std::uint8_t>(out,0);put<std::uint8_t>(out,0);
    name(out,"");put<std::int32_t>(out,2);put<std::uint8_t>(out,modelDirection?1:0);
}
void skill(std::ofstream& out,int id,int actor,int target,int item){
    std::vector<std::string> c(74,"0");c[0]=std::to_string(id);c[50]="1";c[51]=std::to_string(actor);
    c[53]="1";c[54]=std::to_string(item)+",0";c[62]="1";c[63]=std::to_string(target);
    for(std::size_t i=0;i<c.size();++i)out<<(i?"\t":"")<<c[i];out<<'\n';
}
std::size_t draw(const fs::path& root,bool all,bool flight=false){
    if(all)setenv("TOP_HISTORICAL_EFF_PART_COUNT","1",1);
    else unsetenv("TOP_HISTORICAL_EFF_PART_COUNT");
    top::EffectSystem effects;require(effects.open(root.string()),"open count fixture");
    effects.emit({flight?top::EffectEvent::Flight:top::EffectEvent::Cast,0,0,0,0,81,1.f});
    effects.step(.1f);top::MapFile map;effects.build(map,1.f,0.f);
    std::size_t result=0;
    for(const auto& batch:effects.batches())if(batch.texture.find("bound.tga.png")!=std::string::npos)
        result+=batch.verts.size();
    return result;
}
void table(const fs::path& root,const char* models="child.eff",int count=1){
    std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
    f<<"38\tbase\t1\tchild.eff\t0\t0\t0\t0\t0\t0\t0\n";
    f<<"10\tflight\t"<<count<<'\t'<<models<<"\t0\t0\t0\t0\t0\t0\t0\n";
}
}
int main(){
    const auto root=fs::temp_directory_path()/("top_sk218k_"+std::to_string(getpid()));
    try {
        fs::create_directories(root/"scripts/table");fs::create_directories(root/"effect");
        fs::create_directories(root/"texture/effect");fs::create_directories(root/"model/effect");
        std::ofstream(root/"texture/effect/bound.tga.png")<<"fixture";
        lgo(root/"model/effect/mesh.lgo");par(root/"effect/a.par",false);
        {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"336\ta.par\tcount\n";}
        {std::ofstream f(root/"scripts/table/skillinfo.txt");
         std::vector<std::string> c(74,"0");c[0]="81";c[51]="336";c[59]="1010";
         for(std::size_t i=0;i<c.size();++i)f<<(i?"\t":"")<<c[i];f<<'\n';}
        table(root);
        for(bool billboard:{false,true}) {
            eff(root/"effect/child.eff",12,billboard);
            require(draw(root,false)==12*6,"default nested EFF renders all twelve authored children");
            require(draw(root,true)==12*6,"historical nested EFF renders all twelve authored children");
        }
        eff(root/"effect/child.eff",13,false,"RectPlane",true);
        require(draw(root,false)==12*6,"default excludes unsupported child and retains all twelve valid children");
        require(draw(root,true)==12*6,"unsupported child stays excluded while later valid children survive");
        eff(root/"effect/child.eff",12,false,"mesh.lgo");
        require(draw(root,false)==12*3&&draw(root,true)==12*3,"nested external meshes follow authored count");
        for(bool billboard:{false,true}) {
            eff(root/"effect/child.eff",17,billboard);
            require(draw(root,false,true)==17*6,"standalone default preserves seventeen children");
            require(draw(root,true,true)==17*6,"standalone native and billboard children follow authored count");
        }
        eff(root/"effect/child.eff",9,false,"mesh.lgo");
        require(draw(root,false,true)==9*3&&draw(root,true,true)==9*3,"standalone external count follows file");
        eff(root/"effect/child.eff",9,true);eff(root/"effect/other.eff",9,true);
        table(root,"child.eff,other.eff",2);
        require(draw(root,false,true)==18*6&&draw(root,true,true)==18*6,"simultaneous EFF files are not cumulatively truncated");
        table(root);eff(root/"effect/child.eff",64);
        require(draw(root,true)==64*6,"existing parser bound of 64 supported children is reachable");
        eff(root/"effect/child.eff",65);
        std::ifstream input(root/"effect/child.eff",std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
        top::legacy_eff::Document decoded;
        require(!top::legacy_eff::decode(bytes,decoded),"parser bound not weakened");
        unsetenv("TOP_HISTORICAL_EFF_PART_COUNT");fs::remove_all(root);
        std::cout<<"SK2.18-K authored EFF count, environment independence and safety regressions OK\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';fs::remove_all(root);return 1;}
}
