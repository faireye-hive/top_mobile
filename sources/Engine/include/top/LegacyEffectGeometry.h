#pragma once
// 05-K: procedural CEffectModel primitives from the original MindPower client.
// Names and local vertex layouts are defined by I_Effect.cpp and
// CMPResManger::LoadTotalMesh(). External .lgo files are NOT decoded here.
#include "top/LegacyEffFormat.h"
#include "top/GfxDevice.h"
#include "top/EffectViewBasis.h"
#include "top/HistoricalEffectUnits.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace top { namespace legacy_mesh {

using UV = legacy_eff::Subeffect::UV;
struct Point { std::array<float,3> xyz; UV uv; };
struct Primitive {
    std::vector<Point> points;
    std::vector<std::array<std::uint16_t,3>> triangles;
};

inline bool recognized(const std::string& name) {
    return name == "Triangle" || name == "TrianglePlane" || name == "Rect" ||
           name == "RectPlane" || name == "RectZ" || name == "Cone" ||
           name == "Cylinder";
}

// Returns false for unknown models, unsafe parameters and models supplied by
// .lgo assets.  No guessed geometry is emitted for unsupported resources.
inline bool make(const std::string& name, const legacy_eff::Subeffect::ModelParams& p,
                 Primitive& output) {
    Primitive mesh;
    auto add = [&](float x,float y,float z,float u,float v) {
        mesh.points.push_back({{{x,y,z}},{{u,v}}});
    };
    auto tri = [&](std::uint16_t a,std::uint16_t b,std::uint16_t c) {
        mesh.triangles.push_back({{a,b,c}});
    };
    if (name == "Triangle") {
        add(-.5f,0.f,0.f,0.f,1.f);add(0.f,0.f,.5f,.5f,0.f);add(.5f,0.f,0.f,1.f,1.f);
        tri(0,1,2);
    } else if (name == "TrianglePlane") {
        add(-.5f,.5f,0.f,0.f,1.f);add(0.f,-.5f,0.f,.5f,0.f);add(.5f,.5f,0.f,1.f,1.f);
        tri(0,1,2);
    } else if (name == "Rect") {
        add(-.5f,0.f,0.f,0.f,1.f);add(-.5f,0.f,1.f,0.f,0.f);
        add(.5f,0.f,1.f,1.f,0.f);add(.5f,0.f,0.f,1.f,1.f);
        tri(0,1,2);tri(0,2,3);
    } else if (name == "RectPlane") {
        add(-.5f,-.5f,0.f,0.f,1.f);add(-.5f,.5f,0.f,0.f,0.f);
        add(.5f,.5f,0.f,1.f,0.f);add(.5f,-.5f,0.f,1.f,1.f);
        tri(0,1,2);tri(0,2,3);
    } else if (name == "RectZ") {
        add(0.f,0.f,0.f,0.f,1.f);add(0.f,0.f,1.f,0.f,0.f);
        add(0.f,1.f,1.f,1.f,0.f);add(0.f,1.f,0.f,1.f,1.f);
        tri(0,1,2);tri(0,2,3);
    } else if (name == "Cone" || name == "Cylinder") {
        // Native CEffectModel::CreateCone/CreateCylinder generate triangle
        // strips from a 2-vertex pair for each segment (including closure).
        if (p.segments < 3 || p.segments > 60 ||
            !std::isfinite(p.height) || p.height <= 0.f || p.height > 100.f ||
            !std::isfinite(p.bottomRadius) || p.bottomRadius <= 0.f || p.bottomRadius > 100.f ||
            !std::isfinite(p.topRadius) || p.topRadius < 0.f || p.topRadius > 100.f) return false;
        constexpr float tau = 6.2831853071795864769f;
        const bool cone = name == "Cone";
        for (int i=0; i<=p.segments; ++i) {
            const float angle = tau * static_cast<float>(i)/static_cast<float>(p.segments);
            const float sx=std::sin(angle), sy=std::cos(angle);
            const float u = 1.f - static_cast<float>(i)/static_cast<float>(p.segments);
            const float r = cone ? 0.f : p.topRadius;
            add(r*sx,r*sy,p.height,u,0.f);
            add(p.bottomRadius*sx,p.bottomRadius*sy,0.f,u,cone?1.5f:1.f);
        }
        for (int i=0; i<p.segments*2; ++i) {
            // D3DPT_TRIANGLESTRIP alternates winding for consecutive faces.
            const auto k = static_cast<std::uint16_t>(i);
            if ((i&1)==0) tri(k,k+1,k+2); else tri(k+1,k,k+2);
        }
    } else return false;
    output = std::move(mesh);
    return true;
}

