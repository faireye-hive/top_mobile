#pragma once
// lwAnimDataBone::Load(FILE*, version), embedded in lwAnimDataInfo/LGO.
// Decode only for effect models. No fabricated skeleton or external LAB.
#include "top/ModelFile.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>

namespace top::embedded_effect_skin {
using Matrix=std::array<float,16>;
inline Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix out{};
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)
        out[i*4+j]+=a[i*4+k]*b[k*4+j];
    return out;
}
struct Reader {
    const std::vector<std::uint8_t>& bytes;std::size_t at=0;bool ok=true;
    std::uint32_t u32(){if(at>bytes.size() || bytes.size()-at<4){ok=false;return 0;}
        const auto* p=bytes.data()+at;at+=4;
        return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
    float f32(){auto bits=u32();float out;std::memcpy(&out,&bits,4);if(!std::isfinite(out))ok=false;return out;}
    void skip(std::size_t count){if(at>bytes.size() || count>bytes.size()-at){ok=false;return;}at+=count;}
};
inline Matrix quaternion(float x,float y,float z,float w,const std::array<float,3>& p) {
    return {{1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),0,
        2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),0,
        2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y),0,p[0],p[1],p[2],1}};
}
inline bool decode(ModelGeom& geom) {
    if(geom.embeddedBoneData.empty())return false;
    Reader r{geom.embeddedBoneData};
    if(geom.embeddedBoneVersion==0)r.u32();
    const auto count=r.u32(),frames=r.u32(),dummies=r.u32(),type=r.u32();
    const std::uint64_t matrices=std::uint64_t(count)*frames;
    if(!r.ok || count==0 || count>256 || frames==0 || frames>20000 || dummies>256 ||
       type<1 || type>3 || matrices>524288)return false;
    // Validate the minimum serialized size before allocating expanded poses.
    const std::uint64_t headers=std::uint64_t(count)*136+std::uint64_t(dummies)*72;
    const std::uint64_t keys=type==1?matrices*48:type==2?matrices*64:matrices*16+std::uint64_t(count)*12;
    if(headers>r.bytes.size()-r.at || keys>r.bytes.size()-r.at-headers)return false;
    std::vector<std::uint32_t> parent(count);
    for(auto& p:parent){r.skip(68);p=r.u32();if(p!=0xffffffffu && p>=count)return false;}
    std::vector<Matrix> inverse(count);
    for(auto& m:inverse)for(float& v:m)v=r.f32();
    r.skip(std::size_t(dummies)*72);
    std::vector<Matrix> local(static_cast<std::size_t>(matrices));
    for(std::size_t bone=0;bone<count;++bone) {
        if(type==1 || type==2) {
            for(std::size_t frame=0;frame<frames;++frame) {
                auto& m=local[frame*count+bone];
                if(type==2){for(float& v:m)v=r.f32();}
                else {for(int row=0;row<4;++row)for(int col=0;col<3;++col)m[row*4+col]=r.f32();m[15]=1;}
            }
        } else {
            const std::size_t positions=geom.embeddedBoneVersion>=0x1003 || parent[bone]==0xffffffffu?frames:1;
            std::vector<std::array<float,3>> pos(frames);
            for(std::size_t frame=0;frame<positions;++frame)for(float& v:pos[frame])v=r.f32();
            for(std::size_t frame=positions;frame<frames;++frame)pos[frame]=pos[0];
            for(std::size_t frame=0;frame<frames;++frame) {
                const float x=r.f32(),y=r.f32(),z=r.f32(),w=r.f32();
                local[frame*count+bone]=quaternion(x,y,z,w,pos[frame]);
            }
        }
        if(!r.ok)return false;
    }
    if(!r.ok || r.at!=r.bytes.size())return false;
    std::vector<float> skin(static_cast<std::size_t>(matrices)*16);
    for(std::size_t frame=0;frame<frames;++frame) {
        std::vector<Matrix> absolute(count);std::vector<unsigned char> state(count);
        std::function<bool(std::size_t)> visit=[&](std::size_t bone) {
            if(state[bone]==2)return true;if(state[bone]==1)return false;
            state[bone]=1;const auto p=parent[bone];
            if(p!=0xffffffffu && !visit(p))return false;
            absolute[bone]=p==0xffffffffu?local[frame*count+bone]:multiply(local[frame*count+bone],absolute[p]);
            state[bone]=2;return true;
        };
        for(std::size_t bone=0;bone<count;++bone) {
            if(!visit(bone))return false;
            const auto m=multiply(inverse[bone],absolute[bone]);
            if(!std::all_of(m.begin(),m.end(),[](float v){return std::isfinite(v);}))return false;
            std::copy(m.begin(),m.end(),skin.begin()+static_cast<std::ptrdiff_t>((frame*count+bone)*16));
        }
    }
    geom.effectBoneCount=static_cast<int>(count);geom.effectBoneFrames=static_cast<int>(frames);
    geom.effectBoneMatrices=std::move(skin);return true;
}
inline bool skinned(const ModelGeom& geom) {
    const auto n=geom.position.size()/3;
    return geom.blendIndex.size()==n*4 && geom.blendWeight.size()==n*4 &&
        !geom.bonePalette.empty() && geom.effectBoneCount>0 && geom.effectBoneFrames>0 &&
        geom.effectBoneMatrices.size()==std::size_t(geom.effectBoneCount)*geom.effectBoneFrames*16;
}
inline bool position(const ModelGeom& geom,std::size_t vertex,float seconds,float& x,float& y,float& z) {
    if(geom.blendIndex.empty() && geom.blendWeight.empty())return true;
    if(!skinned(geom))return false;
    const float t=std::isfinite(seconds)?std::max(0.f,seconds):0.f;
    const auto frame=static_cast<std::size_t>(std::fmod(t*30.f,float(geom.effectBoneFrames)));
    float sx=0,sy=0,sz=0,total=0;
    for(std::size_t k=0;k<4;++k) {
        const float w=geom.blendWeight[vertex*4+k];
        if(!std::isfinite(w) || w<0.f || w>1.0001f)return false;
        if(w==0.f)continue;
        const auto palette=geom.blendIndex[vertex*4+k];
        if(palette>=geom.bonePalette.size())return false;
        const auto bone=geom.bonePalette[palette];
        if(bone>=static_cast<unsigned>(geom.effectBoneCount))return false;
        const auto* m=geom.effectBoneMatrices.data()+(frame*geom.effectBoneCount+bone)*16;
        sx+=w*(x*m[0]+y*m[4]+z*m[8]+m[12]);
        sy+=w*(x*m[1]+y*m[5]+z*m[9]+m[13]);
        sz+=w*(x*m[2]+y*m[6]+z*m[10]+m[14]);total+=w;
    }
    if(total>0){x=sx;y=sy;z=sz;}return true;
}
}
