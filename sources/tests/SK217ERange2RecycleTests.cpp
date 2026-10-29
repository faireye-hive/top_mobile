#include "top/HistoricalRange2Collision.h"
#include <iostream>
#include <stdexcept>
#include <vector>

#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)

int main(){
    using namespace top::historical_range2_collision;
    const V3 base{{0.f,0.f,.1f}},range{{0.f,0.f,0.f}},down{{0.f,0.f,-1.f}};

    const auto beforeBirth=simulate(.04f,1,1.f,.05f,8.f,base,range,down,11);
    CHECK(beforeBirth.slots.size()==1 && !beforeBirth.slots[0].alive);

    const auto firstImpact=simulate(.10f,1,1.f,.05f,8.f,base,range,down,11);
    CHECK(firstImpact.hits.size()==1 && firstImpact.hits[0].generation==1);
    CHECK(firstImpact.hits[0].position[2]<0.f);

    // A hit slot re-enters _CreateRange2 while the shared controller is live.
    const auto recycled=simulate(.20f,1,1.f,.05f,8.f,base,range,down,11);
    CHECK(recycled.hits.size()>=3);
    CHECK(recycled.hits[0].generation==1 && recycled.hits[1].generation==2 &&
          recycled.hits[2].generation==3);

    // The caller can remember delivered hit generations without suppressing
    // later impacts reconstructed by the deterministic replay.
    const std::vector<unsigned> reported{1};
    const auto subsequent=simulate(.20f,1,1.f,.05f,8.f,base,range,down,11,
                                  {},false,reported);
    CHECK(subsequent.hits.size()==2);
    CHECK(subsequent.hits[0].generation==2 && subsequent.hits[1].generation==3);

    const GroundHeight ground=[](int x,int y){
        CHECK(x==0 && y==0);
        return .25f;
    };
    const auto mapped=simulate(.10f,1,1.f,.05f,8.f,
        V3{{0.f,0.f,.3f}},range,down,11,ground,true);
    CHECK(mapped.hits.size()==1);
    CHECK(mapped.hits[0].position[2]==.25f);

    // The controller stops emitting once its life is exceeded, while already
    // born slots still advance and can collide.
    const auto stopped=simulate(.25f,1,.12f,.05f,8.f,base,range,down,11);
    CHECK(stopped.hits.size()==2);
    CHECK(stopped.slots[0].generation==2 && !stopped.slots[0].alive);
    std::cout<<"SK2.17-E RANGE2 recycled slot lifecycle OK\n";
}
