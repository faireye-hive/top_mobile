#pragma once
// Ray picking offline: geometria aproximada do corpo, sem depender do terreno.
#include <algorithm>
#include <cmath>
#include <limits>
namespace top::screen_pick {
struct Ray { float x=0, y=0, z=0, dx=0, dy=0, dz=0; };
struct Hit { int kind=0; int index=-1; float t=std::numeric_limits<float>::infinity(); };
// Centros e alturas em metros; eixo vertical do OpenGL e Y.
inline float sphere(const Ray& r, float x,float y,float z,float radius) {
    const float a = r.dx*r.dx+r.dy*r.dy+r.dz*r.dz;
    if (!(a>1e-8f) || !(radius>0.f)) return std::numeric_limits<float>::infinity();
    const float px=r.x-x,py=r.y-y,pz=r.z-z;
    const float b=2.f*(px*r.dx+py*r.dy+pz*r.dz);
    const float c=px*px+py*py+pz*pz-radius*radius;
    const float d=b*b-4.f*a*c;
    if (d<0.f) return std::numeric_limits<float>::infinity();
    const float s=std::sqrt(d);
    const float t=(-b-s)/(2.f*a), t2=(-b+s)/(2.f*a);
    if (t>=0.f && t<=1.f) return t;
    return t2>=0.f && t2<=1.f ? t2 : std::numeric_limits<float>::infinity();
}
inline float actor(const Ray& r,float x,float ground,float z,float radius=0.70f,float height=2.f) {
    const float body=std::max(.5f,height);
    float best=std::numeric_limits<float>::infinity();
    // Aproxima uma capsula usando tres volumes alinhados verticalmente.
    for(float part: {.28f,.58f,.85f})
        best=std::min(best,sphere(r,x,ground+body*part,z,radius));
    return best;
}
inline bool visible(float t,float groundT) {
    return std::isfinite(t) && (!std::isfinite(groundT) || t<=groundT+.007f);
}
} // namespace top::screen_pick
