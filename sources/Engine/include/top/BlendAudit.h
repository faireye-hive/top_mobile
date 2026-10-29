#pragma once
// BLEND AUDIT v3 - read-only inventory of the D3D material state authored in
// Client/effect/*.eff and *.par: SRCBLEND/DESTBLEND, billboard flag, texture
// filters and the "shade" flag. Since SK2.6 the renderer applies the authored
// pair for both .eff and .par, so this tool no longer mirrors any renderer
// rule: it calls top::historic_blend::valid() (the single source of truth)
// and reports what the renderer will do with each pair.
// Nothing here renders anything or edits assets; it never reads TOP_* env vars.
#include "top/HistoricalBlend.h"
#include "top/LegacyEffectInspector.h"
#include "top/LegacyParAudit.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace top { namespace blend_audit {
namespace fs = std::filesystem;

struct Factor { int value; const char* d3d; const char* gl; };
// D3D9 D3DBLEND enum. 12/13 are legacy "both" modes that only make sense as
// the source factor; GL has no single-enum equivalent, so they are annotated.
inline const Factor* factor(int v) {
    static const Factor table[] = {
        {1,"ZERO","GL_ZERO"},{2,"ONE","GL_ONE"},{3,"SRCCOLOR","GL_SRC_COLOR"},
        {4,"INVSRCCOLOR","GL_ONE_MINUS_SRC_COLOR"},{5,"SRCALPHA","GL_SRC_ALPHA"},
        {6,"INVSRCALPHA","GL_ONE_MINUS_SRC_ALPHA"},{7,"DESTALPHA","GL_DST_ALPHA"},
        {8,"INVDESTALPHA","GL_ONE_MINUS_DST_ALPHA"},{9,"DESTCOLOR","GL_DST_COLOR"},
        {10,"INVDESTCOLOR","GL_ONE_MINUS_DST_COLOR"},{11,"SRCALPHASAT","GL_SRC_ALPHA_SATURATE"},
        {12,"BOTHSRCALPHA","(legacy: src=SRC_ALPHA,dst=ONE_MINUS_SRC_ALPHA)"},
        {13,"BOTHINVSRCALPHA","(legacy: src=ONE_MINUS_SRC_ALPHA,dst=SRC_ALPHA)"},
        {14,"BLENDFACTOR","GL_CONSTANT_COLOR"},{15,"INVBLENDFACTOR","GL_ONE_MINUS_CONSTANT_COLOR"}};
    return v>=1 && v<=15 ? &table[v-1] : nullptr;
}
inline std::string d3dName(int v){ auto*f=factor(v); return f?f->d3d:"INVALID("+std::to_string(v)+")"; }
inline std::string glName(int v){ auto*f=factor(v); return f?f->gl:"-"; }

// Destination-alpha model. The DX9 client used a back buffer without alpha
// (display-mode format), where D3D reads destination alpha as 1.0. The GL
// renderer reproduces that by clearing alpha to 1 and masking alpha writes
// (GfxDevice::beginFrame, historic_blend::opaqueDestination()). With
// opaqueDest, DESTALPHA behaves as ONE and INVDESTALPHA as ZERO; the audit
// reports that as "effective_equiv".
inline int normFactor(int v,bool opaqueDest){ return !opaqueDest ? v : v==7 ? 2 : v==8 ? 1 : v; }
inline bool involvesDestAlpha(int src,int dst){ return src==7||src==8||dst==7||dst==8; }
inline std::string glPair(int src,int dst){ return glName(src)+"/"+glName(dst); }
inline std::string effectiveEquiv(int src,int dst,bool opaqueDest){
    return glName(normFactor(src,opaqueDest))+"/"+glName(normFactor(dst,opaqueDest)); }
inline std::string hint(int src,int dst){
    if (src==5&&dst==6) return "alpha-over (D3D default)";
    if (src==5&&dst==7) return "SRCALPHA/DESTALPHA: additive while destination alpha is 1.0 (alpha-less back buffer)";
    if (src==5&&dst==2) return "alpha-weighted additive";
    if (src==2&&dst==2) return "pure additive";
    if (src==7&&dst==6) return "DESTALPHA/INVSRCALPHA: ONE/INVSRCALPHA (premultiplied alpha) while destination alpha is 1.0";
    if (src==3&&dst==4) return "SRCCOLOR/INVSRCCOLOR (color-weighted blend)";
    return "";
}
// applied         : both factors accepted by the translator -> renderer uses the authored pair
// legacy_fallback : a real D3D9 value the translator rejects (SRCALPHASAT as dest, BOTH*, BLENDFACTOR)
// invalid_value   : outside 1..15 (includes 0 = never set)
inline std::string status(int src,int dst){
    if(!factor(src)||!factor(dst)) return "invalid_value";
    return historic_blend::valid(src,dst) ? "applied" : "legacy_fallback";
}

