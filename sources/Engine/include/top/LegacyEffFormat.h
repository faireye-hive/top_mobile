#pragma once
// 05-I: layout binario CMPResManger::LoadEffectFromFile + I_Effect::LoadFromFile.
// All reads are bounded and transactional. This is NOT a converter for the
// DirectX geometry/vertex-shaders; it decodes the authored subeffect tracks.

#include "top/MagicEffectCatalog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace top { namespace legacy_eff {

struct Key {
    float time = 0.f; // DX9 I_Effect::getFrameTime: duration of THIS key, NOT an absolute timestamp.
    std::array<float, 3> size{{1.f, 1.f, 1.f}};
    std::array<float, 3> angle{{0.f, 0.f, 0.f}};
    std::array<float, 3> position{{0.f, 0.f, 0.f}};
    std::array<float, 4> color{{1.f, 1.f, 1.f, 1.f}};
};
struct Subeffect {
    std::string name;
    std::string texture;
    std::string model;
    std::vector<std::string> frames;
    // 05-J: original CTexCoordList (UV interpolation), CTexList (UV
    // regions/atlas), CTexFrame (texture filenames). Only rectangular
    // four-vertex billboards are rendered by this OpenGL backend.
    using QuadUV = std::array<std::array<float, 2>, 4>;
    float coordFrameTime = 0.f;
    float regionFrameTime = 0.f;
    float textureFrameTime = 0.f;
    std::vector<QuadUV> coordFrames;
    std::vector<QuadUV> regions;
    // 05-K: keep vertex-specific UV sequences for native procedural meshes.
    // The previous 4-vertex tracks remain unchanged for existing billboards.
    std::uint16_t uvVertexCount = 0;
    using UV = std::array<float, 2>;
    std::vector<std::vector<UV>> meshCoordFrames;
    std::vector<std::vector<UV>> meshRegions;
    struct ModelParams {
        int segments = 0;
        float height = 0.f, topRadius = 0.f, bottomRadius = 0.f;
    };
    ModelParams modelParams;
    std::vector<ModelParams> animatedParams;
    int type = 0;
    int sourceBlend = 0;
    int destBlend = 0;
    bool billboard = false;
    float length = 0.f;
    std::vector<Key> keys;
};
struct Document {
    std::uint32_t version = 0;
    int technique = 0;
    bool usePath = false;
    std::string path;
    bool useSound = false;
    std::string sound;
    bool rotating = false;
    std::array<float, 3> rotation{{0.f, 0.f, 0.f}};
    float rotationSpeed = 0.f;
    std::vector<Subeffect> parts;
};

