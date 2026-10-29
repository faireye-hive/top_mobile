#pragma once

// DX9 EffectObj.cpp::Part_trace, translated from row-vector Z-up coordinates
// to the GL world's X/height/Z coordinates.
#include "top/HistoricalMagicPath.h"
#include "top/MapFile.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace top::historical_part_trace {

inline constexpr float kTargetRange = 1.f;
inline constexpr float kMaxStep = 1.5f;

inline float lengthSquared(historic_magic::Vec3 value) {
    return value.x*value.x + value.y*value.y + value.z*value.z;
}

inline bool inPlanarRange(historic_magic::Vec3 a, historic_magic::Vec3 b) {
    // DX9 PointInstrPointRange compares X/Y only. GL's planar pair is X/Z.
    return std::fabs(a.x-b.x) < kTargetRange &&
           std::fabs(a.z-b.z) < kTargetRange;
}

struct Sample { float age=0.f; historic_magic::Vec3 position{}; };

struct Timeline {
    bool initialized=false;
    bool stopped=false;
    float age=0.f;
    float startDistance=0.f;
    float distanceSquared=0.f;
    historic_magic::Vec3 position{};
    historic_magic::Vec3 oldTarget{};
    historic_magic::Vec3 direction{};
    // Part_trace calls Render before moving. This is the position rendered
    // for the current GL frame after the simulation step has advanced.
    float renderAge=0.f;
    historic_magic::Vec3 renderPosition{};
    historic_magic::Vec3 renderDirection{};
    std::vector<Sample> samples;

    void reset(const historic_magic::Path& path) {
        initialized=true;
        stopped=false;
        age=0.f;
        position=path.from;
        renderPosition=position;
        oldTarget=path.to;
        const auto delta=oldTarget-position;
        startDistance=std::sqrt(lengthSquared(delta));
        distanceSquared=lengthSquared(delta);
        direction=historic_magic::unit(delta);
        renderAge=0.f;
        renderDirection=direction;
        samples.clear();
        samples.push_back({0.f,position});
    }

    bool advance(float dt, historic_magic::Vec3 currentTarget,
                 const MapFile* map, float speed) {
        if (!initialized || stopped || !std::isfinite(dt) || dt<0.f ||
            !std::isfinite(speed) || speed<0.f) return stopped;
        renderAge=age;
        renderPosition=position;
        renderDirection=direction;
        const float nextAge=age+dt;
        if (startDistance<kTargetRange) {
            position=oldTarget;
            stopped=true;
        } else {
            float step=speed*dt;
            if (step*step>distanceSquared) {
                position=oldTarget;
                stopped=true;
            } else {
                step=std::min(step,kMaxStep);
                position=position+direction*step;
                if (inPlanarRange(position,oldTarget)) {
                    stopped=true;
                } else if (map && map->ok() &&
                           position.y<map->sceneGridHeightAt(position.x,position.z)) {
                    stopped=true;
                } else if (!inPlanarRange(currentTarget,oldTarget)) {
                    const auto targetDelta=currentTarget-oldTarget;
                    const float targetDistanceSquared=lengthSquared(targetDelta);
                    const auto targetDirection=historic_magic::unit(targetDelta);
                    const float correction=distanceSquared>1e-12f
                        ? (targetDistanceSquared/distanceSquared)*step : 0.f;
                    position=position+targetDirection*correction;
                    oldTarget=currentTarget;
                    const auto toTarget=oldTarget-position;
                    direction=historic_magic::unit(toTarget);
                    distanceSquared=lengthSquared(toTarget);
                }
            }
        }
        age=nextAge;
        samples.push_back({age,position});
        return stopped;
    }

    historic_magic::Vec3 positionAt(float sampleAge) const {
        if (samples.empty()) return position;
        if (sampleAge<=samples.front().age) return samples.front().position;
        if (sampleAge>=samples.back().age) return samples.back().position;
        const auto upper=std::upper_bound(samples.begin(),samples.end(),sampleAge,
            [](float value,const Sample& sample){return value<sample.age;});
        const auto lower=upper-1;
        const float span=upper->age-lower->age;
        const float t=span>0.f?(sampleAge-lower->age)/span:1.f;
        return lower->position+(upper->position-lower->position)*t;
    }
};

} // namespace top::historical_part_trace
