#include "top/HistoricalParticlePool.h"
#include "top/LegacyParticleCurves.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#define REQUIRE(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
using namespace top::historical_particle_pool;
int main() {
    // FIRE/SNOW: slots are born at distinct times and use their own ages.
    for(int type:{1,2}) {
        auto first=at(type,0,3,.05f,2.f,.20f,-1.f,42u);
        auto second=at(type,1,3,.05f,2.f,.20f,-1.f,42u);
        REQUIRE(first.alive && !second.alive);
        second=at(type,1,3,.25f,2.f,.20f,-1.f,42u);
        REQUIRE(second.alive && std::fabs(second.age-.05f)<.00001f);
        auto future=at(type,0,3,4.f,2.f,.20f,-1.f,42u);
        REQUIRE(future.alive && future.generation>0);
        REQUIRE(!at(type,1,3,4.f,2.f,.20f,.3f,42u).alive); // no new births past Stop()
    }
    // BLAST: group birth is simultaneous; individual randomized death is not.
    auto a=at(3,0,5,.05f,2.f,0.f,-1.f,7u);
    auto b=at(3,4,5,.05f,2.f,0.f,-1.f,7u);
    REQUIRE(a.alive && b.alive && a.age==b.age && a.life!=b.life);
    float longest=0.f;
    for(int slot=0;slot<5;++slot)longest=std::max(longest,at(3,slot,5,0.f,2.f,0.f,-1.f,7u).life);
    bool sawDead=false;
    for(int slot=0;slot<5;++slot) {
        const float life=at(3,slot,5,0.f,2.f,0.f,-1.f,7u).life;
        if(life<longest) {
            REQUIRE(!at(3,slot,5,(life+longest)*.5f,2.f,0.f,-1.f,7u).alive);
            sawDead=true;
        }
    }
    REQUIRE(sawDead);
    REQUIRE(at(3,0,5,4.f,2.f,0.f,-1.f,7u).generation>0);
    REQUIRE(!at(3,0,5,4.f,2.f,0.f,1.f,7u).alive);
    // Curve sampling is from individual age, never full controller age.
    std::vector<float> sizes{1.f,2.f,3.f};
    const auto young=at(2,1,3,.26f,2.f,.20f,-1.f,42u);
    REQUIRE(young.alive);
    REQUIRE(top::legacyfx::sample(sizes,{}, {}, young.normalized).size <
            top::legacyfx::sample(sizes,{}, {}, .75f).size);
    // Out-of-range & malformed values do not create live geometry.
    REQUIRE(!at(2,-1,3,1.f,2.f,.2f,-1.f,1u).alive);
    REQUIRE(!at(2,0,3,NAN,2.f,.2f,-1.f,1u).alive);
    REQUIRE(!at(2,0,3,1.f,-1.f,.2f,-1.f,1u).alive);
    REQUIRE(!at(6,0,3,1.f,2.f,.2f,-1.f,1u).alive);
    std::cout<<"SK2.16-A: per-particle stagger, lifecycle, curves, group bursts, stop and guards OK\n";
}
