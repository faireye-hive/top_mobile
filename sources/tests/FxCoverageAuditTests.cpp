// SK2.14 - generates small historical-format .par fixtures; no Client assets
// or OpenGL context required. Run with asserts enabled (Debug build).
#include "top/FxCoverageAudit.h"
#include <stdexcept>
#define CHECK(expr) do { if (!(expr)) throw std::runtime_error("check failed: " #expr); } while (0)
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <limits>

namespace fs=std::filesystem;
namespace fx=top::fx_coverage;
static void write32(std::ostream& out,const std::string& value){
    char buffer[32]{};std::memcpy(buffer,value.data(),std::min<std::size_t>(31,value.size()));
    out.write(buffer,32);
}
template<class T> static void w(std::ostream& out,T v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));}
static void par(const fs::path& file,int type,std::string model="",int count=2,float accelX=0.f,
                float step=.1f,int version=2,bool shade=false,bool billboard=true,
                std::string hitEffect=""){
    std::ofstream out(file,std::ios::binary);
    w<uint32_t>(out,version); write32(out,"test_controller");w<int32_t>(out,1);
    if(version>=3)w<float>(out,2.f);
    w<int32_t>(out,type);write32(out,"test_system");w<int32_t>(out,count);
    write32(out,"a.tga");write32(out,model);
    for(int i=0;i<3;++i)w<float>(out,float(i));
    w<uint16_t>(out,2);
    for(int i=0;i<2;++i)w<float>(out,1.f);
    for(int i=0;i<6;++i)w<float>(out,0.f);
    for(int i=0;i<8;++i)w<float>(out,1.f);
    w<uint8_t>(out,billboard?1:0);
    w<int32_t>(out,5);w<int32_t>(out,6);w<int32_t>(out,1);w<int32_t>(out,1);
    w<float>(out,0.5f);w<float>(out,1.f);
    for(int i=0;i<3;++i)w<float>(out,0.f);
    for(int i=0;i<3;++i)w<float>(out,i==0?accelX:0.f);
    w<float>(out,step);
    if(version>3){w<uint8_t>(out,0);write32(out,"");}
    if(version>4)for(int i=0;i<3;++i)w<float>(out,0.f);
    if(version>5){w<float>(out,0.f);w<float>(out,0.f);}
    if(version>8)w<uint8_t>(out,0);
    if(version>9)w<uint8_t>(out,shade?1:0);
    if(version>10)write32(out,hitEffect);
}
static void effWithNonFiniteModelParam(const fs::path& file){
    std::ofstream out(file,std::ios::binary);
    w<std::uint32_t>(out,6);w<std::int32_t>(out,0);
    w<std::uint8_t>(out,0);write32(out,"");w<std::uint8_t>(out,0);write32(out,"");
    w<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)w<float>(out,0.f);
    w<std::int32_t>(out,1);write32(out,"base");
    for(int value:{1,5,7})w<std::int32_t>(out,value);
    w<float>(out,5.f);w<std::uint16_t>(out,1);w<float>(out,5.f);
    for(int i=0;i<3;++i)w<float>(out,1.f);
    for(int i=0;i<6;++i)w<float>(out,0.f);
    for(int i=0;i<4;++i)w<float>(out,1.f);
    w<std::uint16_t>(out,4);w<std::uint16_t>(out,1);w<float>(out,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})w<float>(out,uv);
    w<std::uint16_t>(out,0);w<float>(out,0.f);write32(out,"base.tga");
    write32(out,"RectPlane");w<std::uint8_t>(out,1);w<std::int32_t>(out,1);
    w<std::int32_t>(out,8);w<float>(out,std::numeric_limits<float>::quiet_NaN());
    w<float>(out,1.f);w<float>(out,1.f);
    w<std::uint16_t>(out,0);w<float>(out,0.f);w<float>(out,0.f);
    w<std::int32_t>(out,0);w<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)w<float>(out,0.f);
    w<std::uint8_t>(out,0);
}
static std::string read(const fs::path& file){
    std::ifstream in(file);return {std::istreambuf_iterator<char>(in),{}};
}
int main(){
    CHECK(fx::normalizedBase("6.par")=="00000006.par");
    CHECK(fx::normalizedBase("effect/WOLF01.EFF")=="wolf01.eff");
    CHECK(fx::classification("RectPlane")=="native_primitive");
    CHECK(fx::classification("other.lgo")=="external_lgo");
    CHECK(fx::classification("unknown")=="unrecognized_model");
    const auto shrinkTimelineFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="shrink_pool_recycle";});
    CHECK(shrinkTimelineFeature!=fx::features().end());
    top::par_audit::System shrinkSystem;shrinkSystem.type=12;
    CHECK(shrinkTimelineFeature->second(shrinkSystem));
    shrinkSystem.type=11;CHECK(!shrinkTimelineFeature->second(shrinkSystem));
    const auto windWorldFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="wind_world_rectplane_rotation";});
    CHECK(windWorldFeature!=fx::features().end());
    top::par_audit::System windWorldSystem;windWorldSystem.type=7;
    windWorldSystem.model="RectPlane";windWorldSystem.billboard=false;
    CHECK(windWorldFeature->second(windWorldSystem));
    windWorldSystem.billboard=true;CHECK(!windWorldFeature->second(windWorldSystem));
    const auto snowFireEffFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="snow_fire_shared_nested_eff_timeline";});
    CHECK(snowFireEffFeature!=fx::features().end());
    top::par_audit::System snowEffSystem;snowEffSystem.type=1;snowEffSystem.model="child.eff";
    CHECK(snowFireEffFeature->second(snowEffSystem));
    snowEffSystem.type=2;CHECK(snowFireEffFeature->second(snowEffSystem));
    snowEffSystem.mediayEff=true;CHECK(!snowFireEffFeature->second(snowEffSystem));
    snowEffSystem.model="child.lgo";snowEffSystem.mediayEff=false;
    CHECK(!snowFireEffFeature->second(snowEffSystem));
    windWorldSystem.billboard=false;windWorldSystem.model="child.eff";
    CHECK(!windWorldFeature->second(windWorldSystem));
    CHECK(top::blend_audit::outsideClient("Client","build/fx-coverage"));
    auto src=fx::readSource("sources/Engine/src/top/EffectSystem.cpp");
    CHECK(src.valid);
    CHECK(!fx::findType(src,2).outside.empty());
    CHECK(!fx::findType(src,8).outside.empty());
    std::vector<fx::TableRow> coverageRules;std::string coverageError;
    CHECK(fx::readTable("sources/auditoria-historica/fx_port_coverage.csv",coverageRules,coverageError));
    const auto range2Rule=std::find_if(coverageRules.begin(),coverageRules.end(),
        [](const auto& row){return row.kind=="feature"&&row.key=="range2_frame_update_clock";});
    CHECK(range2Rule!=coverageRules.end());
    CHECK(range2Rule->note.find("nominal-60Hz")!=std::string::npos);
    CHECK(range2Rule->note.find("actual FrameMove(dt)")!=std::string::npos);
    CHECK(range2Rule->note.find("TOP_HISTORICAL_RANGE2_ACTUAL_DT")!=std::string::npos);
    const auto roundRule=std::find_if(coverageRules.begin(),coverageRules.end(),
        [](const auto& row){return row.kind=="feature"&&row.key=="round_frame_update_clock";});
    CHECK(roundRule!=coverageRules.end());
    CHECK(roundRule->note.find("one curve frame")!=std::string::npos);
    CHECK(roundRule->note.find("TOP_HISTORICAL_ROUND_TIMELINE")!=std::string::npos);
    CHECK(roundRule->note.find("nested .eff clocks")==std::string::npos);
    const auto roundFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="round_frame_update_clock";});
    CHECK(roundFeature!=fx::features().end());
    top::par_audit::System roundSystem;roundSystem.type=9;
    CHECK(roundFeature->second(roundSystem));
    roundSystem.type=12;CHECK(!roundFeature->second(roundSystem));
    const auto roundEffFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="round_nested_eff_frame_step";});
    CHECK(roundEffFeature!=fx::features().end());
    roundSystem.type=9;roundSystem.model="round-child.eff";
    CHECK(roundEffFeature->second(roundSystem));
    roundSystem.model="round-child.lgo";
    CHECK(!roundEffFeature->second(roundSystem));
    const auto roundEffRule=std::find_if(coverageRules.begin(),coverageRules.end(),
        [](const auto& row){return row.kind=="feature"&&row.key=="round_nested_eff_frame_step";});
    CHECK(roundEffRule!=coverageRules.end());
    CHECK(roundEffRule->token=="roundEffTimelines");
    CHECK(roundEffRule->note.find("normal roundEffTimelines advances a group")!=std::string::npos);
    CHECK(roundEffRule->note.find("GL ROUND branch has no child timeline")==std::string::npos);
    const auto dummyRule=std::find_if(coverageRules.begin(),coverageRules.end(),
        [](const auto& row){return row.kind=="type"&&row.key=="16";});
    CHECK(dummyRule!=coverageRules.end());
    CHECK(dummyRule->name=="DUMMY");
    CHECK(dummyRule->note.find("TOP_HISTORICAL_DUMMY_CONTROLLER")!=std::string::npos);
    CHECK(dummyRule->note.find("ground quad instead of full dummy controller")==std::string::npos);
    const auto blast3EffFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="blast3_nested_eff_region_clock";});
    CHECK(blast3EffFeature!=fx::features().end());
    top::par_audit::System blast3EffSystem;blast3EffSystem.type=11;blast3EffSystem.model="child.eff";
    CHECK(blast3EffFeature->second(blast3EffSystem));
    blast3EffSystem.type=10;CHECK(!blast3EffFeature->second(blast3EffSystem));
    const auto blastGroupFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="blast_group_cycle";});
    CHECK(blastGroupFeature!=fx::features().end());
    top::par_audit::System blastGroupSystem;blastGroupSystem.type=3;
    CHECK(blastGroupFeature->second(blastGroupSystem));
    blastGroupSystem.type=10;CHECK(!blastGroupFeature->second(blastGroupSystem));
    const auto modelNestedEffFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="model_nested_eff_frame_step";});
    CHECK(modelNestedEffFeature!=fx::features().end());
    top::par_audit::System modelNestedEffSystem;modelNestedEffSystem.type=5;
    modelNestedEffSystem.model="child.eff";
    CHECK(modelNestedEffFeature->second(modelNestedEffSystem));
    modelNestedEffSystem.type=6;CHECK(!modelNestedEffFeature->second(modelNestedEffSystem));
    const auto modelMidpointFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="model_midpoint_position";});
    CHECK(modelMidpointFeature!=fx::features().end());
    top::par_audit::System modelMidpointSystem;modelMidpointSystem.type=5;
    modelMidpointSystem.range={{1.f,0.f,0.f}};
    CHECK(modelMidpointFeature->second(modelMidpointSystem));
    modelMidpointSystem.range={{0.f,0.f,0.f}};
    CHECK(!modelMidpointFeature->second(modelMidpointSystem));
    modelMidpointSystem.type=6;modelMidpointSystem.range={{1.f,0.f,0.f}};
    CHECK(!modelMidpointFeature->second(modelMidpointSystem));
    const auto shadeHeightFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="shade_terrain_grid_height";});
    CHECK(shadeHeightFeature!=fx::features().end());
    top::par_audit::System shadeSystem;shadeSystem.type=13;
    CHECK(shadeHeightFeature->second(shadeSystem));
    shadeSystem.type=12;CHECK(!shadeHeightFeature->second(shadeSystem));
    const auto shadeMismatchFeature=std::find_if(fx::features().begin(),fx::features().end(),
        [](const auto& feature){return feature.first=="shade_type_mismatch";});
    CHECK(shadeMismatchFeature!=fx::features().end());
    top::par_audit::System shadeFlagSystem;shadeFlagSystem.type=13;
    shadeFlagSystem.hasShadeFlag=true;shadeFlagSystem.shade=true;
    CHECK(!shadeMismatchFeature->second(shadeFlagSystem));
    shadeFlagSystem.type=6;CHECK(shadeMismatchFeature->second(shadeFlagSystem));
    shadeFlagSystem.type=13;shadeFlagSystem.shade=false;
    CHECK(shadeMismatchFeature->second(shadeFlagSystem));
    shadeFlagSystem.hasShadeFlag=false;CHECK(!shadeMismatchFeature->second(shadeFlagSystem));
    const auto shadeFixture=fs::temp_directory_path()/"top_fx_coverage_shade_flag.par";
    par(shadeFixture,13,"",1,0.f,.1f,10,true);
    const auto parsedShade=top::par_audit::inspectFile(shadeFixture.string());
    CHECK(parsedShade.ok&&parsedShade.document.systems.size()==1);
    CHECK(parsedShade.document.systems[0].hasShadeFlag&&parsedShade.document.systems[0].shade);
    CHECK(!shadeMismatchFeature->second(parsedShade.document.systems[0]));
    fs::remove(shadeFixture);
    const auto tmp=fs::temp_directory_path()/"top_fx_coverage_test_SK214";
    fs::remove_all(tmp);fs::create_directories(tmp/"Client/effect");
    fs::create_directories(tmp/"Client/scripts/table");
    fs::create_directories(tmp/"Client/effect/nested");
    par(tmp/"Client/effect/fire.par",2,"",30,.5f,.1f,11,false,true,"fire(a).par");
    par(tmp/"Client/effect/00000006.par",8,"child.eff");
    par(tmp/"Client/effect/child.par",2,"",2,0.f,0.f);
    {std::ofstream f(tmp/"Client/effect/child.eff",std::ios::binary);f<<"bad fixture";}
    {std::ofstream f(tmp/"Client/effect/unreferenced.eff",std::ios::binary);f<<"bad fixture";}
    {std::ofstream f(tmp/"Client/effect/nested/nested.eff",std::ios::binary);f<<"bad fixture";}
    par(tmp/"Client/effect/dummy.par",13,"",2,0.f,.1f,10,true);
    par(tmp/"Client/effect/item_dummy.par",16,"",2,0.f,.1f,10,false);
    par(tmp/"Client/effect/blast.par",3,"",2,2.f);
    par(tmp/"Client/effect/wind_world.par",7,"RectPlane",1,0.f,.2f,2,false,false);
    par(tmp/"Client/effect/strip.par",6,"rawparams.eff",2,0.f,.1f,11,false,true,"strip-hit.par");
    effWithNonFiniteModelParam(tmp/"Client/effect/rawparams.eff");
    par(tmp/"Client/effect/model.par",5,"child.eff",2,0.f,.1f,11,false,true,"model-hit.par");
    par(tmp/"Client/effect/range2.par",15,"",2,0.f,.1f,11,false,true,"range2-hit.par");
    par(tmp/"Client/effect/ripple.par",4);
    par(tmp/"Client/effect/ripple_nested.par",4,"child.eff");
    {std::ofstream f(tmp/"deps.csv");f<<"skill_id,skill_name,channel,referenced_name\n"
        <<"81,Illusion Slash,cast,6.par\n"
        <<"84,Berserk,cast,fire.par\n"
        <<"84,Berserk,cast,dummy.par\n"
        <<"82,Backstab,impact,blast.par\n"
        <<"83,Strip test,cast,strip.par\n"
        <<"42,Unmatched,cast,absent.par\n";}
    {std::ofstream f(tmp/"Client/scripts/table/sceneffectinfo.txt");
        f<<"880\tstrip.par\tfixture\n"
         <<"881\tchild.par\tfixture sibling PAR; does not prove EFF use\n"
         <<"882\titem_dummy.par\titem channel fixture\n";}
    {std::ofstream f(tmp/"Client/scripts/table/skillinfo.txt");
        std::vector<std::string> columns(74,"0");
        columns[0]="900";columns[1]="Item channel fixture";
        columns[54]="882,2";columns[55]="0,0";
        for(std::size_t i=0;i<columns.size();++i){if(i)f<<'\t';f<<columns[i];}
        f<<'\n';}
    {std::ofstream f(tmp/"mpres.cpp");
        f<<"bool CMPResManger::LoadTotalEffect() {\n"
         <<"  const char* pattern = \"\\\\*.eff\";\n"
         <<"  auto handle = FindFirstFile(pattern, &data);\n"
         <<"  do { LoadEffectFromFile(index, path); _mapEffect[data.cFileName] = index; }\n"
         <<"  while (FindNextFile(handle, &data));\n}\n";}
    {std::ofstream f(tmp/"MPParticleSys.h");
        f<<"#define PARTTICLE_FIRE 2\n#define PARTTICLE_MODEL 5\n"
         <<"#define PARTTICLE_STRIP 6\n#define PARTTICLE_RANGE2 15\n";}
    {std::ofstream f(tmp/"MPParticleSys.cpp");
        f<<"FrameUpdate = _FrameMoveFire;\n"
         <<"FrameUpdate = _FrameMoveRange2;\n"
         <<"void _FrameMoveFire() {\n SendResMessage(m_strHitEff);\n}\n"
         <<"void _FrameMoveRange2() {\n SendResMessage(m_strHitEff);\n}\n";}
    {std::ofstream f(tmp/"Character.cpp");
        f<<"bool CCharacter::ItemEffect(int nEffectID, int nItemDummy) {\n"
         <<" if(nEffectID==0 || nItemDummy==-1) return false;\n}\n";}
    {std::ofstream f(tmp/"STAttack.cpp");
        f<<"_pSelf->ItemEffect(effect1, _pSkillInfo->sItemDummyLink, angle);\n"
         <<"_pSelf->ItemEffect(effect2, _pSkillInfo->sItemDummyLink, angle);\n";}
    {std::ofstream f(tmp/"strips.csv");f<<"file,index,max_length,dummy_a,dummy_b,life,step,texture,src_blend,dst_blend,implementation\n"
        <<"effect/00000006.par,0,100,0,1,0.5,0.03,swordlight02,5,2,not_rendered\n";}
    fx::Options opts;
    opts.client=tmp/"Client";opts.output=tmp/"out";
    opts.deps=tmp/"deps.csv";
    opts.dx9Source=tmp/"mpres.cpp";
    opts.dx9ParticleSource=tmp/"MPParticleSys.cpp";
    opts.dx9ParticleHeader=tmp/"MPParticleSys.h";
    opts.dx9CharacterSource=tmp/"Character.cpp";
    opts.dx9SkillAttackSource=tmp/"STAttack.cpp";
    opts.directStrips=tmp/"strips.csv";
    const auto dx9Loader=fx::readDx9EffCatalog(opts.dx9Source);
    CHECK(dx9Loader.verified());
    CHECK(dx9Loader.wildcardLine>0&&dx9Loader.enumeratorLine>0&&
          dx9Loader.loadLine>0&&dx9Loader.indexLine>0);
    {std::ofstream f(tmp/"EffectSystem.cpp");
        f<<"void EffectSystem::emit(const EffectEvent& event) {\n"
         <<" if (historicalItemEffectRequireDummyEnabled() && requested.item && requested.dummy == -1) continue;\n}\n";}
    const auto itemDummyEvidence=fx::readItemEffectDummyEvidence(
        opts.dx9CharacterSource,opts.dx9SkillAttackSource,tmp/"EffectSystem.cpp");
    CHECK(itemDummyEvidence.verified());
    CHECK(itemDummyEvidence.dx9GuardLine==2&&itemDummyEvidence.dx9SkillDummyLines.size()==2);
    CHECK(itemDummyEvidence.dx9SkillDummyLines[0]==1&&itemDummyEvidence.dx9SkillDummyLines[1]==2);
    CHECK(itemDummyEvidence.glOptInLine==2&&itemDummyEvidence.glFilterLine==2);
    std::ostringstream log;
    CHECK(fx::run(opts,log)==0);
    auto summary=read(opts.output/"fx_coverage_summary.json");
    CHECK(summary.find("\"par_systems\": 12")!=std::string::npos);
    CHECK(summary.find("\"strict_eff_decode_failures\": 4")!=std::string::npos);
    CHECK(summary.find("\"strict_only_eff_decode_failures\": 1")!=std::string::npos);
    CHECK(summary.find("\"dx9_eff_catalog_loader_verified\": true")!=std::string::npos);
    CHECK(summary.find("\"hit_effect_dx9_dispatch_verified\": true")!=std::string::npos);
    CHECK(summary.find("\"hit_effect_gl_event_paths_verified\": true")!=std::string::npos);
    CHECK(summary.find("\"hit_effect_dx9_sendresmessage_sites\": 2")!=std::string::npos);
    CHECK(summary.find("\"item_effect_dummy_dx9_rule_and_gl_gate_verified\": true")!=std::string::npos);
    CHECK(summary.find("\"item_effect_channel_rows\": 1")!=std::string::npos);
    CHECK(summary.find("\"item_effect_channel_skills\": 1")!=std::string::npos);
    CHECK(summary.find("\"item_effect_dummy_minus_one_channels\": 0")!=std::string::npos);
    CHECK(summary.find("\"direct_strip_records\": 1")!=std::string::npos);
    CHECK(summary.find("\"table_drift\": 0")!=std::string::npos);
    CHECK(read(opts.output/"fx_skill_gaps.csv").find("81,Illusion Slash")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("direct_cmpstrip")!=std::string::npos);
    const auto hitEffects=read(opts.output/"fx_hit_effect_coverage.csv");
    CHECK(hitEffects.find("2,FIRE,1,1,")!=std::string::npos);
    CHECK(hitEffects.find("5,MODEL,1,1,")!=std::string::npos);
    CHECK(hitEffects.find("6,STRIP,1,1,")!=std::string::npos);
    CHECK(hitEffects.find("15,RANGE2,1,1,")!=std::string::npos);
    CHECK(hitEffects.find("serialized_field_without_dx9_runtime_dispatch")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:hit_effect")==std::string::npos);
    // SK2.15-A: the auditor must not continue reporting "not rendered" after
    // the renderer adds genuine two-dummy CMPStrip geometry.
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("partial_anchor_dependent")!=std::string::npos);
    CHECK(read(opts.output/"fx_missing_dependencies.csv").find("absent.par")!=std::string::npos);
    CHECK(read(opts.output/"fx_type_coverage.csv").find("8,ARRAW,1,1,1,approximate")!=std::string::npos);
    CHECK(read(opts.output/"fx_type_coverage.csv").find("13,SHADE,1,1,1,partial")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "count_over_24,SNOW/FIRE count above GL draw limit,1,1,1,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "snow_fire_step,SNOW/FIRE nonzero emitter step,1,1,1,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "snow_fire_random_acceleration,SNOW/FIRE random acceleration,1,1,1,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "shrink_pool_recycle,SHRINK shared emitter and slot recycle timeline,0,0,0,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "wind_world_rectplane_rotation,WIND non-billboard RectPlane rotation,1,1,0,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "strip_nested_eff_frame_step,STRIP nested .eff frame stepping,1,1,1,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "blast_discrete_acceleration,BLAST with nonzero acceleration,1,1,1,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "model_frame_step,MODEL with multiple curve frames,1,1,0,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "model_midpoint_position,MODEL parent midpoint position,1,1,0,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "model_nested_eff_frame_step,MODEL nested .eff frame stepping,1,1,0,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "shade_terrain_grid_height,shade terrain tile/subgrid and block height,1,1,1,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "shade_type_mismatch,serialized SHADE flag disagrees with controller type,0,0,0,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "arraw_nested_eff_frame_step,ARRAW nested .eff frame stepping,1,1,1,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:arraw_nested_eff_frame_step")==std::string::npos);
    CHECK(read(opts.output/"fx_decode_failures.csv").find("child.eff,eff,not_fully_decoded")!=std::string::npos);
    CHECK(read(opts.output/"fx_decode_failures.csv").find("unreferenced.eff,eff,not_fully_decoded")!=std::string::npos);
    CHECK(read(opts.output/"fx_decode_failures.csv").find("nested.eff,eff,not_fully_decoded")!=std::string::npos);
    const auto effUse=read(opts.output/"fx_eff_usefulness.csv");
    CHECK(effUse.find("child.eff,failed,failed,referenced_by_par,3,")!=std::string::npos);
    CHECK(effUse.find("unreferenced.eff,failed,failed,no_direct_reference_found,0,,,,,,,no,n/a,n/a,n/a,n/a,n/a,enumerated_loaded_indexed")!=std::string::npos);
    CHECK(effUse.find("nested.eff,failed,failed,no_direct_reference_found,0,,,,,,,no,n/a,n/a,n/a,n/a,n/a,outside_top_level_effect_scan")!=std::string::npos);
    CHECK(effUse.find("00000006.par")!=std::string::npos);
    CHECK(effUse.find("effect/child.par,881,")!=std::string::npos);
    const auto itemChannels=read(opts.output/"fx_skill_item_channels.csv");
    CHECK(itemChannels.find("900,Item channel fixture,item1,882,2,item_dummy.par,effect/item_dummy.par,16,no,0,2,1;2,")!=std::string::npos);
    CHECK(itemChannels.find("source_rule_and_normal_filter_verified")!=std::string::npos);
    CHECK(itemChannels.find("900,Item channel fixture,item2") == std::string::npos);
    CHECK(effUse.find("rawparams.eff,failed,decoded_with_raw_model_params,referenced_by_par,1,effect/strip.par,880,83,,,,no,1,RectPlane,1,0")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "ripple_emission_recycle,RIPPLE nonzero emission step,2,2,0,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "ripple_nested_eff_frame_step,RIPPLE nested .eff frame stepping,1,1,0,referenced_outside_loader")!=std::string::npos);
    // The fourth synthetic file added for BLAST acceleration also authors filters.
    CHECK(read(opts.output/"fx_feature_coverage.csv").find(
        "filters,min mag texture filters,12,12,4,referenced_outside_loader")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:filters")==std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:count_over_24")==std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:snow_fire_step")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:snow_fire_random_acceleration")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:strip_nested_eff_frame_step")==std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:blast_discrete_acceleration")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:model_frame_step")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:model_midpoint_position")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:model_nested_eff_frame_step")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:shade_terrain_grid_height")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:ripple_emission_recycle")!=std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:ripple_nested_eff_frame_step")==std::string::npos);
    CHECK(read(opts.output/"fx_gap_ranking.csv").find("feature:arraw_nested_eff_frame_step")==std::string::npos);
    fs::remove(tmp/"Client/effect/child.eff");
    fs::remove(tmp/"Client/effect/unreferenced.eff");
    fs::remove(tmp/"Client/effect/nested/nested.eff");
    opts.strict=true;
    std::ostringstream strictLog;
    CHECK(fx::run(opts,strictLog)==0);
    par(tmp/"Client/effect/rogue.par",99);
    std::ostringstream driftLog;
    CHECK(fx::run(opts,driftLog)==1);
    CHECK(read(opts.output/"fx_table_drift.csv").find("type:99")!=std::string::npos);
    opts.strict=false; opts.output=opts.client/"reports";
    std::ostringstream safeLog;
    CHECK(fx::run(opts,safeLog)==2);
    fs::remove_all(tmp);
}