struct Use {
    std::string file, kind, name, texture, model;
    int index=0, src=0, dst=0, minFilter=0, magFilter=0, type=0;
    bool billboard=false, shade=false;
};
struct Fail { std::string file, kind, reason; };
struct Report {
    std::vector<Use> uses; std::vector<Fail> failures;
    std::size_t effFiles=0, parFiles=0;
    // lowercase basename -> referenced .eff basenames (par model -> eff)
    std::map<std::string,std::set<std::string>> links;
    std::map<std::string,std::string> relPath;
};

inline std::string lower(std::string s){
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});
    return s;
}
inline std::string csvq(const std::string& v){
    if(v.find_first_of(",\r\n\"")==std::string::npos) return v;
    std::string o="\""; for(char c:v){ if(c=='"') o+='"'; o+=c; } return o+'"';
}
inline void row(std::ostream& o,const std::vector<std::string>& f){
    for(std::size_t i=0;i<f.size();++i){ if(i)o<<','; o<<csvq(f[i]); } o<<'\n';
}
inline std::string pairKey(int s,int d){ return std::to_string(s)+","+std::to_string(d); }

inline Report scan(const fs::path& client){
    Report r; std::error_code ec;
    const fs::path dir=client/"effect";
    std::vector<fs::path> files;
    for(fs::recursive_directory_iterator it(dir,fs::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
        if(ec){ec.clear();continue;}
        if(!it->is_regular_file(ec)) continue;
        const auto ext=lower(it->path().extension().string());
        if(ext==".eff"||ext==".par") files.push_back(it->path());
    }
    std::sort(files.begin(),files.end());
    for(const auto& p:files){
        const auto rel=fs::relative(p,client,ec).generic_string();
        const auto base=lower(p.filename().string());
        r.relPath[base]=rel;
        if(lower(p.extension().string())==".par"){
            ++r.parFiles;
            const auto res=par_audit::inspectFile(p.string());
            if(!res.ok){ r.failures.push_back({rel,"par",res.error}); continue; }
            for(std::size_t i=0;i<res.document.systems.size();++i){
                const auto& s=res.document.systems[i]; Use u;
                u.file=rel; u.kind="par"; u.index=int(i); u.name=s.name; u.type=s.type;
                u.texture=s.texture; u.model=s.model; u.billboard=s.billboard; u.shade=s.shade;
                u.src=s.srcBlend; u.dst=s.dstBlend; u.minFilter=s.minFilter; u.magFilter=s.magFilter;
                r.uses.push_back(u);
                const auto m=lower(s.model);
                if(m.size()>4 && m.substr(m.size()-4)==".eff") r.links[base].insert(m);
            }
        } else {
            ++r.effFiles; LegacyEffectSummary eff;
            if(!inspectLegacyEff(p.string(),eff,true)){ r.failures.push_back({rel,"eff","unreadable_or_bad_header"}); continue; }
            if(!eff.structured){ r.failures.push_back({rel,"eff","structure_not_decoded"}); continue; }
            for(std::size_t i=0;i<eff.document.parts.size();++i){
                const auto& s=eff.document.parts[i]; Use u;
                u.file=rel; u.kind="eff"; u.index=int(i); u.name=s.name; u.type=s.type;
                u.texture=s.texture; u.model=s.model; u.billboard=s.billboard;
                u.src=s.sourceBlend; u.dst=s.destBlend;
                r.uses.push_back(u);
            }
        }
    }
    return r;
}

// dependencies.csv (auditoria-historica) -> skill id/name -> direct effect file
struct SkillDep { std::string id,name,channel,file; };
inline std::vector<std::string> splitCsv(const std::string& line){
    std::vector<std::string> out; std::string cur; bool q=false;
    for(std::size_t i=0;i<line.size();++i){
        const char c=line[i];
        if(q){ if(c=='"'){ if(i+1<line.size()&&line[i+1]=='"'){cur+='"';++i;} else q=false; } else cur+=c; }
        else if(c=='"') q=true; else if(c==',') {out.push_back(cur);cur.clear();} else if(c!='\r') cur+=c;
    }
    out.push_back(cur); return out;
}
inline bool loadDeps(const fs::path& path,std::vector<SkillDep>& out){
    std::ifstream in(path); if(!in) return false;
    std::string line; if(!std::getline(in,line)) return false;
    const auto head=splitCsv(line); auto col=[&](const char* n){
        for(std::size_t i=0;i<head.size();++i) {
            if(head[i]==n) return int(i);
        }
        return -1; };
    const int cId=col("skill_id"),cName=col("skill_name"),cCh=col("channel"),cRef=col("referenced_name");
    if(cId<0||cRef<0) return false;
    while(std::getline(in,line)){
        const auto f=splitCsv(line); if(f.size()<=std::size_t(std::max(cId,cRef))) continue;
        out.push_back({f[cId],cName>=0&&std::size_t(cName)<f.size()?f[cName]:"",
                       cCh>=0&&std::size_t(cCh)<f.size()?f[cCh]:"",lower(f[cRef])});
    }
    return true;
}

// Outputs live outside Client/ (same rule as par_audit).
inline bool outsideClient(const fs::path& client,const fs::path& out){
    std::error_code e; const auto c=fs::weakly_canonical(client,e); if(e) return false;
    const auto o=fs::weakly_canonical(out,e); if(e) return false;
    auto ci=c.begin(),oi=o.begin();
    while(ci!=c.end()&&oi!=o.end()&&*ci==*oi){++ci;++oi;}
    return ci!=c.end();
}

struct Totals { std::size_t uses=0,applied=0,legacy=0,invalid=0,destAlpha=0,failures=0; };

inline int run(const fs::path& client,const fs::path& out,const fs::path& depsPath,bool strict,
               std::ostream& log,bool opaqueDest=true){
    if(!fs::is_directory(client/"effect")){ log<<"Missing directory: "<<client/"effect"<<'\n'; return 2; }
    if(!outsideClient(client,out)){ log<<"Output directory must be outside Client/: "<<out<<'\n'; return 2; }
    std::error_code ec; fs::create_directories(out,ec);
    if(ec){ log<<ec.message()<<'\n'; return 2; }
    const Report rep=scan(client);

    std::ofstream usage(out/"blend_usage.csv"),pairs(out/"blend_pairs.csv"),
                  fails(out/"blend_failures.csv"),sum(out/"blend_summary.json");
    if(!usage||!pairs||!fails||!sum){ log<<"Unable to create output files\n"; return 2; }
    row(usage,{"file","kind","index","name","type","src","src_d3d","dst","dst_d3d","gl_pair","effective_equiv",
               "billboard","shade","min_filter","mag_filter","status","texture","model"});
    row(fails,{"file","kind","reason"});

    struct Agg { std::size_t eff=0,par=0; std::set<std::string> files; };
    std::map<std::pair<int,int>,Agg> agg; Totals t;
    for(const auto& u:rep.uses){
        const auto st=status(u.src,u.dst); ++t.uses;
        if(st=="applied")++t.applied; else if(st=="legacy_fallback")++t.legacy; else ++t.invalid;
        if(involvesDestAlpha(u.src,u.dst))++t.destAlpha;
        row(usage,{u.file,u.kind,std::to_string(u.index),u.name,std::to_string(u.type),
            std::to_string(u.src),d3dName(u.src),std::to_string(u.dst),d3dName(u.dst),
            glPair(u.src,u.dst),effectiveEquiv(u.src,u.dst,opaqueDest),
            u.billboard?"1":"0",u.shade?"1":"0",
            u.kind=="par"?std::to_string(u.minFilter):"",u.kind=="par"?std::to_string(u.magFilter):"",
            st,u.texture,u.model});
        auto& a=agg[{u.src,u.dst}]; (u.kind=="eff"?a.eff:a.par)++; a.files.insert(u.file);
    }
    for(const auto& f:rep.failures) row(fails,{f.file,f.kind,f.reason});
    t.failures=rep.failures.size();

    // Skill reach: direct file + par -> .eff model links (one level, like par_dependencies).
    std::map<std::string,std::set<std::string>> skillsByPair; // "src,dst" -> "id name"
    std::map<std::string,std::set<std::string>> pairsByFile;
    for(const auto& u:rep.uses) pairsByFile[lower(fs::path(u.file).filename().string())].insert(pairKey(u.src,u.dst));
    bool haveDeps=false; std::vector<SkillDep> deps;
    if(!depsPath.empty()){
        haveDeps=loadDeps(depsPath,deps);
        if(!haveDeps) log<<"Warning: could not read --deps "<<depsPath<<" (need skill_id,referenced_name columns)\n";
    }
    if(haveDeps){
        std::ofstream sk(out/"blend_skills.csv");
        row(sk,{"skill_id","skill_name","channel","effect_file","src","src_d3d","dst","dst_d3d","effective_equiv","status"});
        std::set<std::string> seen;
        for(const auto& d:deps){
            std::vector<std::string> names{d.file};
            if(d.file.find('.')==std::string::npos){ names.push_back(d.file+".par"); names.push_back(d.file+".eff"); }
            for(const auto& n:names){
                std::set<std::string> reach{n};
                auto l=rep.links.find(n); if(l!=rep.links.end()) reach.insert(l->second.begin(),l->second.end());
                for(const auto& f:reach){
                    auto p=pairsByFile.find(f); if(p==pairsByFile.end()) continue;
                    for(const auto& k:p->second){
                        const int s=std::stoi(k.substr(0,k.find(','))),dd=std::stoi(k.substr(k.find(',')+1));
                        const std::string key=d.id+"|"+d.channel+"|"+f+"|"+k;
                        if(!seen.insert(key).second) continue;
                        row(sk,{d.id,d.name,d.channel,f,std::to_string(s),d3dName(s),std::to_string(dd),d3dName(dd),
                                effectiveEquiv(s,dd,opaqueDest),status(s,dd)});
                        skillsByPair[k].insert(d.id+" "+d.name);
                    }
                }
            }
        }
    }

    row(pairs,{"src","src_d3d","dst","dst_d3d","gl_pair","effective_equiv","status","involves_destalpha",
               "eff_subeffects","par_systems","files","skills_reached","hint"});
    for(const auto& kv:agg){
        const int s=kv.first.first,d=kv.first.second;
        row(pairs,{std::to_string(s),d3dName(s),std::to_string(d),d3dName(d),glPair(s,d),effectiveEquiv(s,d,opaqueDest),
            status(s,d),involvesDestAlpha(s,d)?"1":"0",std::to_string(kv.second.eff),
            std::to_string(kv.second.par),std::to_string(kv.second.files.size()),
            haveDeps?std::to_string(skillsByPair[pairKey(s,d)].size()):"n/a",hint(s,d)});
    }

    sum<<"{\n  \"schema\": \"BLEND-AUDIT-v3\",\n  \"eff_files\": "<<rep.effFiles<<",\n  \"par_files\": "<<rep.parFiles
       <<",\n  \"decode_failures\": "<<t.failures<<",\n  \"blend_records\": "<<t.uses
       <<",\n  \"dest_alpha_model\": \""<<(opaqueDest?"opaque (alpha=1, alpha writes masked)":"raw enum")<<"\""
       <<",\n  \"applied\": "<<t.applied<<",\n  \"legacy_fallback\": "<<t.legacy<<",\n  \"invalid_value\": "<<t.invalid
       <<",\n  \"records_involving_destalpha\": "<<t.destAlpha<<",\n  \"distinct_pairs\": "<<agg.size()
       <<",\n  \"notes\": [\"applied = renderer uses the authored pair (historic_blend::valid)\","
         "\"effective_equiv assumes the renderer's opaque destination alpha\","
         "\"applied is not proof of visual parity\"]\n}\n";

    log<<"Blend audit v3: eff="<<rep.effFiles<<" par="<<rep.parFiles<<" records="<<t.uses
       <<" | applied="<<t.applied<<" legacy_fallback="<<t.legacy<<" invalid="<<t.invalid
       <<" | involving DESTALPHA="<<t.destAlpha<<" | distinct pairs="<<agg.size()
       <<" | decode failures="<<t.failures<<" | out="<<out<<'\n';
    for(const auto& kv:agg){
        const int s=kv.first.first,d=kv.first.second;
        log<<"  ("<<s<<','<<d<<") "<<d3dName(s)<<'/'<<d3dName(d)<<"  eff="<<kv.second.eff<<" par="<<kv.second.par
           <<"  "<<status(s,d)<<"  => "<<effectiveEquiv(s,d,opaqueDest)
           <<(haveDeps?"  skills="+std::to_string(skillsByPair[pairKey(s,d)].size()):"")<<'\n';
    }
    return (strict&&(t.invalid||t.failures))?1:0;
}
}} // namespace top::blend_audit
