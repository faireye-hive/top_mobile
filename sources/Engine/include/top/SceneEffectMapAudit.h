#pragma once

#include "top/BlendAudit.h"
#include "top/LegacyParAudit.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace top { namespace scene_effect_map_audit {
namespace fs = std::filesystem;
namespace ba = blend_audit;
namespace pa = par_audit;

struct Options {
    fs::path client = "Client";
    fs::path out = "build/scene-effect-map-audit";
    fs::path sceneEffectInfo = "scripts/table/sceneffectinfo.txt";
    fs::path dx9GameAppMsg = "arquivo/fontes/Client/src/GameAppMsg.cpp";
    fs::path dx9SceneObjFile = "arquivo/fontes/Client/src/SceneObjFile.cpp";
    fs::path dx9EffectObj = "arquivo/fontes/Client/src/EffectObj.cpp";
    fs::path dx9EffectSet = "arquivo/fontes/Client/src/EffectSet.h";
    fs::path dx9TableLoader = "arquivo/fontes/Client/src/GameAppInterface.cpp";
    fs::path glSceneWorld = "sources/Engine/src/top/SceneWorld.cpp";
};

struct Definition {
    std::string particle;
    std::string name;
    std::string alternateName;
};

struct Placement {
    std::string map;
    int section = 0;
    int record = 0;
    int id = 0;
    int x = 0;
    int y = 0;
    int heightOffset = 0;
    int yaw = 0;
    Definition definition;
    bool catalogued = false;
};

inline std::uint16_t u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (std::uint16_t(p[1]) << 8));
}
inline std::uint32_t u32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
inline std::int32_t i32(const std::uint8_t* p) {
    return static_cast<std::int32_t>(u32(p));
}
inline std::string lower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
        return static_cast<char>(c>='A'&&c<='Z'?c+('a'-'A'):c);
    });
    return value;
}
inline bool readBytes(const fs::path& path,std::vector<std::uint8_t>& bytes) {
    std::ifstream in(path,std::ios::binary);
    if(!in)return false;
    bytes.assign(std::istreambuf_iterator<char>(in),{});
    return in.eof()||!in.bad();
}
inline std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> result;
    std::size_t start=0;
    for(;;){
        const auto end=line.find('\t',start);
        result.push_back(line.substr(start,end==std::string::npos?end:end-start));
        if(end==std::string::npos)break;
        start=end+1;
    }
    return result;
}
inline bool readCatalog(const fs::path& path,std::map<int,Definition>& catalog) {
    std::ifstream in(path);if(!in)return false;
    std::string line;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='/'||line[0]=='#')continue;
        const auto fields=splitTabs(line);
        if(fields.size()<2||fields[0].empty()||fields[1].empty())continue;
        char* end=nullptr;const long id=std::strtol(fields[0].c_str(),&end,10);
        if(end==fields[0].c_str()||*end!='\0'||id<=0||id>0x3fff)continue;
        Definition def;def.particle=fs::path(fields[1]).filename().string();
        if(fields.size()>2)def.name=fields[2];
        if(fields.size()>3)def.alternateName=fields[3];
        catalog[static_cast<int>(id)]=std::move(def);
    }
    return true;
}
inline bool readText(const fs::path& path,std::string& value) {
    std::ifstream in(path,std::ios::binary);
    if(!in)return false;
    value.assign(std::istreambuf_iterator<char>(in),{});
    return in.eof()||!in.bad();
}

