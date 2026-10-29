// SK2.10: execute the CPU effect pipeline for every declared skill channel.
// No GL context or combat/save mutation. This is NOT a visual fidelity test.
#include "top/EffectSystem.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"Usage: top_skill_fx_probe Client output.csv\n";return 2;}
    const std::string root=argv[1];
    top::EffectSystem fx;if(!fx.open(root)){std::cerr<<"EffectSystem::open failed\n";return 1;}
    std::ifstream table(root+"/scripts/table/skillinfo.txt");
    std::ofstream out(argv[2]);if(!table||!out)return 1;
    out<<"skill_id,declared_channels,events_executed,nonempty_samples,vertices_sampled,nonfinite_vertices,texture_count\n";
    std::string line;int skills=0,errors=0;
    while(std::getline(table,line)) {
        const auto end=line.find('\t');
        if(end==std::string::npos || line.empty() || line[0]<'0'||line[0]>'9')continue;
        int id=0;try{id=std::stoi(line.substr(0,end));}catch(...){continue;}
        const auto* visual=fx.skillVisuals().find(id);if(!visual)continue;
        const int channels=static_cast<int>(visual->channels.size());
        int events=0,samples=0,bad=0;std::size_t vertices=0;std::set<std::string> textures;
        for(auto kind:{top::EffectEvent::Cast,top::EffectEvent::Flight,top::EffectEvent::SkillHit}) {
            bool declared=false;
            for(const auto& c:visual->channels) {
                using K=top::SkillVisualChannelKind;
                declared|=kind==top::EffectEvent::Cast?(c.kind==K::Actor||c.kind==K::Item||c.kind==K::ItemExtra):
                    kind==top::EffectEvent::Flight?c.kind==K::Flight:
                    (c.kind==K::Target||c.kind==K::Ground||c.kind==K::Water);
            }
            if(!declared)continue;
            // Independent channels cannot contaminate subsequent observations.
            top::EffectSystem sampleFx;if(!sampleFx.open(root))return 1;
            top::EffectEvent event{kind,0,0,0,10,id,1.f};event.surface=top::SkillEffectSurface::Water;
            sampleFx.emit(event);++events;
            top::MapFile map;float last=0;
            for(float at:{0.f,.1f,.35f,.7f,1.1f,2.f}) {
                sampleFx.step(at-last);last=at;sampleFx.build(map,top::EffectViewBasis{{{1.f,0.f,0.f}},{{0.f,.70710678118f,-.70710678118f}}});
                bool nonempty=false;
                for(const auto& b:sampleFx.batches()) {
                    if(!b.verts.empty()){nonempty=true;textures.insert(b.texture);}
                    vertices+=b.verts.size();
                    for(const auto& v:b.verts)
                        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||
                           !std::isfinite(v.u)||!std::isfinite(v.v)||!std::isfinite(v.r)||
                           !std::isfinite(v.g)||!std::isfinite(v.b)||!std::isfinite(v.a))++bad;
                }
                samples+=nonempty;
            }
        }
        out<<id<<','<<channels<<','<<events<<','<<samples<<','<<vertices<<','<<bad<<','<<textures.size()<<'\n';
        ++skills;errors+=bad;
    }
    std::cout<<"Skills probed="<<skills<<" nonfinite vertices="<<errors<<" (CPU geometry only; no visual validation)\n";
    return errors?1:0;
}
