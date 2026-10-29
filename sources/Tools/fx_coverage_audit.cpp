#include "top/FxCoverageAudit.h"
#include <iostream>
int main(int argc,char** argv){
    top::fx_coverage::Options options;
    for(int i=1;i<argc;++i){
        const std::string a=argv[i];
        auto take=[&](std::filesystem::path& dest)->bool{if(i+1>=argc)return false;dest=argv[++i];return true;};
        if(a=="--client"){if(!take(options.client))return 2;}
        else if(a=="--out"){if(!take(options.output))return 2;}
        else if(a=="--deps"){if(!take(options.deps))return 2;}
        else if(a=="--table"){if(!take(options.table))return 2;}
        else if(a=="--source"){if(!take(options.source))return 2;}
        else if(a=="--dx9-source"){if(!take(options.dx9Source))return 2;}
        else if(a=="--dx9-particle-source"){if(!take(options.dx9ParticleSource))return 2;}
        else if(a=="--dx9-particle-header"){if(!take(options.dx9ParticleHeader))return 2;}
        else if(a=="--dx9-character"){if(!take(options.dx9CharacterSource))return 2;}
        else if(a=="--dx9-skill-attack"){if(!take(options.dx9SkillAttackSource))return 2;}
        else if(a=="--controller-strips"){if(!take(options.directStrips))return 2;}
        else if(a=="--strict")options.strict=true;
        else if(a=="--help"||a=="-h"){
            std::cout<<"top_fx_coverage_audit [--client Client] [--out build/fx-coverage] [--strict]\n"
              " [--deps sources/auditoria-historica/dependencies.csv]\n"
              " [--table sources/auditoria-historica/fx_port_coverage.csv]\n"
              " [--source sources/Engine/src/top/EffectSystem.cpp]\n"
              " [--dx9-source arquivo/fontes/Engine/src/MPResManger.cpp]\n"
              " [--dx9-particle-source arquivo/fontes/Engine/src/MPParticleSys.cpp]\n"
              " [--dx9-particle-header arquivo/fontes/Engine/include/MPParticleSys.h]\n"
              " [--dx9-character arquivo/fontes/Client/src/Character.cpp]\n"
              " [--dx9-skill-attack arquivo/fontes/Client/src/STAttack.cpp]\n"
              " [--controller-strips path/to/DIRECT_CONTROLLER_STRIPS.csv]\n"
              "Writes fx_skill_item_channels.csv by joining skillinfo item channels,\n"
              "sceneffectinfo IDs, and decoded PAR controller types.\n"
              "Read-only inventory; writes CSV/JSON outside Client. No GL required.\n";
            return 0;
        }else{std::cerr<<"Unknown argument: "<<a<<'\n';return 2;}
    }
    if(options.directStrips.empty()) {
        const std::filesystem::path bundled="sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv";
        if(std::filesystem::is_regular_file(bundled)) options.directStrips=bundled;
    }
    return top::fx_coverage::run(options,std::cout);
}
