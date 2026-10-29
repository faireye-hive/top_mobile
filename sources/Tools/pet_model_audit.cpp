// Read-only structural inventory of embedded bone animation in effect LGO.
#include "top/LegacyExternalEffectMesh.h"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    std::filesystem::path root=argv[1],path=argv[2];
    std::ofstream out(path);if(!out)return 1;
    out<<"file,raw_loaded,has_skin,geometry_supported,bones,frames,triangles,span_x,span_y,span_z,moving_vertices\n";
    int files=0,skin=0,animated=0;
    for(const auto& entry:std::filesystem::directory_iterator(root/"model/effect")) {
        if(entry.path().extension()!=".lgo")continue;
        top::Model raw,model;bool parsed=raw.loadGeom(entry.path().string());
        bool skinned=parsed && std::any_of(raw.geoms.begin(),raw.geoms.end(),[](const auto& g){return !g.blendIndex.empty();});
        bool supported=top::legacy_external::load(root.string(),entry.path().filename().string(),model);
        int bones=0,frames=0,moved=0;std::size_t triangles=0;
        float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
        if(supported)for(const auto& g:model.geoms) {
            bones=std::max(bones,g.effectBoneCount);frames=std::max(frames,g.effectBoneFrames);
            for(const auto& s:g.subsets){top::legacy_mesh::Primitive a,b;
                if(!top::legacy_external::subsetPrimitive(g,s,0,a) || !top::legacy_external::subsetPrimitive(g,s,.1f,b))continue;
                triangles+=a.triangles.size();
                for(std::size_t i=0;i<a.points.size();++i){bool different=false;for(int c=0;c<3;++c){
                    float v=a.points[i].xyz[c];lo[c]=std::min(lo[c],v);hi[c]=std::max(hi[c],v);
                    different|=std::fabs(v-b.points[i].xyz[c])>1e-5f;}moved+=different;}
            }
        }
        out<<entry.path().filename().string()<<','<<parsed<<','<<skinned<<','<<supported<<','<<bones<<','<<frames<<','<<triangles;
        for(int c=0;c<3;++c){out<<',';if(triangles)out<<hi[c]-lo[c];}
        out<<','<<moved<<'\n';++files;skin+=skinned;animated+=skinned&&supported&&moved>0;
    }
    std::cout<<"LGO files="<<files<<" skinned="<<skin<<" supported_and_moving="<<animated<<" (CPU audit, no visual certification)\n";
}
