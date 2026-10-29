#pragma once
// SK2.17-AD: stateful BLAST3 replay of CMPParticle::GetCurFrame and
// _FrameMoveBlast3. Update cadence remains observable.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace top { namespace historical_blast3_timeline {
using V3=std::array<float,3>;
constexpr float pi=3.14159265358979323846f;

inline constexpr bool enabled() { return true; }

struct Sample {
    bool alive=false;
    float age=0.f;
    float normalized=0.f;
    V3 position{{0.f,0.f,0.f}};
};
struct SystemSample { std::vector<Sample> slots; };

struct Particle {
    bool alive=true;
    unsigned frame=0;
    float frameTime=0.f;
    float frameDuration=0.f;
    V3 position{{0.f,0.f,0.f}};
    V3 velocity{{0.f,0.f,0.f}}; // DX9 m_vOldPos, updated after movement.
};

struct SystemTimeline {
    bool initialized=false;
    bool stopped=false;
    int frameCount=0;
    float life=0.f;
    std::vector<Particle> particles;

    void reset(int count,float authoredLife,int frames,const V3& direction) {
        initialized=true;stopped=false;frameCount=frames;life=authoredLife;
        particles.assign(static_cast<std::size_t>(std::max(0,count)),Particle{});
        const float divisor=count>0?static_cast<float>(count):1.f;
        for(int n=0;n<count;++n) {
            const float angle=2.f*pi*static_cast<float>(n)/divisor;
            // D3DXMatrixRotationYawPitchRoll(0,0,angle) followed by
            // D3DXVec3Transform uses row vectors: (1,1,-1) ->
            // (cos(angle)-sin(angle), sin(angle)+cos(angle), -1).
            auto& p=particles[static_cast<std::size_t>(n)];
            p.velocity={{std::cos(angle)-std::sin(angle)+direction[0],
                         std::sin(angle)+std::cos(angle)+direction[1],
                         -1.f+direction[2]}};
            p.frameDuration=authoredLife/static_cast<float>(frames);
        }
    }

    void advance(int count,float dt,float authoredLife,int frames,float speed,
                 const V3& direction,const V3& acceleration) {
        if(count<1||count>4096||!std::isfinite(dt)||dt<0.f||
           !std::isfinite(authoredLife)||authoredLife<=0.f||frames<1||frames>512||
           !std::isfinite(speed)||
           !std::all_of(direction.begin(),direction.end(),[](float v){return std::isfinite(v);})||
           !std::all_of(acceleration.begin(),acceleration.end(),[](float v){return std::isfinite(v);}))
            return;
        if(!initialized||particles.size()!=static_cast<std::size_t>(count)||
           frameCount!=frames||life!=authoredLife)
            reset(count,authoredLife,frames,direction);
        if(stopped)return;
        for(auto& p:particles) {
            if(!p.alive)continue;
            // GetCurFrame advances at most one frame and discards all excess
            // time. When it reaches frameCount, DX9 returns before movement.
            p.frameTime+=dt;
            if(p.frameTime>=p.frameDuration) {
                ++p.frame;
                p.frameTime=0.f;
                if(p.frame>=static_cast<unsigned>(frames)) {
                    p.frame=static_cast<unsigned>(frames);
                    p.alive=false;
                    stopped=true;
                    break; // _FrameMoveBlast3 returns on the first ended slot.
                }
            }
            const float scaledDt=speed*dt;
            for(std::size_t axis=0;axis<3;++axis) {
                p.position[axis]+=p.velocity[axis]*scaledDt;
                p.velocity[axis]+=acceleration[axis]*dt;
            }
        }
    }

    SystemSample snapshot() const {
        SystemSample result;result.slots.resize(particles.size());
        for(std::size_t i=0;i<particles.size();++i) {
            const auto& p=particles[i];if(!p.alive||stopped)continue;
            const float frameDuration=p.frameDuration>0.f?p.frameDuration:1.f;
            const float normalized=std::clamp((static_cast<float>(p.frame)+
                p.frameTime/frameDuration)/static_cast<float>(std::max(1,frameCount)),0.f,1.f);
            result.slots[i]={true,normalized*life,normalized,p.position};
        }
        return result;
    }
};
}} // namespace top::historical_blast3_timeline
