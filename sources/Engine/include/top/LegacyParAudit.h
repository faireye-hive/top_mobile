#pragma once
// SK2.3: read-only structural audit of CMPPartCtrl .par files.
// Layout mirrors EffectSystem.cpp::loadParticles; this inspector does NOT imply
// rendering support and deliberately preserves values before runtime clamping.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace top { namespace par_audit {
struct System {
    int32_t type = 0, count = 0;
    std::string name, texture, model, modelRangeName;
    std::array<float, 3> range{}, direction{}, acceleration{}, offset{};
    uint16_t frames = 0;
    std::vector<float> sizes, rotations, colors;
    float life = 0, velocity = 0, step = 0, delay = 0, play = 0, pathVelocity = 0;
    bool modelRange = false, hasPath = false, hasShadeFlag = false;
    bool mediayEff = false;
    bool modelDirection = false; // v14 _bModelDir controls full dummy binding
    int32_t pathPoints = 0;
    // Material state authored in the file (read, never clamped). EffectSystem's
    // loadParticles discards these; the blend audit reports them.
    bool billboard = false, shade = false;
    int32_t srcBlend = 0, dstBlend = 0, minFilter = 0, magFilter = 0;
    std::string hitEffect;
};
struct Document {
    uint32_t version = 0;
    std::string controller;
    float controllerLife = 0;
    int32_t declaredSystems = 0;
    std::vector<System> systems;
    std::size_t bytes = 0, consumed = 0;
};
struct Result {
    bool ok = false;
    Document document;
    std::string error;
    std::size_t errorOffset = 0;
};

