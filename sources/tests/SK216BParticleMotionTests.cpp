// SK2.16-B: DX9 coordinate mapping, per-type velocity and opt-in isolation.
#include "top/HistoricalParticleMotion.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace top::historical_particle_motion;
#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while (0)
static bool near(float a,float b,float eps=0.0001f){return std::fabs(a-b)<eps;}
int main(){
    unsetenv("TOP_HISTORICAL_PARTICLE_MOTION"); CHECK(enabled());
    setenv("TOP_HISTORICAL_PARTICLE_MOTION","1",1);CHECK(enabled());
    setenv("TOP_HISTORICAL_PARTICLE_MOTION","0",1);CHECK(enabled());
    const V3 none{{0,0,0}}, x{{1,0,0}}, up{{0,0,1}};
    auto fire0=at(2,none,V3{{3,4,0}},none,2.f,0.f,4,0);
    auto fire2=at(2,none,V3{{3,4,0}},none,2.f,2.f,4,0);
    CHECK(near(fire2.position[0]-fire0.position[0],2.4f));
    CHECK(near(fire2.position[1]-fire0.position[1],3.2f));
    CHECK(near(fire2.position[2],0.f));
    // DX Randf(range) spawns in [0,range], not [-range/2,+range/2].
    auto fireR=at(2,V3{{2,3,4}},none,none,1.f,0.f,7,3);
    for(unsigned i=0;i<3;++i)CHECK(fireR.position[i]>=0.f && fireR.position[i]<=(i+2));
    auto snow0=at(1,none,up,none,2.f,0.f,12,1);
    auto snow1=at(1,none,up,none,2.f,1.f,12,1);
    CHECK(snow1.position[2]>1.f && snow1.position[2]<=2.f);
    CHECK(near(snow1.position[0],0.f) && near(snow1.position[1],0.f));
    auto blast0=at(3,none,x,V3{{2,0,0}},4.f,0.f,1,0);
    auto blast2=at(3,none,x,V3{{2,0,0}},4.f,2.f,1,0);
    CHECK(near(blast2.position[0]-blast0.position[0],2.f*blast0.velocity[0]+4.f));
    CHECK(near(blast2.position[1],0.f) && near(blast2.position[2],0.f));
    // Different generations sample different spawn ranges/velocities.
    const auto a=at(1,V3{{3,3,3}},up,none,2.f,.1f,12,1,0);
    const auto b=at(1,V3{{3,3,3}},up,none,2.f,.1f,12,1,1);
    CHECK(a.position!=b.position);
    unsetenv("TOP_HISTORICAL_PARTICLE_MOTION");
    std::cout<<"SK2.16-B pure motion: FIRE direction, SNOW velocity, BLAST accel, ranges and generations OK\n";
}
