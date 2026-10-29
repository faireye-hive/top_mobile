// Fixture test for top_blend_audit. Uses explicit checks (not assert) so it
// still verifies under Release/NDEBUG.
#include "top/BlendAudit.h"
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace top::blend_audit;
#define CHECK(c) do{ if(!(c)){ std::cerr<<"FAIL line "<<__LINE__<<": "#c"\n"; return 1; } }while(0)
namespace {
template<class T> void add(std::vector<unsigned char>& v,T x){
    const auto* p=reinterpret_cast<const unsigned char*>(&x); v.insert(v.end(),p,p+sizeof(T)); }
void name(std::vector<unsigned char>& v,const std::string& t){
    for(std::size_t i=0;i<32;++i) v.push_back(i<t.size()?static_cast<unsigned char>(t[i]):0); }
std::vector<unsigned char> par(uint32_t ver,int src,int dst,int minF,int magF,bool bill,bool shade,const std::string& model){
    std::vector<unsigned char> b; add(b,ver); name(b,"Ctl"); add<int32_t>(b,1); if(ver>=3) add<float>(b,1.f);
    add<int32_t>(b,6); name(b,"sys"); add<int32_t>(b,1); name(b,"tex.tga"); name(b,model);
    for(int i=0;i<3;++i) add<float>(b,0.f);
    add<uint16_t>(b,2); for(int i=0;i<2;++i) add<float>(b,1.f);
    for(int i=0;i<6;++i) add<float>(b,0.f);
    for(int i=0;i<8;++i) add<float>(b,1.f);
    add<uint8_t>(b,bill?1:0); add<int32_t>(b,src); add<int32_t>(b,dst); add<int32_t>(b,minF); add<int32_t>(b,magF);
    add<float>(b,4.5f); add<float>(b,1.f); for(int i=0;i<7;++i) add<float>(b,0.f);
    if(ver>3){ add<uint8_t>(b,0); name(b,""); }
    if(ver>4) for(int i=0;i<3;++i) add<float>(b,0.f);
    if(ver>5){ add<float>(b,0.f); add<float>(b,.5f); }
    if(ver>8) add<uint8_t>(b,0);
    if(ver>9) add<uint8_t>(b,shade?1:0);
    if(ver>10) name(b,"hit.par");
    if(ver>12) add<int32_t>(b,0);
    if(ver>13) add<uint8_t>(b,0);
    if(ver>14) add<uint8_t>(b,0);
    return b; }
std::vector<unsigned char> eff7(int src,int dst){
    std::vector<unsigned char> b; add<uint32_t>(b,7); add<int32_t>(b,0);
    add<uint8_t>(b,0); name(b,""); add<uint8_t>(b,0); name(b,""); add<uint8_t>(b,0);
    for(int i=0;i<3;++i) add<float>(b,0.f);
    add<float>(b,0.f); add<int32_t>(b,1);
    name(b,"part"); add<int32_t>(b,0); add<int32_t>(b,src); add<int32_t>(b,dst); add<float>(b,1.f); add<uint16_t>(b,1);
    add<float>(b,0.f); for(int i=0;i<3;++i) add<float>(b,1.f); for(int i=0;i<6;++i) add<float>(b,0.f);
    for(int i=0;i<4;++i) add<float>(b,1.f);
    add<uint16_t>(b,4); add<uint16_t>(b,0); add<float>(b,0.f);
    add<uint16_t>(b,0); add<float>(b,0.f); name(b,"eff0009");
    name(b,"RectPlane"); add<uint8_t>(b,0); add<int32_t>(b,0);
    add<int32_t>(b,0); for(int i=0;i<3;++i) add<float>(b,0.f);
    add<uint16_t>(b,0); add<float>(b,0.f); add<float>(b,0.f);
    add<int32_t>(b,0);
    add<uint8_t>(b,0); for(int i=0;i<4;++i) add<float>(b,0.f);
    add<uint8_t>(b,0); add<uint8_t>(b,0);
    return b; }
void write(const fs::path& p,const std::vector<unsigned char>& b){
    std::ofstream o(p,std::ios::binary); o.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size())); }
