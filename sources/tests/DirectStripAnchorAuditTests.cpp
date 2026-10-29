#include "top/DirectStripAnchorAudit.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unistd.h>

namespace fs=std::filesystem;
template<class T> void put(std::ostream& out,T value){
    out.write(reinterpret_cast<const char*>(&value),sizeof(value));
}
void name(std::ostream& out,const char* text){
    char bytes[32]{};for(int i=0;text[i]&&i<31;++i)bytes[i]=text[i];
    out.write(bytes,sizeof(bytes));
}
void fixturePar(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,7);name(out,"test");put<std::int32_t>(out,1);put<float>(out,1.f);
    put<std::int32_t>(out,6);name(out,"strip");put<std::int32_t>(out,1);
    name(out,"trail.tga");name(out,"");
    for(int i=0;i<3;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(int i=0;i<3;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,1);
    for(int value:{5,6,1,1})put<std::int32_t>(out,value);
    put<float>(out,.5f);put<float>(out,1.f);
    for(int i=0;i<7;++i)put<float>(out,0.f); // direction, acceleration, step
    put<std::uint8_t>(out,0);name(out,""); // v4 model-range data
    for(int i=0;i<3;++i)put<float>(out,0.f); // v5 offset
    put<float>(out,0.f);put<float>(out,.5f); // v6 delay/play
    put<std::int32_t>(out,1); // one direct CMPStrip
    put<std::int32_t>(out,80);put<std::int32_t>(out,1);put<std::int32_t>(out,2);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<float>(out,.5f);put<float>(out,.03f);name(out,"swordlight02");
    put<std::int32_t>(out,5);put<std::int32_t>(out,2);
}
void write(const fs::path& path,const std::string& value){std::ofstream(path)<<value;}
std::string read(const fs::path& path){std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
int main(){
    const fs::path root=fs::temp_directory_path()/(
        "top_direct_strip_audit_"+std::to_string(getpid()));
    fs::remove_all(root);
    fs::create_directories(root/"Client/effect");
    fs::create_directories(root/"Client/scripts/table");
    fs::create_directories(root/"src");
    fixturePar(root/"Client/effect/trail.par");
    fs::copy_file(root/"Client/effect/trail.par",root/"Client/effect/catalog_only.par");
    write(root/"Client/scripts/table/sceneffectinfo.txt",
        "217\tcatalog_only.par\t\tSea Wave\t0\t0\t-1\t-1\t0\t1\t0\t-1\n");
    write(root/"Client/scripts/table/magicsingleinfo.txt",
        "//ID\tname\tmodel_count\tmodel\tvelocity\tparticle_count\tparticle\tdummy\trender\tlight\tresult\n"
        "14\tTest Magic\t1\titem.eff\t10\t1\ttrail\t-1\t2\t0\t0\n");
    write(root/"deps.csv","skill_id,skill_name,channel,referenced_name\n"
        "81,Test Trail,item,trail.par\n"
        "82,Actor Trail,actor,trail.par\n");
    write(root/"model.cpp","void Play(){ if (_pItem) {} else if(_pCha) {} }\n");
    write(root/"particle.cpp","void SetStripCharacter(MPCharacter* p) {}\n");
    write(root/"effect.cpp","void Setup(){ ctrl.SetStripItem(item,true); }\n");
    write(root/"client_effect.cpp",
        "void Setup(){ ctrl->SetStripCharacter(character); ctrl->SetStripItem(item); }\n");
    write(root/"gl_effect.cpp","effect.kind!=EffectEvent::Cast; playerActorCast; playerItemCast;\n");
    write(root/"gl_actor.cpp","playerDummyWorldMatrices() const {}\nplayerItemDummyWorldMatrices() const {}\n");
    top::direct_strip_anchor_audit::Options options;
    options.client=root/"Client";options.out=root/"out";options.deps=root/"deps.csv";
    options.dx9ModelEff=root/"model.cpp";options.dx9ParticleCtrl=root/"particle.cpp";
    options.dx9EffectCtrl=root/"effect.cpp";options.dx9ClientEffectObj=root/"client_effect.cpp";
    options.glEffectSystem=root/"gl_effect.cpp";
    options.glActorSkin=root/"gl_actor.cpp";
    options.sceneEffectInfo=root/"Client/scripts/table/sceneffectinfo.txt";
    options.magicSingleInfo=root/"Client/scripts/table/magicsingleinfo.txt";
    std::ostringstream log;
    assert(top::direct_strip_anchor_audit::run(options,log)==0);
    const auto csv=read(root/"out/direct_strip_anchor_coverage.csv");
    assert(csv.find("file,index,dummy_a,dummy_b,skills,channels")!=std::string::npos);
    assert(csv.find("trail.par,0,1,2,81;82,actor;item")!=std::string::npos);
    assert(csv.find("actor_setter_callsite_present_runtime_unproven")!=std::string::npos);
    assert(csv.find("item_setter_callsite_present_runtime_unproven")!=std::string::npos);
    assert(csv.find("catalog_only.par,0,1,2,,,217,Sea Wave,yes")!=std::string::npos);
    assert(csv.find("trail.par,0,1,2,81;82,actor;item,,,no,14,Test Magic")!=std::string::npos);
    assert(csv.find("scene_effect_catalogued_runtime_unproven")!=std::string::npos);
    const auto summary=read(root/"out/direct_strip_anchor_summary.json");
    assert(summary.find("\"direct_strip_records\": 2")!=std::string::npos);
    assert(summary.find("\"scene_effect_catalogued_records\": 1")!=std::string::npos);
    assert(summary.find("\"magic_single_catalog_loaded\": true")!=std::string::npos);
    assert(summary.find("\"magic_single_linked_direct_strip_records\": 1")!=std::string::npos);
    assert(summary.find("\"dx9_mpeffectctrl_loop_true_call_observed\": true")!=std::string::npos);
    assert(summary.find("\"dx9_item_setter_callsite_observed\": true")!=std::string::npos);
    assert(summary.find("\"dx9_client_effectobj_item_setter_callsite_observed\": true")!=std::string::npos);
    assert(summary.find("\"dx9_mpeffectctrl_item_setter_callsite_observed\": true")!=std::string::npos);
    assert(summary.find("\"dx9_character_setter_callsite_observed\": true")!=std::string::npos);
    assert(summary.find("\"dx9_client_effectobj_character_setter_callsite_observed\": true")!=std::string::npos);
    fs::remove_all(root);
}
