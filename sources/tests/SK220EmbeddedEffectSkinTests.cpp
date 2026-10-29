#include "top/EmbeddedEffectSkin.h"
#include "top/LegacyExternalEffectMesh.h"
#include "top/HistoricalPetCatalog.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <filesystem>
using namespace top;
namespace {
void put(std::vector<std::uint8_t>& b,std::uint32_t v){for(int i=0;i<4;++i)b.push_back((v>>(i*8))&255);}
void real(std::vector<std::uint8_t>& b,float v){std::uint32_t u;std::memcpy(&u,&v,4);put(b,u);}
void matrix(std::vector<std::uint8_t>& b,float tx,bool full){for(int r=0;r<4;++r)for(int c=0;c<(full?4:3);++c)real(b,r==3&&c==0?tx:r==c?1.f:0.f);}
ModelGeom fixture(unsigned type) {
    ModelGeom g;g.embeddedBoneVersion=0x1005;
    auto& b=g.embeddedBoneData;
    for(unsigned value:{2u,2u,0u,type})put(b,value);
    for(unsigned bone=0;bone<2;++bone){b.resize(b.size()+64);put(b,bone);put(b,bone?0u:0xffffffffu);}
    matrix(b,-10,true);matrix(b,-15,true);
    for(unsigned bone=0;bone<2;++bone) {
        if(type==3) {
            for(float value:bone?std::vector<float>{5,7}:std::vector<float>{10,20}) {
                real(b,value);real(b,0);real(b,0);
            }
            for(int frame=0;frame<2;++frame)for(float v:{0.f,0.f,0.f,1.f})real(b,v);
        } else {
            matrix(b,bone?5:10,type==2);matrix(b,bone?7:20,type==2);
        }
    }
    g.position={1,2,3, 2,2,3, 1,3,3};g.indices={0,1,2};
    g.subsets.push_back({});g.subsets[0].indexCount=3;
    g.bonePalette={1,0};
    for(int i=0;i<3;++i){for(auto v:{0,1,0,0})g.blendIndex.push_back(v);for(float v:{.25f,.75f,0.f,0.f})g.blendWeight.push_back(v);}
    g.local[12]=7;
    return g;
}
}
int main() {
    for(unsigned type:{1u,2u,3u}) {
        auto g=fixture(type);assert(embedded_effect_skin::decode(g));
        legacy_mesh::Primitive first,next,loop;
        assert(legacy_external::subsetPrimitive(g,g.subsets[0],0,first));
        assert(legacy_external::subsetPrimitive(g,g.subsets[0],1.f/30.f,next));
        assert(legacy_external::subsetPrimitive(g,g.subsets[0],2.f/30.f,loop));
        assert(std::fabs(first.points[0].xyz[0]-8.f)<1e-5f);
        // Weighted child: inverse bind(-15) * child(7) * parent(20) = +12;
        // weighted root: -10 + 20 = +10; then geom translation +7.
        assert(std::fabs(next.points[0].xyz[0]-18.5f)<1e-5f);
        assert(std::fabs(loop.points[0].xyz[0]-8.f)<1e-5f);
        auto bad=fixture(type);bad.embeddedBoneData.pop_back();assert(!embedded_effect_skin::decode(bad));
        bad=fixture(type);bad.embeddedBoneData[16+68]=1;bad.embeddedBoneData[16+69]=0;
        bad.embeddedBoneData[16+70]=0;bad.embeddedBoneData[16+71]=0; // parent cycle 0 -> 1 -> 0
        assert(!embedded_effect_skin::decode(bad));
        g.blendIndex[0]=7;assert(!legacy_external::subsetPrimitive(g,g.subsets[0],0,first));
    }
    if(std::filesystem::is_regular_file("Client/model/effect/gelf.lgo")) {
        for(const char* name:{"gelf.lgo","xdelf.lgo"}) {
            Model model;assert(legacy_external::load("Client",name,model));
            const auto& g=model.geoms[0];assert(g.effectBoneCount>0 && g.effectBoneFrames>1);
            legacy_mesh::Primitive a,b;assert(legacy_external::subsetPrimitive(g,g.subsets[0],0,a));
            assert(legacy_external::subsetPrimitive(g,g.subsets[0],.1f,b));
            bool moved=false;for(std::size_t i=0;i<a.points.size();++i)for(int c=0;c<3;++c)
                moved|=std::fabs(a.points[i].xyz[c]-b.points[i].xyz[c])>.0001f;
            assert(moved);
        }
        const auto pets=loadHistoricalPets("Client");assert(pets.size()>=3);
        const auto p=std::find_if(pets.begin(),pets.end(),[](const auto& p){return p.itemId==183;});
        assert(p!=pets.end() && p->effectId==514 && p->dummy==0);
    }
    std::cout<<"Embedded LGO bone MAT43/MAT44/QUAT hierarchy, inverse-bind, palette weights, loop and real fairy mesh animation OK\n";
}
