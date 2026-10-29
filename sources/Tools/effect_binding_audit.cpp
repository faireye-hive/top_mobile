#include "top/EffectBindingAudit.h"
#include <iostream>
int main(int argc,char** argv){
    std::filesystem::path client="Client",out="build/effect-binding-audit";
    for(int i=1;i<argc;++i){
        const std::string arg=argv[i];
        if(arg=="--help"){std::cout<<"top_effect_binding_audit [--client Client] [--out build/effect-binding-audit]\n";return 0;}
        if((arg!="--client"&&arg!="--out")||i+1>=argc)return 2;
        (arg=="--client"?client:out)=argv[++i];
    }
    return top::effect_binding_audit::run(client,out,std::cout);
}
