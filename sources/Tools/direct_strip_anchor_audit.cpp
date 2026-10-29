#include "top/DirectStripAnchorAudit.h"
#include <iostream>

int main(int argc,char** argv){
    top::direct_strip_anchor_audit::Options options;
    for(int i=1;i<argc;++i){
        const std::string arg=argv[i];
        auto take=[&](std::filesystem::path& path){if(i+1>=argc)return false;path=argv[++i];return true;};
        if(arg=="--client"){if(!take(options.client))return 2;}
        else if(arg=="--out"){if(!take(options.out))return 2;}
        else if(arg=="--deps"){if(!take(options.deps))return 2;}
        else if(arg=="--scene-effects"){if(!take(options.sceneEffectInfo))return 2;}
        else if(arg=="--magic-single"){if(!take(options.magicSingleInfo))return 2;}
        else if(arg=="--dx9-model-eff"){if(!take(options.dx9ModelEff))return 2;}
        else if(arg=="--dx9-particle-ctrl"){if(!take(options.dx9ParticleCtrl))return 2;}
        else if(arg=="--dx9-effect-ctrl"){if(!take(options.dx9EffectCtrl))return 2;}
        else if(arg=="--dx9-client-effect-obj"){if(!take(options.dx9ClientEffectObj))return 2;}
        else if(arg=="--gl-effect-system"){if(!take(options.glEffectSystem))return 2;}
        else if(arg=="--gl-actor-skin"){if(!take(options.glActorSkin))return 2;}
        else if(arg=="--help"||arg=="-h"){
            std::cout<<"top_direct_strip_anchor_audit [--client Client] [--out build/direct-strip-anchor-audit]\n"
                <<"  [--deps sources/auditoria-historica/dependencies.csv]\n"
                <<"  [--scene-effects scripts/table/sceneffectinfo.txt; relative to --client]\n"
                <<"  [--magic-single scripts/table/magicsingleinfo.txt; relative to --client]\n"
                <<"  [--dx9-model-eff arquivo/fontes/Engine/src/MPModelEff.cpp]\n"
                <<"  [--dx9-particle-ctrl arquivo/fontes/Engine/src/MPParticleCtrl.cpp]\n"
                <<"  [--dx9-effect-ctrl arquivo/fontes/Engine/src/MPEffectCtrl.cpp]\n"
                <<"  [--dx9-client-effect-obj arquivo/fontes/Client/src/EffectObj.cpp]\n"
                <<"  [--gl-effect-system sources/Engine/src/top/EffectSystem.cpp]\n"
                <<"  [--gl-actor-skin sources/Engine/src/top/ActorSkin.cpp]\n"
                <<"Read-only: decodes CMPStrip trailers and inspects owner-binding code.\n";
            return 0;
        }else{std::cerr<<"Unknown argument: "<<arg<<'\n';return 2;}
    }
    return top::direct_strip_anchor_audit::run(options,std::cout);
}