class Reader {
public:
    explicit Reader(const std::vector<unsigned char>& data) : data_(data) {}
    std::size_t position() const { return pos_; }
    std::size_t remaining() const { return data_.size() - pos_; }
    bool skip(std::size_t bytes) {
        if (bytes > remaining()) return false;
        pos_ += bytes; return true;
    }
    bool byte(std::uint8_t& v) {
        if (remaining() < 1) return false;
        v = data_[pos_++]; return true;
    }
    bool u16(std::uint16_t& v) {
        if (remaining() < 2) return false;
        v = std::uint16_t(data_[pos_]) | (std::uint16_t(data_[pos_+1]) << 8);
        pos_ += 2; return true;
    }
    bool u32(std::uint32_t& v) {
        if (remaining() < 4) return false;
        v = std::uint32_t(data_[pos_]) | (std::uint32_t(data_[pos_+1]) << 8) |
            (std::uint32_t(data_[pos_+2]) << 16) | (std::uint32_t(data_[pos_+3]) << 24);
        pos_ += 4; return true;
    }
    bool i32(int& v) {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        std::int32_t signedValue;
        std::memcpy(&signedValue, &u, 4);
        v = signedValue;
        return true;
    }
    bool f32(float& v) {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        std::memcpy(&v, &u, 4);
        return std::isfinite(v);
    }
    // I_Effect::LoadFromFile reads the authored model parameters verbatim.
    // Some Client .eff files store non-finite values here even for models
    // whose geometry never consumes these fields (for example RectPlane).
    // Keep those bits; native Cone/Cylinder generation validates them before use.
    bool rawF32(float& v) {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        std::memcpy(&v, &u, 4);
        return true;
    }
    bool uvList(std::vector<Subeffect::UV>& out, std::size_t n) {
        out.resize(n);
        for (auto& uv : out) if (!array(uv)) return false;
        return true;
    }
    bool uvQuad(Subeffect::QuadUV& out) {
        for (auto& uv : out) if (!array(uv)) return false;
        return true;
    }
    bool name32(std::string& out) {
        if (remaining() < 32) return false;
        const char* start = reinterpret_cast<const char*>(data_.data() + pos_);
        const void* terminator = std::memchr(start, 0, 32);
        if (!terminator) return false;
        out.assign(start, static_cast<const char*>(terminator));
        pos_ += 32;
        return true;
    }
    template<std::size_t N> bool array(std::array<float, N>& out) {
        for (float& value : out) if (!f32(value)) return false;
        return true;
    }
private:
    const std::vector<unsigned char>& data_;
    std::size_t pos_ = 0;
};

