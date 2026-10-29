#pragma once
// SK2.15-A: CMPPartCtrl >= v7 direct CMPStrip tail (NOT type=6 CMPPartSys).
// Binary layout from DX9 CMPStrip::LoadFromFile / SaveToFile. This helper
// contains no guessed bone positions; two authentic world dummies are required.
#include "top/GfxDevice.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>

namespace top { namespace legacy_direct_strip {
struct Definition {
    int maxLength=0, dummyA=-1, dummyB=-1;
    std::array<float,4> color{{1.f,1.f,1.f,1.f}};
    float life=0.f, step=0.f;
    std::string texture;
    int srcBlend=0,dstBlend=0;
};

template<class T> inline bool read(const std::vector<unsigned char>& bytes,std::size_t& offset,T& out){
    if(offset>bytes.size()||sizeof(T)>bytes.size()-offset)return false;
    std::memcpy(&out,bytes.data()+offset,sizeof(T));offset+=sizeof(T);return true;
}
// Additional CChaModel data (>=v8) follows the strips and is intentionally
// left to its own decoder. Reject truncated declared strips atomically.
inline bool decodeTail(const std::vector<unsigned char>& bytes,std::size_t offset,
                       std::uint32_t version,std::vector<Definition>& result) {
    result.clear();
    if(version<7||offset==bytes.size())return true; // empty older synthetic fixtures
    std::int32_t count=0;
    if(!read(bytes,offset,count)||count<0||count>16)return false;
    std::vector<Definition> decoded;
    for(int i=0;i<count;++i){
        Definition d;
        if(!read(bytes,offset,d.maxLength)||!read(bytes,offset,d.dummyA)||
           !read(bytes,offset,d.dummyB))return false;
        for(float& f:d.color)if(!read(bytes,offset,f))return false;
        if(!read(bytes,offset,d.life)||!read(bytes,offset,d.step))return false;
        if(offset>bytes.size()||32>bytes.size()-offset)return false;
        const char* name=reinterpret_cast<const char*>(bytes.data()+offset);
        std::size_t n=0;while(n<32&&name[n])++n;
        d.texture.assign(name,n);offset+=32;
        if(!read(bytes,offset,d.srcBlend)||!read(bytes,offset,d.dstBlend))return false;
        // DX9 CMPStrip::GetTrack grows by pairs, max 2000 vertices.
        if(d.maxLength<2||d.maxLength>2000||d.life<=0.f||d.life>120.f||
           d.step<=0.f||d.step>10.f||d.dummyA<0||d.dummyB<0||
           d.dummyA>255||d.dummyB>255||d.texture.empty()||d.texture.size()>31||
           d.srcBlend<1||d.srcBlend>15||d.dstBlend<1||d.dstBlend>15||
           !std::isfinite(d.life)||!std::isfinite(d.step)||
           !std::all_of(d.color.begin(),d.color.end(),[](float f){return std::isfinite(f);}))return false;
        // CMPStrip::LoadFromFile removes the optional .tga/.dds extension.
        const auto dot=d.texture.rfind('.');
        if(dot!=std::string::npos&&(d.texture.substr(dot)==".tga"||d.texture.substr(dot)==".dds"))
            d.texture.resize(dot);
        decoded.push_back(std::move(d));
    }
    result=std::move(decoded);return true;
}
struct Pair {
    std::array<float,3> a{},b{}; // OpenGL world X,Y(height),Z
    float time=0.f;
};
struct Trail {
    std::vector<Pair> pairs;
    float lastSample=-1.f;
    std::size_t capacityVertices=0;
    bool stopped=false;
};
inline constexpr bool strictStepEnabled() { return true; }
inline constexpr bool lifecycleEnabled() { return true; }
inline std::array<float,3> endpoint(const std::array<float,16>& m){
    // Source is DX9 row-vector X,Y,Z (Z-up); client GL is X,Z,Y.
    return {{m[12],m[14],m[13]}};
}
inline bool append(Trail& trail,const Definition& d,float at,
                   const std::array<float,16>& ma,const std::array<float,16>& mb,
                   bool loop=false) {
    if(!std::isfinite(at)||at<0.f)return false;
    const bool historicalLifecycle=lifecycleEnabled();
    if(historicalLifecycle&&trail.stopped)return false;
    if(trail.lastSample>=0.f){
        const float elapsed=at-trail.lastSample;
        if(strictStepEnabled() ? elapsed<=d.step : elapsed+1e-6f<d.step)return false;
    }
    if(historicalLifecycle){
        if(trail.capacityVertices==0)trail.capacityVertices=static_cast<std::size_t>(d.maxLength);
        if(trail.pairs.size()*2u>=trail.capacityVertices){
            if(!loop){trail.stopped=true;return false;}
            trail.capacityVertices+=100u;
            if(trail.capacityVertices>2000u){trail.stopped=true;return false;}
        }
    }
    const auto a=endpoint(ma),b=endpoint(mb);
    if(!std::all_of(a.begin(),a.end(),[](float x){return std::isfinite(x);})||
       !std::all_of(b.begin(),b.end(),[](float x){return std::isfinite(x);}))return false;
    trail.pairs.push_back({a,b,at});trail.lastSample=at;
    if(!historicalLifecycle){
        const std::size_t limit=static_cast<std::size_t>(d.maxLength/2);
        if(trail.pairs.size()>limit)trail.pairs.erase(trail.pairs.begin(),
                                                    trail.pairs.begin()+(trail.pairs.size()-limit));
        // Legacy GL mode uses a rolling trail and discards expired pairs.
        while(!trail.pairs.empty()&&at-trail.pairs.front().time>=d.life)
            trail.pairs.erase(trail.pairs.begin());
    }
    return true;
}
inline MeshVertex vertex(const std::array<float,3>& p,const Definition& d,float age,float u,float v){
    MeshVertex out{};out.x=p[0];out.y=p[1];out.z=p[2];
    out.r=d.color[0];out.g=d.color[1];out.b=d.color[2];
    out.a=std::clamp(d.color[3]*(1.f-age/std::max(0.0001f,d.life)),0.f,1.f);
    out.u=u;out.v=v;return out;
}
inline void emit(const Trail& trail,const Definition& d,float now,std::vector<MeshVertex>& verts){
    if(lifecycleEnabled()&&trail.stopped)return;
    if(trail.pairs.size()<2)return;
    // CMPStrip::GetTrack increments U by 1 at each pair; V=1/0 at dummy A/B.
    for(std::size_t j=1;j<trail.pairs.size();++j){
        const auto& a=trail.pairs[j-1];const auto& b=trail.pairs[j];
        const float older=std::max(0.f,now-a.time),newer=std::max(0.f,now-b.time);
        if(!lifecycleEnabled()&&(older>=d.life||newer>=d.life))continue;
        const auto a0=vertex(a.a,d,older,static_cast<float>(j-1),1.f);
        const auto a1=vertex(a.b,d,older,static_cast<float>(j-1),0.f);
        const auto b0=vertex(b.a,d,newer,static_cast<float>(j),1.f);
        const auto b1=vertex(b.b,d,newer,static_cast<float>(j),0.f);
        verts.insert(verts.end(),{a0,a1,b0,a1,b1,b0});
    }
}
}} // top::legacy_direct_strip
