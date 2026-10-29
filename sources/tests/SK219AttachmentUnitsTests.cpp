#include "top/EffectSystem.h"
#include "top/LegacyEffectGeometry.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
namespace fs=std::filesystem;
namespace {
template<class T> void put(std::ofstream& f,T value){f.write(reinterpret_cast<const char*>(&value),sizeof value);}
void name(std::ofstream& f,const char* s){char b[32]{};for(int i=0;s[i]&&i<31;++i)b[i]=s[i];f.write(b,32);}
bool near(float a,float b){return std::fabs(a-b)<.0001f;}
void eff(const fs::path& path,bool billboard) {
    std::ofstream f(path,std::ios::binary);
    put<unsigned>(f,6);put<int>(f,0);put<unsigned char>(f,0);name(f,"");
    put<unsigned char>(f,0);name(f,"");put<unsigned char>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0);put<int>(f,1);
    name(f,"layer");for(int v:{1,5,7})put<int>(f,v);
    put<float>(f,5);put<unsigned short>(f,1);put<float>(f,5);
    for(int i=0;i<3;++i)put<float>(f,1);
    for(int i=0;i<3;++i)put<float>(f,0);
    for(int i=0;i<3;++i)put<float>(f,0.f);
    for(int i=0;i<4;++i)put<float>(f,1);
    put<unsigned short>(f,4);put<unsigned short>(f,1);put<float>(f,1);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(f,uv);
    put<unsigned short>(f,0);put<float>(f,0);name(f,"glow.tga");name(f,billboard?"RectPlane":"Rect");
    put<unsigned char>(f,billboard);put<int>(f,1);put<int>(f,8);
    put<float>(f,3);put<float>(f,1);put<float>(f,2);
    put<unsigned short>(f,0);put<float>(f,0);put<float>(f,0);put<int>(f,0);
    put<unsigned char>(f,0);for(int i=0;i<4;++i)put<float>(f,0);put<unsigned char>(f,0);
}
void par(const fs::path& path,bool billboard,float delay=0.f,float end=0.f) {
    std::ofstream f(path,std::ios::binary);
    put<unsigned>(f,7);name(f,"controller");put<int>(f,1);put<float>(f,5);
    put<int>(f,6);name(f,"strip");put<int>(f,1);name(f,"mask");name(f,billboard?"glow.eff":"beam.eff");
    for(int i=0;i<3;++i)put<float>(f,0);put<unsigned short>(f,1);put<float>(f,2);
    for(int i=0;i<3;++i)put<float>(f,0.f);
    for(int i=0;i<4;++i)put<float>(f,1);
    put<unsigned char>(f,billboard);for(int i:{5,7,1,1})put<int>(f,i);
    put<float>(f,5);for(int i=0;i<8;++i)put<float>(f,0);
    put<unsigned char>(f,0);name(f,"");for(int i=0;i<3;++i)put<float>(f,0);
    put<float>(f,delay);put<float>(f,end);
}
const std::vector<top::MeshVertex>& geometry(const top::EffectSystem& fx) {
    for(const auto& b:fx.batches())if(!b.verts.empty())return b.verts;
    std::abort();
}
}
int main() {
    static_assert(top::historical_effect_units::fromCentimetres(100.f)==1.f);
    const auto root=fs::temp_directory_path()/("top_sk219_"+std::to_string(getpid()));
    fs::create_directories(root/"scripts/table");fs::create_directories(root/"effect");
    fs::create_directories(root/"texture/effect");
    eff(root/"effect/glow.eff",true);eff(root/"effect/beam.eff",false);
    par(root/"effect/beam.par",false);
    {std::ofstream f(root/"texture/effect/glow.tga.png");f<<"fixture";}
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tdefault\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n";
     f<<"18\tprojectile\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"236\tbeam.par\tmodel\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     for(int id=501;id<=505;++id) {
        std::vector<std::string> c(74,"0");c[0]=std::to_string(id);c[62]="1";
        if(id==501){c[50]="1";c[51]="236";}
        if(id==502){c[53]="7";c[54]="236,0";}
        if(id==503)c[63]="236";
        if(id==504)c[65]="236";
        if(id==505){c[59]="1018";c[57]="1";c[58]="7";}
        for(std::size_t i=0;i<c.size();++i)f<<(i?"\t":"")<<c[i];f<<'\n';
     }}
    std::array<float,16> bone{{0,2,0,0, -3,0,0,0, 0,0,4,0, 10,20,30,1}};
    std::array<float,16> item{{1,0,0,0, 0,1,0,0, 0,0,1,0, 7,8,9,1}};
    top::MapFile map;
    // Same full affine chain for player, enemy source, item and target.
    for(int owner:{-1,0})for(int id:{501,502,503}) {
        top::EffectSystem fx;assert(fx.open(root.string()));
        fx.setPlayerDummyMatrices({{1,bone}});fx.setPlayerStripItemMatrices({{7,bone}});
        fx.setTargetDummyMatrices({{0,{{1,bone}}}});fx.setActorItemDummyMatrices({{0,{{7,bone}}}});
        top::EffectEvent event;event.variant=id;event.sourceActorIndex=owner;
        event.kind=id==503?top::EffectEvent::SkillHit:top::EffectEvent::Cast;event.targetActorIndex=0;
        fx.emit(event);fx.step(.1f);fx.build(map,1,0);
        const auto& v=geometry(fx);assert(v.size()==6);
        // Rect first point (-.5,0,0), parent scale 2, then full affine bone.
        assert(near(v[0].x,10)&&near(v[0].y,30)&&near(v[0].z,18));
        assert(near(v[1].y-v[0].y,8)); // 1 world unit * parent 2 * bone Z scale 4
        bone[12]+=3;fx.setPlayerDummyMatrices({{1,bone}});fx.setPlayerStripItemMatrices({{7,bone}});
        fx.setTargetDummyMatrices({{0,{{1,bone}}}});fx.setActorItemDummyMatrices({{0,{{7,bone}}}});
        fx.build(map,1,0);assert(near(geometry(fx)[0].x,13));bone[12]-=3;
    }
    // Projectile origin precedence and exact world heights; target dummy is
    // retained even though the table declares no target-effect resource.
    for(int owner:{-1,0})for(int path=0;path<3;++path) {
        top::EffectSystem fx;assert(fx.open(root.string()));fx.setPlayerCastAnchor(1,2,3);
        fx.setTargetActorRoots({{0,{{1,3,2}}}});
        if(path==0)fx.setPlayerDummyMatrices({{1,bone}});
        if(path<2){fx.setPlayerStripItemMatrices({{7,item}});fx.setActorItemDummyMatrices({{0,{{7,item}}}});}
        std::unordered_map<int,std::unordered_map<int,std::array<float,16>>> targets{{1,{{1,bone}}}};
        if(path==0)targets[0]={{1,bone}};
        fx.setTargetDummyMatrices(std::move(targets));
        top::EffectEvent e{top::EffectEvent::Flight,0,0,50,50,505,1};
        e.sourceActorIndex=owner;e.targetActorIndex=1;e.toHeight=0;
        fx.emit(e);fx.build(map,1,0);const auto& v=geometry(fx);
        const float x=(v[0].x+v[4].x)*.5f,y=(v[0].y+v[4].y)*.5f,z=(v[0].z+v[4].z)*.5f;
        assert(near(x,path==0?10.f:path==1?7.f:1.f));
        assert(near(y,path==0?30.f:path==1?9.f:3.f));
        assert(near(z,path==0?20.f:path==1?8.f:2.f));
        // After a short flight the target must be near bone (10,30,20),
        // not the artificial event destination (50,0,50).
        if(path==1){fx.step(.1f);fx.build(map,1,0);const auto& w=geometry(fx);
            assert((w[0].y+w[4].y)*.5f>9.f);}
    }
    // Missing actor dummy follows the real root at feet, never +0.9.
    for(int id:{501,502}) {
        top::EffectSystem fallback;assert(fallback.open(root.string()));fallback.setPlayerCastAnchor(1,2,3);
        top::EffectEvent e{top::EffectEvent::Cast,0,0,0,0,id};e.sourceIsPlayer=true;
        fallback.emit(e);fallback.step(.1f);fallback.build(map,1,0);
        assert(near(geometry(fallback)[0].y,3));
    }
    // Persistent equipment effects use the same resource/matrix pipeline,
    // survive their short reusable clips, and replace/remove only their slot.
    top::EffectSystem persistent;assert(persistent.open(root.string()));
    persistent.setPlayerDummyMatrices({{1,bone}});
    assert(persistent.setPlayerAttachedEffect(16,236,1));
    for(int frame=0;frame<600;++frame) {
        assert(persistent.setPlayerAttachedEffect(16,236,1));
        persistent.step(1.f/60.f);persistent.build(map,1,0);
    }
    assert(persistent.activeCount()==1 && geometry(persistent).size()==6);
    assert(near(geometry(persistent)[0].x,10));
    assert(persistent.setPlayerAttachedEffect(16,0,-1));
    assert(persistent.activeCount()==0);
    assert(!persistent.setPlayerAttachedEffect(16,999999,1));
    assert(persistent.activeCount()==0); // unavailable asset has no invented pet
    fs::remove_all(root);
    std::cout<<"SK2.19: world units, full owner/item/target matrices and projectile dummy precedence OK\n";
}
