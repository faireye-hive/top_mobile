#pragma once
// Read-only audit of CMPPartCtrl's direct CMPStrip trailer, the DX9 owner
// attachment paths, and the current GL owner providers. This is distinct from
// CMPPartSys type 6 and derives trail definitions from each .par file.
#include "top/BlendAudit.h"
#include "top/LegacyDirectStrip.h"
#include "top/LegacyParAudit.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace top { namespace direct_strip_anchor_audit {
namespace fs = std::filesystem;
namespace ba = blend_audit;

struct Options {
    fs::path client = "Client";
    fs::path out = "build/direct-strip-anchor-audit";
    fs::path deps = "sources/auditoria-historica/dependencies.csv";
    fs::path sceneEffectInfo = "scripts/table/sceneffectinfo.txt";
    fs::path magicSingleInfo = "scripts/table/magicsingleinfo.txt";
    fs::path dx9ModelEff = "arquivo/fontes/Engine/src/MPModelEff.cpp";
    fs::path dx9ParticleCtrl = "arquivo/fontes/Engine/src/MPParticleCtrl.cpp";
    fs::path dx9EffectCtrl = "arquivo/fontes/Engine/src/MPEffectCtrl.cpp";
    fs::path dx9ClientEffectObj = "arquivo/fontes/Client/src/EffectObj.cpp";
    fs::path glEffectSystem = "sources/Engine/src/top/EffectSystem.cpp";
    fs::path glActorSkin = "sources/Engine/src/top/ActorSkin.cpp";
};
struct Evidence {
    std::vector<std::string> lines;
    bool contains(const std::string& token) const {
        return std::any_of(lines.begin()+std::min<std::size_t>(1,lines.size()),lines.end(),
            [&](const std::string& line){return line.find(token)!=std::string::npos;});
    }
    std::vector<int> find(const std::string& token) const {
        std::vector<int> result;
        for(std::size_t i=1;i<lines.size();++i)
            if(lines[i].find(token)!=std::string::npos)result.push_back(static_cast<int>(i));
        return result;
    }
};
inline Evidence readEvidence(const fs::path& path) {
    Evidence out;std::ifstream in(path);std::string line;out.lines.push_back("");
    while(std::getline(in,line))out.lines.push_back(std::move(line));
    return out;
}
inline std::string joinInts(const std::vector<int>& values) {
    std::string out;
    for(int value:values){if(!out.empty())out+=' ';out+=std::to_string(value);}
    return out;
}
inline std::vector<int> callSites(const Evidence& source,const std::string& method) {
    auto dot=source.find("."+method+"(");
    auto arrow=source.find("->"+method+"(");
    dot.insert(dot.end(),arrow.begin(),arrow.end());
    std::sort(dot.begin(),dot.end());
    dot.erase(std::unique(dot.begin(),dot.end()),dot.end());
    return dot;
}
struct SkillRef { std::string id,name,channel; };
struct SceneEffectRef { std::string id,name; };
struct MagicSingleRef { std::string id,name; };
inline std::string normalizedPar(std::string value) {
    value=ba::lower(fs::path(value).filename().string());
    if(value.find('.')==std::string::npos)value+=".par";
    return value;
}
inline bool readDependencies(const fs::path& path,
        std::map<std::string,std::vector<SkillRef>>& byPar,std::string& error) {
    std::ifstream in(path);if(!in){error="dependencies_open_failed";return false;}
    std::string line;if(!std::getline(in,line)){error="dependencies_empty";return false;}
    const auto header=ba::splitCsv(line);
    auto col=[&](const std::string& name){
        for(std::size_t i=0;i<header.size();++i)if(header[i]==name)return static_cast<int>(i);
        return -1;
    };
    const int id=col("skill_id"),name=col("skill_name"),channel=col("channel"),ref=col("referenced_name");
    if(id<0||channel<0||ref<0){error="dependencies_missing_columns";return false;}
    while(std::getline(in,line)){
        const auto row=ba::splitCsv(line);
        if(row.size()<=static_cast<std::size_t>(std::max({id,channel,ref})))continue;
        if(row[id].empty())continue;
        byPar[normalizedPar(row[ref])].push_back({row[id],
            name>=0&&static_cast<std::size_t>(name)<row.size()?row[name]:"",row[channel]});
    }
    return true;
}
inline std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin=0;
    for(;;){
        const auto end=line.find('\t',begin);
        fields.push_back(line.substr(begin,end==std::string::npos?end:end-begin));
        if(end==std::string::npos)break;
        begin=end+1;
    }
    return fields;
}
inline bool readSceneEffectCatalog(const fs::path& path,
        std::map<std::string,std::vector<SceneEffectRef>>& byPar,std::string& error) {
    std::ifstream in(path);if(!in){error="sceneffectinfo_open_failed";return false;}
    std::string line;
    while(std::getline(in,line)){
        const auto fields=splitTabs(line);
        if(fields.size()<2||fields[0].empty()||fields[1].empty())continue;
        const std::string name=fields.size()>3&&!fields[3].empty()?fields[3]:
            fields.size()>2?fields[2]:std::string{};
        byPar[normalizedPar(fields[1])].push_back({fields[0],name});
    }
    return true;
}
inline bool readMagicSingleCatalog(const fs::path& path,
        std::map<std::string,std::vector<MagicSingleRef>>& byPar,std::string& error) {
    std::ifstream in(path);if(!in){error="magicsingleinfo_open_failed";return false;}
    std::string line;
    while(std::getline(in,line)){
        const auto fields=splitTabs(line);
        if(fields.size()<11||fields[0].empty()||!std::isdigit(
                static_cast<unsigned char>(fields[0][0])))continue;
        int partCount=0;
        try{partCount=std::stoi(fields[5]);}catch(...){continue;}
        if(partCount<=0)continue;
        std::stringstream parts(fields[6]);std::string part;
        while(std::getline(parts,part,',')){
            if(part.empty())continue;
            byPar[normalizedPar(part)].push_back({fields[0],fields[1]});
        }
    }
    return true;
}
inline bool readBytes(const fs::path& path,std::vector<unsigned char>& bytes) {
    std::ifstream in(path,std::ios::binary);
    if(!in)return false;
    bytes.assign(std::istreambuf_iterator<char>(in),{});
    return in.eof()||!in.bad();
}
inline std::string joinUnique(const std::set<std::string>& values) {
    std::string out;
    for(const auto& value:values){if(!out.empty())out+=';';out+=value;}
    return out;
}
inline int run(const Options& options,std::ostream& log) {
    std::map<std::string,std::vector<SkillRef>> refs;
    std::map<std::string,std::vector<SceneEffectRef>> sceneEffects;
    std::map<std::string,std::vector<MagicSingleRef>> magicSingles;
    std::string dependencyError;
    std::string sceneEffectError;
    const bool haveDeps=readDependencies(options.deps,refs,dependencyError);
    const fs::path sceneEffectPath=options.sceneEffectInfo.is_absolute()?
        options.sceneEffectInfo:options.client/options.sceneEffectInfo;
    const bool haveSceneEffects=readSceneEffectCatalog(
        sceneEffectPath,sceneEffects,sceneEffectError);
    const fs::path magicSinglePath=options.magicSingleInfo.is_absolute()?
        options.magicSingleInfo:options.client/options.magicSingleInfo;
    std::string magicSingleError;
    const bool haveMagicSingles=readMagicSingleCatalog(
        magicSinglePath,magicSingles,magicSingleError);
    const auto dx9Model=readEvidence(options.dx9ModelEff);
    const auto dx9Ctrl=readEvidence(options.dx9ParticleCtrl);
    const auto dx9Effect=readEvidence(options.dx9EffectCtrl);
    const auto dx9ClientEffect=readEvidence(options.dx9ClientEffectObj);
    const auto glEffect=readEvidence(options.glEffectSystem);
    const auto glActor=readEvidence(options.glActorSkin);
    if(dx9Model.lines.size()<2||dx9Ctrl.lines.size()<2||dx9Effect.lines.size()<2||
       dx9ClientEffect.lines.size()<2||
       glEffect.lines.size()<2||glActor.lines.size()<2){
        log<<"Could not read one or more required DX9/GL source files\n";return 2;
    }
    std::error_code ec;
    fs::create_directories(options.out,ec);
    if(ec){log<<"Could not create output directory: "<<ec.message()<<'\n';return 2;}
    std::ofstream csv(options.out/"direct_strip_anchor_coverage.csv");
    std::ofstream summary(options.out/"direct_strip_anchor_summary.json");
    if(!csv||!summary){log<<"Could not open audit outputs\n";return 2;}
    ba::row(csv,{"file","index","dummy_a","dummy_b","skills","channels",
        "scene_effect_ids","scene_effect_names","scene_effect_catalogued",
        "magic_single_ids","magic_single_names","dx9_magicctrl_loop_true_call_lines",
        "dx9_play_item_or_character","dx9_item_attach_lines","dx9_character_attach_call_lines",
        "dx9_mpeffectctrl_item_attach_lines","dx9_client_effectobj_item_attach_lines",
        "dx9_mpeffectctrl_character_attach_lines","dx9_mpcharacterctrl_character_attach_lines",
        "dx9_client_effectobj_character_attach_lines",
        "gl_cast_only","gl_player_actor_provider_lines","gl_player_item_provider_lines",
        "assessment","note"});
    const auto itemEffectCtrlCalls=callSites(dx9Effect,"SetStripItem");
    const auto itemClientCalls=callSites(dx9ClientEffect,"SetStripItem");
    auto itemAttachCalls=itemEffectCtrlCalls;
    itemAttachCalls.insert(itemAttachCalls.end(),itemClientCalls.begin(),itemClientCalls.end());
    std::sort(itemAttachCalls.begin(),itemAttachCalls.end());
    itemAttachCalls.erase(std::unique(itemAttachCalls.begin(),itemAttachCalls.end()),
        itemAttachCalls.end());
    const auto characterEffectCtrlCalls=callSites(dx9Effect,"SetStripCharacter");
    const auto characterCtrlCalls=callSites(dx9Ctrl,"SetStripCharacter");
    const auto characterClientCalls=callSites(dx9ClientEffect,"SetStripCharacter");
    std::vector<int> magicLoopTrueCalls;
    for(std::size_t i=1;i<dx9Effect.lines.size();++i)
        if(dx9Effect.lines[i].find("SetStripItem(")!=std::string::npos&&
           dx9Effect.lines[i].find("true")!=std::string::npos)
            magicLoopTrueCalls.push_back(static_cast<int>(i));
    auto characterAttachCalls=characterEffectCtrlCalls;
    characterAttachCalls.insert(characterAttachCalls.end(),
        characterCtrlCalls.begin(),characterCtrlCalls.end());
    characterAttachCalls.insert(characterAttachCalls.end(),
        characterClientCalls.begin(),characterClientCalls.end());
    std::sort(characterAttachCalls.begin(),characterAttachCalls.end());
    characterAttachCalls.erase(std::unique(characterAttachCalls.begin(),
        characterAttachCalls.end()),characterAttachCalls.end());
    const bool glActorProviderPresent=glActor.contains("playerDummyWorldMatrices()");
    const bool glItemProviderPresent=glActor.contains("playerItemDummyWorldMatrices()");
    const auto glActorProviderLines=glActor.find("playerDummyWorldMatrices()");
    const auto glItemProviderLines=glActor.find("playerItemDummyWorldMatrices()");
    std::size_t parCount=0,stripCount=0,decodeFailures=0,unmatchedRefs=0,
        sceneCataloguedRecords=0,magicSingleMatchedRecords=0;
    std::set<std::string> seenFiles;
    if(!fs::is_directory(options.client/"effect")){
        log<<"Missing Client/effect directory\n";return 2;
    }
    std::error_code walkError;
    for(fs::recursive_directory_iterator it(options.client/"effect",
            fs::directory_options::skip_permission_denied,walkError),end;
        it!=end;it.increment(walkError)){
        if(walkError){walkError.clear();continue;}
        if(!it->is_regular_file()||ba::lower(it->path().extension().string())!=".par")continue;
        ++parCount;
        const auto rel=fs::relative(it->path(),options.client,ec).generic_string();
        const auto parsed=par_audit::inspectFile(it->path().string());
        if(!parsed.ok){++decodeFailures;continue;}
        std::vector<unsigned char> bytes;
        if(!readBytes(it->path(),bytes)){++decodeFailures;continue;}
        std::vector<legacy_direct_strip::Definition> definitions;
        if(!legacy_direct_strip::decodeTail(bytes,parsed.document.consumed,
                                             parsed.document.version,definitions)){
            ++decodeFailures;continue;
        }
        if(definitions.empty())continue;
        const auto key=normalizedPar(rel);
        const auto found=refs.find(key);
        if(found==refs.end())++unmatchedRefs;
        const auto sceneFound=sceneEffects.find(key);
        const auto magicFound=magicSingles.find(key);
        std::set<std::string> sceneIds,sceneNames;
        std::set<std::string> magicIds,magicNames;
        if(sceneFound!=sceneEffects.end())for(const auto& ref:sceneFound->second){
            sceneIds.insert(ref.id);if(!ref.name.empty())sceneNames.insert(ref.name);
        }
        if(!sceneIds.empty())sceneCataloguedRecords+=definitions.size();
        if(magicFound!=magicSingles.end())for(const auto& ref:magicFound->second){
            magicIds.insert(ref.id);if(!ref.name.empty())magicNames.insert(ref.name);
        }
        if(!magicIds.empty())magicSingleMatchedRecords+=definitions.size();
        for(std::size_t index=0;index<definitions.size();++index){
            const auto& def=definitions[index];
            std::set<std::string> skillIds,channels;
            if(found!=refs.end())for(const auto& ref:found->second){
                skillIds.insert(ref.id);
                if(!ref.channel.empty())channels.insert(ref.channel);
            }
            const auto playOwner=dx9Model.find("if (_pItem)");
            const auto glCast=glEffect.find("effect.kind!=EffectEvent::Cast");
            const bool hasDx9Owner=dx9Model.contains("if (_pItem)")&&
                dx9Model.contains("else if(_pCha)");
            const bool hasGlSupport=glEffect.contains("playerActorCast")&&
                glEffect.contains("playerItemCast")&&glActorProviderPresent&&glItemProviderPresent;
            std::set<std::string> assessments;
            if(found==refs.end()&&sceneFound!=sceneEffects.end())
                assessments.insert("scene_effect_catalogued_runtime_unproven");
            else if(found==refs.end()) assessments.insert("dependency_unmatched_owner_unknown");
            else if(channels.empty()) assessments.insert("dependency_channel_missing");
            else for(const auto& channel:channels){
                if(channel=="actor") assessments.insert(characterAttachCalls.empty()?
                    "actor_setter_callsite_missing":"actor_setter_callsite_present_runtime_unproven");
                else if(channel=="item") assessments.insert(itemAttachCalls.empty()?
                    "item_setter_callsite_missing":"item_setter_callsite_present_runtime_unproven");
                else if(channel=="impact_ground"||channel=="water")
                    assessments.insert("surface_channel_owner_runtime_unproven");
                else assessments.insert("channel_owner_unclassified");
            }
            if(!hasDx9Owner) assessments.insert("dx9_owner_implementation_unobserved");
            if(!hasGlSupport) assessments.insert("gl_owner_provider_unobserved");
            const std::string assessment=joinUnique(assessments);
            const std::string note="DX9 EffectObj and controller setter callsites plus GL player providers are static evidence only, not proof of runtime invocation; non-player owner matrices are not established";
            ba::row(csv,{rel,std::to_string(index),std::to_string(def.dummyA),
                std::to_string(def.dummyB),joinUnique(skillIds),joinUnique(channels),
                joinUnique(sceneIds),joinUnique(sceneNames),sceneFound!=sceneEffects.end()?"yes":"no",
                joinUnique(magicIds),joinUnique(magicNames),joinInts(magicLoopTrueCalls),
                joinInts(playOwner),joinInts(itemAttachCalls),joinInts(characterAttachCalls),
                joinInts(itemEffectCtrlCalls),joinInts(itemClientCalls),
                joinInts(characterEffectCtrlCalls),joinInts(characterCtrlCalls),
                joinInts(characterClientCalls),
                joinInts(glCast),joinInts(glActorProviderLines),
                joinInts(glItemProviderLines),assessment,note});
            ++stripCount;
            seenFiles.insert(key);
        }
    }
    if(!haveDeps)log<<"Dependency links unavailable: "<<dependencyError<<'\n';
    if(!haveSceneEffects)log<<"Scene-effect catalog unavailable: "<<sceneEffectError<<'\n';
    if(!haveMagicSingles)log<<"MagicSingleinfo catalog unavailable: "<<magicSingleError<<'\n';
    summary<<"{\n  \"par_files\": "<<parCount<<",\n  \"direct_strip_records\": "<<stripCount
        <<",\n  \"files_with_direct_strips\": "<<seenFiles.size()<<",\n  \"decode_failures\": "<<decodeFailures
        <<",\n  \"direct_strip_files_without_dependency_row\": "<<unmatchedRefs
        <<",\n  \"scene_effect_catalog_loaded\": "<<(haveSceneEffects?"true":"false")
        <<",\n  \"scene_effect_catalogued_records\": "<<sceneCataloguedRecords
        <<",\n  \"magic_single_catalog_loaded\": "<<(haveMagicSingles?"true":"false")
        <<",\n  \"magic_single_linked_direct_strip_records\": "<<magicSingleMatchedRecords
        <<",\n  \"dx9_mpeffectctrl_loop_true_call_observed\": "
            <<(!magicLoopTrueCalls.empty()?"true":"false")
        <<",\n  \"dependencies_loaded\": "<<(haveDeps?"true":"false")
        <<",\n  \"dx9_item_setter_callsite_observed\": "<<(!itemAttachCalls.empty()?"true":"false")
        <<",\n  \"dx9_client_effectobj_item_setter_callsite_observed\": "<<(!itemClientCalls.empty()?"true":"false")
        <<",\n  \"dx9_mpeffectctrl_item_setter_callsite_observed\": "<<(!itemEffectCtrlCalls.empty()?"true":"false")
        <<",\n  \"dx9_character_setter_callsite_observed\": "<<(!characterAttachCalls.empty()?"true":"false")
        <<",\n  \"dx9_client_effectobj_character_setter_callsite_observed\": "<<(!characterClientCalls.empty()?"true":"false")
        <<",\n  \"dx9_mpeffectctrl_character_setter_callsite_observed\": "<<(!characterEffectCtrlCalls.empty()?"true":"false")
        <<",\n  \"gl_player_actor_provider_observed\": "<<(glActorProviderPresent?"true":"false")
        <<",\n  \"gl_player_item_provider_observed\": "<<(glItemProviderPresent?"true":"false")<<"\n}\n";
    log<<"Direct CMPStrip audit: par="<<parCount<<" records="<<stripCount
       <<" files="<<seenFiles.size()<<" decode_failures="<<decodeFailures
       <<" no_dependency_row="<<unmatchedRefs
       <<" magic_single_linked_records="<<magicSingleMatchedRecords
       <<" | \""<<options.out.string()<<"\"\n";
    return decodeFailures?1:0;
}
}} // namespace top::direct_strip_anchor_audit
