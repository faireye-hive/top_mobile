#include "top/LegacyEffFormat.h"
#include "top/LegacyEffectGeometry.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace top;
namespace {
bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
legacy_eff::Subeffect make(const std::initializer_list<float>& durations,
                           const std::initializer_list<float>& sizes,
                           const std::initializer_list<float>& alphas) {
    legacy_eff::Subeffect part;
    auto itSize=sizes.begin(),itAlpha=alphas.begin();
    for(float sec:durations){
        legacy_eff::Key key;
        key.time=sec; key.size={*itSize,*itSize,*itSize}; key.color[3]=*itAlpha;
        ++itSize; ++itAlpha; part.keys.push_back(key);
    }
    return part;
}
}
int main(){
    assert(legacy_eff::historicalTimeMode());
    // Timings copied from the three original 01010003.eff impact subeffects.
    auto impact=make({.06f,.21f,.31f},{1.f,2.f,7.f},{0.f,1.f,0.f});
    assert(near(legacy_eff::duration(impact),.58f));
    assert(near(legacy_eff::sample(impact,.03f).size[0],1.5f));
    assert(near(legacy_eff::sample(impact,.03f).color[3],.5f));
    assert(near(legacy_eff::sample(impact,.06f).size[0],2.f));
    assert(near(legacy_eff::sample(impact,.165f).size[0],4.5f));
    assert(near(legacy_eff::sample(impact,.27f).size[0],7.f));
    // DX9's LAST frame interpolates back to FIRST until the model stops.
    assert(near(legacy_eff::sample(impact,.425f).size[0],4.f));
    assert(near(legacy_eff::sample(impact,.425f).color[3],0.f));
    assert(legacy_eff::framePhase(impact,.579f).from==2);
    assert(legacy_eff::framePhase(impact,.58f).finished);
    assert(legacy_eff::framePhase(impact,12345.f).finished);
    // Two staggered layers are shorter than the first.
    const auto layer2=make({.21f,.21f,.06f},{1.f,2.f,7.f},{0.f,1.f,0.f});
    const auto layer3=make({.13f,.11f,.03f},{1.f,4.f,7.f},{0.f,1.f,0.f});
    assert(near(legacy_eff::duration(layer2),.48f));
    assert(near(legacy_eff::duration(layer3),.27f));
    assert(legacy_eff::framePhase(layer2,.47f).from==2);
    assert(legacy_eff::framePhase(layer3,.27f).finished);
    // The source Illusion Slash 04000009.eff stores durations .9/1.8/.9;
    // its size is 2 throughout. Do NOT invent a growth curve for flight.
    auto flight=make({.9f,1.8f,.9f},{2.f,2.f,2.f},{1.f,1.f,1.f});
    assert(near(legacy_eff::duration(flight),3.6f));
    for(float at:{0.f,.45f,1.35f,2.7f,3.1f})
        assert(near(legacy_eff::sample(flight,at).size[0],2.f));
    assert(legacy_eff::framePhase(flight,2.0f).from==1);
    // Dynamic mesh parameters must share the SAME segment and interpolation.
    flight.animatedParams.resize(3);
    flight.animatedParams[0].height=1.f;
    flight.animatedParams[1].height=5.f;
    flight.animatedParams[2].height=9.f;
    assert(near(legacy_mesh::paramsAt(flight,.45f).height,3.f));
    assert(near(legacy_mesh::paramsAt(flight,1.8f).height,7.f));
    assert(near(legacy_mesh::paramsAt(flight,3.15f).height,5.f));
    // Zero-duration keys are skipped; positive one-key clips stop at their duration.
    auto zero=make({0.f,.5f,.5f},{1.f,2.f,4.f},{0.f,1.f,0.f});
    assert(legacy_eff::framePhase(zero,0.f).from==1);
    assert(near(legacy_eff::sample(zero,.25f).size[0],3.f));
    assert(!legacy_eff::framePhase(make({0.f},{3.f},{1.f}),10.f).finished);
    assert(!legacy_eff::framePhase(make({.5f},{3.f},{1.f}),.49f).finished);
    assert(legacy_eff::framePhase(make({.5f},{3.f},{1.f}),.5f).finished);
    assert(!legacy_eff::framePhase(impact,std::numeric_limits<float>::quiet_NaN()).finished);
    // Old sampler remains available for before/after comparison.
    auto previous=legacy_eff::absoluteFramePhase(impact,.165f);
    assert(previous.from==0 && previous.to==1);
    assert(!near(previous.fraction,.5f));
    std::cout << "SK2.7: DX9 duration keyframes, last-to-first, impact layers, model params, zero keys OK\n";
}
