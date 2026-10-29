#pragma once
// 05-L: reusable .lgo decoder adapter for MindPower effect meshes.
// Only reads a single lwGeomObjInfo from model/effect, not an invented mesh.
// ModelFile.cpp preserves embedded animation. Unresolved external skeletons
// and unsupported/oversize geometry return false without inventing a model.
#include "top/ModelFile.h"
#include "top/EmbeddedEffectSkin.h"
#include "top/LegacyEffectGeometry.h"
#include "top/MagicEffectCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace top { namespace legacy_external {

inline std::string filename(const std::string& name) {
    if (!MagicEffectCatalog::safeName(name)) return {};
    std::string result = name;
    const std::size_t dot = result.rfind('.');
    if (dot == std::string::npos) result += ".lgo";
    else if (result.substr(dot) != ".lgo") return {};
    return result;
}

inline bool load(const std::string& root, const std::string& name, Model& model) {
    const std::string file = filename(name);
    if (file.empty()) return false;
    Model candidate;
    if (!candidate.loadGeom(root + "/model/effect/" + file)) return false;
    if (candidate.geoms.size() != 1) return false;
    ModelGeom& geom = candidate.geoms.front();
    if((!geom.blendIndex.empty() || !geom.blendWeight.empty()) &&
       (!embedded_effect_skin::decode(geom) || !embedded_effect_skin::skinned(geom)))return false;
    const std::size_t n = geom.position.size() / 3;
    if (n < 3 || n > 8192 || geom.position.size() != n * 3 ||
        geom.indices.size() < 3 || geom.indices.size() > 36000 ||
        geom.subsets.empty() || geom.subsets.size() > 16 ||
        (!geom.uv.empty() && geom.uv.size() != n * 2)) return false;
    for (float x : geom.position) if (!std::isfinite(x) || std::fabs(x) > 10000.f) return false;
    for (float x : geom.uv) if (!std::isfinite(x) || std::fabs(x) > 10000.f) return false;
    for (float x : geom.local) if (!std::isfinite(x)) return false;
    if (geom.matrixFrames < 0 || (geom.matrixFrames > 0 &&
        geom.matrixAnimation.size() != static_cast<std::size_t>(geom.matrixFrames) * 16)) return false;
    for (float x : geom.matrixAnimation) if (!std::isfinite(x)) return false;
    for (auto index : geom.indices) if (index >= n) return false;
    bool usable = false;
    for (const auto& subset : geom.subsets) {
        if (subset.indexCount == 0) continue;
        if (subset.indexCount % 3 != 0 || subset.startIndex > geom.indices.size() ||
            subset.indexCount > geom.indices.size() - subset.startIndex) return false;
        usable = true;
    }
    if (!usable) return false;
    model = std::move(candidate);
    return true;
}

// .lgo uses MindPower local XYZ (Z-up). Reuse 05-K emission to apply the
// .eff keyframes and the same MindPower -> OpenGL axis conversion.
inline bool subsetPrimitive(const ModelGeom& geom, const ModelSubset& subset,
                            float seconds, legacy_mesh::Primitive& output) {
    const std::size_t n = geom.position.size() / 3;
    if (n < 3 || n > 8192 || geom.position.size() != n * 3 ||
        subset.indexCount == 0 || subset.indexCount % 3 != 0 ||
        subset.startIndex > geom.indices.size() ||
        subset.indexCount > geom.indices.size() - subset.startIndex ||
        ((!geom.blendIndex.empty() || !geom.blendWeight.empty()) &&
         !embedded_effect_skin::skinned(geom))) return false;
    const float* matrix = geom.local;
    if (geom.matrixFrames > 0 && geom.matrixAnimation.size() == static_cast<std::size_t>(geom.matrixFrames) * 16) {
        const float safe = std::isfinite(seconds) ? std::max(0.f,seconds) : 0.f;
        const int frame = std::min(geom.matrixFrames-1, static_cast<int>(std::fmod(safe*30.f,static_cast<float>(geom.matrixFrames))));
        matrix = geom.matrixAnimation.data() + static_cast<std::size_t>(frame)*16;
    }
    for(int i=0; i<16; ++i) if(!std::isfinite(matrix[i])) return false;
    legacy_mesh::Primitive result;
    result.points.reserve(n);
    for (std::size_t i=0; i<n; ++i) {
        float x=geom.position[i*3], y=geom.position[i*3+1], z=geom.position[i*3+2];
        if(!embedded_effect_skin::position(geom,i,seconds,x,y,z))return false;
        const float ox = x*matrix[0]+y*matrix[4]+z*matrix[8]+matrix[12];
        const float oy = x*matrix[1]+y*matrix[5]+z*matrix[9]+matrix[13];
        const float oz = x*matrix[2]+y*matrix[6]+z*matrix[10]+matrix[14];
        if (!std::isfinite(ox) || !std::isfinite(oy) || !std::isfinite(oz) ||
            std::fabs(ox)>10000.f || std::fabs(oy)>10000.f || std::fabs(oz)>10000.f) return false;
        const float u = geom.uv.size() == n*2 ? geom.uv[i*2] : 0.f;
        const float v = geom.uv.size() == n*2 ? geom.uv[i*2+1] : 0.f;
        result.points.push_back({{{ox,oy,oz}},{{u,v}}});
    }
    for (std::size_t j=subset.startIndex; j<subset.startIndex+subset.indexCount; j+=3) {
        const auto a=geom.indices[j], b=geom.indices[j+1], c=geom.indices[j+2];
        if (a>=n || b>=n || c>=n) return false;
        result.triangles.push_back({{static_cast<std::uint16_t>(a),
                                      static_cast<std::uint16_t>(b),
                                      static_cast<std::uint16_t>(c)}});
    }
    if (result.triangles.empty()) return false;
    output=std::move(result);
    return true;
}

}} // namespace top::legacy_external
