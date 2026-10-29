#include "top/SceneWorld.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <unistd.h>
namespace fs=std::filesystem;
namespace {
template<class T> void put(std::ofstream& f, T value){f.write(reinterpret_cast<const char*>(&value),sizeof(value));}
void patch32(char* data, std::size_t i,std::uint32_t x){std::memcpy(data+i,&x,4);}
void makeModel(const fs::path& file,bool animated){
    std::ofstream f(file,std::ios::binary);
    const std::uint32_t meshBytes=24+96+16+36+12;
    const std::uint32_t animBytes=animated? (2+16*4+16*4)*4+(4+2*48) : 0;
    const std::uint32_t geomBytes=116+meshBytes+animBytes;
    put<std::uint32_t>(f,0x1003);put<std::uint32_t>(f,1);
    put<std::uint32_t>(f,1);put<std::uint32_t>(f,20);put<std::uint32_t>(f,geomBytes);
    char head[116]{};
    patch32(head,4,0xffffffffu);
    const float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::memcpy(head+12,matrix,sizeof(matrix));
    patch32(head,104,meshBytes);patch32(head,112,animBytes);
    f.write(head,sizeof(head));
    for(auto n:{std::uint32_t(0),std::uint32_t(4),std::uint32_t(3),
                std::uint32_t(3),std::uint32_t(1),std::uint32_t(0)})put(f,n);
    char reserved[96]{};f.write(reserved,sizeof(reserved));
    for(auto n:{std::uint32_t(1),std::uint32_t(0),std::uint32_t(0),std::uint32_t(0)})put(f,n);
    // Flat walkable triangle; identical material grouping to the real parser.
    for(float n:{0.f,0.f,0.f, 1.f,0.f,0.f, 0.f,1.f,0.f})put(f,n);
    for(auto n:{std::uint32_t(0),std::uint32_t(1),std::uint32_t(2)})put(f,n);
    if(animated){
        put<std::uint32_t>(f,0);put<std::uint32_t>(f,100);
        for(int i=0;i<16*4+16*4;++i)put<std::uint32_t>(f,0);
        put<std::uint32_t>(f,2);
        for(int frame=0;frame<2;++frame){
            const float m[12]={1,0,0, 0,1,0, 0,0,1, frame?1.f:0.f,0,0};
            for(float value:m)put(f,value);
        }
    }
}
void makeObjectFile(const fs::path& file){
    std::ofstream f(file,std::ios::binary);
    char head[44]{};
    std::memcpy(head,"HF Object File!",15);
    patch32(head,16,600);patch32(head,24,1);patch32(head,28,1);
    patch32(head,32,8);patch32(head,36,8);
    f.write(head,sizeof(head));
    put<std::uint32_t>(f,52);put<std::uint32_t>(f,3);
    for(int id=1;id<=3;++id){
        char object[20]{};
        const std::uint16_t kind=static_cast<std::uint16_t>(id==3 ? 0x4007 : id);
        std::memcpy(object,&kind,sizeof(kind));
        patch32(object,4,static_cast<std::uint32_t>(id==3 ? 600 : id*200));
        patch32(object,8,static_cast<std::uint32_t>(id==3 ? 700 : 200));
        if(id==3){const std::int16_t height=125,yaw=250;std::memcpy(object+12,&height,2);std::memcpy(object+14,&yaw,2);}
        f.write(object,sizeof(object));
    }
}
int total(const top::SceneMesh& mesh){int n=0;for(const auto& b:mesh.batches)n+=int(b.verts.size());return n;}
std::vector<float> vertices(const top::SceneMesh& m){
    std::vector<float> out;for(const auto& b:m.batches)for(const auto& v:b.verts){out.push_back(v.x);out.push_back(v.y);out.push_back(v.z);}
    std::sort(out.begin(),out.end());return out;
}
}
int main(){
    assert(top::SceneWorld::selfTest());
    const fs::path root=fs::temp_directory_path()/ ("top_05x3_"+std::to_string(::getpid()));
    fs::remove_all(root);fs::create_directories(root/"map");
    fs::create_directories(root/"model/scene");fs::create_directories(root/"scripts/table");
    {std::ofstream f(root/"scripts/table/sceneobjinfo.txt");
     f<<"1\tstatic.lmo\tstatic\t0\n2\tanimated.lmo\tanimated\t0\n";}
    makeModel(root/"model/scene/static.lmo",false);
    makeModel(root/"model/scene/animated.lmo",true);
    makeObjectFile(root/"map/test.obj");
    top::SceneWorld world;top::MapFile map;
    assert(world.open(root.string(),"test"));
    world.setAnimationTime(0.f);
    top::SceneMesh all,staticMesh,animatedMesh;
    world.build(map,4.f,2.f,8.f,all);
    world.build(map,4.f,2.f,8.f,staticMesh,top::SceneWorld::Pass::Static);
    world.build(map,4.f,2.f,8.f,animatedMesh,top::SceneWorld::Pass::Animated);
    assert(all.triangles==2 && all.placed==2 && total(all)==6);
    assert(all.mapEffects.size()==1 && all.mapEffects[0].effectId==7); // normal scene path
    assert(staticMesh.triangles==1 && animatedMesh.triangles==1);
    assert(total(staticMesh)+total(animatedMesh)==total(all));
    auto combined=vertices(staticMesh),animatedVerts=vertices(animatedMesh);
    combined.insert(combined.end(),animatedVerts.begin(),animatedVerts.end());
    std::sort(combined.begin(),combined.end());
    assert(vertices(all)==combined);
    assert(::setenv("TOP_HISTORICAL_MAP_SCENE_EFFECTS","1",1)==0);
    top::SceneMesh withMapEffects;
    world.build(map,4.f,2.f,8.f,withMapEffects,top::SceneWorld::Pass::Static);
    assert(withMapEffects.mapEffects.size()==1);
    assert(withMapEffects.mapEffects[0].effectId==7);
    assert(withMapEffects.mapEffects[0].x==6.f && withMapEffects.mapEffects[0].y==7.f);
    assert(withMapEffects.mapEffects[0].height==1.25f);
    assert(withMapEffects.mapEffects[0].yaw==2.5f);
    top::SceneMesh animatedWithMapEffects;
    world.build(map,4.f,2.f,8.f,animatedWithMapEffects,top::SceneWorld::Pass::Animated);
    assert(animatedWithMapEffects.mapEffects.empty());
    assert(::unsetenv("TOP_HISTORICAL_MAP_SCENE_EFFECTS")==0);
    // All walkable collision triangles remain in the static snapshot.
    assert(staticMesh.walk==all.walk);
    assert(animatedMesh.walk.empty());
    assert(staticMesh.animatedObjects==1);
    const auto staticVerts=vertices(staticMesh);
    world.setAnimationTime(3.f/30.f);
    top::SceneMesh later;
    world.build(map,4.f,2.f,8.f,later,top::SceneWorld::Pass::Animated);
    assert(later.triangles==1 && vertices(later)!=animatedVerts);
    assert(vertices(staticMesh)==staticVerts); // VBO static remains unchanged.
    // A map transition and an empty scene leave both passes empty.
    {std::ofstream f(root/"scripts/table/sceneobjinfo.txt");f<<"1\tstatic.lmo\tstatic\t0\n";}
    assert(world.open(root.string(),"missing"));
    top::SceneMesh empty;world.build(map,4.f,2.f,8.f,empty,top::SceneWorld::Pass::Animated);
    assert(empty.batches.empty());
    fs::remove_all(root);
    std::cout<<"05-X3: static/animated scene equivalence, collision, animation and map reset OK\n";
}