class Reader {
public:
    explicit Reader(const std::vector<unsigned char>& d): bytes_(d) {}
    template<class T> bool get(T& x) {
        if (pos_ > bytes_.size() || sizeof(T) > bytes_.size()-pos_) return false;
        std::memcpy(&x, bytes_.data()+pos_, sizeof(T)); pos_ += sizeof(T); return true;
    }
    bool name(std::string& out) {
        if (pos_ > bytes_.size() || 32 > bytes_.size()-pos_) return false;
        const char* p = reinterpret_cast<const char*>(bytes_.data()+pos_);
        std::size_t len = 0; while (len < 32 && p[len]) ++len;
        out.assign(p,len); pos_ += 32; return true;
    }
    bool skip(std::size_t count) {
        if (pos_ > bytes_.size() || count > bytes_.size()-pos_) return false;
        pos_ += count; return true;
    }
    std::size_t at() const { return pos_; }
private:
    const std::vector<unsigned char>& bytes_;
    std::size_t pos_ = 0;
};
inline bool finite(const std::vector<float>& xs) {
    for (float x: xs) if (!std::isfinite(x)) return false;
    return true;
}
inline bool finite(const std::array<float,3>& xs) {
    for (float x: xs) if (!std::isfinite(x)) return false;
    return true;
}
inline Result parse(const std::vector<unsigned char>& bytes) {
    Result out; out.document.bytes = bytes.size();
    Reader r(bytes);
    auto fail = [&](const char* message) {
        out.ok = false; out.error = message; out.errorOffset = r.at();
        out.document.systems.clear(); return out;
    };
    if (bytes.size() < 44 || bytes.size() > 8U*1024*1024)
        return fail("invalid_file_size");
    auto& d=out.document;
    if (!r.get(d.version) || d.version < 2 || d.version > 15) return fail("unsupported_version_or_header");
    if (!r.name(d.controller) || !r.get(d.declaredSystems) ||
        d.declaredSystems < 0 || d.declaredSystems > 100 ||
        (d.version >= 3 && !r.get(d.controllerLife))) return fail("invalid_controller_header");
    if (!std::isfinite(d.controllerLife)) return fail("nonfinite_controller_life");
    for (int i=0; i<d.declaredSystems; ++i) {
        System s; uint8_t flag=0; int32_t setting=0;
        if (!r.get(s.type) || !r.name(s.name) || !r.get(s.count) ||
            !r.name(s.texture) || !r.name(s.model) ||
            !r.get(s.range[0]) || !r.get(s.range[1]) || !r.get(s.range[2]) ||
            !r.get(s.frames) || s.frames==0 || s.frames>256) return fail("invalid_system_header");
        s.sizes.resize(s.frames);
        s.rotations.resize(static_cast<std::size_t>(s.frames)*3);
        s.colors.resize(static_cast<std::size_t>(s.frames)*4);
        for (float& v: s.sizes) if (!r.get(v)) return fail("truncated_size_keys");
        for (float& v: s.rotations) if (!r.get(v)) return fail("truncated_rotation_keys");
        for (float& v: s.colors) if (!r.get(v)) return fail("truncated_color_keys");
        if (!r.get(flag)) return fail("truncated_system_flag");
        s.billboard = (flag!=0);
        for (int k=0; k<4; ++k) {
            if (!r.get(setting)) return fail("truncated_system_settings");
            // Original order: SRCBLEND, DESTBLEND, MINFILTER, MAGFILTER.
            (k==0?s.srcBlend:k==1?s.dstBlend:k==2?s.minFilter:s.magFilter) = setting;
        }
        if (!r.get(s.life) || !r.get(s.velocity) ||
            !r.get(s.direction[0]) || !r.get(s.direction[1]) || !r.get(s.direction[2]) ||
            !r.get(s.acceleration[0]) || !r.get(s.acceleration[1]) || !r.get(s.acceleration[2]) ||
            !r.get(s.step)) return fail("truncated_system_motion");
        if (d.version>3) {
            if (!r.get(flag) || !r.name(s.modelRangeName)) return fail("truncated_model_range");
            s.modelRange=(flag!=0);
        }
        if (d.version>4) for (float& f:s.offset) if (!r.get(f)) return fail("truncated_offset");
        if (d.version>5 && (!r.get(s.delay) || !r.get(s.play))) return fail("truncated_timing");
        if (d.version>8) {
            if (!r.get(flag)) return fail("truncated_path_flag");
            s.hasPath=(flag!=0);
            if (s.hasPath) {
                if (!r.get(s.pathPoints) || !r.get(s.pathVelocity) ||
                    s.pathPoints<0 || s.pathPoints>200) return fail("invalid_path_header");
                if (!r.skip(static_cast<std::size_t>(s.pathPoints)*12)) return fail("truncated_path_points");
                if (s.pathPoints>=2 && !r.skip(static_cast<std::size_t>(s.pathPoints-1)*24))
                    return fail("truncated_path_segments");
            }
        }
        if (d.version>9) {
            if (!r.get(flag)) return fail("truncated_v10_flag");
            s.shade=(flag!=0);s.hasShadeFlag=true;
        }
        if (d.version>10 && !r.name(s.hitEffect)) return fail("truncated_v11_extra");
        if (d.version>11 && s.modelRange) {
            uint16_t count=0;
            if (!r.get(count) || !r.skip(static_cast<std::size_t>(count)*12))
                return fail("truncated_model_range_points");
        }
        if (d.version>12 && !r.skip(4)) return fail("truncated_v13_extra");
        if (d.version>13) {
            if (!r.get(flag)) return fail("truncated_v14_extra");
            s.modelDirection=(flag!=0);
        }
        if (d.version>14) {
            if (!r.get(flag)) return fail("truncated_v15_mediay_eff");
            s.mediayEff=(flag!=0);
        }
        if (!s.modelRange) s.modelRangeName.clear(); // ignored bytes in older files may contain garbage
        if (!finite(s.sizes) || !finite(s.rotations) || !finite(s.colors) ||
            !finite(s.range) || !finite(s.direction) || !finite(s.acceleration) ||
            !finite(s.offset) || !std::isfinite(s.life) || !std::isfinite(s.velocity) ||
            !std::isfinite(s.step) || !std::isfinite(s.delay) || !std::isfinite(s.play) ||
            !std::isfinite(s.pathVelocity)) return fail("nonfinite_system_data");
        d.systems.push_back(std::move(s));
    }
    // Runtime returns false for empty sets. Audit retains them as structurally
    // readable but reports empty_controller separately from runtime compatibility.
    d.consumed=r.at(); out.ok=true; return out;
}
inline Result inspectFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { Result result; result.error="file_open_failed"; return result; }
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)),{});
    if (stream.bad()) { Result result; result.error="file_read_failed"; return result; }
    return parse(bytes);
}
}} // namespace top::par_audit
