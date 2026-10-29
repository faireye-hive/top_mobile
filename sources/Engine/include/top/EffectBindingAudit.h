#pragma once
// Static channel/data inventory, not proof of a live dummy or visual parity.
#include "top/BlendAudit.h"
#include "top/SceneEffectCatalog.h"
#include "top/SkillVisual.h"

namespace top::effect_binding_audit {
inline int run(const std::filesystem::path& client,const std::filesystem::path& output,std::ostream& log){
    namespace fs=std::filesystem;
    namespace ba=blend_audit;
    if(!fs::is_directory(client/"effect")||!ba::outsideClient(client,output))return 2;
    SkillVisualCatalog skills;SceneEffectCatalog scenes;
    if(!skills.load((client/"scripts/table/skillinfo.txt").string())||
       !scenes.load((client/"scripts/table/sceneffectinfo.txt").string()))return 2;
    std::map<std::string,std::vector<fs::path>> assets;
    for(const auto& entry:fs::recursive_directory_iterator(client/"effect"))
        if(entry.is_regular_file())assets[ba::lower(entry.path().filename().string())].push_back(entry.path());
    std::set<int> ids;
    {std::ifstream file(client/"scripts/table/skillinfo.txt");std::string line;
     while(std::getline(file,line)){
         char* end=nullptr;const long id=std::strtol(line.c_str(),&end,10);
         if(end!=line.c_str()&&id>0&&id<=10000000&&skills.find(static_cast<int>(id)))ids.insert(static_cast<int>(id));
     }}
    fs::create_directories(output);std::ofstream csv(output/"bound_billboard_channels.csv");
    if(!csv)return 2;
    ba::row(csv,{"skill_id","channel","effect_id","dummy","par","system","type","parent_billboard",
        "model_direction","eff","part","child_billboard","child_position_nonzero","status","runtime_binding_proven",
        "non_billboard_binding_candidate"});
    std::map<std::string,par_audit::Result> pars;
    std::map<std::string,LegacyEffectSummary> effs;
    std::set<int> reached;std::set<std::string> resources;
    std::size_t rows=0,candidates=0,unresolved=0;
    std::size_t nonBillboardCandidates=0;
    std::set<int> nonBillboardSkills;
    for(int id:ids)for(const auto& channel:skills.find(id)->channels){
        const char* kind=nullptr;
        switch(channel.kind){
        case SkillVisualChannelKind::Actor:kind="actor";break;
        case SkillVisualChannelKind::Target:kind="target";break;
        case SkillVisualChannelKind::Item:kind="item";break;
        case SkillVisualChannelKind::ItemExtra:kind="item_extra";break;
        default:continue;
        }
        const auto* scene=scenes.find(channel.effectId);
        const auto missing=[&](const std::string& status){
            ++unresolved;ba::row(csv,{std::to_string(id),kind,std::to_string(channel.effectId),
                std::to_string(channel.dummy),scene?scene->particle:"","","","","","","","","",status,"no","0"});
        };
        if(!scene){missing("no_scene_par");continue;}
        const auto matches=assets.find(ba::lower(scene->particle));
        if(matches==assets.end()){missing("missing_par");continue;}
        for(const auto& path:matches->second){
            auto inserted=pars.try_emplace(path.string());
            if(inserted.second)inserted.first->second=par_audit::inspectFile(path.string());
            const auto& par=inserted.first->second;
            if(!par.ok){missing("par_decode_failure");continue;}
            for(std::size_t system=0;system<par.document.systems.size();++system){
                const auto& s=par.document.systems[system];
                const auto children=assets.find(ba::lower(s.model));
                if(children==assets.end()){
                    if(ba::lower(fs::path(s.model).extension().string())==".eff")missing("missing_eff");
                    continue;
                }
                for(const auto& child:children->second){
                    if(ba::lower(child.extension().string())!=".eff")continue;
                    auto effect=effs.try_emplace(child.string());
                    if(effect.second)inspectLegacyEff(child.string(),effect.first->second,true);
                    if(!effect.first->second.structured){missing("eff_decode_failure");continue;}
                    const auto& parts=effect.first->second.document.parts;
                    for(std::size_t index=0;index<parts.size();++index){
                        const auto& part=parts[index];
                        const bool position=std::any_of(part.keys.begin(),part.keys.end(),[](const auto& key){
                            return std::any_of(key.position.begin(),key.position.end(),[](float v){return v!=0.f;});
                        });
                        // MPParticleSys::BindingBone MODEL/STRIP .eff branch,
                        // then MPModelEff::Render's bound-child billboard branch.
                        const bool parent=(s.type==5||s.type==6)&&!s.billboard&&!s.modelDirection;
                        const bool candidate=channel.dummy>=0&&parent&&part.billboard&&position;
                        const bool nonBillboard=channel.dummy>=0&&parent&&!part.billboard;
                        if(nonBillboard){++nonBillboardCandidates;nonBillboardSkills.insert(id);}
                        const std::string status=candidate?"requires_live_dummy":
                            channel.dummy<0?"no_dummy_configured":!parent?"parent_branch_excluded":
                            !part.billboard?"child_not_billboard":"zero_child_position";
                        ++rows;if(candidate){++candidates;reached.insert(id);resources.insert(path.string());}
                        ba::row(csv,{std::to_string(id),kind,std::to_string(channel.effectId),std::to_string(channel.dummy),
                            fs::relative(path,client).generic_string(),std::to_string(system),std::to_string(s.type),
                            s.billboard?"1":"0",s.modelDirection?"1":"0",fs::relative(child,client).generic_string(),
                            std::to_string(index),part.billboard?"1":"0",position?"1":"0",status,"no",nonBillboard?"1":"0"});
                    }
                }
            }
        }
    }
    if(!csv.good())return 2;
    log<<"Binding audit: rows="<<rows<<" candidates="<<candidates<<" candidate_skills="<<reached.size()
       <<" candidate_pars="<<resources.size()<<" unresolved_channels="<<unresolved
       <<" non_billboard_candidates="<<nonBillboardCandidates
       <<" non_billboard_candidate_skills="<<nonBillboardSkills.size()<<'\n';
    return 0;
}
}