std::string slurp(const fs::path& p){ std::ifstream i(p); std::stringstream s; s<<i.rdbuf(); return s.str(); }
bool has(const std::string& hay,const std::string& n){ return hay.find(n)!=std::string::npos; }
}
int main(){
    // enum names + translator-backed status (no renderer mirror to drift)
    CHECK(d3dName(5)=="SRCALPHA" && d3dName(6)=="INVSRCALPHA" && d3dName(7)=="DESTALPHA");
    CHECK(status(5,2)=="applied" && status(5,7)=="applied" && status(5,6)=="applied" && status(11,2)=="applied");
    CHECK(status(5,11)=="legacy_fallback" && status(12,6)=="legacy_fallback" && status(5,14)=="legacy_fallback");
    CHECK(status(0,6)=="invalid_value" && status(5,16)=="invalid_value" && status(5,0)=="invalid_value");
    CHECK(normFactor(7,true)==2 && normFactor(8,true)==1 && normFactor(7,false)==7);
    CHECK(effectiveEquiv(5,7,true)=="GL_SRC_ALPHA/GL_ONE" && effectiveEquiv(5,7,false)=="GL_SRC_ALPHA/GL_DST_ALPHA");
    CHECK(glPair(5,7)=="GL_SRC_ALPHA/GL_DST_ALPHA" && involvesDestAlpha(7,6) && !involvesDestAlpha(5,6));
    const fs::path root=fs::temp_directory_path()/"top_blend_audit_test";
    fs::remove_all(root); fs::create_directories(root/"Client/effect/sub");
    write(root/"Client/effect/A.par",par(6,5,6,2,2,true,false,"c.eff"));
    write(root/"Client/effect/b.par",par(11,2,2,1,1,false,true,""));
    write(root/"Client/effect/sub/Bad.par",par(6,5,6,0,0,false,false,"")); // truncated below
    { auto b=par(6,5,6,0,0,false,false,""); b.resize(b.size()-5); write(root/"Client/effect/sub/Bad.par",b); }
    write(root/"Client/effect/c.eff",eff7(5,7));
    write(root/"Client/effect/d.eff",eff7(5,0));
    { std::ofstream d(root/"deps.csv");
      d<<"skill_id,skill_name,channel,visual_or_group_id,magic_id,dependency_type,referenced_name,resolved_path_or_status,evidence\r\n"
       <<"7,Seven,flight,1,,scene_par,A.par,x,y\r\n8,\"Eight, the\",actor,2,,scene_par,b,x,y\r\n"; }
    // outputs inside Client/ must be refused
    CHECK(run(root/"Client",root/"Client/out","",false,std::cerr)==2);
    const fs::path out=root/"out";
    CHECK(run(root/"Client",out,root/"deps.csv",false,std::cerr)==0);
    CHECK(run(root/"Client",root/"out2","",true,std::cerr)==1); // strict: decode failure + invalid (5,0)
    const auto usage=slurp(out/"blend_usage.csv"), pairs=slurp(out/"blend_pairs.csv"),
               skills=slurp(out/"blend_skills.csv"), fails=slurp(out/"blend_failures.csv"), sum=slurp(out/"blend_summary.json");
    CHECK(has(usage,"effect/A.par,par,0,sys,6,5,SRCALPHA,6,INVSRCALPHA"));
    CHECK(has(usage,",applied,"));
    CHECK(has(usage,"effect/c.eff,eff,0,part,0,5,SRCALPHA,7,DESTALPHA"));
    CHECK(has(usage,"GL_SRC_ALPHA/GL_DST_ALPHA,GL_SRC_ALPHA/GL_ONE,0,0,,,applied,"));
    CHECK(has(usage,"INVALID(0)"));
    CHECK(has(usage,"effect/b.par,par,0,sys,6,2,ONE,2,ONE,GL_ONE/GL_ONE,GL_ONE/GL_ONE,0,1,1,1,applied"));
    CHECK(has(fails,"Bad.par,par,"));
    CHECK(has(pairs,"5,SRCALPHA,6,INVSRCALPHA,GL_SRC_ALPHA/GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA/GL_ONE_MINUS_SRC_ALPHA,applied,0,0,1,1,1,"));
    CHECK(has(pairs,"5,SRCALPHA,7,DESTALPHA,GL_SRC_ALPHA/GL_DST_ALPHA,GL_SRC_ALPHA/GL_ONE,applied,1,1,0,1,1,"));
    // skill 7 reaches A.par (5,6) and, through its model link, c.eff (5,7); skill 8 resolves "b" -> b.par
    CHECK(has(skills,"7,Seven,flight,a.par,5,SRCALPHA,6,INVSRCALPHA,GL_SRC_ALPHA/GL_ONE_MINUS_SRC_ALPHA,applied"));
    CHECK(has(skills,"7,Seven,flight,c.eff,5,SRCALPHA,7,DESTALPHA,GL_SRC_ALPHA/GL_ONE,applied"));
    CHECK(has(skills,"8,\"Eight, the\",actor,b.par,2,ONE,2,ONE"));
    CHECK(has(sum,"\"applied\": 3") && has(sum,"\"invalid_value\": 1") && has(sum,"\"records_involving_destalpha\": 1") && has(sum,"BLEND-AUDIT-v3")); CHECK(has(sum,"\"eff_files\": 2") && has(sum,"\"par_files\": 3") && has(sum,"\"decode_failures\": 1"));
    // framebuffer-alpha model reports the raw enum for DESTALPHA
    CHECK(run(root/"Client",root/"out3","",false,std::cerr,false)==0);
    CHECK(has(slurp(root/"out3/blend_usage.csv"),"GL_SRC_ALPHA/GL_DST_ALPHA,GL_SRC_ALPHA/GL_DST_ALPHA,0,0,,,applied,"));
    fs::remove_all(root);
    std::cout<<"BlendAudit v3: translator status, dest-alpha model, par v6/v11 fields, eff v7, truncation, deps join, output guard OK\n";
}
