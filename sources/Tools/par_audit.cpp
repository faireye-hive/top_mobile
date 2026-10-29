// SK2.3 - Inventory ALL Client/effect/*.par without modifying game assets.
// C++17; no OpenGL, network, or third-party dependencies.
#include "top/LegacyParAudit.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace fs=std::filesystem;
using namespace top::par_audit;
namespace {
std::string lower(std::string text) {
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return text;
}
std::string csv(std::string v) {
    bool quote=v.find_first_of(",\r\n\"")!=std::string::npos;
    if (!quote) return v;
    std::string out="\"";
    for (char c:v){ if(c=='\"')out+='\"';out+=c; }
    return out+'\"';
}
std::string json(std::string s) {
    std::string out="\"";
    for (unsigned char c:s) {
        if(c=='"' || c=='\\') {out+='\\';out+=static_cast<char>(c);}
        else if(c=='\n') out+="\\n";
        else if(c=='\r') out+="\\r";
        else if(c=='\t') out+="\\t";
        else if(c<32){char temp[7];std::snprintf(temp,sizeof(temp),"\\u%04x",c);out+=temp;}
        else out+=static_cast<char>(c);
    }
    return out+'"';
}
std::string num(double x) {std::ostringstream o;o<<std::setprecision(8)<<x;return o.str();}
std::string str(int64_t x){return std::to_string(x);}
void row(std::ostream& o,const std::vector<std::string>& fields) {
    for(std::size_t i=0;i<fields.size();++i){if(i)o<<',';o<<csv(fields[i]);}o<<'\n';
}
bool safeName(std::string name){
    if(name.empty() || name.size()>250 || name.find("..")!=std::string::npos ||
        name.front()=='/' || name.front()=='\\' || name.find(':')!=std::string::npos) return false;
    for(char c:name) if(c=='\\' || c=='/' || static_cast<unsigned char>(c)<32) return false;
    return true;
}
struct Index {
    fs::path directory;
    std::map<std::string,std::string> members;
    explicit Index(fs::path dir):directory(std::move(dir)) {
        std::error_code ec;
        if(!fs::exists(directory,ec))return;
        for(fs::directory_iterator it(directory,fs::directory_options::skip_permission_denied,ec),end;
            it!=end;it.increment(ec)){
            if(ec){ec.clear();continue;}
            if(it->is_regular_file(ec)) {
                std::string name=it->path().filename().string();
                members.emplace(lower(name),it->path().generic_string());
            }
        }
    }
    std::string lookup(const std::string& name) const {
        const auto it=members.find(lower(name));return it==members.end()?"":it->second;
    }
};
struct RefResult {std::string name, resolved, state;};
RefResult resolve(const Index& index,std::string name,bool texture){
    RefResult out; out.name=name;
    if(!safeName(name)) {out.state="invalid_name";return out;}
    std::vector<std::string> options{name};
    const auto dot=name.find_last_of('.');
    if(dot==std::string::npos) options.push_back(name+(texture?".tga":".eff"));
    if(texture){
        // The original file .tga/.dds was converted to .tga.png/.dds.png.
        // A plain .png variant is also legal, without deleting the suffix.
        for(const auto& option: std::vector<std::string>(options)) {
            if(lower(option).size()>=4 && lower(option).substr(lower(option).size()-4)!=".png")
                options.push_back(option+".png");
            if(option.size()>4 && lower(option).substr(lower(option).size()-4)==".tga")
                options.push_back(option.substr(0,option.size()-4)+".png");
        }
    }
    for(const auto& option:options) {
        out.resolved=index.lookup(option);
        if(!out.resolved.empty()){out.state="found";return out;}
    }
    out.state="missing";return out;
}
std::string modelKind(std::string name){
    if(name.empty())return "none";
    name=lower(name);
    if(name.size()>=4&&name.substr(name.size()-4)==".eff")return "eff";
    if(name.size()>=4&&name.substr(name.size()-4)==".lgo")return "lgo";
    return "other";
}
struct Summary {
    uint64_t found=0,parsed=0,failed=0,empty=0,trailing=0,systems=0,missing=0,invalidNames=0;
    std::map<int64_t,uint64_t> versions,types;
    std::map<std::string,uint64_t> failures,refs;
};
void writeSummary(std::ostream& out,const Summary& s,const fs::path& input){
    out<<"{\n  \"schema\": \"SK2.3-v1\",\n  \"client_effect_dir\": "<<json(input.generic_string())<<",\n"
       <<"  \"files_scanned\": "<<s.found<<",\n  \"files_decoded\": "<<s.parsed
       <<",\n  \"files_failed\": "<<s.failed<<",\n  \"empty_controllers\": "<<s.empty
       <<",\n  \"files_with_trailing_bytes\": "<<s.trailing<<",\n  \"systems_decoded\": "<<s.systems
       <<",\n  \"unresolved_references\": "<<s.missing<<",\n  \"unsafe_reference_names\": "<<s.invalidNames<<",\n";
    auto writeMap=[&](const char* name,const auto& map,bool keysNumeric){
        out<<"  "<<json(name)<<": {";
        bool first=true;for(const auto& p:map){if(!first)out<<',';out<<"\n    ";
            if constexpr(std::is_integral_v<std::decay_t<decltype(p.first)>>)out<<json(str(p.first));
            else out<<json(p.first);
            out<<": "<<p.second;first=false;}
        if(!first)out<<'\n';out<<"  },\n";
        (void)keysNumeric;
    };
    writeMap("versions",s.versions,true);writeMap("particle_types",s.types,true);
    writeMap("parse_failures",s.failures,false);
    out<<"  \"notes\": [\"Decoded does not imply visually correct or supported in OpenGL\","
          "\"Controller with zero systems is parsed but rejected by EffectSystem loadParticles\","
          "\"Only dependencies explicitly named in par binary are checked\"]\n}\n";
}
int audit(const fs::path& client,const fs::path& output,bool strict){
    // Never write reports inside the asset tree, even when a user points --out there.
    std::error_code canonicalError;
    const fs::path canonicalClient=fs::weakly_canonical(client,canonicalError);
    if(canonicalError){std::cerr<<"Client path error: "<<canonicalError.message()<<'\n';return 2;}
    const fs::path canonicalOutput=fs::weakly_canonical(output,canonicalError);
    if(canonicalError){std::cerr<<"Output path error: "<<canonicalError.message()<<'\n';return 2;}
    auto c=canonicalClient.begin(), o=canonicalOutput.begin();
    while(c!=canonicalClient.end() && o!=canonicalOutput.end() && *c==*o){++c;++o;}
    if(c==canonicalClient.end()){
        std::cerr<<"Output directory must be outside Client/: "<<output<<'\n';return 2;
    }
    fs::path effect=client/"effect";
    std::error_code ec;
    if(!fs::is_directory(effect,ec)){std::cerr<<"Missing directory: "<<effect<<'\n';return 2;}
    if(!fs::create_directories(output,ec)&&ec){std::cerr<<ec.message()<<'\n';return 2;}
    std::ofstream files(output/"par_files.csv"),systems(output/"par_systems.csv"),deps(output/"par_dependencies.csv"),failures(output/"par_failures.csv"),summary(output/"par_summary.json");
    if(!files||!systems||!deps||!failures||!summary){std::cerr<<"Unable to create output files\n";return 2;}
    row(files,{"file","bytes","status","version","controller","declared_systems","decoded_systems","consumed_bytes","trailing_bytes","error","error_offset"});
    row(systems,{"file","index","name","version","type","count_raw","frames","texture_name","model_name","model_kind","billboard","model_range","model_range_name","mediay_eff","life","velocity","step","delay","play","has_path","path_points","path_velocity","range_x","range_y","range_z","offset_x","offset_y","offset_z","size_start","size_end","alpha_start","alpha_end","runtime_visual_status","model_direction"});
    row(deps,{"file","index","kind","declared_name","resolved_path","status"});
    row(failures,{"file","reason","offset","bytes"});
    Index textures(client/"texture"/"effect"), effects(effect), models(client/"model"/"effect");
    std::vector<fs::path> paths;
    for(fs::recursive_directory_iterator it(effect,fs::directory_options::skip_permission_denied,ec),end;
        it!=end;it.increment(ec)){
        if(ec){std::cerr<<"Directory read warning: "<<ec.message()<<'\n';ec.clear();continue;}
        if(it->is_regular_file(ec) && lower(it->path().extension().string())==".par")
            paths.push_back(it->path());
    }
    std::sort(paths.begin(),paths.end());
    Summary stats;
    auto dep=[&](const std::string& file,int idx,const std::string& kind,
                 const std::string& name,const RefResult& found){
        row(deps,{file,str(idx),kind,name,found.resolved,found.state});
        if(found.state=="missing")++stats.missing;
        if(found.state=="invalid_name")++stats.invalidNames;
    };
    for(const auto& path: paths){
        ++stats.found;
        const auto rel=fs::relative(path,client,ec).generic_string();
        const auto file=ec?path.generic_string():rel;ec.clear();
        const Result r=inspectFile(path.string());
        if(!r.ok){
            ++stats.failed; ++stats.failures[r.error];
            row(files,{file,str(r.document.bytes),"decode_failed",str(r.document.version),r.document.controller,str(r.document.declaredSystems),"0","0","",r.error,str(r.errorOffset)});
            row(failures,{file,r.error,str(r.errorOffset),str(r.document.bytes)});
            continue;
        }
        ++stats.parsed; ++stats.versions[r.document.version];
        const auto& d=r.document;
        if(d.systems.empty())++stats.empty;
        if(d.consumed!=d.bytes)++stats.trailing;
        row(files,{file,str(d.bytes),d.systems.empty()?"empty_controller":"decoded",str(d.version),d.controller,str(d.declaredSystems),str(d.systems.size()),str(d.consumed),str(d.bytes-d.consumed),"",""});
        for(std::size_t i=0;i<d.systems.size();++i){
            const auto& s=d.systems[i];++stats.systems;++stats.types[s.type];
            const auto idx=static_cast<int>(i);
            const auto firstSize=s.sizes.empty()?"":num(s.sizes.front());
            const auto lastSize=s.sizes.empty()?"":num(s.sizes.back());
            const auto firstAlpha=s.colors.size()<4?"":num(s.colors[3]);
            const auto lastAlpha=s.colors.size()<4?"":num(s.colors[s.colors.size()-1]);
            row(systems,{file,str(idx),s.name,str(d.version),str(s.type),str(s.count),str(s.frames),s.texture,s.model,modelKind(s.model),s.billboard?"1":"0",s.modelRange?"1":"0",s.modelRangeName,s.mediayEff?"1":"0",num(s.life),num(s.velocity),num(s.step),num(s.delay),num(s.play),s.hasPath?"1":"0",str(s.pathPoints),num(s.pathVelocity),num(s.range[0]),num(s.range[1]),num(s.range[2]),num(s.offset[0]),num(s.offset[1]),num(s.offset[2]),firstSize,lastSize,firstAlpha,lastAlpha,"not_visual_tested",s.modelDirection?"1":"0"});
            if(!s.texture.empty())dep(file,idx,"texture",s.texture,resolve(textures,s.texture,true));
            if(!s.model.empty()){
                auto kind=modelKind(s.model);
                if(kind=="eff")dep(file,idx,"model_eff",s.model,resolve(effects,s.model,false));
                else if(kind=="lgo")dep(file,idx,"model_lgo",s.model,resolve(models,s.model,false));
                else dep(file,idx,"model_unclassified",s.model,{s.model,"","unclassified_not_missing"});
            }
            if(!s.modelRangeName.empty())dep(file,idx,"model_range_name",s.modelRangeName,{s.modelRangeName,"","unclassified_not_missing"});
        }
    }
    writeSummary(summary,stats,effect);
    std::cout<<"SK2.3 .par audited: "<<stats.found<<" | decoded="<<stats.parsed
             <<" | failed="<<stats.failed<<" | systems="<<stats.systems
             <<" | missing references="<<stats.missing<<" | output="<<output<<'\n';
    return (strict && (stats.failed || stats.missing || stats.invalidNames))?1:0;
}
}
int main(int argc,char** argv){
    fs::path client="Client",output="build/sk23-par-audit";
    bool strict=false;
    for(int i=1;i<argc;++i){
        std::string arg=argv[i];
        if(arg=="--client" && i+1<argc)client=argv[++i];
        else if(arg=="--out" && i+1<argc)output=argv[++i];
        else if(arg=="--strict")strict=true;
        else if(arg=="--help"||arg=="-h"){
            std::cout<<"Usage: top_par_audit [--client Client] [--out build/sk23-par-audit] [--strict]\n"
                "Scans all Client/effect/**/*.par. Default return code 0 even when individual files fail to decode; --strict returns 1.\n";
            return 0;
        } else {std::cerr<<"Unknown/incomplete argument: "<<arg<<'\n';return 2;}
    }
    return audit(client,output,strict);
}
