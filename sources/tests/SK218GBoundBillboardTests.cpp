#include "top/EffectSystem.h"
#include "top/EffectBindingAudit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
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
void eff(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);put<std::uint8_t>(out,0);name(out,"");
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1);name(out,"bound billboard");
    for(int v:{1,5,7})put<std::int32_t>(out,v);
    put<float>(out,1.f);put<std::uint16_t>(out,2);put<float>(out,0.f);put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,1.f); // scale
    for(int i=0;i<6;++i)put<float>(out,0.f); // rotation
    for(int i=0;i<6;++i)put<float>(out,i%3==2?2.f:0.f); // child Z translation
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<std::uint16_t>(out,0);put<float>(out,0.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name(out,"bound.tga");name(out,"RectPlane");
    put<std::uint8_t>(out,1);put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    put<float>(out,3.f);put<float>(out,1.f);put<float>(out,2.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);for(int i=0;i<4;++i)put<float>(out,0.f);put<std::uint8_t>(out,0);
}
void par(const fs::path& path,bool modelDirection,int type=6){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,14);name(out,"controller");put<std::int32_t>(out,1);put<float>(out,4.f);
    put<std::int32_t>(out,type);name(out,"parent");put<std::int32_t>(out,1);name(out,"bound.tga");name(out,"child.eff");
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(int i=0;i<3;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,0);for(int v:{5,7,1,1})put<std::int32_t>(out,v);
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
float center(const fs::path& root,int id,bool enabled,bool bound){
    if(enabled)setenv("TOP_HISTORICAL_BOUND_BILLBOARD_CENTER","1",1);
    else unsetenv("TOP_HISTORICAL_BOUND_BILLBOARD_CENTER");
    setenv("TOP_HISTORICAL_TARGET_DUMMY_BIND","1",1);setenv("TOP_HISTORICAL_ITEM_DUMMY_BIND","1",1);
    top::EffectSystem effect;require(effect.open(root.string()),"open synthetic client");
    std::array<float,16> m{};m[0]=m[5]=m[10]=m[15]=1.f;m[12]=10.f;m[13]=20.f;m[14]=30.f;
    if(bound){effect.setPlayerDummyMatrices({{1,m}});effect.setTargetDummyMatrices({{0,{{1,m}}}});effect.setPlayerStripItemMatrices({{1,m}});}
    effect.setPlayerCastAnchor(10.f,20.f,30.f);
    top::EffectEvent event;event.kind=id==243?top::EffectEvent::SkillHit:top::EffectEvent::Cast;
    event.variant=id;event.sourceIsPlayer=true;event.targetActorIndex=0;
    effect.emit(event);effect.step(.1f);top::MapFile map;effect.build(map,1.f,0.f);
    float low=1e9f,high=-1e9f;
    for(const auto& batch:effect.batches())if(batch.texture.find("bound.tga.png")!=std::string::npos)
        for(const auto& v:batch.verts){low=std::min(low,v.y);high=std::max(high,v.y);}
    require(low<high,"authored billboard geometry exists");return (low+high)*.5f;
}
}
int main(){
    const auto root=fs::temp_directory_path()/("top_sk218g_"+std::to_string(getpid()));
    try{
        fs::create_directories(root/"scripts/table");fs::create_directories(root/"effect");fs::create_directories(root/"texture/effect");
        eff(root/"effect/child.eff");par(root/"effect/bound.par",false);par(root/"effect/directed.par",true);
        par(root/"effect/model.par",false,5);
        std::ofstream(root/"texture/effect/bound.tga.png")<<"fixture";
        {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");f<<"38\tdefault\t1\tchild.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
        {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"336\tbound.par\tbound\tx\t0\t0\t-1\t-1\t0\t1\t0\t-1\n337\tdirected.par\tdir\tx\t0\t0\t-1\t-1\t0\t1\t0\t-1\n338\tmodel.par\tmodel\tx\t0\t0\t-1\t-1\t0\t1\t0\t-1\n";}
        {std::ofstream f(root/"scripts/table/skillinfo.txt");skill(f,242,336,0,0);skill(f,243,0,336,0);skill(f,244,337,0,0);skill(f,245,0,0,336);skill(f,246,338,0,0);}
        for(int id:{242,243,245,246}){
            require(std::fabs(center(root,id,false,true)-31.f)<1e-4f,"default replaces child center with dummy Z=30 plus parent offset=1");
            require(std::fabs(center(root,id,true,true)-31.f)<1e-4f,"DX9 bound billboard replaces child center with dummy plus parent offset");
        }
        require(std::fabs(center(root,244,false,true)-center(root,244,true,true))<1e-4f,"modelDirection excludes full parent binding");
        require(std::fabs(center(root,242,false,false)-center(root,242,true,false))<1e-4f,"missing dummy retains unbound child position");
        const auto audit=fs::path(root.string()+"_audit");std::ostringstream auditLog;
        require(top::effect_binding_audit::run(root,audit,auditLog)==0,"static binding audit runs on synthetic format");
        require(auditLog.str().find("candidates=4 candidate_skills=4")!=std::string::npos,
                "audit matches MODEL/STRIP actor/target/item candidates and excludes modelDirection");
        fs::remove_all(audit);
        unsetenv("TOP_HISTORICAL_BOUND_BILLBOARD_CENTER");unsetenv("TOP_HISTORICAL_TARGET_DUMMY_BIND");unsetenv("TOP_HISTORICAL_ITEM_DUMMY_BIND");
        fs::remove_all(root);std::cout<<"SK2.18-G bound child billboard centers OK\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';fs::remove_all(root);return 1;}
}
