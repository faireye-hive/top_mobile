#include "top/HistoricalRange2Collision.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)

int main() {
    using namespace top::historical_range2_collision;
    const V3 base{{10.f,20.f,1.f}},range{{2.5f,3.25f,0.f}},direction{{0.f,0.f,0.f}};
    unsetenv("TOP_HISTORICAL_PARTICLE_RANDF");
    const auto defaultPath=at(.10f,0,1,1.f,.05f,0.f,base,range,direction,91u);
    const float previousX=top::historical_particle_random::deterministicRandf(range[0],91u^(0x85ebca6bu*1u),0u);
    const float previousY=top::historical_particle_random::deterministicRandf(range[1],91u^(0x85ebca6bu*1u),4u);
    CHECK(defaultPath.alive);
    CHECK(std::fabs(defaultPath.position[0]-(base[0]+previousX))<1e-6f);
    CHECK(std::fabs(defaultPath.position[1]-(base[1]+previousY))<1e-6f);

    setenv("TOP_HISTORICAL_PARTICLE_RANDF","1",1);
    const auto mapped=at(.10f,0,1,1.f,.05f,0.f,base,range,direction,91u);
    const auto repeated=at(.10f,0,1,1.f,.05f,0.f,base,range,direction,91u);
    CHECK(mapped.alive);
    CHECK(std::fabs(mapped.position[0]-repeated.position[0])<1e-6f);
    CHECK(std::fabs(mapped.position[1]-repeated.position[1])<1e-6f);
    const float x=mapped.position[0]-base[0];
    const float y=mapped.position[1]-base[1];
    CHECK(x>=0.f&&x<range[0]&&y>=0.f&&y<range[1]);
    CHECK(std::fabs(x*1000.f-std::round(x*1000.f))<.01f);
    CHECK(std::fabs(y*1000.f-std::round(y*1000.f))<.01f);
    unsetenv("TOP_HISTORICAL_PARTICLE_RANDF");
    std::cout<<"SK2.17-BI RANGE2 Randf mapping is opt-in, deterministic and quantized\n";
}
