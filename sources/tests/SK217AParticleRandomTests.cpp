#include "top/HistoricalParticleRandom.h"
#include "top/HistoricalParticlePool.h"
#include "top/HistoricalParticleMotion.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace top;
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while (0)
int main() {
    using historical_particle_random::decodedDivisor;
    // MPParticleSys.h::Randf(float) uses integer modulo and a 0.001 grid.
    {
        unsigned values[]={3u,499u};
        unsigned index=0;
        auto next=[&]() { return values[index++]; };
        CHECK(std::fabs(historical_particle_random::dx9Randf(4.5f,next)-3.499f)<1e-6f);
        CHECK(index==2u);
    }
    {
        unsigned values[]={999u};
        unsigned index=0;
        auto next=[&]() { return values[index++]; };
        CHECK(std::fabs(historical_particle_random::dx9Randf(.4f,next)-.199f)<1e-6f);
        CHECK(index==1u);
    }
    {
        unsigned values[]={3u,199u};
        unsigned index=0;
        auto next=[&]() { return values[index++]; };
        CHECK(std::fabs(historical_particle_random::dx9Randf(2.f,next)-1.199f)<1e-6f);
        CHECK(index==2u);
    }
    for(unsigned lane=0;lane<32;++lane) {
        const float sampled=historical_particle_random::deterministicRandf(4.5f,72u,lane);
        CHECK(sampled>=0.f && sampled<4.5f);
        CHECK(std::fabs(sampled*1000.f-std::round(sampled*1000.f))<.01f);
    }
    CHECK(historical_particle_random::deterministicRandModulo(2u,17u)<2u);
    // DX9 MPParticleSys::Load selects 4 for v2-v12 snow/fire/dummy.
    for(unsigned version:{2u,6u,12u}) {
        for(int type:{1,2,16}) CHECK(decodedDivisor(version,type)==4);
        for(int type:{3,5,6}) CHECK(decodedDivisor(version,type)==2);
    }
    // v13+ reads the actual serialized field, not the old type default.
    for(unsigned version:{13u,14u,15u}) {
        CHECK(decodedDivisor(version,1,1)==1);
        CHECK(decodedDivisor(version,2,3)==3);
        CHECK(decodedDivisor(version,3,4)==4);
        CHECK(decodedDivisor(version,1,0)==2); // corrupt or unsafe divisor
        CHECK(decodedDivisor(version,1,-5)==2);
    }
    using historical_particle_pool::individualLife;
    for(int slot=0; slot<64; ++slot) {
        float oldLife=individualLife(2.f,42,slot,0,2);
        float newLife=individualLife(2.f,42,slot,0,4);
        CHECK(oldLife>=1.f && oldLife<=2.f);
        CHECK(newLife>=.5f && newLife<=2.f);
        CHECK(newLife<=oldLife);
        CHECK(individualLife(2.f,42,slot,0,1)==2.f);
    }
    // Different divisors must change the actual pool's live-age curve.
    bool different=false;
    for(int slot=0; slot<20; ++slot) {
        auto a=historical_particle_pool::at(1,slot,20,2.f,2.f,.01f,-1.f,42,2);
        auto b=historical_particle_pool::at(1,slot,20,2.f,2.f,.01f,-1.f,42,4);
        if(a.alive!=b.alive || std::fabs(a.age-b.age)>.0001f) different=true;
    }
    CHECK(different);
    // With DX9's Randf implementation, Randf(0.5, 2.0) evaluates the
    // fractional range through rand()%1, so the result stays below 1.0.
    // This odd interval is source behavior, not a desired uniform distribution.
    for(int slot=0;slot<64;++slot) {
        const float sampled=historical_particle_random::snowFireLife(2.f,42u,slot,0u,4);
        CHECK(sampled>=.5f && sampled<1.f);
        CHECK(std::fabs(sampled*1000.f-std::round(sampled*1000.f))<.01f);
    }
    const char* oldRandfEnv=std::getenv("TOP_HISTORICAL_PARTICLE_RANDF");
    const bool hadRandfEnv=oldRandfEnv!=nullptr;
    const std::string savedRandfEnv=oldRandfEnv?oldRandfEnv:"";
    setenv("TOP_HISTORICAL_PARTICLE_RANDF","0",1);
    CHECK(historical_particle_random::randfMappingEnabled());
    setenv("TOP_HISTORICAL_PARTICLE_RANDF","1",1);
    CHECK(historical_particle_random::randfMappingEnabled());
    historical_particle_pool::SystemTimeline randfTimeline;
    randfTimeline.advance(2,1,.02f,2.f,.01f,-1.f,42u,4,4,true);
    const auto randfSample=randfTimeline.snapshot(4).slots[0];
    CHECK(randfSample.alive && randfSample.life>=.5f && randfSample.life<1.f);
    if(hadRandfEnv)setenv("TOP_HISTORICAL_PARTICLE_RANDF",savedRandfEnv.c_str(),1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_RANDF");
    using historical_particle_motion::V3;
    const V3 none{{0.f,0.f,0.f}}, up{{0.f,0.f,1.f}};
    for(int slot=0;slot<50;++slot) {
        const auto v2=historical_particle_motion::at(1,none,up,none,2.f,0.f,72,slot,0,2).velocity[2];
        const auto v4=historical_particle_motion::at(1,none,up,none,2.f,0.f,72,slot,0,4).velocity[2];
        CHECK(v2>=1.f && v2<=2.f && v4>=.5f && v4<=v2);
        CHECK(historical_particle_motion::at(1,none,up,none,2.f,0.f,72,slot,0,1).velocity[2]==2.f);
    }
    setenv("TOP_HISTORICAL_PARTICLE_RANDF","1",1);
    const V3 blastRange{{4.5f,2.f,1.f}}, blastDirection{{1.f,1.f,1.f}};
    for(int slot=0;slot<64;++slot) {
        const float life=historical_particle_pool::individualLife(2.f,42u,slot,0u,4);
        CHECK(life>=.5f && life<1.f);
        const auto blast=historical_particle_motion::at(
            3,blastRange,blastDirection,none,4.5f,0.f,72u,slot,0,4);
        for(unsigned axis=0;axis<3;++axis) {
            CHECK(blast.position[axis]>=0.f && blast.position[axis]<blastRange[axis]);
            CHECK(std::fabs(blast.position[axis]*1000.f-
                            std::round(blast.position[axis]*1000.f))<.01f);
            CHECK(std::fabs(blast.velocity[axis])<4.5f);
            CHECK(std::fabs(std::fabs(blast.velocity[axis])*1000.f-
                            std::round(std::fabs(blast.velocity[axis])*1000.f))<.01f);
        }
    }
    if(hadRandfEnv)setenv("TOP_HISTORICAL_PARTICLE_RANDF",savedRandfEnv.c_str(),1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_RANDF");
    std::cout<<"SK2.17-A/BE/BG: divisor and DX9 Randf sampling verified\n";
}
