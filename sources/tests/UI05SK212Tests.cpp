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
    for(int i=0;i<3;++i)put<float>(f,2);
    for(int i=0;i<3;++i)put<float>(f,0);
    for(int i=0;i<3;++i)put<float>(f,billboard?0.f:static_cast<float>(i+1));
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
    for(int i=0;i<3;++i)put<float>(f,!billboard && i==1?.2f:0.f);
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
    // Independent known result for three successive D3DX rotations:
    // (1,2,3) -> roll (-2,1,3) -> pitch (-2,-3,1) -> yaw (1,-3,2).
    constexpr float halfPi=1.57079632679f;
    const auto r=top::legacy_mesh::rotate({{1,2,3}},{{halfPi,halfPi,halfPi}});
    assert(near(r[0],1)&&near(r[1],-3)&&near(r[2],2));
    const auto root=fs::temp_directory_path()/("top_sk212_"+std::to_string(getpid()));
    fs::create_directories(root/"scripts/table");fs::create_directories(root/"effect");
    fs::create_directories(root/"texture/effect");
    eff(root/"effect/glow.eff",true);eff(root/"effect/beam.eff",false);
    par(root/"effect/glow.par",true,.2f,.6f);par(root/"effect/beam.par",false);
    {std::ofstream f(root/"texture/effect/glow.tga.png");f<<"texture resolved by CPU fixture";}
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");f<<"38\tdefault\t1\tglow.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"117\tglow.par\tglow\n236\tbeam.par\tbeam\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     for(int id:{501,502,503,504,505,506,507,508}) {std::vector<std::string> c(74,"0");c[0]=std::to_string(id);
        if(id<=502)c[51]=id==501?"117":"236";
        if(id==506){c[51]="117";c[50]="2";}
        else if(id==507){c[51]="117";c[50]="-1";}
        else if(id==508)c[54]="117";
        else if(id==505)c[51]="1038";
        else {c[65]="236";if(id==504)c[63]="236";}
        for(std::size_t i=0;i<c.size();++i)f<<(i?"\t":"")<<c[i];f<<'\n';}}
    top::MapFile map;top::EffectSystem glow;assert(glow.open(root.string()));
    glow.emit({top::EffectEvent::Cast,0,0,0,0,501});glow.step(.1f);
    glow.build(map,1,0);for(const auto& b:glow.batches())assert(b.verts.empty());
    glow.step(.2f);
    const float c=std::sqrt(.5f);
    const float view[16]={1,0,0,0, 0,c,c,0, 0,-c,c,0, 0,0,0,1};
    const auto basis=top::EffectViewBasis::fromView(view);
    glow.build(map,basis);const auto& v=geometry(glow);assert(v.size()==6);
    const auto normal=basis.normal();
    auto edge=[&](int index) {return std::array<float,3>{{v[index].x-v[0].x,v[index].y-v[0].y,v[index].z-v[0].z}};};
    const auto h=edge(1),up=edge(2);
    float h2=0,u2=0,dot=0,plane=0;
    for(int i=0;i<3;++i){h2+=h[i]*h[i];u2+=up[i]*up[i];dot+=h[i]*up[i];plane+=up[i]*normal[i];}
    assert(near(h2,4.f*4.f)&&near(u2,h2)&&near(dot,0)&&near(plane,0));
    assert(near(up[1],4.f*c)&&near(up[2],-4.f*c)); // tilted camera, no world-up flattening
    top::EffectSystem direct;assert(direct.open(root.string()));
    direct.emit({top::EffectEvent::Cast,0,0,0,0,505});direct.step(.1f);direct.build(map,basis);
    const auto& d=geometry(direct);assert(d.size()==6);
    assert(near(d[1].x-d[0].x,2.f));
    assert(near(d[2].y-d[0].y,2.f*c)&&near(d[2].z-d[0].z,-2.f*c));
    glow.build(map,1,0);assert(near(geometry(glow)[2].z-geometry(glow)[0].z,0)); // old CPU API remains available
    glow.step(.31f);glow.build(map,basis); // controller .61, NOT delay+.6
    for(const auto& batch:glow.batches())assert(batch.verts.empty());
    top::EffectSystem beam;assert(beam.open(root.string()));
    beam.emit({top::EffectEvent::Cast,0,0,0,0,502});beam.step(.1f);beam.build(map,basis);
    const auto& b=geometry(beam);assert(b.size()==6);
    assert(near(b[1].x-b[0].x,4.f*std::sin(.2f)));
    assert(near(b[1].y-b[0].y,4.f*std::cos(.2f)));
    // Child translation is scaled/rotated too. The Rect diagonal center is
    // local (1,2,4), then the parent's scale 2 and yaw .2, then world anchor.
    assert(near((b[0].x+b[2].x)*.5f,2*std::cos(.2f)+8.f*std::sin(.2f)));
    assert(near((b[0].y+b[2].y)*.5f,.9f-2*std::sin(.2f)+8.f*std::cos(.2f)));
    assert(near((b[0].z+b[2].z)*.5f,4));
    top::EffectSystem ground;assert(ground.open(root.string()));
    ground.emit({top::EffectEvent::SkillHit,0,0,0,0,503});ground.step(.1f);ground.build(map,basis);
    const auto& g=geometry(ground);assert(g.size()==b.size());
    for(std::size_t i=0;i<g.size();++i)assert(near(b[i].y-g[i].y,.9f)); // no body-height offset on ground
    top::EffectSystem both;assert(both.open(root.string()));
    both.emit({top::EffectEvent::SkillHit,0,0,0,0,504});both.step(.1f);both.build(map,basis);
    // Equal visual IDs in target and ground channels are distinct instances.
    assert(geometry(both).size()==12);
    // Ground/target effects must not follow the actor, even if a player
    // anchor exists elsewhere in the system.
    const float stationaryX=geometry(ground)[0].x;
    ground.setPlayerCastAnchor(20,30,2.f);
    ground.build(map,basis);
    assert(near(geometry(ground)[0].x,stationaryX));
    // SK2.13-A: only player-owned actor effects follow dummy 2, and
    // the anchor is refreshed on subsequent frames instead of frozen at cast.
    top::EffectSystem actorBound;assert(actorBound.open(root.string()));
    top::EffectEvent attached{top::EffectEvent::Cast,0,0,0,0,506};
    attached.sourceIsPlayer=true;
    actorBound.emit(attached);actorBound.step(.25f);
    std::array<float,16> dummy{};
    dummy[0]=dummy[5]=dummy[10]=dummy[15]=1.f;
    dummy[12]=8.f;dummy[13]=4.f;dummy[14]=1.5f;
    actorBound.setPlayerDummyMatrices({{2,dummy}});
    actorBound.build(map,basis);
    const auto& anchored=geometry(actorBound);
    const float firstX=anchored[0].x,firstY=anchored[0].y,firstZ=anchored[0].z;
    dummy[12]=9.f;dummy[14]=1.8f;
    actorBound.setPlayerDummyMatrices({{2,dummy}});
    actorBound.build(map,basis);
    assert(near(geometry(actorBound)[0].x-firstX,1.f));
    assert(near(geometry(actorBound)[0].y-firstY,.3f));
    assert(near(geometry(actorBound)[0].z-firstZ,0));
    // Actor channel with no valid dummy follows root with the switch OFF.
    top::EffectSystem rootBound;assert(rootBound.open(root.string()));
    top::EffectEvent rootCast{top::EffectEvent::Cast,0,0,0,0,507};
    rootCast.sourceIsPlayer=true;
    rootBound.emit(rootCast);rootBound.step(.25f);
    rootBound.setPlayerCastAnchor(2,3,1.2f);
    rootBound.build(map,basis);
    const auto& rootBefore=geometry(rootBound);
    const float rootX=rootBefore[0].x,rootY=rootBefore[0].y,rootZ=rootBefore[0].z;
    rootBound.setPlayerCastAnchor(4,5,1.6f);
    rootBound.build(map,basis);
    assert(near(geometry(rootBound)[0].x-rootX,2.f));
    assert(near(geometry(rootBound)[0].y-rootY,.4f));
    assert(near(geometry(rootBound)[0].z-rootZ,2.f));
    // Item channels must not be mistaken for body-owned effects.
    top::EffectSystem itemCast;assert(itemCast.open(root.string()));
    top::EffectEvent item{top::EffectEvent::Cast,0,0,0,0,508};
    item.sourceIsPlayer=true;
    itemCast.emit(item);itemCast.step(.25f);
    itemCast.setPlayerCastAnchor(20,30,4.f);itemCast.build(map,basis);
    assert(!near(geometry(itemCast)[0].x,geometry(rootBound)[0].x));
    top::EffectSystem monsterCast;assert(monsterCast.open(root.string()));
    monsterCast.emit({top::EffectEvent::Cast,0,0,0,0,506}); // NOT player-owned
    monsterCast.step(.25f);monsterCast.setPlayerDummyMatrices({{2,dummy}});
    monsterCast.build(map,basis);
    assert(!near(geometry(monsterCast)[0].x,geometry(actorBound)[0].x));
    fs::remove_all(root);
    std::cout<<"SK2.12: camera-facing aura, D3DX rotation order and nested parent transforms OK\n";
}