inline bool decodeHeader(Reader& read, Document& doc, int& subCount) {
    std::uint8_t flag = 0;
    if (!read.u32(doc.version) || doc.version < 1 || doc.version > 7 ||
        !read.i32(doc.technique) || !read.byte(flag) || flag > 1) return false;
    doc.usePath = flag != 0;
    if (!read.name32(doc.path) || !read.byte(flag) || flag > 1) return false;
    doc.useSound = flag != 0;
    if (!read.name32(doc.sound) || !read.byte(flag) || flag > 1 ||
        !read.array(doc.rotation) || !read.f32(doc.rotationSpeed) ||
        !read.i32(subCount) || subCount < 0 || subCount > 64) return false;
    doc.rotating = flag != 0;
    return true;
}
inline bool header(const std::vector<unsigned char>& bytes, Document& out, int& count) {
    Reader reader(bytes);
    Document temp;
    int subCount = 0;
    if (!decodeHeader(reader, temp, subCount)) return false;
    out = std::move(temp);
    count = subCount;
    return true;
}
inline bool decode(const std::vector<unsigned char>& bytes, Document& output,
                   bool allowRawModelParams = false) {
    Reader read(bytes);
    Document doc;
    int subCount = 0;
    std::uint8_t flag = 0;
    if (!decodeHeader(read, doc, subCount)) return false;
    for (int index = 0; index < subCount; ++index) {
        Subeffect part;
        std::uint16_t frames = 0, uvVertices = 0, uvCount = 0, texCount = 0;
        float scratchFloat = 0.f;
        int scratchInt = 0;
        if (!read.name32(part.name) || !read.i32(part.type) ||
            !read.i32(part.sourceBlend) || !read.i32(part.destBlend) ||
            !read.f32(part.length) || part.length < 0.f || part.length > 3600.f ||
            !read.u16(frames) || frames == 0 || frames > 512) return false;
        part.keys.resize(frames);
        for (auto& key : part.keys) if (!read.f32(key.time)) return false;
        for (auto& key : part.keys) if (!read.array(key.size)) return false;
        for (auto& key : part.keys) if (!read.array(key.angle)) return false;
        for (auto& key : part.keys) if (!read.array(key.position)) return false;
        for (auto& key : part.keys) if (!read.array(key.color)) return false;
        if (!read.u16(uvVertices) || !read.u16(uvCount) ||
            uvVertices > 4096 || uvCount > 512 || !read.f32(part.coordFrameTime) ||
            part.coordFrameTime < 0.f) return false;
        part.uvVertexCount = uvVertices;
        if (uvVertices == 4 && uvCount > 0) {
            part.coordFrames.resize(uvCount);
            for (auto& coords : part.coordFrames) if (!read.uvQuad(coords)) return false;
        } else if (uvVertices >= 3 && uvVertices <= 128 && uvCount > 0) {
            part.meshCoordFrames.resize(uvCount);
            for (auto& coords : part.meshCoordFrames)
                if (!read.uvList(coords, uvVertices)) return false;
        } else if (!read.skip(std::size_t(uvVertices) * uvCount * 8u)) return false;
        if (!read.u16(texCount) || texCount > 512 || !read.f32(part.regionFrameTime) ||
            part.regionFrameTime < 0.f || !read.name32(part.texture)) return false;
        if (uvVertices == 4 && texCount > 0) {
            part.regions.resize(texCount);
            for (auto& coords : part.regions) if (!read.uvQuad(coords)) return false;
        } else if (uvVertices >= 3 && uvVertices <= 128 && texCount > 0) {
            part.meshRegions.resize(texCount);
            for (auto& coords : part.meshRegions)
                if (!read.uvList(coords, uvVertices)) return false;
        } else if (!read.skip(std::size_t(texCount) * uvVertices * 8u)) return false;
        if (!read.name32(part.model) || !read.byte(flag) || flag > 1 ||
            !read.i32(scratchInt)) return false;
        part.billboard = flag != 0;
        if (doc.version > 1) {
            if (!read.i32(part.modelParams.segments) ||
                !(allowRawModelParams ? read.rawF32(part.modelParams.height) :
                                         read.f32(part.modelParams.height)) ||
                !(allowRawModelParams ? read.rawF32(part.modelParams.topRadius) :
                                         read.f32(part.modelParams.topRadius)) ||
                !(allowRawModelParams ? read.rawF32(part.modelParams.bottomRadius) :
                                         read.f32(part.modelParams.bottomRadius))) return false;
        }
        if (doc.version > 2) {
            std::uint16_t texFrames = 0;
            if (!read.u16(texFrames) || texFrames > 512 ||
                !read.f32(part.textureFrameTime) || part.textureFrameTime < 0.f) return false;
            for (std::uint16_t i = 0; i < texFrames; ++i) {
                std::string tex;
                if (!read.name32(tex)) return false;
                // Preserve frame indices. Unsafe or empty historical names
                // become blank placeholders, never filesystem paths.
                part.frames.push_back(MagicEffectCatalog::safeName(tex) ? std::move(tex) : std::string{});
            }
            if (!read.f32(scratchFloat)) return false;
        }
        if (doc.version > 3) {
            int useParam = 0;
            if (!read.i32(useParam) || useParam < 0 || useParam > 1) return false;
            if (useParam) {
                part.animatedParams.resize(frames);
                for (auto& param : part.animatedParams) {
                    if (!read.i32(param.segments) || !read.f32(param.height) ||
                        !read.f32(param.topRadius) || !read.f32(param.bottomRadius)) return false;
                }
            }
        }
        if (doc.version > 4) {
            std::array<float, 4> rotating;
            if (!read.byte(flag) || flag > 1 || !read.array(rotating)) return false;
        }
        if (doc.version > 5 && (!read.byte(flag) || flag > 1)) return false;
        if (doc.version > 6 && (!read.byte(flag) || flag > 1)) return false;
        // A texture may be empty/unsafe; keep the tracks but never use
        // unsafe paths to load filesystem resources.
        if (!MagicEffectCatalog::safeName(part.texture)) part.texture.clear();
        doc.parts.push_back(std::move(part));
    }
    // Some client versions append unknown metadata. Do not claim a verified
    // decode if the structure ends in unexplained trailing bytes.
    if (read.remaining() != 0) return false;
    output = std::move(doc);
    return true;
}

