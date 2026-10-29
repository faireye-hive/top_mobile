#include "top/StripEffAnimationAudit.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace top;

template<class T> void write(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void name(std::ostream& out, const char* value) {
    char bytes[32]{};
    for (std::size_t i=0; value[i] && i<31; ++i) bytes[i]=value[i];
    out.write(bytes,sizeof(bytes));
}
void quad(std::ostream& out, float first) {
    for (float value : {first,1.f,first,0.f,1.f,0.f,1.f,1.f}) write(out,value);
}
void eff(const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    write<std::uint32_t>(out,6); write<std::int32_t>(out,0);
    write<std::uint8_t>(out,0); name(out,""); write<std::uint8_t>(out,0);
    name(out,""); write<std::uint8_t>(out,0);
    for (int i=0;i<4;++i) write<float>(out,0.f);
    write<std::int32_t>(out,3);
    for (int type : {1,2,3}) {
        name(out,"part"); write<std::int32_t>(out,type);
        write<std::int32_t>(out,5); write<std::int32_t>(out,7);
        write<float>(out,.1f); write<std::uint16_t>(out,1); write<float>(out,.1f);
        for(int i=0;i<3;++i)write<float>(out,1.f);
        for(int i=0;i<3;++i)write<float>(out,0.f);
        for(int i=0;i<3;++i)write<float>(out,0.f);
        for(int i=0;i<4;++i)write<float>(out,1.f);
        write<std::uint16_t>(out,4); write<std::uint16_t>(out,2); write<float>(out,.1f);
        quad(out,0.f); quad(out,.5f);
        write<std::uint16_t>(out,2); write<float>(out,.1f); name(out,"base.tga");
        quad(out,0.f); quad(out,.5f);
        name(out,type==2 ? "spark.lgo" : "RectPlane");
        write<std::uint8_t>(out,0); write<std::int32_t>(out,1);
        write<std::int32_t>(out,8);
        for(int i=0;i<3;++i)write<float>(out,1.f);
        write<std::uint16_t>(out,2); write<float>(out,.1f);
        name(out,"frame0.tga"); name(out,"frame1.tga"); write<float>(out,0.f);
        write<std::int32_t>(out,0); // no animated model parameters
        write<std::uint8_t>(out,0); for(int i=0;i<4;++i)write<float>(out,0.f);
        write<std::uint8_t>(out,0); // alpha flag
    }
}
void par(const fs::path& path, const char* model) {
    std::ofstream out(path,std::ios::binary);
    write<std::uint32_t>(out,2); name(out,"strip"); write<std::int32_t>(out,1);
    write<std::int32_t>(out,6); name(out,"strip_child"); write<std::int32_t>(out,1);
    name(out,"mask.tga"); name(out,model);
    for(int i=0;i<3;++i)write<float>(out,0.f);
    write<std::uint16_t>(out,2); write<float>(out,1.f); write<float>(out,1.f);
    for(int i=0;i<6;++i)write<float>(out,0.f);
    for(int i=0;i<8;++i)write<float>(out,1.f);
    write<std::uint8_t>(out,0);
    for(int v:{5,6,1,1})write<std::int32_t>(out,v);
    write<float>(out,1.f); write<float>(out,1.f);
    for(int i=0;i<6;++i)write<float>(out,0.f);
    write<float>(out,0.f);
}
std::string read(const fs::path& path) {
    std::ifstream in(path);
    return {std::istreambuf_iterator<char>(in),{}};
}

int main() {
    const fs::path root=fs::temp_directory_path()/
        ("strip-eff-audit-"+std::to_string(getpid()));
    fs::create_directories(root/"Client/effect");
    fs::create_directories(root/"out");
    eff(root/"Client/effect/child.eff");
    par(root/"Client/effect/strip.par","child.eff");
    par(root/"Client/effect/broken_ref.par","broken.eff");
    { std::ofstream(root/"Client/effect/broken.eff",std::ios::binary)<<"bad"; }
    { std::ofstream f(root/"deps.csv");
      f<<"skill_id,skill_name,channel,referenced_name,dependency_type\n"
       <<"81,Test Skill,cast,strip.par,par\n"; }
    strip_eff_animation_audit::Options options;
    options.client=root/"Client"; options.output=root/"out"; options.deps=root/"deps.csv";
    std::ostringstream log;
    assert(strip_eff_animation_audit::run(options,log)==0);
    const auto summary=read(options.output/"fx_strip_eff_animation_summary.csv");
    const auto parts=read(options.output/"fx_strip_eff_parts.csv");
    const auto unresolved=read(options.output/"fx_strip_eff_unresolved.csv");
    assert(summary.find("1,EFFECT_FRAMETEX,1,1,1,1,1,1,")!=std::string::npos);
    assert(summary.find("2,EFFECT_MODELUV,1,1,1,1,1,1,")!=std::string::npos);
    assert(summary.find("3,EFFECT_MODELTEXTURE,1,1,1,1,1,1,")!=std::string::npos);
    assert(parts.find("spark.lgo,external_lgo")!=std::string::npos);
    assert(parts.find("possible_lgo_item_branch_not_serialized")!=std::string::npos);
    assert(unresolved.find("broken.eff,1,1,1,referenced_eff_decode_failed")!=std::string::npos);
    fs::remove_all(root);
}
