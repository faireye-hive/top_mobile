#include "top/SceneEffectMapAudit.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs=std::filesystem;
void u16(std::vector<std::uint8_t>& b,std::size_t p,std::uint16_t v){
    b[p]=static_cast<std::uint8_t>(v);b[p+1]=static_cast<std::uint8_t>(v>>8);
}
void i32(std::vector<std::uint8_t>& b,std::size_t p,std::int32_t v){
    const auto n=static_cast<std::uint32_t>(v);
    for(int i=0;i<4;++i)b[p+static_cast<std::size_t>(i)]=
        static_cast<std::uint8_t>(n>>(8*i));
}
void placement(std::vector<std::uint8_t>& b,std::size_t p,int id,int x,int y,int h,int yaw){
    u16(b,p,static_cast<std::uint16_t>(0x4000|id));
    i32(b,p+4,x);i32(b,p+8,y);u16(b,p+12,static_cast<std::uint16_t>(h));
    u16(b,p+14,static_cast<std::uint16_t>(yaw));u16(b,p+16,0);
}
std::string read(const fs::path& path){
    std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};
}
void writeObj(const fs::path& path){
    std::vector<std::uint8_t> b(44+8+2*20,0);
    const char magic[]="HF Object File!";
    for(std::size_t i=0;i<15;++i)b[i]=static_cast<std::uint8_t>(magic[i]);
    i32(b,16,600);i32(b,20,static_cast<std::int32_t>(b.size()));
    i32(b,24,1);i32(b,28,1);i32(b,32,8);i32(b,36,8);
    i32(b,44,52);i32(b,48,2);
    placement(b,52,217,345,678,-3,90);
    placement(b,72,21,901,234,0,180);
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
}
void writeEmptyPar(const fs::path& path){
    std::vector<std::uint8_t> b(44,0);
    i32(b,0,7);
    const char controller[]="scene controller";
    for(std::size_t i=0;i<sizeof(controller)-1;++i)b[4+i]=static_cast<std::uint8_t>(controller[i]);
    i32(b,36,0);
    i32(b,40,0); // controller life float 0, little-endian
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
}
int main(){
    const fs::path root=fs::temp_directory_path()/
        ("scene_effect_map_audit_"+std::to_string(getpid()));
    fs::remove_all(root);
    fs::create_directories(root/"Client/map");
    fs::create_directories(root/"Client/effect");
    fs::create_directories(root/"Client/scripts/table");
    fs::create_directories(root/"src");
    writeObj(root/"Client/map/test.obj");
    writeEmptyPar(root/"Client/effect/sea.par");
    {std::ofstream out(root/"Client/scripts/table/sceneffectinfo.txt");
     out<<"217\tsea.par\tLocalized\tAlt Name\t0\t0\n";}
    {std::ofstream(root/"src/GameAppMsg.cpp")<<
        "if (infoex[i].GetType() == 1) pScene->AddSceneEffect(infoex[i].GetID());\n";}
    {std::ofstream(root/"src/EffectObj.cpp")<<
        "CMagicInfo* pInfo = GetMagicInfo(iIdxID);\nif(!pInfo) return FALSE;\n";}
    {std::ofstream(root/"src/EffectSet.h")<<
        "#define CEffectSet CMagicSet\n"
        "inline CMagicInfo* GetMagicInfo(int nTypeID) {\n"
        "return (CMagicInfo*)CMagicSet::I()->GetRawDataInfo(nTypeID);\n}\n";}
    {std::ofstream(root/"src/GameAppInterface.cpp")<<
        "LoadRawDataInfo(\"scripts/table/sceneffectinfo\", bBinary);\n";}
    {std::ofstream(root/"src/SceneObjFile.cpp")<<
        "ReadSectionObjInfo SSceneObjInfo\n";}
    {std::ofstream(root/"src/SceneWorld.cpp")<<
        "if (kind != 0) continue;\n";}
    top::scene_effect_map_audit::Options options;
    options.client=root/"Client";options.out=root/"out";
    options.dx9GameAppMsg=root/"src/GameAppMsg.cpp";
    options.dx9SceneObjFile=root/"src/SceneObjFile.cpp";
    options.dx9EffectObj=root/"src/EffectObj.cpp";
    options.dx9EffectSet=root/"src/EffectSet.h";
    options.dx9TableLoader=root/"src/GameAppInterface.cpp";
    options.glSceneWorld=root/"src/SceneWorld.cpp";
    std::ostringstream log;
    assert(top::scene_effect_map_audit::run(options,log)==0);
    const auto csv=read(root/"out/scene_effect_map_placements.csv");
    assert(csv.find("map,section,record_index,scene_effect_id,particle")!=std::string::npos);
    assert(csv.find("test.obj,0,0,217,sea.par,Localized,Alt Name,yes,345,678,-3,90")!=std::string::npos);
    assert(csv.find("test.obj,0,1,21,,,,no,901,234,0,180")!=std::string::npos);
    const auto controllers=read(root/"out/scene_effect_map_controllers.csv");
    assert(controllers.find("scene_effect_id,placement_count,catalogued,particle,par_decode,dx9_factory_outcome,dx9_factory_evidence")!=std::string::npos);
    assert(controllers.find("217,1,yes,sea.par,decoded,dx9_catalog_record_present,map_call+sceneffectinfo_loader+CMagicSet_lookup+Create_null_guard,7,scene controller,0")!=std::string::npos);
    assert(controllers.find("21,1,no,,no_catalog_entry,dx9_catalog_record_absent_factory_result_unproven,map_call+sceneffectinfo_loader+CMagicSet_lookup+Create_null_guard")!=std::string::npos);
    const auto json=read(root/"out/scene_effect_map_summary.json");
    assert(json.find("\"scene_effect_placements\": 2")!=std::string::npos);
    assert(json.find("\"catalogued_placements\": 1")!=std::string::npos);
    assert(json.find("\"uncatalogued_placements\": 1")!=std::string::npos);
    assert(json.find("\"catalogued_controller_files\": 1")!=std::string::npos);
    assert(json.find("\"decoded_controller_files\": 1")!=std::string::npos);
    assert(json.find("\"dx9_scene_effect_creation_branch_observed\": true")!=std::string::npos);
    assert(json.find("\"dx9_missing_catalog_ids_factory_result_unproven\": 1")!=std::string::npos);
    assert(json.find("\"dx9_scene_effect_factory_evidence_complete\": true")!=std::string::npos);
    assert(json.find("\"gl_scene_world_skips_non_model_kinds\": true")!=std::string::npos);
    options.dx9EffectObj=root/"src/missing_EffectObj.cpp";
    options.out=root/"out_incomplete";
    assert(top::scene_effect_map_audit::run(options,log)==0);
    const auto incomplete=read(root/"out_incomplete/scene_effect_map_controllers.csv");
    assert(incomplete.find("21,1,no,,no_catalog_entry,dx9_factory_source_unverified,incomplete_source_chain")!=std::string::npos);
    assert(read(root/"out_incomplete/scene_effect_map_summary.json").find(
        "\"dx9_scene_effect_factory_evidence_complete\": false")!=std::string::npos);
    fs::remove_all(root);
}
