// Blend/material-state audit for Client/effect/*.eff and *.par. Read-only.
// C++17; no OpenGL, network or third-party dependencies.
#include "top/BlendAudit.h"
#include <iostream>
#include <string>
int main(int argc,char** argv){
    std::filesystem::path client="Client",out="build/blend-audit",deps;
    bool strict=false,opaqueDest=true;
    for(int i=1;i<argc;++i){
        const std::string a=argv[i];
        if(a=="--client"&&i+1<argc)client=argv[++i];
        else if(a=="--out"&&i+1<argc)out=argv[++i];
        else if(a=="--deps"&&i+1<argc)deps=argv[++i];
        else if(a=="--strict")strict=true;
        else if(a=="--dest-alpha"&&i+1<argc){
            const std::string v=argv[++i];
            if(v=="opaque")opaqueDest=true;
            else if(v=="framebuffer")opaqueDest=false;
            else {std::cerr<<"--dest-alpha expects opaque|framebuffer\n";return 2;}
        }
        else if(a=="--help"||a=="-h"){
            std::cout<<"Usage: top_blend_audit [--client Client] [--out build/blend-audit]\n"
                       "                       [--deps auditoria-historica/dependencies.csv]\n"
                       "                       [--dest-alpha opaque|framebuffer] [--strict]\n"
                       "Writes blend_usage.csv, blend_pairs.csv, blend_skills.csv (with --deps),\n"
                       "blend_failures.csv and blend_summary.json.\n"
                       "  --dest-alpha opaque (default): DESTALPHA reads 1.0, like the renderer's default.\n"
                       "  --dest-alpha framebuffer: report the raw enum (renderer opt-in path).\n"
                       "Exit 0 even with legacy_fallback pairs; --strict returns 1 on invalid values\n"
                       "or decode failures.\n";
            return 0;
        } else {std::cerr<<"Unknown/incomplete argument: "<<a<<'\n';return 2;}
    }
    return top::blend_audit::run(client,out,deps,strict,std::cout,opaqueDest);
}