// Fallback UV order follows the historical RectPlane vertex order:
// bottom-left, top-left, top-right, bottom-right (GL quads are triangulated).
inline Subeffect::QuadUV defaultQuadUV() {
    return {{{{0.f, 1.f}}, {{0.f, 0.f}}, {{1.f, 0.f}}, {{1.f, 1.f}}}};
}

inline std::size_t textureFrameIndex(float seconds, float frameSeconds, std::size_t count) {
    if (count < 2 || !std::isfinite(seconds) || !std::isfinite(frameSeconds) || frameSeconds <= 0.f)
        return 0;
    const double frame = std::floor(std::max(0.0, static_cast<double>(seconds)) / frameSeconds);
    // Avoid casting an unbounded user-controlled floating-point value to size_t.
    return static_cast<std::size_t>(std::fmod(frame, static_cast<double>(count)));
}

inline Subeffect::QuadUV sampleUV(const Subeffect& part, float seconds) {
    auto result = defaultQuadUV();
    // The original CTexCoordList interpolates consecutive sets of UVs.
    if (!part.coordFrames.empty()) {
        const std::size_t i = textureFrameIndex(seconds, part.coordFrameTime, part.coordFrames.size());
        const std::size_t next = std::min(i + 1, part.coordFrames.size() - 1);
        float t = 0.f;
        if (part.coordFrames.size() > 1 && part.coordFrameTime > 0.f && std::isfinite(seconds))
            t = std::clamp(std::fmod(std::max(0.f, seconds), part.coordFrameTime) /
                           part.coordFrameTime, 0.f, 1.f);
        for (std::size_t vertex = 0; vertex < 4; ++vertex)
            for (std::size_t axis = 0; axis < 2; ++axis)
                result[vertex][axis] = part.coordFrames[i][vertex][axis] * (1.f - t) +
                                      part.coordFrames[next][vertex][axis] * t;
    }
    // The original CTexList switches full sets of atlas coordinates.
    if (!part.regions.empty())
        result = part.regions[textureFrameIndex(seconds, part.regionFrameTime, part.regions.size())];
    return result;
}

// Sample per-vertex coordinates of non-billboard procedural meshes.  Regions
// are discrete; authored coordinate tracks are interpolated. Incompatible
// vertex counts deliberately fall back to the primitive's original UVs.
inline std::vector<Subeffect::UV> sampleMeshUV(const Subeffect& part, float seconds,
                                             const std::vector<Subeffect::UV>& defaults) {
    std::vector<Subeffect::UV> result = defaults;
    if (!part.meshCoordFrames.empty()) {
        const auto index = textureFrameIndex(seconds, part.coordFrameTime, part.meshCoordFrames.size());
        const auto next = std::min(index + 1, part.meshCoordFrames.size()-1);
        if (part.meshCoordFrames[index].size() == result.size() &&
            part.meshCoordFrames[next].size() == result.size()) {
            float t = 0.f;
            if (part.coordFrameTime > 0.f && std::isfinite(seconds))
                t = std::clamp(std::fmod(std::max(0.f, seconds), part.coordFrameTime) /
                               part.coordFrameTime, 0.f, 1.f);
            for (std::size_t i=0; i < result.size(); ++i)
                for (int axis=0; axis<2; ++axis)
                    result[i][axis] = part.meshCoordFrames[index][i][axis]*(1.f-t) +
                                      part.meshCoordFrames[next][i][axis]*t;
        }
    }
    if (!part.meshRegions.empty()) {
        const auto& region = part.meshRegions[textureFrameIndex(seconds, part.regionFrameTime,
                                                                 part.meshRegions.size())];
        if (region.size() == result.size()) result = region;
    }
    // Quad UV fields are reused for built-in 4-vertex meshes.
    if (result.size() == 4 && (!part.regions.empty() || !part.coordFrames.empty())) {
        const auto quad = sampleUV(part, seconds);
        for (std::size_t i=0; i<4; ++i) result[i] = quad[i];
    }
    return result;
}

