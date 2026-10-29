#include "top/HistoricalParticleExtended.h"
#include "top/HistoricalParticleMotion.h"
#include "top/HistoricalParticleJitter.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace top;
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
static bool close(float x,float y){return std::fabs(x-y)<.001f;}
int main(){
    using namespace historical_particle_extended;
    const V3 rng{{2.f,4.f,6.f}},zero{{0.f,0.f,0.f}},dir{{0.f,1.f,0.f}};
    // ROUND: four equally spaced positions rotate at authored radians/sec.
    auto r0=at(9,0,4,0.f,2.f,0.f,-1.f,rng,zero,zero,pi/2,1);
    auto r1=at(9,1,4,0.f,2.f,0.f,-1.f,rng,zero,zero,pi/2,1);
    auto r2=at(9,0,4,1.f,2.f,0.f,-1.f,rng,zero,zero,pi/2,1);
    CHECK(r0.alive && r1.alive && r2.alive);
    CHECK(close(r0.position[0],1.f) && close(r0.position[1],6.f));
    CHECK(close(r1.position[0],-3.f) && close(r1.position[1],2.f));
    CHECK(close(r2.position[0],r1.position[0]) && close(r2.position[1],r1.position[1]));
    // BLAST3: DX9 rotated (1,1,-1) + direction, moves at authored speed.
    auto b=at(11,0,4,.5f,1.f,0.f,-1.f,rng,zero,zero,2.f,5);
    CHECK(b.alive && close(b.position[0],1.f) && close(b.position[1],1.f)
          && close(b.position[2],-1.f));
    CHECK(!at(11,0,4,1.5f,1.f,0.f,-1.f,rng,zero,zero,2.f,5).alive);
    // RANGE keeps seeded spawn positions invariant across frames.
    auto a=at(14,2,4,.1f,2.f,0.f,-1.f,rng,zero,zero,0.f,42);
    auto c=at(14,2,4,1.3f,2.f,0.f,-1.f,rng,zero,zero,0.f,42);
    CHECK(a.alive && c.alive && a.position==c.position);
    for(int i=0;i<3;++i)CHECK(a.position[i]>=0.f && a.position[i]<rng[i]);
    // WIND: staggered births, local seed, angular position + authored drift.
    CHECK(!at(7,0,2,.05f,1.f,.1f,-1.f,rng,dir,zero,2.f,8).alive);
    auto w=at(7,0,2,.2f,1.f,.1f,-1.f,rng,dir,zero,2.f,8);
    CHECK(w.alive && close(w.age,.1f));
    auto wEnd=at(7,1,2,.5f,1.f,.1f,.2f,rng,dir,zero,2.f,8);
    CHECK(!wEnd.alive); // stopAt excludes next emission at .2 s
    // ARRAW loops its timeline at one-life boundaries and centers range.
    auto arrow=at(8,0,1,1.3f,1.f,0.f,-1.f,rng,zero,zero,1.f,1);
    CHECK(arrow.alive && close(arrow.age,.3f) && close(arrow.position[2],3.f));
    // RANGE2: fixed Z and normalized velocity, not random Z, no 0.12 factor.
    const V3 upward{{0.f,0.f,3.f}};
    auto range2=at(15,0,2,.6f,1.f,.1f,-1.f,rng,upward,zero,2.f,42);
    CHECK(range2.alive && close(range2.position[2],6.f+2.f*.5f));
    // Acceleration noise in DX9 FIRE/SNOW is per frame, not ballistic.
    const top::historical_particle_jitter::V3 accel{{2.f,1.f,-3.f}};
    const auto j1=top::historical_particle_jitter::offset(accel,.5f,123,1);
    const auto j2=top::historical_particle_jitter::offset(accel,.5f,123,1);
    CHECK(j1==j2);
    CHECK(std::fabs(j1[0])<1.f && close(j1[1]*2.f,j1[0]));
    CHECK(std::fabs(j1[0])>1e-7f);
    // FIRE uses authored _fVecl after normalizing _vDir (DX9 _FrameMoveFire).
    const top::historical_particle_motion::V3 rangeZero{{0.f,0.f,0.f}};
    const top::historical_particle_motion::V3 fireDir{{3.f,0.f,0.f}};
    auto fire=top::historical_particle_motion::at(2,rangeZero,fireDir,
        rangeZero,4.f,.5f,11,0);
    CHECK(close(fire.position[0],2.f) && close(fire.velocity[0],4.f));
    const char* previousRandf=std::getenv("TOP_HISTORICAL_PARTICLE_RANDF");
    const std::string previousRandfValue=previousRandf?previousRandf:"";
    unsetenv("TOP_HISTORICAL_PARTICLE_RANDF");
    CHECK(setenv("TOP_HISTORICAL_PARTICLE_RANDF","1",1)==0);
    const auto windRandf=at(7,0,2,.2f,1.f,.1f,-1.f,rng,zero,zero,0.f,8u);
    const std::uint32_t windSeed=8u ^ 0x85ebca6bu;
    for(int axis=0;axis<3;++axis){
        const float expectedWind=historical_particle_random::deterministicRandf(
            rng[axis],windSeed,static_cast<unsigned>(axis)*4u);
        CHECK(windRandf.alive && close(windRandf.position[axis],expectedWind));
    }
    if(previousRandf)setenv("TOP_HISTORICAL_PARTICLE_RANDF",previousRandfValue.c_str(),1);
    else unsetenv("TOP_HISTORICAL_PARTICLE_RANDF");
    std::cout<<"SK2.17-B extended WIND Randf mapping and other particle rules OK\n";
}
