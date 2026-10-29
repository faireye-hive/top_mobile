#pragma once
// SK2.9: EffectObj.cpp MagicList and MPEffectCtrl.cpp CalculateEmission.
// OpenGL axes: X, height, Z. Continuous sampling retains elapsed time at low FPS.
#include <algorithm>
#include <cmath>

namespace top::historic_magic {
struct Vec3 { float x=0, y=0, z=0; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float k) { return {a.x*k,a.y*k,a.z*k}; }
inline float length(Vec3 a) { return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); }
inline Vec3 unit(Vec3 a) { const float d=length(a); return d>1e-7f?a*(1.f/d):Vec3{}; }
inline bool knownMode(int mode) { return mode>=0 && mode<=6; }
inline const char* modeName(int mode) {
    static constexpr const char* names[]={"Part_drop","Part_fly","Part_trace",
        "Part_fshade","Part_arc","Part_dirlight","Part_dist"};
    return knownMode(mode)?names[mode]:"undefined";
}
struct Path {
    int mode=-1;
    Vec3 from, to;
    float speed=0;
    float clipLength=0;
    Vec3 direction() const {
        Vec3 delta=to-from;
        if(mode==5) delta.y=0; // Emission: horizontal RotatingXZ for dirlight
        return unit(delta);
    }
    Vec3 atDistance(float distance) const {
        if(mode==5) return from;
        const Vec3 delta=to-from;
        const float d=length(delta);
        if(d<=1e-7f) return to;
        if(mode!=4) return from+unit(delta)*std::clamp(distance,0.f,d);
        // Historical arc is a radial projection of a point moving on the
        // chord onto a circle; it is NOT a sine/parabolic height offset.
        Vec3 center=from+delta*.5f;
        center.y-=d*.5f;
        const float radius=length(from-center);
        return center+unit(from+unit(delta)*std::max(0.f,distance)-center)*radius;
    }
    float endDistance() const {
        const Vec3 delta=to-from;
        const float d=length(delta);
        if(mode==5) return 0;
        const float tolerance=mode==2?1.f:((mode==3||mode==4)?.5f:0.f);
        if(tolerance==0 || d<=1e-7f) return d;
        if(mode==2 && d<1.f) return 0; // Part_trace initial near-target shortcut
        // PointInstrPointRange tests X/Y in DX (X/Z here), ignoring height.
        // Find entry into that box analytically along straight paths, and
        // monotonically by bisection along the historical projected arc.
        if(mode!=4) {
            const float planar=std::max(std::fabs(delta.x),std::fabs(delta.z));
            return planar>tolerance?d*(1.f-tolerance/planar):0.f;
        }
        // Part_arc expires at ten seconds if the projected circle never
        // reaches the target box (possible with unequal endpoint heights).
        float lo=0,hi=std::max(0.f,speed)*10.f;
        const bool useX=std::fabs(delta.x)>std::fabs(delta.z);
        const float sign=(useX?delta.x:delta.z)>=0.f?1.f:-1.f;
        for(int i=0;i<40;++i) {
            const float mid=(lo+hi)*.5f;
            const Vec3 p=atDistance(mid);
            if(sign*(useX?p.x-to.x:p.z-to.z)>=-tolerance) hi=mid;
            else lo=mid;
        }
        return hi;
    }
    float duration() const {
        if(mode==5) return std::max(0.f,clipLength); // Play(true) == one cycle
        return speed>0 && std::isfinite(speed)?endDistance()/speed:0.f;
    }
    Vec3 position(float age) const {
        return atDistance(std::min(endDistance(),std::max(0.f,age)*speed));
    }
    float yaw() const { const auto d=direction(); return -std::atan2(d.x,d.z); }
    float pitch() const { return std::asin(std::clamp(direction().y,-1.f,1.f)); }
};
inline float modelTime(float age,float cycleLength,int mode) {
    // CMagicCtrl::Emission: Play(false) loops, Play(true) (dirlight) once.
    return knownMode(mode) && mode!=5 && cycleLength>0.f
        ? std::fmod(std::max(0.f,age),cycleLength):age;
}
} // namespace top::historic_magic