// SK2.7: The DX9 CMPModelEff::FrameMove advances m_wCurFrame after
// getFrameTime(CURRENT frame) elapsed, and lerps CURRENT -> NEXT (the last
// frame interpolates to frame zero). No key.time is an absolute timestamp.
// Sample using cumulative durations instead of relying on GL frame rate.
struct FramePhase {
    std::size_t from=0, to=0;
    float fraction=0.f;
    bool finished=false;
};
inline float duration(const Subeffect& part) {
    double result=0.0;
    for(const auto& key:part.keys) {
        if(!std::isfinite(key.time) || key.time<0.f)return 0.f;
        result+=key.time;
    }
    return std::isfinite(result) && result<=3600.0 ? static_cast<float>(result) : 0.f;
}
// EFF key times are durations, as consumed by CMPModelEff::FrameMove.
inline constexpr bool historicalTimeMode() { return true; }
inline FramePhase absoluteFramePhase(const Subeffect& part, float seconds) {
    FramePhase phase;
    if(part.keys.size()<2 || !std::isfinite(seconds))return phase;
    const float at=std::max(0.f,seconds);
    for(std::size_t i=1;i<part.keys.size();++i) {
        const auto& left=part.keys[i-1];
        const auto& right=part.keys[i];
        if(at>right.time && i+1<part.keys.size())continue;
        phase.from=i-1;phase.to=i;
        const float delta=right.time-left.time;
        phase.fraction=delta>1e-6f?std::clamp((at-left.time)/delta,0.f,1.f):1.f;
        return phase;
    }
    return phase;
}
inline FramePhase framePhase(const Subeffect& part, float seconds) {
    if(!historicalTimeMode())return absoluteFramePhase(part,seconds);
    FramePhase phase;
    const auto n=part.keys.size();
    if(n==0){phase.finished=true;return phase;}
    if(!std::isfinite(seconds))return phase;
    const double at=std::max(0.0,static_cast<double>(seconds));
    double cursor=0.0;
    for(std::size_t i=0;i<n;++i){
        const float d=part.keys[i].time;
        if(!std::isfinite(d) || d<0.f){phase.finished=true;return phase;}
        const double end=cursor+d;
        // Float32 frame times accumulate small rounding errors (0.06+0.21+0.31).
        // Treat near-equal boundaries as the next frame / completed clip.
        if(d>1e-7f && at<end-1e-7){
            phase.from=i; phase.to=(i+1)%n;
            phase.fraction=static_cast<float>(std::clamp((at-cursor)/d,0.0,1.0));
            return phase;
        }
        cursor=end;
    }
    phase.from=n-1;phase.to=n-1;phase.finished=(cursor>1e-7);
    return phase;
}

inline Key samplePhase(const Subeffect& part, const FramePhase& phase) {
    if(part.keys.empty())return {};
    const std::size_t from=std::min(phase.from,part.keys.size()-1);
    const std::size_t to=std::min(phase.to,part.keys.size()-1);
    const Key& left=part.keys[from];
    const Key& right=part.keys[to];
    const float t=std::clamp(phase.fraction,0.f,1.f);
    Key result;
    result.time=left.time;
    for(int j=0;j<3;++j){
        result.size[j]=left.size[j]+(right.size[j]-left.size[j])*t;
        result.angle[j]=left.angle[j]+(right.angle[j]-left.angle[j])*t;
        result.position[j]=left.position[j]+(right.position[j]-left.position[j])*t;
    }
    for(int j=0;j<4;++j)
        result.color[j]=left.color[j]+(right.color[j]-left.color[j])*t;
    return result;
}
inline Key sample(const Subeffect& part, float seconds) {
    if(part.keys.empty())return {};
    auto result=samplePhase(part,framePhase(part,seconds));
    result.time=std::isfinite(seconds)?std::max(0.f,seconds):0.f;
    return result;
}

}} // namespace top::legacy_eff
