#include "top/ClassicCamera.h"
#include "top/EquipmentStats.h"
#include "top/SelectionRing.h"
#include "top/PerformanceMeter.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
int main(){
    using namespace top;
    assert(classic_camera::distance(2.f)==4.5f);
    assert(classic_camera::distance(11.f)==11.f);
    assert(classic_camera::lift(.4f)==classic_camera::kMinLift);
    std::vector<std::string> item(94,"0,0"); item[37]="50,50";
    auto one=equipment_stats::fromColumns(item);
    assert(one.aspdCoefficient==50);
    auto pair=one;pair+=one;
    assert(pair.aspdCoefficient==100);
    auto delay=[&](int gear){return .85f/(1.f+gear/1000.f);};
    assert(delay(pair.aspdCoefficient)<delay(one.aspdCoefficient));
    assert(delay(one.aspdCoefficient)<delay(0));
    auto ring=selection_ring::build(8.f,9.f,2.f,1.2f);
    assert(ring.size()==48U*6U);
    for (const auto& v:ring) {
        assert(std::fabs(v.y-2.065f)<.001f);
        assert(std::hypot(v.x-8.f,v.z-9.f)>.9f);
        assert(v.b>v.r && v.a>0.f);
    }
    performance::Sample s;performance::Meter m;
    const auto start=performance::Meter::Clock::now();
    assert(!m.frame(start,s));
    assert(m.frame(start+std::chrono::milliseconds(1100),s));
    assert(s.fps>0 && s.ms>0 && s.ramMiB>=0);
    std::puts("05-X: camera, ASPD de duas armas, circulo e FPS/RAM OK");
}
