#pragma once
// SK2.8: DX9 MagicList[3] = Part_fshade, from Client/src/EffectObj.cpp.
#include <algorithm>
#include <cmath>
namespace top { namespace historic_magic {
constexpr int kFshade=3;
constexpr float kInterval=.6f;
constexpr float kSpacing=.5f;
struct ShadeState {float mainScale=1.f; int copies=0;};
// DX9 Part_fshade discards timer overshoot and increments at most once per
// call. Keep this state per flight when the historical switch
// is enabled; the age-derived helper below remains the compatibility path.
struct ShadeTimeline {
    float accumulated=0.f;
    int copies=1;
    void advance(float dt) {
        if(!std::isfinite(dt)||dt<0.f)return;
        accumulated+=dt;
        if(accumulated>kInterval){
            accumulated=0.f;
            ++copies;
        }
    }
    ShadeState state(float age) const {
        return {age<=0.f?1.f:3.f,copies};
    }
};
inline ShadeState shadeAt(int mode,float age) {
    if(mode!=kFshade || !std::isfinite(age) || age<0.f)return {};
    ShadeState state;
    // DX9 renders first, then sets an ABSOLUTE Scaling(3,3,3).
    state.mainScale=age<=0.f?1.f:3.f;
    state.copies=std::min(16,1+static_cast<int>(std::floor(
        std::max(0.f,std::min(age,9.f)-1e-6f)/kInterval)));
    return state;
}
inline float shadeAlpha(int copy,int count) {
    if(count<=0||copy<0||copy>=count)return 0.f;
    return std::max(0.f,1.f-static_cast<float>(copy+1)/count);
}
template<typename Callback> void forEachModelCopy(
    int mode,float age,float baseX,float baseZ,
    float fromX,float fromZ,float toX,float toZ,Callback&& emit) {
    if(mode!=kFshade){emit(baseX,baseZ,1.f,1.f);return;}
    if(!std::isfinite(age)||age<0.f)return;
    const auto state=shadeAt(mode,age);
    emit(baseX,baseZ,state.mainScale,1.f);
    const float dx=toX-fromX,dz=toZ-fromZ;
    const float distance=std::hypot(dx,dz);
    if(distance<=1e-5f)return;
    for(int i=0;i<state.copies;++i){
        const float alpha=shadeAlpha(i,state.copies);
        if(alpha<=0.f)continue; // zero-alpha DX9 model is visually empty
        const float offset=kSpacing*static_cast<float>(i+1)/distance;
        emit(baseX-dx*offset,baseZ-dz*offset,state.mainScale,alpha);
    }
}
template<typename Callback> void forEachModelCopy(
    int mode,float age,ShadeState state,float baseX,float baseZ,
    float fromX,float fromZ,float toX,float toZ,Callback&& emit) {
    if(mode!=kFshade){emit(baseX,baseZ,1.f,1.f);return;}
    if(!std::isfinite(age)||age<0.f)return;
    emit(baseX,baseZ,state.mainScale,1.f);
    const float dx=toX-fromX,dz=toZ-fromZ;
    const float distance=std::hypot(dx,dz);
    if(distance<=1e-5f)return;
    for(int i=0;i<state.copies;++i){
        const float alpha=shadeAlpha(i,state.copies);
        if(alpha<=0.f)continue;
        const float offset=kSpacing*static_cast<float>(i+1)/distance;
        emit(baseX-dx*offset,baseZ-dz*offset,state.mainScale,alpha);
    }
}
}}
