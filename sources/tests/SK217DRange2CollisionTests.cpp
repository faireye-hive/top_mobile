#include "top/HistoricalRange2Collision.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)
static bool close(float a,float b){return std::fabs(a-b)<.002f;}

int main(){
    using namespace top::historical_range2_collision;
    const V3 base{{10.f,20.f,1.f}},range{{0.f,0.f,0.f}},down{{0.f,0.f,-4.f}};
    const GroundHeight flat=[](int x,int y){
        CHECK(x==10 && y==20);
        return .25f;
    };
    auto before=at(.04f,0,1,1.f,.05f,4.f,base,range,down,7,flat,true);
    CHECK(!before.alive && !before.hit); // birth happens on the authored interval
    auto airborne=at(.10f,0,1,1.f,.05f,4.f,base,range,down,7,flat,true);
    CHECK(airborne.alive && !airborne.hit && airborne.position[2]<1.f &&
          airborne.position[2]>.25f);
    auto terrainHit=at(.50f,0,1,1.f,.05f,4.f,base,range,down,7,flat,true);
    CHECK(terrainHit.hit && !terrainHit.alive);
    CHECK(close(terrainHit.position[0],10.f) && close(terrainHit.position[1],20.f));
    CHECK(close(terrainHit.position[2],.25f) && terrainHit.hitAge>=0.f);

    // Without a map, DX9 kills below Z=0 but sends the un-clamped position.
    const V3 origin{{0.f,0.f,.1f}};
    auto noMap=at(.20f,0,1,1.f,.05f,4.f,origin,range,down,8,{},false);
    CHECK(noMap.hit && noMap.position[2]<0.f);
    // The DX9 controller stops before emitting a slot whose scheduled birth
    // is after _fLife; already-born slots remain simulated independently.
    auto stopped=at(.20f,1,2,.15f,.1f,1.f,base,range,down,7,flat,true);
    CHECK(!stopped.alive && !stopped.hit);
    // Direction is normalized just as D3DXVec3Normalize does.
    const V3 up{{0.f,0.f,5.f}};
    auto upper=at(.30f,0,1,1.f,.05f,10.f,V3{{0.f,0.f,49.f}},range,up,3,{},false);
    CHECK(upper.hit && upper.position[2]>50.f);

    // SK2.17-AI: persistent replay uses each actual delta, preserves the
    // shared dead-slot timer, and does not recycle a hit slot in the same call.
    SystemTimeline timeline;
    const V3 lowBase{{0.f,0.f,.02f}},zeroRange{{0.f,0.f,0.f}};
    auto state=timeline.advance(.02f,1,1.f,.05f,1.f,lowBase,zeroRange,down,31u);
    CHECK(state.hits.empty()&&!state.slots[0].alive);
    state=timeline.advance(.02f,1,1.f,.05f,1.f,lowBase,zeroRange,down,31u);
    CHECK(!state.slots[0].alive);
    state=timeline.advance(.02f,1,1.f,.05f,1.f,lowBase,zeroRange,down,31u);
    CHECK(state.slots[0].alive&&state.slots[0].generation==1u);
    state=timeline.advance(.03f,1,1.f,.05f,1.f,lowBase,zeroRange,down,31u);
    CHECK(state.hits.size()==1u&&!state.slots[0].alive);
    CHECK(close(state.hits[0].position[2],-.01f)); // no-map DX9 branch keeps overshoot
    state=timeline.advance(.05f,1,1.f,.05f,1.f,lowBase,zeroRange,down,31u);
    CHECK(state.hits.empty()&&state.slots[0].alive&&state.slots[0].generation==2u);
    std::cout<<"SK2.17-D RANGE2 terrain/bounds collision rules OK\n";
}