inline int run(const Options& options,std::ostream& log) {
    const fs::path catalogPath=options.sceneEffectInfo.is_absolute()?
        options.sceneEffectInfo:options.client/options.sceneEffectInfo;
    std::map<int,Definition> catalog;
    const bool catalogLoaded=readCatalog(catalogPath,catalog);
    std::string dx9Msg,dx9Obj,dx9Effect,dx9Set,dx9Loader,glWorld;
    const bool dx9MessageRead=readText(options.dx9GameAppMsg,dx9Msg);
    const bool dx9ObjRead=readText(options.dx9SceneObjFile,dx9Obj);
    const bool dx9EffectRead=readText(options.dx9EffectObj,dx9Effect);
    const bool dx9SetRead=readText(options.dx9EffectSet,dx9Set);
    const bool dx9LoaderRead=readText(options.dx9TableLoader,dx9Loader);
    const bool glWorldRead=readText(options.glSceneWorld,glWorld);
    const bool dx9LoadsTable=dx9MessageRead&&dx9Msg.find("GetType() == 1")!=std::string::npos&&
        dx9Msg.find("AddSceneEffect(infoex[i].GetID())")!=std::string::npos;
    const bool dx9UsesSceneCatalog=dx9SetRead&&dx9LoaderRead&&
        dx9Set.find("CMagicSet::I()->GetRawDataInfo(nTypeID)")!=std::string::npos&&
        dx9Set.find("GetMagicInfo(int nTypeID)")!=std::string::npos&&
        dx9Set.find("#define")!=std::string::npos&&
        dx9Set.find("CEffectSet")!=std::string::npos&&
        dx9Set.find("CMagicSet")!=std::string::npos&&
        dx9Loader.find("LoadRawDataInfo(\"scripts/table/sceneffectinfo\"")!=std::string::npos;
    const bool dx9CreateChecksCatalog=dx9EffectRead&&
        dx9Effect.find("GetMagicInfo(iIdxID)")!=std::string::npos&&
        dx9Effect.find("if(!pInfo)")!=std::string::npos&&
        dx9Effect.find("return FALSE")!=std::string::npos;
    const bool dx9FactoryEvidence=dx9MessageRead&&dx9LoadsTable&&dx9UsesSceneCatalog&&
                                  dx9CreateChecksCatalog;
    const std::string dx9FactoryEvidenceText=!dx9FactoryEvidence?"incomplete_source_chain":
        "map_call+sceneffectinfo_loader+CMagicSet_lookup+Create_null_guard";
    std::error_code ec;
    const fs::path mapDir=options.client/"map";
    if(!fs::is_directory(mapDir)){
        log<<"Missing Client/map directory\n";return 2;
    }
    fs::create_directories(options.out,ec);
    if(ec){log<<"Could not create output directory: "<<ec.message()<<'\n';return 2;}
    std::ofstream csv(options.out/"scene_effect_map_placements.csv");
    std::ofstream summary(options.out/"scene_effect_map_summary.json");
    if(!csv||!summary){log<<"Could not open audit outputs\n";return 2;}
    ba::row(csv,{"map","section","record_index","scene_effect_id","particle",
        "name","alternate_name","catalogued","world_x_centimeters",
        "world_y_centimeters","height_offset","yaw_angle"});

    std::vector<fs::path> paths;
    for(fs::directory_iterator it(mapDir,ec),end;it!=end;it.increment(ec)){
        if(ec){ec.clear();continue;}
        if(!it->is_regular_file())continue;
        if(lower(it->path().extension().string())==".obj")paths.push_back(it->path());
    }
    std::sort(paths.begin(),paths.end());
    std::size_t scanned=0,valid=0,unsupported=0,malformed=0,placements=0,
        catalogued=0,uncatalogued=0,overLimitSections=0;
    std::set<int> ids;
    std::map<int,std::size_t> placementsById;
    for(const fs::path& path:paths){
        ++scanned;
        std::vector<std::uint8_t> bytes;
        if(!readBytes(path,bytes)||bytes.size()<44||
           std::memcmp(bytes.data(),"HF Object File!",15)!=0){++malformed;continue;}
        if(i32(bytes.data()+16)!=600){++unsupported;continue;}
        const int sx=i32(bytes.data()+24),sy=i32(bytes.data()+28);
        const int sectionW=i32(bytes.data()+32),sectionH=i32(bytes.data()+36);
        if(sx<=0||sy<=0||sectionW<=0||sectionH<=0||
           static_cast<std::uint64_t>(sx)*static_cast<std::uint64_t>(sy)>4000000u){
            ++malformed;continue;
        }
        const std::uint64_t sectionCount=static_cast<std::uint64_t>(sx)*sy;
        const std::uint64_t indexEnd=44u+sectionCount*8u;
        if(indexEnd>bytes.size()){++malformed;continue;}
        ++valid;
        bool badRecord=false;
        for(std::uint64_t sec=0;sec<sectionCount;++sec){
            const std::uint8_t* index=bytes.data()+44u+sec*8u;
            const std::int32_t pos=i32(index),count=i32(index+4);
            if(pos<=0||count<=0)continue;
            if(count>100000||static_cast<std::uint64_t>(pos)+
                static_cast<std::uint64_t>(count)*20u>bytes.size()){
                ++malformed;badRecord=true;continue;
            }
            if(count>25)++overLimitSections;
            const int sectionX=static_cast<int>(sec%static_cast<std::uint64_t>(sx));
            const int sectionY=static_cast<int>(sec/static_cast<std::uint64_t>(sx));
            for(int rec=0;rec<count;++rec){
                const std::uint8_t* data=bytes.data()+pos+static_cast<std::size_t>(rec)*20u;
                const std::uint16_t typeId=u16(data);
                if((typeId>>14)!=1)continue; // SSceneObjInfo::GetType()==1
                Placement p;
                p.map=path.filename().string();p.section=static_cast<int>(sec);p.record=rec;
                p.id=typeId&0x3fff;
                p.x=sectionX*sectionW*100+i32(data+4);
                p.y=sectionY*sectionH*100+i32(data+8);
                p.heightOffset=static_cast<std::int16_t>(u16(data+12));
                p.yaw=static_cast<std::int16_t>(u16(data+14));
                const auto found=catalog.find(p.id);
                p.catalogued=found!=catalog.end();
                if(p.catalogued)p.definition=found->second;
                ++placements;ids.insert(p.id);++placementsById[p.id];
                if(p.catalogued)++catalogued;else ++uncatalogued;
                ba::row(csv,{p.map,std::to_string(p.section),std::to_string(p.record),
                    std::to_string(p.id),p.definition.particle,p.definition.name,
                    p.definition.alternateName,p.catalogued?"yes":"no",
                    std::to_string(p.x),std::to_string(p.y),
                    std::to_string(p.heightOffset),std::to_string(p.yaw)});
            }
        }
        (void)badRecord;
    }
    std::ofstream controllers(options.out/"scene_effect_map_controllers.csv");
    if(!controllers){log<<"Could not open controller audit output\n";return 2;}
    ba::row(controllers,{"scene_effect_id","placement_count","catalogued","particle",
        "par_decode","dx9_factory_outcome","dx9_factory_evidence",
        "par_version","controller_name","declared_systems",
        "system_index","controller_type","system_name","count_raw","model_name","error"});
    std::set<fs::path> controllerFilePaths,decodedControllerFilePaths;
    std::size_t controllerSystems=0,controllerDecodeFailures=0;
    for(const int id:ids){
        const auto found=catalog.find(id);
        const bool hasCatalog=found!=catalog.end();
        Definition def;
        if(hasCatalog)def=found->second;
        std::vector<std::uint8_t> parBytes;
        const fs::path parPath=options.client/"effect"/def.particle;
        const bool parPresent=hasCatalog&&!def.particle.empty()&&readBytes(parPath,parBytes);
        pa::Result parsed;
        if(parPresent){controllerFilePaths.insert(parPath);parsed=pa::parse(parBytes);}
        if(parPresent&&parsed.ok)decodedControllerFilePaths.insert(parPath);
        else if(hasCatalog)++controllerDecodeFailures;
        const std::string decode=!hasCatalog?"no_catalog_entry":
            !parPresent?"par_missing":parsed.ok?"decoded":"decode_failed";
        const std::string dx9FactoryOutcome=!dx9FactoryEvidence?"dx9_factory_source_unverified":
            !hasCatalog?"dx9_catalog_record_absent_factory_result_unproven":"dx9_catalog_record_present";
        const std::string version=parPresent?std::to_string(parsed.document.version):"";
        const std::string controller=parPresent?parsed.document.controller:"";
        const std::string declared=parPresent?std::to_string(parsed.document.declaredSystems):"";
        const std::string error=parPresent&&!parsed.ok?parsed.error:"";
        if(parPresent&&parsed.ok&&parsed.document.systems.empty()){
            ba::row(controllers,{std::to_string(id),std::to_string(placementsById[id]),
                hasCatalog?"yes":"no",def.particle,decode,dx9FactoryOutcome,dx9FactoryEvidenceText,
                version,controller,declared,
                "","","","","",error});
        }else if(parPresent&&parsed.ok){
            for(std::size_t i=0;i<parsed.document.systems.size();++i){
                const auto& system=parsed.document.systems[i];
                ba::row(controllers,{std::to_string(id),std::to_string(placementsById[id]),
                    hasCatalog?"yes":"no",def.particle,decode,dx9FactoryOutcome,dx9FactoryEvidenceText,
                    version,controller,declared,
                    std::to_string(i),std::to_string(system.type),system.name,
                    std::to_string(system.count),system.model,error});
                ++controllerSystems;
            }
        }else{
            ba::row(controllers,{std::to_string(id),std::to_string(placementsById[id]),
                hasCatalog?"yes":"no",def.particle,decode,dx9FactoryOutcome,dx9FactoryEvidenceText,
                version,controller,declared,
                "","","","","",error});
        }
    }
    const bool dx9SectionReader=dx9ObjRead&&dx9Obj.find("ReadSectionObjInfo")!=std::string::npos&&
        dx9Obj.find("SSceneObjInfo")!=std::string::npos;
    std::size_t dx9MissingCatalogIds=0;
    if(dx9FactoryEvidence)for(const int id:ids)if(catalog.find(id)==catalog.end())
        ++dx9MissingCatalogIds;
    const bool glSkipsEffects=glWorldRead&&glWorld.find("if (kind != 0) continue")!=std::string::npos;
    summary<<"{\n  \"map_obj_files\": "<<scanned<<",\n  \"valid_v600_maps\": "<<valid
        <<",\n  \"unsupported_version_maps\": "<<unsupported
        <<",\n  \"malformed_maps_or_sections\": "<<malformed
        <<",\n  \"map_sections_with_over_25_records\": "<<overLimitSections
        <<",\n  \"scene_effect_placements\": "<<placements
        <<",\n  \"catalogued_placements\": "<<catalogued
        <<",\n  \"uncatalogued_placements\": "<<uncatalogued
        <<",\n  \"unique_scene_effect_ids\": "<<ids.size()
        <<",\n  \"catalogued_controller_files\": "<<controllerFilePaths.size()
        <<",\n  \"decoded_controller_files\": "<<decodedControllerFilePaths.size()
        <<",\n  \"controller_systems\": "<<controllerSystems
        <<",\n  \"controller_decode_failures_or_missing\": "<<controllerDecodeFailures
        <<",\n  \"dx9_missing_catalog_ids_factory_result_unproven\": "<<dx9MissingCatalogIds
        <<",\n  \"dx9_scene_effect_factory_evidence_complete\": "<<(dx9FactoryEvidence?"true":"false")
        <<",\n  \"dx9_scene_effect_creation_branch_observed\": "<<(dx9LoadsTable?"true":"false")
        <<",\n  \"dx9_reads_section_object_records\": "<<(dx9SectionReader?"true":"false")
        <<",\n  \"gl_scene_world_skips_non_model_kinds\": "<<(glSkipsEffects?"true":"false")
        <<",\n  \"scene_effect_catalog_loaded\": "<<(catalogLoaded?"true":"false")<<"\n}\n";
    log<<"Scene-effect map audit: maps="<<scanned<<" valid="<<valid
       <<" placements="<<placements<<" ids="<<ids.size()
       <<" catalogued="<<catalogued<<" uncatalogued="<<uncatalogued
       <<" controller_files="<<controllerFilePaths.size()
       <<" controller_decoded="<<decodedControllerFilePaths.size()
       <<" systems="<<controllerSystems
       <<" dx9_missing_catalog_result_unproven="<<dx9MissingCatalogIds
       <<" factory_evidence="<<(dx9FactoryEvidence?"complete":"incomplete")
       <<" malformed="<<malformed<<" | \""<<options.out.string()<<"\"\n";
    return malformed?1:0;
}
}} // namespace top::scene_effect_map_audit
