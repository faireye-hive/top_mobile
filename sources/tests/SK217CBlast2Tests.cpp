#include "top/HistoricalBlast2.h"
#include "top/HistoricalParticleExtended.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace top;
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
static bool near(float a,float b,float eps=0.001f){return std::fabs(a-b)<eps;}
int main(){
    const historical_blast2::V3 zero{{0.f,0.f,0.f}};
    const auto initial=historical_blast2::at(0.f,1.f,4,4.f,zero,1.25f);
    CHECK(initial.alive && initial.frame==0 && near(initial.position[1],0.f));
    // DX9 frame zero has a 3x rate and a 3x speed. With 60 Hz sampling,
    // the first lateral transition starts at ~0.083s for life=1, N=4.
    const auto start=historical_blast2::at(.05f,1.f,4,4.f,zero,1.25f);
    CHECK(start.alive && start.frame==0 && near(start.position[1],.6f));
    const auto right=historical_blast2::at(.084f,1.f,4,4.f,zero,1.25f);
    const auto left=historical_blast2::at(.101f,1.f,4,4.f,zero,1.25f);
    const auto center=historical_blast2::at(.118f,1.f,4,4.f,zero,1.25f);
    CHECK(right.alive && right.frame==1 && right.lateralState==1 && near(right.position[0],1.25f));
    CHECK(left.alive && left.frame==1 && left.lateralState==2 && near(left.position[0],-1.25f));
    CHECK(center.alive && center.frame==1 && center.lateralState==3 && near(center.position[0],0.f));
    CHECK(near(right.position[1],left.position[1]) && near(left.position[1],center.position[1]));
    const auto later=historical_blast2::at(.5f,1.f,4,4.f,zero,1.25f);
    CHECK(later.alive && later.frame>=2 && later.position[1]>start.position[1]);
    CHECK(!historical_blast2::at(2.f,1.f,4,4.f,zero,1.25f).alive);
    const auto acceleration=historical_blast2::V3{{0.f,2.f,0.f}};
    const auto moved=historical_blast2::at(.5f,1.f,4,4.f,acceleration,1.25f);
    CHECK(moved.alive && moved.position[1]>later.position[1]);
    // All BLAST2 slots start simultaneously, with a common _vFontDir.
    const historical_particle_extended::V3 range{{3.f,4.f,5.f}},direction{{8.f,9.f,10.f}};
    const auto b0=historical_particle_extended::at(10,0,6,.084f,1.f,.1f,-1.f,
        range,direction,zero,4.f,12,4,1.25f);
    const auto b5=historical_particle_extended::at(10,5,6,.084f,1.f,.1f,-1.f,
        range,direction,zero,4.f,12,4,1.25f);
    CHECK(b0.alive && b5.alive && b0.position==b5.position &&
          near(b0.position[0],1.25f));
    CHECK(!historical_particle_extended::at(10,0,6,2.f,1.f,.1f,-1.f,
        range,direction,zero,4.f,12,4,1.25f).alive);
    CHECK(!historical_blast2::at(1.f,1.f,0,4.f,zero,1.f).alive);
    CHECK(!historical_blast2::at(NAN,1.f,4,4.f,zero,1.f).alive);
    std::cout<<"SK2.17-C BLAST2 startup, lateral states, final stop, integration policy OK\n";
}