inline legacy_eff::Subeffect::ModelParams paramsAt(const legacy_eff::Subeffect& part, float seconds) {
    if (part.animatedParams.empty() || part.keys.empty()) return part.modelParams;
    // The historical controller updates morph-parameters at authored keyframes.
    // Interpolate scalar radii/height only for same topology; different segment
    // counts are discrete to avoid morphing between incompatible vertex buffers.
    const auto n = std::min(part.keys.size(), part.animatedParams.size());
    if(n==0)return part.modelParams;
    const auto phase=legacy_eff::framePhase(part,seconds);
    const auto from=std::min(phase.from,n-1);
    const auto to=std::min(phase.to,n-1);
    const auto& left=part.animatedParams[from];
    const auto& right=part.animatedParams[to];
    const float t=phase.fraction;
    auto out=left;
    out.segments=t>=1.f?right.segments:left.segments;
    out.height=left.height+(right.height-left.height)*t;
    out.topRadius=left.topRadius+(right.topRadius-left.topRadius)*t;
    out.bottomRadius=left.bottomRadius+(right.bottomRadius-left.bottomRadius)*t;
    return out;
}

// D3DXMatrixRotationYawPitchRoll: roll (Z), then pitch (X), then yaw (Y).
inline std::array<float,3> rotate(const std::array<float,3>& v,
                                const std::array<float,3>& angle) {
    const float sx=std::sin(angle[0]),cx=std::cos(angle[0]);
    const float sy=std::sin(angle[1]),cy=std::cos(angle[1]);
    const float sz=std::sin(angle[2]),cz=std::cos(angle[2]);
    const float x=v[0]*cz-v[1]*sz, y=v[0]*sz+v[1]*cz;
    const float py=y*cx-v[2]*sx, pz=y*sx+v[2]*cx;
    return {{x*cy+pz*sy,py,-x*sy+pz*cy}};
}

// CMPModelEff: child key transform * parent scale/rotation * translation.
// Keep the child's translated origin inside this transform, not just its size.
struct ParentTransform {
    float scale=1.f;
    std::array<float,3> angle{{0.f,0.f,0.f}};
    bool billboard=false;
    EffectViewBasis view;
    // Optional DX9 row-vector binding; translation is the caller's origin.
    const std::array<float,16>* bone = nullptr;

    std::array<float,3> apply(std::array<float,3> local) const {
        for(float& v:local)v*=scale;
        if(bone) {
            const auto& m=*bone;
            return {{local[0]*m[0]+local[1]*m[4]+local[2]*m[8],
                     local[0]*m[1]+local[1]*m[5]+local[2]*m[9],
                     local[0]*m[2]+local[1]*m[6]+local[2]*m[10]}};
        }
        return rotate(local,angle);
    }
};

// Rotations follow the original D3DX yaw/pitch/roll convention in local
// MindPower axes. Its Z-up world is converted to the port's Y-up coordinates.
// No GL context required: returned triangles can be regression-tested.
inline void emit(const Primitive& mesh, const legacy_eff::Key& key,
                 const std::vector<UV>& animatedUV, float originX,float originY,float originZ,
                 std::vector<MeshVertex>& out, float modelScale = historical_effect_units::mesh, float worldYaw = 0.f,
                 float worldPitch = 0.f, const ParentTransform* parent = nullptr) {
    if (mesh.triangles.empty()) return;
    std::vector<MeshVertex> transformed;
    transformed.reserve(mesh.points.size());
    // DX CMagicCtrl::Emission -> RotatingXZ(): orienta o modelo no sentido
    // do voo. Aplicar APOS a rotacao local; zero preserva todos os usos legados.
    const float sh=std::sin(worldYaw), ch=std::cos(worldYaw);
    const float sp=std::sin(worldPitch), cp=std::cos(worldPitch);
    for (std::size_t i=0;i<mesh.points.size();++i) {
        const auto& v=mesh.points[i];
        const float x=v.xyz[0]*key.size[0]*modelScale;
        const float y=v.xyz[1]*key.size[1]*modelScale;
        const float z=v.xyz[2]*key.size[2]*modelScale;
        const auto rotated=rotate({{x,y,z}},key.angle);
        MeshVertex w{};
        std::array<float,3> local{{key.position[0]+rotated[0],
                                  key.position[1]+rotated[1],key.position[2]+rotated[2]}};
        if(parent)local=parent->apply(local);
        const float localX=local[0],forward=local[1],up=local[2];
        const float localZ = forward*cp-up*sp;
        w.x=originX+localX*ch-localZ*sh;
        w.y=originY+forward*sp+up*cp;
        w.z=originZ+localX*sh+localZ*ch;
        if(parent && parent->billboard) {
            const auto normal=parent->view.normal();
            float* axes[]={&w.x,&w.y,&w.z};
            const float origin[]={originX,originY,originZ};
            for(int c=0;c<3;++c)*axes[c]=origin[c]+local[0]*parent->view.right[c]+
                local[1]*parent->view.up[c]+local[2]*normal[c];
        }
        w.r=std::clamp(key.color[0],0.f,1.f);w.g=std::clamp(key.color[1],0.f,1.f);
        w.b=std::clamp(key.color[2],0.f,1.f);w.a=std::clamp(key.color[3],0.f,1.f);
        const UV uv = animatedUV.size()==mesh.points.size()?animatedUV[i]:v.uv;
        w.u=uv[0];w.v=uv[1];
        transformed.push_back(w);
    }
    for (const auto& tri:mesh.triangles) {
        for (auto idx:tri) out.push_back(transformed[idx]);
    }
}

}} // namespace top::legacy_mesh
