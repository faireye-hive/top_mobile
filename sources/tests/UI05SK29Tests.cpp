#include "top/EffectSystem.h"
#include "top/HistoricalMagicPath.h"
#include "top/LegacyEffectGeometry.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
namespace fs=std::filesystem;
namespace {
bool near(float a,float b,float e=1e-4f){return std::fabs(a-b)<e;}
template <typename T> void write(std::ofstream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}
void name32(std::ofstream& out, const char* name) {
    char str[32]{};
    for (std::size_t i = 0; name[i] && i < sizeof(str) - 1; ++i) str[i] = name[i];
    out.write(str, 32);
}
void eff(const fs::path& file, const char* textureName) {
    std::ofstream out(file, std::ios::binary);
    write<std::uint32_t>(out, 1);
    out.write(textureName, std::char_traits<char>::length(textureName) + 1);
    out.write("data", 4);
}
// Minimal structured version-6 I_Effect using a native RectPlane.
// Its texture must override the alpha-only controller texture.
void nativeEff(const fs::path& file, const char* texture = "nested_tex.tga", int destBlend = 7) {
    std::ofstream out(file, std::ios::binary);
    write<std::uint32_t>(out, 6); write<std::int32_t>(out, 0);
    write<std::uint8_t>(out, 0); name32(out, "");
    write<std::uint8_t>(out, 0); name32(out, "");
    write<std::uint8_t>(out, 0);
    for (int i=0; i<4; ++i) write<float>(out, 0.f);
    write<std::int32_t>(out, 1); name32(out, "nested particle mesh");
    for (int i : {1,5,destBlend}) write<std::int32_t>(out, i);
    write<float>(out, 1.f); write<std::uint16_t>(out, 2);
    write<float>(out, 0.f); write<float>(out, 1.f);
    for (int i=0; i<6; ++i) write<float>(out, 1.f);
    for (int i=0; i<6; ++i) write<float>(out, 0.f);
    for (int i=0; i<6; ++i) write<float>(out, 0.f);
    for (int i=0; i<8; ++i) write<float>(out, 1.f);
    write<std::uint16_t>(out, 0); write<std::uint16_t>(out, 0);
    write<float>(out, 0.f);
    write<std::uint16_t>(out, 0); write<float>(out, 0.f);
    name32(out, texture); name32(out, "RectPlane");
    write<std::uint8_t>(out, 0); write<std::int32_t>(out, 1);
    write<std::int32_t>(out, 8); write<float>(out, 3.f);
    write<float>(out, 1.f); write<float>(out, 2.f);
    write<std::uint16_t>(out, 0); write<float>(out, 0.f);
    write<float>(out, 0.f); write<std::int32_t>(out, 0);
    write<std::uint8_t>(out, 0);
    for (int i=0; i<4; ++i) write<float>(out, 0.f);
    write<std::uint8_t>(out, 0);
}

float center(const top::EffectSystem& fx,int axis) {
    float sum=0; int count=0;
    for(const auto& batch:fx.batches()) for(const auto& v:batch.verts) {
        sum+=axis==0?v.x:axis==1?v.y:v.z; ++count;
    }
    assert(count>0);return sum/count;
}
}
int main() {
    using namespace top::historic_magic;
    for(int mode:{0,1,6}) {
        Path p{mode,{0,2,0},{0,2,20},4,1};
        assert(near(p.duration(),5));
        assert(near(p.position(2).z,8));
        assert(near(p.position(100).z,20));
    }
    Path trace{2,{0,1,0},{6,1,8},10,0};
    assert(near(trace.duration(),.875f)); // DX square tolerance, not sphere
    assert(near(trace.position(trace.duration()).z,7));
    Path shade{3,{0,1,0},{6,1,8},10,0};
    assert(near(shade.duration(),.9375f));
    Path arc{4,{0,0,0},{0,0,10},10,0};
    assert(near(arc.atDistance(5).y,5*(std::sqrt(2.f)-1)));
    assert(near(arc.position(0).y,0));
    assert(near(arc.atDistance(10).y,0));
    assert(arc.duration()<1 && arc.duration()>.8f);
    Path light{5,{1,3,2},{9,8,7},10,1.25f};
    assert(near(light.duration(),1.25f));
    assert(near(light.position(.8f).x,1));assert(near(light.pitch(),0));
    assert(near(modelTime(2.25f,1.f,1),.25f));
    assert(near(modelTime(2.25f,1.f,5),2.25f));
    assert(!knownMode(-1)&&!knownMode(7));
    // Orientation of a +Y-forward local mesh: all quadrants, plus elevation.
    top::legacy_mesh::Primitive mesh;
    mesh.points={{{{0,1,0}},{{0,0}}},{{{0,1,0}},{{0,0}}},{{{0,1,0}},{{0,0}}}};
    mesh.triangles={{{0,1,2}}};
    for(auto target:{Vec3{3,4,0},Vec3{-3,4,0},Vec3{0,-4,-3},Vec3{0,4,3}}) {
        Path p{1,{},target,1,0}; std::vector<top::MeshVertex> v;
        top::legacy_eff::Key key;
        top::legacy_mesh::emit(mesh,key,{},0,0,0,v,1,p.yaw(),p.pitch());
        const auto d=p.direction();
        assert(near(v[0].x,d.x)&&near(v[0].y,d.y)&&near(v[0].z,d.z));
    }
    // Integration: use binary .eff fixture with NO GL context.
    const auto root=fs::temp_directory_path()/("top_sk29_"+std::to_string(getpid()));
    fs::create_directories(root/"effect"); fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    nativeEff(root/"effect/model.eff","model.tga");
    {std::ofstream f(root/"texture/effect/model.tga.png");f<<"fixture";}
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tdefault\t1\tmodel.eff\t4\t0\t0\t-1\t2\t0\t0\n";
     for(int mode=0;mode<=6;++mode) f<<10+mode<<"\tmode\t1\tmodel.eff\t4\t0\t0\t-1\t"<<mode<<"\t0\t0\n";
    }
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     for(int mode=0;mode<=6;++mode) {
        std::vector<std::string> fields(74,"0");fields[0]=std::to_string(100+mode);
        fields[59]=std::to_string(1010+mode);
        for(std::size_t i=0;i<fields.size();++i) f<<(i?"\t":"")<<fields[i];
        f<<'\n';
     }
    }
    top::MapFile map;
    for(int mode=0;mode<=6;++mode) {
        top::EffectSystem fx;assert(fx.open(root.string()));
        top::EffectEvent event{top::EffectEvent::Flight,0,0,0,20,0,5};
        // Table maps every skill to its own MagicList mode.
        event.variant=100+mode; event.fromHeight=2;event.toHeight=2;
        fx.emit(event);fx.step(.25f);fx.build(map,1,0);
        Path expected{mode,{0,2,0},{0,2,20},4,1};
        assert(near(center(fx,2),expected.position(.25f).z));
        if(mode!=4) assert(near(center(fx,1),2));
        if(mode==5) {fx.step(.76f);assert(fx.activeCount()==0);}
        else {
            fx.step(2.f);fx.build(map,1,0); // .eff repeats beyond first cycle
            assert(fx.activeCount()==1);assert(!fx.batches().empty());
            fx.step(4);assert(fx.activeCount()==0);
        }
    }
    // Table velocity is used only when Play supplied no skill duration.
    top::EffectSystem tableSpeed;assert(tableSpeed.open(root.string()));
    tableSpeed.emit({top::EffectEvent::Flight,0,0,0,20,101,0});
    tableSpeed.step(3.5f);tableSpeed.build(map,1,0);
    assert(tableSpeed.activeCount()==1&&near(center(tableSpeed,2),14));
    // Large versus small simulation steps have identical sampled positions.
    top::EffectSystem big,small;assert(big.open(root.string())&&small.open(root.string()));
    top::EffectEvent event{top::EffectEvent::Flight,0,0,0,20,101,5};
    big.emit(event);small.emit(event);big.step(2.25f);
    for(int i=0;i<9;++i)small.step(.25f);
    big.build(map,1,0);small.build(map,1,0);
    assert(near(center(big,2),center(small,2)));
    fs::remove_all(root);
    std::cout<<"SK2.9 motion equations, orientation, duration override, loops and step invariance OK\n";
}
