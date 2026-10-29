// SK2.19: raw geometry and exact authored-key bounds, not pixel visibility.
#include "top/LegacyEffectInspector.h"
#include "top/LegacyEffectGeometry.h"
#include "top/LegacyExternalEffectMesh.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
namespace fs=std::filesystem;
static std::string quoted(const std::string& s){std::string out="\"";for(char c:s){out+=c;if(c=='\"')out+=c;}return out+'\"';}
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"Usage: top_effect_unit_audit Client output.csv\n";return 2;}
    fs::path root=argv[1],output=argv[2];
    if(!output.parent_path().empty())fs::create_directories(output.parent_path());
    std::ofstream out(output);if(!out)return 1;
    out<<"file,part,key,status,model,billboard,raw_span_x,raw_span_y,raw_span_z,key_span_x,key_span_y,key_span_z,world_units_per_vertex,previous_extent_ratio\n";
    out<<std::setprecision(9);int files=0,parts=0,keys=0,unread=0,unsupported=0;
    for(const auto& entry:fs::directory_iterator(root/"effect")) {
        if(entry.path().extension()!=".eff")continue;
        ++files;top::LegacyEffectSummary summary;
        const auto file=entry.path().filename().string();
        if(!top::inspectLegacyEff(entry.path().string(),summary)||!summary.structured){
            out<<quoted(file)<<",-1,-1,undecoded,,,,,,,,,,\n";++unread;continue;
        }
        for(std::size_t i=0;i<summary.document.parts.size();++i) {
            const auto& part=summary.document.parts[i];++parts;
            top::Model model;const bool external=!part.billboard&&!top::legacy_mesh::recognized(part.model);
            const auto lgo=top::legacy_external::filename(part.model);
            const bool loaded=!external||top::legacy_external::load(root.string(),part.model,model);
            float time=0;
            for(std::size_t k=0;k<part.keys.size();++k) {
                top::legacy_mesh::Primitive primitive;
                bool valid=false;
                if(!external)valid=top::legacy_mesh::make(part.billboard?"RectPlane":part.model,
                    top::legacy_mesh::paramsAt(part,time),primitive);
                else if(loaded) {
                    // Include every subset's positions; bounds are a union.
                    for(const auto& geom:model.geoms)for(const auto& subset:geom.subsets) {
                        top::legacy_mesh::Primitive mesh;
                        if(!top::legacy_external::subsetPrimitive(geom,subset,time,mesh))continue;
                        const auto offset=static_cast<std::uint16_t>(primitive.points.size());
                        if(primitive.points.size()+mesh.points.size()>65535)continue;
                        for(auto tri:mesh.triangles){for(auto& p:tri)p+=offset;primitive.triangles.push_back(tri);}
                        primitive.points.insert(primitive.points.end(),mesh.points.begin(),mesh.points.end());valid=true;
                    }
                }
                out<<quoted(file)<<','<<i<<','<<k<<',';
                if(!valid) {
                    const bool missing=external&&!fs::is_regular_file(root/"model/effect"/lgo);
                    out<<(missing?"missing_lgo":"unsupported_geometry")<<','<<quoted(part.model)<<','<<part.billboard<<",,,,,,,,\n";
                    ++unsupported;time+=std::max(0.f,part.keys[k].time);continue;
                }
                float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
                for(const auto& p:primitive.points)for(int c=0;c<3;++c){lo[c]=std::min(lo[c],p.xyz[c]);hi[c]=std::max(hi[c],p.xyz[c]);}
                std::vector<top::MeshVertex> vertices;
                top::legacy_mesh::emit(primitive,part.keys[k],{},0,0,0,vertices);
                float keyLo[3]={INFINITY,INFINITY,INFINITY},keyHi[3]={-INFINITY,-INFINITY,-INFINITY};
                for(const auto& v:vertices){const float xyz[]={v.x,v.z,v.y};for(int c=0;c<3;++c){keyLo[c]=std::min(keyLo[c],xyz[c]);keyHi[c]=std::max(keyHi[c],xyz[c]);}}
                out<<"decoded_key_bounds,"<<quoted(part.model)<<','<<part.billboard;
                for(int c=0;c<3;++c)out<<','<<hi[c]-lo[c];
                for(int c=0;c<3;++c)out<<','<<keyHi[c]-keyLo[c];
                out<<",1,"<<(part.billboard?.6f:.3f)<<'\n';++keys;
                time+=std::max(0.f,part.keys[k].time);
            }
        }
    }
    std::cout<<"EFF files="<<files<<" parts="<<parts<<" decoded_key_bounds="<<keys
        <<" undecoded_files="<<unread<<" unsupported_or_missing_keys="<<unsupported
        <<"; DX local axes, no parent/camera/emission or visibility certification\n";
}
