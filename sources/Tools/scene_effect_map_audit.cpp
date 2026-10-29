#include "top/SceneEffectMapAudit.h"

#include <iostream>

int main(int argc,char** argv){
    top::scene_effect_map_audit::Options options;
    for(int i=1;i<argc;++i){
        const std::string arg=argv[i];
        auto take=[&](std::filesystem::path& path){
            if(i+1>=argc)return false;
            path=argv[++i];return true;
        };
        if(arg=="--client"){if(!take(options.client))return 2;}
        else if(arg=="--out"){if(!take(options.out))return 2;}
        else if(arg=="--scene-effects"){if(!take(options.sceneEffectInfo))return 2;}
        else if(arg=="--dx9-game-app-msg"){if(!take(options.dx9GameAppMsg))return 2;}
        else if(arg=="--dx9-scene-obj-file"){if(!take(options.dx9SceneObjFile))return 2;}
        else if(arg=="--dx9-effect-obj"){if(!take(options.dx9EffectObj))return 2;}
        else if(arg=="--dx9-effect-set"){if(!take(options.dx9EffectSet))return 2;}
        else if(arg=="--dx9-table-loader"){if(!take(options.dx9TableLoader))return 2;}
        else if(arg=="--gl-scene-world"){if(!take(options.glSceneWorld))return 2;}
        else if(arg=="--help"||arg=="-h"){
            std::cout<<"top_scene_effect_map_audit [--client Client] [--out build/scene-effect-map-audit]\n"
                <<"  [--scene-effects scripts/table/sceneffectinfo.txt]\n"
                <<"  [--dx9-game-app-msg arquivo/fontes/Client/src/GameAppMsg.cpp]\n"
                <<"  [--dx9-scene-obj-file arquivo/fontes/Client/src/SceneObjFile.cpp]\n"
                <<"  [--dx9-effect-obj arquivo/fontes/Client/src/EffectObj.cpp]\n"
                <<"  [--dx9-effect-set arquivo/fontes/Client/src/EffectSet.h]\n"
                <<"  [--dx9-table-loader arquivo/fontes/Client/src/GameAppInterface.cpp]\n"
                <<"  [--gl-scene-world sources/Engine/src/top/SceneWorld.cpp]\n"
                <<"Read-only: maps version-600 scene-effect placements to sceneffectinfo IDs.\n";
            return 0;
        }else{std::cerr<<"Unknown argument: "<<arg<<'\n';return 2;}
    }
    return top::scene_effect_map_audit::run(options,std::cout);
}
