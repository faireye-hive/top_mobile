#pragma once
// SK2.14 read-only coverage inventory. Presence of a C++ branch never proves
// DX9 equivalence. Human assessments are explicit, with drift checks against
// the *current* renderer and separate columns for evidence and validation.
#include "top/BlendAudit.h"
#include "top/LegacyEffectGeometry.h"
#include <functional>
#include <iomanip>
#include <numeric>
#include <regex>
#include <stdexcept>

namespace top { namespace fx_coverage {
namespace fs = std::filesystem;
namespace ba = blend_audit;

struct TableRow {
    std::string kind,key,name,assessment,expected,token,note;
};
struct SourceEvidence {
    std::vector<int> outside,inside;
};
struct Source {
    std::vector<std::string> lines;
    int loaderBegin=0,loaderEnd=0;
    bool valid=false;
    SourceEvidence find(const std::regex& regex) const {
        SourceEvidence result;
        for(std::size_t i=1;i<lines.size();++i){
            if(!std::regex_search(lines[i],regex))continue;
            (loaderBegin && int(i)>=loaderBegin && int(i)<=loaderEnd ? result.inside : result.outside).push_back(int(i));
        }
        return result;
    }
};
struct Dx9EffCatalogEvidence {
    bool sourceAvailable=false,bodyFound=false;
    int begin=0,end=0,wildcardLine=0,enumeratorLine=0,loadLine=0,indexLine=0;
    bool verified() const {
        return sourceAvailable&&bodyFound&&wildcardLine&&enumeratorLine&&loadLine&&indexLine;
    }
};
struct HitEffectHandlerEvidence {
    std::string constant, function;
    int type=-1, dispatchLine=0, glType=-1;
    std::vector<int> dx9CallLines, glLines;
    std::string glGate;
    bool dx9Dispatch=false, glPath=false;
};
struct HitEffectDispatchEvidence {
    bool sourceAvailable=false, allDx9SitesAccounted=false;
    std::vector<int> allDx9CallLines;
    std::vector<HitEffectHandlerEvidence> handlers;
    bool dx9Verified() const {
        return sourceAvailable && allDx9SitesAccounted && handlers.size()==2 &&
            std::all_of(handlers.begin(),handlers.end(),[](const auto& h){return h.dx9Dispatch;});
    }
    bool verified() const {
        return dx9Verified() && std::all_of(handlers.begin(),handlers.end(),[](const auto& h){
            return h.glPath && h.type==h.glType;
        });
    }
    const HitEffectHandlerEvidence* forType(int type) const {
        for(const auto& h:handlers)if(h.type==type&&h.dx9Dispatch)return &h;
        return nullptr;
    }
};
struct ItemEffectDummyEvidence {
    bool dx9FunctionFound=false,dx9DummyMinusOneGuard=false,dx9SkillDummyCallsite=false;
    bool glEmitFound=false,glOptInCall=false,glDummyMinusOneFilter=false;
    int dx9GuardLine=0,glOptInLine=0,glFilterLine=0;
    std::vector<int> dx9SkillDummyLines;
    bool verified() const {
        return dx9FunctionFound&&dx9DummyMinusOneGuard&&dx9SkillDummyCallsite&&
            dx9SkillDummyLines.size()>=2&&
            glEmitFound&&glOptInCall&&glDummyMinusOneFilter;
    }
};
inline std::string trim(std::string v){
    const auto a=v.find_first_not_of(" \r\t\n"),b=v.find_last_not_of(" \r\t\n");
    return a==std::string::npos?"":v.substr(a,b-a+1);
}
inline std::string stripComment(const std::string& src){
    bool quoted=false; bool escaped=false;
    for(std::size_t i=0;i<src.size();++i){
        if(src[i]=='"'&&!escaped)quoted=!quoted;
        if(!quoted&&src[i]=='/'&&i+1<src.size()&&src[i+1]=='/')return src.substr(0,i);
        if(src[i]=='\\'&&!escaped)escaped=true; else escaped=false;
    }
    return src;
}
inline Source readSource(const fs::path& file){
    Source src; std::ifstream in(file);
    if(!in)return src;
    src.lines.push_back("");
    std::string line;
    while(std::getline(in,line))src.lines.push_back(stripComment(line));
    // Find the function body rather than assuming a fixed line number.
    for(std::size_t i=1;i<src.lines.size();++i){
        if(src.lines[i].find("bool loadParticles(")==std::string::npos)continue;
        int depth=0; bool opened=false;
        for(std::size_t j=i;j<src.lines.size();++j){
            for(char c:src.lines[j]){
                if(c=='{'){++depth;opened=true;}
                else if(c=='}'&&opened)--depth;
            }
            if(opened&&depth==0){src.loaderBegin=int(i);src.loaderEnd=int(j);break;}
        }
        break;
    }
    src.valid=!src.lines.empty() && src.loaderBegin!=0;
    return src;
}
inline Dx9EffCatalogEvidence readDx9EffCatalog(const fs::path& file){
    Dx9EffCatalogEvidence result;
    std::ifstream in(file);
    if(!in)return result;
    result.sourceAvailable=true;
    std::vector<std::string> lines(1);
    std::string line;
    while(std::getline(in,line))lines.push_back(stripComment(line));
    for(std::size_t i=1;i<lines.size();++i){
        if(lines[i].find("CMPResManger::LoadTotalEffect")==std::string::npos)continue;
        int depth=0;bool opened=false;
        for(std::size_t j=i;j<lines.size();++j){
            for(char c:lines[j]){
                if(c=='{'){++depth;opened=true;}
                else if(c=='}'&&opened)--depth;
            }
            if(opened&&depth==0){result.begin=int(i);result.end=int(j);break;}
        }
        break;
    }
    if(!result.begin)return result;
    result.bodyFound=true;
    for(int i=result.begin;i<=result.end;++i){
        const auto& bodyLine=lines[static_cast<std::size_t>(i)];
        if(!result.wildcardLine&&bodyLine.find("*.eff")!=std::string::npos)result.wildcardLine=i;
        if(!result.enumeratorLine&&bodyLine.find("FindFirstFile")!=std::string::npos&&
           bodyLine.find("FindNextFile")==std::string::npos)result.enumeratorLine=i;
        if(!result.loadLine&&bodyLine.find("LoadEffectFromFile")!=std::string::npos)result.loadLine=i;
        if(!result.indexLine&&bodyLine.find("_mapEffect[")!=std::string::npos)result.indexLine=i;
        if(bodyLine.find("FindNextFile")!=std::string::npos&&!result.enumeratorLine)
            result.enumeratorLine=i;
    }
    return result;
}
inline std::vector<std::string> readSourceLines(const fs::path& file){
    std::ifstream in(file);std::vector<std::string> lines(1);std::string line;
    while(std::getline(in,line))lines.push_back(stripComment(line));
    return lines;
}
inline bool sourceFunctionBlock(const std::vector<std::string>& lines,
        const std::string& signature,int& begin,int& end){
    for(std::size_t i=1;i<lines.size();++i){
        if(lines[i].find(signature)==std::string::npos)continue;
        int depth=0;bool opened=false;
        for(std::size_t j=i;j<lines.size();++j){
            for(char c:lines[j]){
                if(c=='{'){++depth;opened=true;}
                else if(c=='}'&&opened)--depth;
            }
            if(opened&&depth==0){begin=static_cast<int>(i);end=static_cast<int>(j);return true;}
        }
        return false;
    }
    return false;
}
inline std::vector<int> sourceTokenLines(const std::vector<std::string>& lines,
        const std::string& token,int begin=1,int end=-1){
    std::vector<int> result;
    if(end<0)end=static_cast<int>(lines.size())-1;
    for(int i=std::max(1,begin);i<=end&&static_cast<std::size_t>(i)<lines.size();++i)
        if(lines[static_cast<std::size_t>(i)].find(token)!=std::string::npos)result.push_back(i);
    return result;
}
inline int sourceDefineValue(const std::vector<std::string>& lines,const std::string& name){
    const std::regex define("^\\s*#define\\s+"+name+"\\s+([0-9]+)\\b");
    std::smatch match;
    for(std::size_t i=1;i<lines.size();++i)
        if(std::regex_search(lines[i],match,define))return std::stoi(match[1].str());
    return -1;
}
inline std::string sourceDefineName(const std::vector<std::string>& lines,int value){
    const std::regex define("^\\s*#define\\s+PARTTICLE_([A-Z0-9_]+)\\s+([0-9]+)\\b");
    std::smatch match;
    for(std::size_t i=1;i<lines.size();++i)
        if(std::regex_search(lines[i],match,define)&&std::stoi(match[2].str())==value)
            return match[1].str();
    return "unknown";
}
inline HitEffectDispatchEvidence readHitEffectDispatch(const fs::path& dx9Source,
        const fs::path& dx9Header,const fs::path& glSource){
    HitEffectDispatchEvidence out;
    const auto dx9=readSourceLines(dx9Source),header=readSourceLines(dx9Header),
                gl=readSourceLines(glSource);
    if(dx9.size()<2||header.size()<2||gl.size()<2)return out;
    out.sourceAvailable=true;
    out.allDx9CallLines=sourceTokenLines(dx9,"SendResMessage(");
    std::set<int> accounted;
    struct HandlerSpec {const char* constant;const char* function;const char* gate;};
    const HandlerSpec specs[]={{"PARTTICLE_FIRE","_FrameMoveFire","historical_fire_hit::enabled()"},
        {"PARTTICLE_RANGE2","_FrameMoveRange2","historical_range2_collision::enabled()"}};
    for(const auto& spec:specs){
        HitEffectHandlerEvidence handler;handler.constant=spec.constant;
        handler.function=spec.function;handler.type=sourceDefineValue(header,spec.constant);
        int begin=0,end=0;
        if(sourceFunctionBlock(dx9,std::string(spec.function)+"(",begin,end)){
            for(int line=begin;line<=end;++line){
                const auto& text=dx9[static_cast<std::size_t>(line)];
                if(text.find("SendResMessage(")!=std::string::npos&&
                   text.find("m_strHitEff")!=std::string::npos){
                    handler.dx9CallLines.push_back(line);accounted.insert(line);
                }
            }
        }
        const std::regex dispatch("FrameUpdate\\s*=\\s*"+std::string(spec.function)+"\\s*;");
        for(std::size_t i=1;i<dx9.size();++i)
            if(std::regex_search(dx9[i],dispatch)){handler.dispatchLine=static_cast<int>(i);break;}
        handler.dx9Dispatch=handler.type>=0&&handler.dispatchLine>0&&
            !handler.dx9CallLines.empty();
        const auto gate=sourceTokenLines(gl,spec.gate);
        handler.glLines.insert(handler.glLines.end(),gate.begin(),gate.end());
        const bool isFire=handler.constant=="PARTTICLE_FIRE";
        const std::regex typeCheck(isFire?"profile\\.type\\s*!=\\s*([0-9]+)":
            "profile\\.type\\s*==\\s*"+std::to_string(handler.type)+"\\b");
        for(int gateLine:gate){
            const int scanBegin=isFire?gateLine:std::max(1,gateLine-2);
            const int scanEnd=isFire?std::min(static_cast<int>(gl.size())-1,gateLine+64):
                std::min(static_cast<int>(gl.size())-1,gateLine+2);
            for(int line=scanBegin;line<=scanEnd;++line){
                std::smatch match;
                if(std::regex_search(gl[static_cast<std::size_t>(line)],match,typeCheck)){
                    handler.glType=isFire?std::stoi(match[1].str()):handler.type;
                    handler.glLines.push_back(line);
                    break;
                }
            }
            if(handler.glType>=0)break;
        }
        const std::string trigger=isFire?"historical_fire_hit::at(":"system.hits";
        auto triggerLines=sourceTokenLines(gl,trigger);
        if(!isFire){
            const auto frameHits=sourceTokenLines(gl,"frameResult.hits");
            triggerLines.insert(triggerLines.end(),frameHits.begin(),frameHits.end());
        }
        handler.glLines.insert(handler.glLines.end(),triggerLines.begin(),triggerLines.end());
        handler.glPath=!gate.empty()&&handler.glType>=0&&!triggerLines.empty()&&
            handler.type==handler.glType;
        handler.glGate="normal_engine";
        std::sort(handler.glLines.begin(),handler.glLines.end());
        handler.glLines.erase(std::unique(handler.glLines.begin(),handler.glLines.end()),
            handler.glLines.end());
        out.handlers.push_back(std::move(handler));
    }
    out.allDx9SitesAccounted=accounted.size()==out.allDx9CallLines.size()&&
        std::all_of(out.allDx9CallLines.begin(),out.allDx9CallLines.end(),
            [&](int line){return accounted.count(line)!=0&&
                dx9[static_cast<std::size_t>(line)].find("m_strHitEff")!=std::string::npos;});
    return out;
}
inline ItemEffectDummyEvidence readItemEffectDummyEvidence(const fs::path& dx9Character,
        const fs::path& dx9SkillAttack,const fs::path& glSource){
    ItemEffectDummyEvidence out;
    const auto character=readSourceLines(dx9Character),attack=readSourceLines(dx9SkillAttack),
                gl=readSourceLines(glSource);
    int begin=0,end=0;
    if(sourceFunctionBlock(character,"CCharacter::ItemEffect(",begin,end)){
        out.dx9FunctionFound=true;
        const std::regex dummyGuard("nItemDummy\\s*==\\s*-1\\b");
        for(int line=begin;line<=end;++line){
            if(std::regex_search(character[static_cast<std::size_t>(line)],dummyGuard)){
                out.dx9DummyMinusOneGuard=true;out.dx9GuardLine=line;break;
            }
        }
    }
    for(std::size_t line=1;line<attack.size();++line){
        if(attack[line].find("ItemEffect(")!=std::string::npos&&
           attack[line].find("sItemDummyLink")!=std::string::npos){
            out.dx9SkillDummyCallsite=true;out.dx9SkillDummyLines.push_back(static_cast<int>(line));
        }
    }
    if(sourceFunctionBlock(gl,"void EffectSystem::emit(",begin,end)){
        out.glEmitFound=true;
        const std::regex itemRequest("requested\\.item\\b");
        const std::regex dummyFilter("requested\\.dummy\\s*==\\s*-1\\b");
        for(int line=begin;line<=end;++line){
            const auto& text=gl[static_cast<std::size_t>(line)];
            if(text.find("historicalItemEffectRequireDummyEnabled()")!=std::string::npos){
                out.glOptInCall=true;out.glOptInLine=line;
            }
            if(std::regex_search(text,itemRequest)){
                const int last=std::min(end,line+3);
                for(int filterLine=line;filterLine<=last;++filterLine){
                    if(std::regex_search(gl[static_cast<std::size_t>(filterLine)],dummyFilter)){
                        out.glDummyMinusOneFilter=true;out.glFilterLine=filterLine;break;
                    }
                }
            }
        }
    }
    return out;
}
inline SourceEvidence findType(const Source& src,int type){
    return src.find(std::regex("\\b(?:profile\\.type|s\\.type)\\s*==\\s*"+std::to_string(type)+"\\b"));
}
inline SourceEvidence findToken(const Source& src,const std::string& token){
    if(token.empty())return {};
    std::string escaped;
    for(char c:token){if(c=='.')escaped+="\\.";else escaped+=c;}
    return src.find(std::regex(escaped+"\\b"));
}
inline std::string lineList(const std::vector<int>& v){
    std::string out;
    for(std::size_t i=0;i<v.size()&&i<8;++i){if(i)out+=' ';out+=std::to_string(v[i]);}
    if(v.size()>8)out+=" ...";
    return out;
}
inline std::string baseName(const std::string& value){
    return ba::lower(fs::path(value).filename().string());
}
inline std::string normalizedBase(std::string value){
    value=baseName(value);
    auto dot=value.find('.');
    auto stem=value.substr(0,dot);
    if(!stem.empty()&&stem.size()<8&&stem.find_first_not_of("0123456789")==std::string::npos)
        value=std::string(8-stem.size(),'0')+value;
    return value;
}
inline std::vector<std::string> possibleNames(const std::string& raw){
    std::vector<std::string> names;
    const auto name=normalizedBase(raw);
    if(name.empty())return names;
    names.push_back(name);
    if(name.find('.')==std::string::npos){names.push_back(name+".eff");names.push_back(name+".par");}
    return names;
}
inline bool readTable(const fs::path& path,std::vector<TableRow>& rows,std::string& error){
    std::ifstream in(path);
    if(!in){error="coverage table missing: "+path.string();return false;}
    std::string line;
    if(!std::getline(in,line)||trim(line)!="kind,key,name,assessment,expected_branch,source_token,note"){
        error="bad coverage table header";return false;
    }
    std::set<std::string> unique;
    while(std::getline(in,line)){
        if(trim(line).empty())continue;
        auto f=ba::splitCsv(line);
        if(f.size()!=7||!(f[0]=="type"||f[0]=="feature")||f[1].empty()||!unique.insert(f[0]+":"+f[1]).second){
            error="malformed or duplicate coverage table row";return false;
        }
        rows.push_back({f[0],f[1],f[2],f[3],f[4],f[5],f[6]});
    }
    return true;
}
// Skip texture/model lookup rows from AUDITORIA.md: those files are not
// .par/.eff roots. Reporting them as missing effects would inflate failures.
inline bool loadEffectDeps(const fs::path& path,std::vector<ba::SkillDep>& out,
                           std::size_t& skipped){
    std::ifstream in(path);if(!in)return false;
    std::string line;if(!std::getline(in,line))return false;
    const auto h=ba::splitCsv(line);
    auto column=[&](const std::string& name)->int{
        for(std::size_t i=0;i<h.size();++i)if(h[i]==name)return int(i);
        return -1;
    };
    const int id=column("skill_id"),name=column("skill_name"),ch=column("channel"),
              ref=column("referenced_name"),kind=column("dependency_type");
    if(id<0||ref<0)return false;
    const std::set<std::string> accepted={"scene_par","par_status","eff","par"};
    while(std::getline(in,line)){
        auto f=ba::splitCsv(line);
        if(f.size()<=std::size_t(std::max(id,ref)))continue;
        if(kind>=0 && (f.size()<=std::size_t(kind) || !accepted.count(f[kind]))){++skipped;continue;}
        out.push_back({f[id],name>=0&&std::size_t(name)<f.size()?f[name]:"",
            ch>=0&&std::size_t(ch)<f.size()?f[ch]:"",ba::lower(f[ref])});
    }
    return true;
}
inline const std::vector<std::pair<std::string,std::function<bool(const par_audit::System&)>>>& features(){
    static const std::vector<std::pair<std::string,std::function<bool(const par_audit::System&)>>> v = {
        {"billboard",[](const auto& s){return s.billboard;}},
        {"model",[](const auto& s){return !s.model.empty() && s.model!="0";}},
        {"path",[](const auto& s){return s.hasPath;}},
        {"delay",[](const auto& s){return s.delay>0;}},
        {"play",[](const auto& s){return s.play>0;}},
        {"offset",[](const auto& s){return std::any_of(s.offset.begin(),s.offset.end(),[](float x){return x!=0;});}},
        {"step",[](const auto& s){return s.step>0;}},
        {"snow_fire_step",[](const auto& s){return (s.type==1||s.type==2)&&s.step>0;}},
        {"snow_fire_random_acceleration",[](const auto& s){return (s.type==1||s.type==2)&&std::any_of(
            s.acceleration.begin(),s.acceleration.end(),[](float x){return x!=0;});}},
        {"snow_fire_shared_nested_eff_timeline",[](const auto& s){
            const auto model=ba::lower(s.model);
            return (s.type==1||s.type==2) && !s.mediayEff &&
                model.size()>4 && model.substr(model.size()-4)==".eff";
        }},
        {"wind_world_rectplane_rotation",[](const auto& s){
            return s.type==7 && !s.billboard && ba::lower(s.model)=="rectplane";
        }},
        {"strip_nested_eff_frame_step",[](const auto& s){
            const auto model=ba::lower(s.model);
            return s.type==6 && model.size()>4 && model.substr(model.size()-4)==".eff";
        }},
        {"blast_discrete_acceleration",[](const auto& s){return s.type==3&&std::any_of(
            s.acceleration.begin(),s.acceleration.end(),[](float x){return x!=0;});}},
        {"blast_group_cycle",[](const auto& s){return s.type==3;}},
        {"blast3_frame_timeline",[](const auto& s){return s.type==11&&s.frames>1;}},
        {"shrink_pool_recycle",[](const auto& s){return s.type==12;}},
        {"blast3_nested_eff_region_clock",[](const auto& s){
            const auto model=ba::lower(s.model);
            return s.type==11&&model.size()>4&&model.substr(model.size()-4)==".eff";
        }},
        {"shade_terrain_grid_height",[](const auto& s){return s.type==13;}},
        {"model_frame_step",[](const auto& s){return s.type==5&&s.frames>1;}},
        {"model_delayed_frame_step",[](const auto& s){return s.type==5&&s.frames>1&&s.delay>0.f;}},
        {"model_midpoint_position",[](const auto& s){return s.type==5&&std::any_of(
            s.range.begin(),s.range.end(),[](float x){return x!=0;});}},
        {"model_nested_eff_frame_step",[](const auto& s){
            const auto model=ba::lower(s.model);
            return s.type==5&&model.size()>4&&model.substr(model.size()-4)==".eff";
        }},
        {"model_controller_lifetime",[](const auto& s){
            return (s.type==5||s.type==6)&&s.play>0.f;
        }},
        {"arraw_nested_eff_frame_step",[](const auto& s){
            const auto model=ba::lower(s.model);
            return s.type==8&&model.size()>4&&model.substr(model.size()-4)==".eff";
        }},
        {"ripple_emission_recycle",[](const auto& s){return s.type==4&&s.step>0;}},
        {"ripple_nested_eff_frame_step",[](const auto& s){
            const auto model=ba::lower(s.model);
            return s.type==4&&model.size()>4&&model.substr(model.size()-4)==".eff";
        }},
        {"range2_frame_update_clock",[](const auto& s){return s.type==15;}},
        {"round_frame_update_clock",[](const auto& s){return s.type==9;}},
        {"round_nested_eff_frame_step",[](const auto& s){
            const auto model=ba::lower(s.model);
            return s.type==9&&model.size()>4&&model.substr(model.size()-4)==".eff";
        }},
        {"direction",[](const auto& s){return std::any_of(s.direction.begin(),s.direction.end(),[](float x){return x!=0;});}},
        {"acceleration",[](const auto& s){return std::any_of(s.acceleration.begin(),s.acceleration.end(),[](float x){return x!=0;});}},
        {"range",[](const auto& s){return std::any_of(s.range.begin(),s.range.end(),[](float x){return x!=0;});}},
        {"path_velocity",[](const auto& s){return s.hasPath && s.pathVelocity!=0;}},
        {"curves",[](const auto& s){return s.frames>1;}},
        {"model_range",[](const auto& s){return s.modelRange;}},
        {"shade_type_mismatch",[](const auto& s){
            return s.hasShadeFlag && s.shade!=(s.type==13);
        }},
        {"hit_effect",[](const auto& s){return !s.hitEffect.empty();}},
        {"count_over_24",[](const auto& s){return (s.type==1||s.type==2)&&s.count>24;}},
        {"filters",[](const auto& s){return s.minFilter!=0||s.magFilter!=0;}}
    };
    return v;
}
struct Instance {std::string file,id;};
struct Item {
    std::string kind,key,name,assessment,note,evidence,validation="not_visually_verified";
    std::size_t count=0;
    std::set<std::string> files,skills;
};
struct StrictEffFailure {
    std::string file;
    bool diagnosticDecoded=false;
    std::size_t parts=0,nonFiniteBaseParams=0,nonFiniteAnimatedParams=0;
    std::set<std::string> models;
    std::vector<std::string> modelParamDetails;
};
struct EffPartCount {
    std::string file;
    std::size_t parts=0,billboards=0,native=0,external=0;
    bool strictDecoded=false;
};
struct Options {
    fs::path client="Client",output="build/fx-coverage";
    fs::path deps="sources/auditoria-historica/dependencies.csv";
    fs::path table="sources/auditoria-historica/fx_port_coverage.csv";
    fs::path source="sources/Engine/src/top/EffectSystem.cpp";
    fs::path dx9Source="arquivo/fontes/Engine/src/MPResManger.cpp";
    fs::path dx9ParticleSource="arquivo/fontes/Engine/src/MPParticleSys.cpp";
    fs::path dx9ParticleHeader="arquivo/fontes/Engine/include/MPParticleSys.h";
    fs::path dx9CharacterSource="arquivo/fontes/Client/src/Character.cpp";
    fs::path dx9SkillAttackSource="arquivo/fontes/Client/src/STAttack.cpp";
    fs::path directStrips; // optional inventory generated from DX9 controller tail
    bool strict=false;
};
inline std::string classification(const std::string& model){
    if(legacy_mesh::recognized(model))return "native_primitive";
    auto b=ba::lower(model);
    if(b.size()>4 && b.substr(b.size()-4)==".lgo")return "external_lgo";
    if(b.empty()||b=="0")return "empty";
    return "unrecognized_model";
}
inline int run(const Options& opt,std::ostream& log){
    if(!fs::is_directory(opt.client/"effect")){log<<"Missing Client/effect directory\n";return 2;}
    if(!ba::outsideClient(opt.client,opt.output)){log<<"Refusing to write inside Client\n";return 2;}
    std::vector<TableRow> rules;std::string error;
    if(!readTable(opt.table,rules,error)){log<<error<<'\n';return 2;}
    auto renderer=readSource(opt.source);
    if(!renderer.valid){log<<"Cannot inspect loadParticles in: "<<opt.source<<'\n';return 2;}
    const auto dx9EffCatalog=readDx9EffCatalog(opt.dx9Source);
    const auto hitEffectDispatch=readHitEffectDispatch(
        opt.dx9ParticleSource,opt.dx9ParticleHeader,opt.source);
    const auto itemEffectDummy=readItemEffectDummyEvidence(
        opt.dx9CharacterSource,opt.dx9SkillAttackSource,opt.source);
    const auto dx9ParticleHeader=readSourceLines(opt.dx9ParticleHeader);
    if(!opt.directStrips.empty()&&!fs::is_regular_file(opt.directStrips)){
        log<<"Missing direct-strip inventory: "<<opt.directStrips<<'\n';return 2;
    }
    std::error_code ec;
    fs::create_directories(opt.output,ec);if(ec){log<<"Cannot create output: "<<ec.message()<<'\n';return 2;}
    std::map<std::string,Item> items;
    // basename => files (collisions retained and reported, not silently merged).
    std::map<std::string,std::set<std::string>> basenameFiles;
    std::map<std::string,std::set<std::string>> fileItems,fileLinks,fileSkills;
    std::map<std::string,std::set<std::string>> parTypesByBase;
    std::map<int,std::size_t> hitEffectSystemsByType;
    std::map<int,std::set<std::string>> hitEffectFilesByType,hitEffectNamesByType;
    std::vector<std::tuple<std::string,std::string,std::string>> failures;
    std::vector<StrictEffFailure> strictEffFailures;
    std::vector<EffPartCount> effPartCounts;
    // Compatibility limits come from the current renderer, not a second table.
    std::map<std::string,std::string> effCompatibilityLimits;
    const std::regex effLimitPattern(R"((modelTracks|externalMeshes|nativeMeshes|billboards)\.size\(\)\s*>=\s*([0-9]+))");
    for(const auto& line:renderer.lines)
        for(std::sregex_iterator it(line.begin(),line.end(),effLimitPattern),end;it!=end;++it)
            effCompatibilityLimits[(*it)[1].str()]=(*it)[2].str();
    // Distinguish absence of a clipping comparison from an unparsed comparison.
    // This is source evidence only, never proof that a resource was drawn.
    for (const char* kind : {"modelTracks", "billboards", "nativeMeshes", "externalMeshes"}) {
        const std::regex guard(std::string(kind) + R"(\.size\(\)\s*>\s*=?)");
        const bool found = std::any_of(renderer.lines.begin(), renderer.lines.end(),
            [&](const std::string& line) { return std::regex_search(line, guard); });
        if (!found) effCompatibilityLimits[kind] = "none";
    }
    std::set<std::string> directStripFiles;
    std::size_t parFiles=0,effFiles=0,parSystems=0,effParts=0,drift=0;
    auto increment=[&](const std::string& id,const std::string& file){
        auto& it=items[id];it.key=id; ++it.count;it.files.insert(file);fileItems[file].insert(id);
    };
    std::vector<fs::path> paths;
    for(fs::recursive_directory_iterator it(opt.client/"effect",fs::directory_options::skip_permission_denied,ec),end;
        it!=end;it.increment(ec)){
        if(ec){ec.clear();continue;}
        if(!it->is_regular_file(ec))continue;
        auto ext=ba::lower(it->path().extension().string());
        if(ext==".par"||ext==".eff")paths.push_back(it->path());
    }
    std::sort(paths.begin(),paths.end());
    for(const auto& path:paths){
        const auto rel=fs::relative(path,opt.client,ec).generic_string();
        basenameFiles[normalizedBase(path.filename().string())].insert(rel);
        if(ba::lower(path.extension().string())==".par"){
            ++parFiles;
            auto result=par_audit::inspectFile(path.string());
            if(!result.ok){failures.emplace_back(rel,"par",result.error);continue;}
            for(const auto& s:result.document.systems){
                ++parSystems;increment("type:"+std::to_string(s.type),rel);
                parTypesByBase[normalizedBase(path.filename().string())].insert(std::to_string(s.type));
                for(const auto& feature:features())if(feature.second(s))increment("feature:"+feature.first,rel);
                if(!s.hitEffect.empty()){
                    ++hitEffectSystemsByType[s.type];
                    hitEffectFilesByType[s.type].insert(rel);
                    hitEffectNamesByType[s.type].insert(s.hitEffect);
                }
                for(const auto& model:possibleNames(s.model))if(model.size()>4&&model.substr(model.size()-4)==".eff")
                    fileLinks[rel].insert(model);
            }
        }else{
            ++effFiles;
            LegacyEffectSummary strictEffect, effect;
            const bool strictDecoded=inspectLegacyEff(path.string(),strictEffect,false)&&
                                     strictEffect.structured;
            const bool diagnosticDecoded=inspectLegacyEff(path.string(),effect,true)&&
                                         effect.structured;
            if(!strictDecoded){
                StrictEffFailure failed;failed.file=rel;failed.diagnosticDecoded=diagnosticDecoded;
                if(diagnosticDecoded){
                    failed.parts=effect.document.parts.size();
                    for(const auto& part:effect.document.parts){
                        if(!part.model.empty())failed.models.insert(part.model);
                        const auto& p=part.modelParams;
                        std::size_t baseNonFinite=0,animatedNonFinite=0;
                        for(float value:{p.height,p.topRadius,p.bottomRadius})
                            if(!std::isfinite(value)){++failed.nonFiniteBaseParams;++baseNonFinite;}
                        for(const auto& frame:part.animatedParams)
                            for(float value:{frame.height,frame.topRadius,frame.bottomRadius})
                                if(!std::isfinite(value)){++failed.nonFiniteAnimatedParams;++animatedNonFinite;}
                        failed.modelParamDetails.push_back(part.model+"(base="+
                            std::to_string(baseNonFinite)+",animated="+
                            std::to_string(animatedNonFinite)+")");
                    }
                }
                strictEffFailures.push_back(std::move(failed));
            }
            if(!diagnosticDecoded){
                failures.emplace_back(rel,"eff","not_fully_decoded");continue;
            }
            EffPartCount count;count.file=rel;count.parts=effect.document.parts.size();count.strictDecoded=strictDecoded;
            for(const auto& part:effect.document.parts){
                if(part.billboard)++count.billboards;
                else if(legacy_mesh::recognized(part.model))++count.native;
                else if(!part.model.empty()&&part.model!="0")++count.external;
            }
            effPartCounts.push_back(std::move(count));
            for(const auto& s:effect.document.parts){
                ++effParts;
                const auto cl=classification(s.model);
                if(cl!="empty")increment("eff_model:"+s.model,rel);
            }
        }
    }
    // Direct CMPStrip trails are not par_audit::Document::systems. Importing a
    // separate, attributed inventory prevents the six known trails vanishing.
    std::size_t directStripRecords=0,missingStripRecords=0;
    if(!opt.directStrips.empty()){
        std::ifstream in(opt.directStrips);std::string line;
        std::getline(in,line);
        if(trim(line)!="file,index,max_length,dummy_a,dummy_b,life,step,texture,src_blend,dst_blend,implementation"){
            log<<"Bad direct strip inventory header\n";return 2;
        }
        while(std::getline(in,line)){
            if(trim(line).empty())continue;
            auto f=ba::splitCsv(line);if(f.size()<10){log<<"Invalid direct strip row\n";return 2;}
            const std::string filename=normalizedBase(f[0]);
            if(!basenameFiles.count(filename)){
                ++missingStripRecords;continue;
            }
            for(const auto& rel:basenameFiles[filename]){
                increment("direct_cmpstrip",rel);directStripFiles.insert(rel);
            }
            ++directStripRecords;
        }
    }
    std::vector<std::tuple<std::string,std::string,std::string>> drifts;
    std::set<std::string> covered;
    for(const auto& r:rules){
        const auto id=r.kind+":"+r.key;covered.insert(id);
        auto& item=items[id];item.key=id;item.kind=r.kind;item.name=r.name;
        item.assessment=r.assessment;item.note=r.note;
        auto ev=r.kind=="type"?findType(renderer,std::stoi(r.key)):findToken(renderer,r.token);
        // Loader mentioning a type is not a runtime branch.
        const bool branch=!ev.outside.empty();
        if(r.kind=="type"&&((r.expected=="yes")!=branch)){
            ++drift;drifts.emplace_back(id,"branch_expectation_mismatch",r.expected=="yes"?"missing_branch":"unexpected_branch");
        }
        if(r.kind=="feature"){
            if(ev.outside.empty())item.evidence=ev.inside.empty()?"not_observed":"parsed_only";
            else item.evidence=ev.inside.empty()?"referenced_outside_loader":"parsed_and_referenced";
        } else item.evidence=branch?"branch_present":"generic_or_no_branch";
        item.note += " | code_lines="+lineList(ev.outside)+" | loader_lines="+lineList(ev.inside);
    }
    for(auto& kv:items){
        auto& item=kv.second;
        if(!covered.count(kv.first)&&kv.first!="direct_cmpstrip" && kv.first.rfind("eff_model:",0)!=0){
            item.kind=kv.first.substr(0,kv.first.find(':'));item.name=kv.first;item.assessment="unknown";
            item.evidence="not_in_rules";item.note="Data found but no rule in table";
            ++drift;drifts.emplace_back(kv.first,"missing_rule","add review entry");
        }
        if(kv.first.rfind("eff_model:",0)==0){
            const auto model=kv.first.substr(10);
            item.kind="eff_model";item.name=model;item.assessment=classification(model);
            item.evidence="model_field_decoded";item.note="model name from .eff; geometry not run";
        }
    }
    if(items.count("direct_cmpstrip")){
        auto& item=items["direct_cmpstrip"];item.kind="controller_strip";item.name="CMPStrip (direct controller)";
        const bool present=!findToken(renderer,"legacy_direct_strip::emit").outside.empty();
        item.assessment=present?"partial_anchor_dependent":"not_rendered";
        item.evidence=present?"renderer_source_and_external_inventory":"external_inventory";
        item.note=present?"direct strips render from live paired actor/item dummies; non-player anchors and visual parity pending"
                         :"separate from normal type=6 systems; not decoded by par_audit";
    }
    // A manifest with a missing mandatory rule is itself a drift signal.
    for(int t=1;t<=18;++t)if(!covered.count("type:"+std::to_string(t))){
        ++drift;drifts.emplace_back("type:"+std::to_string(t),"missing_rule","required historical type");
    }
    // Link to skills: traverse direct resources + nested .par->.eff dependencies.
    std::map<std::string,std::string> skillNames;
    std::size_t dependencyRows=0,unmatched=0,skippedDeps=0;
    std::set<std::pair<std::string,std::string>> missingLinks;
    std::vector<ba::SkillDep> deps;
    const bool haveDeps=!opt.deps.empty()&&loadEffectDeps(opt.deps,deps,skippedDeps);
    if(!opt.deps.empty()&&!haveDeps){log<<"WARNING: dependencies not available; skill counts are unknown\n";}
    if(haveDeps){
        for(const auto& d:deps){
            ++dependencyRows; skillNames[d.id]=d.name;
            std::vector<std::string> frontier=possibleNames(d.file);
            std::set<std::string> visited;
            bool found=false;
            for(std::size_t pos=0;pos<frontier.size()&&pos<2000;++pos){
                const auto n=frontier[pos];
                auto fit=basenameFiles.find(n);
                if(fit==basenameFiles.end())continue;
                for(const auto& path:fit->second){
                    if(!visited.insert(path).second)continue;
                    found=true;
                    fileSkills[path].insert(d.id);
                    for(const auto& id:fileItems[path])items[id].skills.insert(d.id);
                    for(const auto& child:fileLinks[path]){
                        for(const auto& target:possibleNames(child))frontier.push_back(target);
                    }
                }
            }
            if(!found){++unmatched;missingLinks.insert({d.id,d.file});}
        }
    }
    auto csv=[&](const char* name)->std::ofstream{return std::ofstream(opt.output/name);};
    auto typ=csv("fx_type_coverage.csv"),feat=csv("fx_feature_coverage.csv"),hitEff=csv("fx_hit_effect_coverage.csv"),models=csv("fx_eff_primitives.csv"),
        gap=csv("fx_gap_ranking.csv"),sk=csv("fx_skill_gaps.csv"),dr=csv("fx_table_drift.csv"),
        fails=csv("fx_decode_failures.csv"),coll=csv("fx_basename_collisions.csv"),
        miss=csv("fx_missing_dependencies.csv"),effUse=csv("fx_eff_usefulness.csv"),
        summary=csv("fx_coverage_summary.json"),partCounts=csv("fx_eff_part_counts.csv");
    if(!typ||!feat||!hitEff||!models||!gap||!sk||!dr||!fails||!coll||!miss||!effUse||!summary||!partCounts){
        log<<"Could not open CSV or JSON output\n";return 2;
    }
    ba::row(typ,{"type","name","systems","files","skills_reached","assessment","code_evidence","visual_validation","note"});
    ba::row(feat,{"feature","name","systems","files","skills_reached","code_evidence","visual_validation","note"});
    ba::row(hitEff,{"type","controller","systems_with_hit_effect","files","skills_reached",
        "par_files","hit_effect_names","dx9_runtime_dispatch","dx9_function","dx9_dispatch_line",
        "dx9_sendresmessage_lines","gl_event_path","gl_gate","assessment"});
    ba::row(models,{"model","model_class","parts","files","skills_reached","visual_validation","note"});
    ba::row(gap,{"gap","kind","assessment","instances","files","skills_reached","code_evidence","visual_validation","note"});
    ba::row(sk,{"skill_id","skill_name","gap_count","gaps"});
    ba::row(dr,{"item","problem","detail"});
    const auto joinSet=[](const std::set<std::string>& values){
        std::string out;for(const auto& value:values){if(!out.empty())out+=';';out+=value;}
        return out;
    };
    for(const auto& entry:hitEffectSystemsByType){
        std::set<std::string> skills;
        const auto files=hitEffectFilesByType.find(entry.first);
        if(files!=hitEffectFilesByType.end())for(const auto& file:files->second){
            const auto found=fileSkills.find(file);
            if(found!=fileSkills.end())skills.insert(found->second.begin(),found->second.end());
        }
        const auto* handler=hitEffectDispatch.forType(entry.first);
        std::string dx9Functions,dispatchLines,sendLines,glPath,glGate,assessment;
        if(handler){
            dx9Functions=handler->function;dispatchLines=std::to_string(handler->dispatchLine);
            for(int line:handler->dx9CallLines){if(!sendLines.empty())sendLines+=';';sendLines+=std::to_string(line);}
            for(int line:handler->glLines){if(!glPath.empty())glPath+=';';glPath+=std::to_string(line);}
            glGate=handler->glGate;
            assessment=handler->glPath?"dx9_dispatch_and_gl_normal_path_found":"gl_event_path_unverified";
        }else if(hitEffectDispatch.dx9Verified()){
            assessment="serialized_field_without_dx9_runtime_dispatch";
        }else{
            assessment="dx9_dispatch_inventory_incomplete";
        }
        ba::row(hitEff,{std::to_string(entry.first),handler?handler->constant.substr(10):
                sourceDefineName(dx9ParticleHeader,entry.first),
            std::to_string(entry.second),files==hitEffectFilesByType.end()?"":
                std::to_string(files->second.size()),haveDeps?std::to_string(skills.size()):"n/a",
            files==hitEffectFilesByType.end()?"":joinSet(files->second),
            joinSet(hitEffectNamesByType[entry.first]),handler?"yes":
                (hitEffectDispatch.dx9Verified()?"no":"unknown"),dx9Functions,dispatchLines,
            sendLines,glPath,glGate,assessment});
    }
    ba::row(fails,{"file","kind","error"});
    ba::row(effUse,{"file","strict_decode","diagnostic_decode","usage_evidence",
        "direct_par_files_count","direct_par_files","sceneffectinfo_ids",
        "dependency_graph_skill_ids","same_basename_par_files",
        "same_basename_par_sceneffectinfo_ids","same_basename_par_skill_ids",
        "runtime_use_proven","decoded_parts",
        "decoded_models","nonfinite_base_model_params","nonfinite_animated_model_params",
        "per_part_model_param_nonfinite_counts","dx9_catalog_load_evidence"});
    ba::row(coll,{"basename","files"});
    ba::row(miss,{"skill_id","referenced_name"});
    auto skcount=[&](const Item& i){return haveDeps?std::to_string(i.skills.size()):"n/a";};
    auto significant=[&](const Item& i){
        if(i.kind=="type")return i.assessment!="fully_verified";
        if(i.kind=="controller_strip")return true;
        if(i.kind=="eff_model")return i.assessment=="unrecognized_model";
        if(i.kind=="feature"){
            if(i.key=="feature:hit_effect"&&hitEffectDispatch.verified())return false;
            return i.evidence=="not_observed"||i.evidence=="parsed_only"||
            i.name.find("model range")!=std::string::npos||
            i.name.find("shade")!=std::string::npos||i.name.find("hit-effect")!=std::string::npos||
            i.name.find("SNOW/FIRE nonzero emitter step")!=std::string::npos||
            i.name.find("SNOW/FIRE random acceleration")!=std::string::npos||
            i.name.find("BLAST with nonzero acceleration")!=std::string::npos||
            i.name.find("BLAST group restart after all slots die")!=std::string::npos||
            i.name.find("MODEL with multiple curve frames")!=std::string::npos||
            i.name.find("MODEL parent midpoint position")!=std::string::npos||
            i.name.find("MODEL nested .eff frame stepping")!=std::string::npos||
            i.name.find("RIPPLE nonzero emission step")!=std::string::npos||
            i.name.find("RANGE2 per-frame collision/update clock")!=std::string::npos||
            i.key=="feature:shade_terrain_grid_height";
        }
        return true;
    };
    std::vector<const Item*> gaps;
    std::map<std::string,std::set<std::string>> perSkill;
    for(const auto& kv:items){
        const auto& i=kv.second;
        if(i.kind=="type")ba::row(typ,{kv.first.substr(5),i.name,std::to_string(i.count),std::to_string(i.files.size()),skcount(i),i.assessment,i.evidence,i.validation,i.note});
        if(i.kind=="feature")ba::row(feat,{kv.first.substr(8),i.name,std::to_string(i.count),std::to_string(i.files.size()),skcount(i),i.evidence,i.validation,i.note});
        if(i.kind=="eff_model")ba::row(models,{i.name,i.assessment,std::to_string(i.count),std::to_string(i.files.size()),skcount(i),i.validation,i.note});
        if(i.count && significant(i)){
            gaps.push_back(&i);
            for(const auto& id:i.skills)perSkill[id].insert(kv.first);
        }
    }
    std::sort(gaps.begin(),gaps.end(),[&](const Item* a,const Item* b){
        if(haveDeps&&a->skills.size()!=b->skills.size())return a->skills.size()>b->skills.size();
        if(a->count!=b->count)return a->count>b->count;
        return a->key<b->key;
    });
    for(const Item* i:gaps){
        ba::row(gap,{i->key,i->kind,i->assessment,std::to_string(i->count),std::to_string(i->files.size()),skcount(*i),i->evidence,i->validation,i->note});
    }
    for(const auto& kv:perSkill){
        std::string ids;for(const auto& id:kv.second){if(!ids.empty())ids+=';';ids+=id;}
        ba::row(sk,{kv.first,skillNames[kv.first],std::to_string(kv.second.size()),ids});
    }
    // skillinfo item channels are a separate runtime path from the prebuilt
    // dependency manifest. Keep this evidence explicit and do not infer that
    // a scene catalog row alone proves the channel is invoked in a session.
    struct SkillItemChannel {
        std::string skillId,skillName,channel,effectId,keyFrame,scenePar,
                    parCandidates,controllerTypes,itemDummy;
    };
    std::map<std::string,std::string> sceneParById;
    {
        std::ifstream in(opt.client/"scripts/table/sceneffectinfo.txt");
        std::string line;
        while(std::getline(in,line)){
            std::istringstream row(line);std::string id,par;
            if(std::getline(row,id,'\t')&&std::getline(row,par,'\t'))
                sceneParById[trim(id)]=trim(par);
        }
    }
    std::vector<SkillItemChannel> skillItemChannels;
    std::set<std::string> itemChannelSkills;
    std::set<std::string> itemDummyMinusOneSkills;
    {
        std::ifstream in(opt.client/"scripts/table/skillinfo.txt");
        std::string line;
        while(std::getline(in,line)){
            std::vector<std::string> columns;std::istringstream row(line);std::string field;
            while(std::getline(row,field,'\t'))columns.push_back(trim(field));
            if(columns.size()<56)continue;
            int itemDummyValue=-1;
            if(columns.size()>53){
                char* dummyEnd=nullptr;
                const long parsed=std::strtol(columns[53].c_str(),&dummyEnd,10);
                if(dummyEnd!=columns[53].c_str()&&*dummyEnd=='\0'&&
                   parsed>=std::numeric_limits<int>::min()&&parsed<=std::numeric_limits<int>::max())
                    itemDummyValue=static_cast<int>(parsed);
            }
            const std::string itemDummy=std::to_string(itemDummyValue);
            for(const auto& channel:std::array<std::pair<std::size_t,const char*>,2>{{
                    {54,"item1"},{55,"item2"}}}){
                std::vector<std::string> values;std::istringstream valueRow(columns[channel.first]);
                while(std::getline(valueRow,field,','))values.push_back(trim(field));
                if(values.empty())continue;
                char* end=nullptr;const long effectId=std::strtol(values[0].c_str(),&end,10);
                if(end==values[0].c_str()||*end!='\0'||effectId<=0)continue;
                const auto scene=sceneParById.find(values[0]);
                const std::string scenePar=scene==sceneParById.end()?"":scene->second;
                const std::string base=normalizedBase(scenePar);
                std::string candidates,types;
                const auto found=basenameFiles.find(base);
                if(found!=basenameFiles.end())for(const auto& candidate:found->second){
                    if(ba::lower(fs::path(candidate).extension().string())!=".par")continue;
                    if(!candidates.empty())candidates+=';';candidates+=candidate;
                }
                const auto typeIt=parTypesByBase.find(base);
                if(typeIt!=parTypesByBase.end())for(const auto& type:typeIt->second){
                    if(!types.empty())types+=';';types+=type;
                }
                skillItemChannels.push_back({columns[0],columns[1],channel.second,values[0],
                    values.size()>1?values[1]:"",scenePar,candidates,types,itemDummy});
                itemChannelSkills.insert(columns[0]);
                if(itemDummy=="-1")itemDummyMinusOneSkills.insert(columns[0]);
            }
        }
    }
    auto itemChannels=csv("fx_skill_item_channels.csv");
    if(!itemChannels){log<<"Could not open skill item-channel CSV output\n";return 2;}
    ba::row(itemChannels,{"skill_id","skill_name","channel","sceneffect_id","key_frame",
        "sceneffect_par","par_candidates","controller_types","runtime_use_proven",
        "item_dummy","dx9_itemeffect_guard_line","dx9_skill_dummy_callsite_lines",
        "gl_item_dummy_gate_line","gl_item_dummy_filter_line","code_comparison"});
    std::string dx9SkillDummyCallLines;
    for(int line:itemEffectDummy.dx9SkillDummyLines){
        if(!dx9SkillDummyCallLines.empty())dx9SkillDummyCallLines+=';';
        dx9SkillDummyCallLines+=std::to_string(line);
    }
    for(const auto& ref:skillItemChannels)
        ba::row(itemChannels,{ref.skillId,ref.skillName,ref.channel,ref.effectId,ref.keyFrame,
            ref.scenePar,ref.parCandidates,ref.controllerTypes,"no",ref.itemDummy,
            itemEffectDummy.dx9GuardLine?std::to_string(itemEffectDummy.dx9GuardLine):"",
            dx9SkillDummyCallLines,
            itemEffectDummy.glOptInLine?std::to_string(itemEffectDummy.glOptInLine):"",
            itemEffectDummy.glFilterLine?std::to_string(itemEffectDummy.glFilterLine):"",
            itemEffectDummy.verified()?"source_rule_and_normal_filter_verified":"source_evidence_incomplete"});
    const std::size_t itemDummyMinusOneChannels=std::count_if(skillItemChannels.begin(),
        skillItemChannels.end(),[](const SkillItemChannel& channel){return channel.itemDummy=="-1";});
    for(const auto& f:drifts)ba::row(dr,{std::get<0>(f),std::get<1>(f),std::get<2>(f)});
    for(const auto& f:failures)ba::row(fails,{std::get<0>(f),std::get<1>(f),std::get<2>(f)});
    std::map<std::string,std::set<std::string>> parentsByEff;
    for(const auto& parent:fileLinks){
        for(const auto& child:parent.second){
            const auto lowered=ba::lower(child);
            if(lowered.size()>4&&lowered.substr(lowered.size()-4)==".eff")
                parentsByEff[normalizedBase(child)].insert(parent.first);
        }
    }
    std::map<std::string,std::set<std::string>> sceneIdsByPar;
    {
        std::ifstream sceneTable(opt.client/"scripts/table/sceneffectinfo.txt");
        std::string row;
        while(std::getline(sceneTable,row)){
            std::vector<std::string> columns;
            std::istringstream input(row);std::string field;
            while(std::getline(input,field,'\t'))columns.push_back(trim(field));
            if(columns.size()>1&&ba::lower(columns[1]).size()>4&&
               ba::lower(columns[1]).substr(ba::lower(columns[1]).size()-4)==".par")
                sceneIdsByPar[normalizedBase(columns[1])].insert(columns[0]);
        }
    }
    ba::row(partCounts,{"file","parts","billboard_parts","native_parts","other_model_parts","decode_mode",
        "nested_compat_limit","billboard_compat_limit","native_compat_limit","external_compat_limit",
        "potential_nested_truncation","potential_billboard_truncation","potential_native_truncation","potential_external_truncation",
        "direct_par_files","dependency_graph_skill_ids","runtime_clipping_proven","visual_validation"});
    for(const auto& count:effPartCounts){
        const auto parents=parentsByEff.find(normalizedBase(count.file));
        std::set<std::string> parentFiles,skills;
        if(parents!=parentsByEff.end())for(const auto& parent:parents->second){
            parentFiles.insert(parent);
            const auto ids=fileSkills.find(parent);
            if(ids!=fileSkills.end())skills.insert(ids->second.begin(),ids->second.end());
        }
        const auto direct=fileSkills.find(count.file);
        if(direct!=fileSkills.end())skills.insert(direct->second.begin(),direct->second.end());
        const auto over=[&](const char* kind,std::size_t value){
            const auto limit=effCompatibilityLimits.find(kind);
            return limit==effCompatibilityLimits.end()?std::string("unknown"):
                limit->second=="none"?std::string("0"):
                (value>std::stoul(limit->second)?std::string("1"):std::string("0"));
        };
        const auto limit=[&](const char* kind){
            const auto found=effCompatibilityLimits.find(kind);
            return found==effCompatibilityLimits.end()?std::string{}:found->second;
        };
        ba::row(partCounts,{count.file,std::to_string(count.parts),std::to_string(count.billboards),std::to_string(count.native),std::to_string(count.external),
            count.strictDecoded?"strict":"diagnostic_raw_model_params",
            limit("modelTracks"),limit("billboards"),limit("nativeMeshes"),limit("externalMeshes"),
            parentFiles.empty()?"0":over("modelTracks",count.parts),over("billboards",count.billboards),over("nativeMeshes",count.native),over("externalMeshes",count.external),
            joinSet(parentFiles),joinSet(skills),"no","not_performed"});
    }
    for(const auto& failed:strictEffFailures){
        const auto asset=normalizedBase(failed.file);
        const auto parents=parentsByEff.find(asset);
        std::set<std::string> parentFiles,sceneIds,skillIds;
        if(parents!=parentsByEff.end())for(const auto& parent:parents->second){
            parentFiles.insert(parent);
            const auto ids=sceneIdsByPar.find(normalizedBase(parent));
            if(ids!=sceneIdsByPar.end())sceneIds.insert(ids->second.begin(),ids->second.end());
            const auto skills=fileSkills.find(parent);
            if(skills!=fileSkills.end())skillIds.insert(skills->second.begin(),skills->second.end());
        }
        const auto directSkills=fileSkills.find(failed.file);
        if(directSkills!=fileSkills.end())skillIds.insert(directSkills->second.begin(),directSkills->second.end());
        std::string siblingPar=asset;
        if(siblingPar.size()>=4&&siblingPar.substr(siblingPar.size()-4)==".eff")
            siblingPar.replace(siblingPar.size()-4,4,".par");
        std::set<std::string> siblingParFiles,siblingSceneIds,siblingSkillIds;
        const auto siblings=basenameFiles.find(siblingPar);
        if(siblings!=basenameFiles.end())for(const auto& sibling:siblings->second){
            if(ba::lower(fs::path(sibling).extension().string())!=".par")continue;
            siblingParFiles.insert(sibling);
            const auto ids=sceneIdsByPar.find(siblingPar);
            if(ids!=sceneIdsByPar.end())siblingSceneIds.insert(ids->second.begin(),ids->second.end());
            const auto skills=fileSkills.find(sibling);
            if(skills!=fileSkills.end())siblingSkillIds.insert(skills->second.begin(),skills->second.end());
        }
        const std::string usage=parentFiles.empty()&&skillIds.empty()?
            "no_direct_reference_found":(!parentFiles.empty()?"referenced_by_par":"direct_catalog_dependency");
        const bool inDx9EffectDirectory=fs::path(failed.file).parent_path()==fs::path("effect");
        const std::string dx9CatalogLoad=!dx9EffCatalog.verified()?"unverified":
            (inDx9EffectDirectory?"enumerated_loaded_indexed":"outside_top_level_effect_scan");
        const auto join=[](const std::set<std::string>& values){
            std::string result;for(const auto& value:values){if(!result.empty())result+=';';result+=value;}return result;
        };
        const auto joinList=[](const std::vector<std::string>& values){
            std::string result;for(const auto& value:values){if(!result.empty())result+=';';result+=value;}return result;
        };
        ba::row(effUse,{failed.file,"failed",failed.diagnosticDecoded?
            "decoded_with_raw_model_params":"failed",usage,
            std::to_string(parentFiles.size()),join(parentFiles),join(sceneIds),join(skillIds),
            join(siblingParFiles),join(siblingSceneIds),join(siblingSkillIds),"no",
            failed.diagnosticDecoded?std::to_string(failed.parts):"n/a",
            failed.diagnosticDecoded?join(failed.models):"n/a",
            failed.diagnosticDecoded?std::to_string(failed.nonFiniteBaseParams):"n/a",
            failed.diagnosticDecoded?std::to_string(failed.nonFiniteAnimatedParams):"n/a",
            failed.diagnosticDecoded?joinList(failed.modelParamDetails):"n/a",
            dx9CatalogLoad});
    }
    for(const auto& kv:basenameFiles)if(kv.second.size()>1){
        std::string all;for(const auto& path:kv.second){if(!all.empty())all+=';';all+=path;}
        ba::row(coll,{kv.first,all});
    }
    for(const auto& kv:missingLinks)ba::row(miss,{kv.first,kv.second});
    summary<<"{\n  \"schema\": \"FX-COVERAGE-v2\",\n"
           <<"  \"par_files\": "<<parFiles<<",\n  \"eff_files\": "<<effFiles
           <<",\n  \"par_systems\": "<<parSystems<<",\n  \"eff_parts\": "<<effParts
           <<",\n  \"decode_failures\": "<<failures.size()
           <<",\n  \"strict_eff_decode_failures\": "<<strictEffFailures.size()
           <<",\n  \"strict_only_eff_decode_failures\": "
           <<std::count_if(strictEffFailures.begin(),strictEffFailures.end(),
               [](const StrictEffFailure& failure){return failure.diagnosticDecoded;})
           <<",\n  \"dx9_eff_catalog_loader_verified\": "
           <<(dx9EffCatalog.verified()?"true":"false")
           <<",\n  \"dx9_eff_catalog_loader_lines\": {\"wildcard\": "
           <<dx9EffCatalog.wildcardLine<<", \"enumerator\": "
           <<dx9EffCatalog.enumeratorLine<<", \"load\": "
           <<dx9EffCatalog.loadLine<<", \"index\": "
           <<dx9EffCatalog.indexLine<<"}"
           <<",\n  \"hit_effect_dx9_dispatch_verified\": "
           <<(hitEffectDispatch.dx9Verified()?"true":"false")
           <<",\n  \"hit_effect_gl_event_paths_verified\": "
           <<(hitEffectDispatch.verified()?"true":"false")
           <<",\n  \"item_effect_dummy_dx9_rule_and_gl_gate_verified\": "
           <<(itemEffectDummy.verified()?"true":"false")
           <<",\n  \"item_effect_channel_rows\": "<<skillItemChannels.size()
           <<",\n  \"item_effect_channel_skills\": "<<itemChannelSkills.size()
           <<",\n  \"item_effect_dummy_minus_one_channels\": "<<itemDummyMinusOneChannels
           <<",\n  \"item_effect_dummy_minus_one_skills\": "<<itemDummyMinusOneSkills.size()
           <<",\n  \"hit_effect_dx9_sendresmessage_sites\": "
           <<hitEffectDispatch.allDx9CallLines.size()
           <<",\n  \"direct_strip_records\": "<<directStripRecords
           <<",\n  \"unmatched_direct_strip_records\": "<<missingStripRecords
           <<",\n  \"dependency_rows\": "<<dependencyRows
           <<",\n  \"non_effect_dependencies_skipped\": "<<skippedDeps
           <<",\n  \"unmatched_dependency_rows\": "<<unmatched
           <<",\n  \"skill_linkage_available\": "<<(haveDeps?"true":"false")
           <<",\n  \"coverage_gaps\": "<<gaps.size()
           <<",\n  \"skills_with_gaps\": "<<(haveDeps?std::to_string(perSkill.size()):"null")
           <<",\n  \"table_drift\": "<<drift
           <<",\n  \"visual_parity_verified\": false,\n"
           <<"  \"notes\": [\"code branches and fields are evidence only\","
           <<"\"type assessments are manually reviewed approximations\","
           <<"\"direct CMPStrip is imported from a separate inventory if supplied\"]\n}\n";
    log<<"FX coverage: par="<<parFiles<<" eff="<<effFiles<<" systems="<<parSystems
       <<" eff_parts="<<effParts<<" gaps="<<gaps.size()<<" skills_with_gaps="
       <<(haveDeps?std::to_string(perSkill.size()):"unknown")<<" drift="<<drift
       <<" failures="<<failures.size()<<" unmatched_strips="<<missingStripRecords<<" | "<<opt.output<<'\n';
    log<<"  item channels="<<skillItemChannels.size()<<" dummy_minus_one="
       <<itemDummyMinusOneChannels<<" skills="<<itemDummyMinusOneSkills.size()
       <<" of "<<itemChannelSkills.size()
       <<" dx9_gl_rule="<<(itemEffectDummy.verified()?"verified":"incomplete")<<'\n';
    for(std::size_t i=0;i<gaps.size()&&i<12;++i){
        const auto& g=*gaps[i];
        log<<"  "<<g.key<<" ["<<g.assessment<<"] instances="<<g.count
           <<" skills="<<skcount(g)<<'\n';
    }
    return opt.strict&&(drift||!failures.empty()||missingStripRecords)?1:0;
}
}} // namespace top::fx_coverage
