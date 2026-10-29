#include "top/HistoricalSkillPreviewBinding.h"
#include "top/EffectBindingTrace.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    // Compiled child fixture for the Python isolation test; no GL or assets.
    if(argc>1 && std::string(argv[1])=="--character") {
        std::string shot;
        for(int i=1;i+1<argc;++i)if(std::string(argv[i])=="--shot")shot=argv[i+1];
        std::ofstream save("Client/user/save.txt");save<<"isolated";
        std::ofstream capture(shot);capture<<"fake test capture";
        return save&&capture?0:1;
    }
    using top::EffectEvent;
    try {
        unsetenv("TOP_HISTORICAL_SKILL_PREVIEW_BIND");
        EffectEvent cast;cast.kind=EffectEvent::Cast;
        top::historical_skill_preview_binding::qualifyPlayer(cast);
        require(cast.sourceIsPlayer&&cast.targetActorIndex==-2,"default preview identifies its known player source");
        setenv("TOP_HISTORICAL_SKILL_PREVIEW_BIND","1",1);
        for(auto kind:{EffectEvent::Cast,EffectEvent::CastBegin,EffectEvent::CastAction,EffectEvent::CastEnd}){
            EffectEvent event;event.kind=kind;
            top::historical_skill_preview_binding::qualifyPlayer(event);
            require(event.sourceIsPlayer&&event.targetActorIndex==-2,"cast preview has known player source only");
        }
        EffectEvent hit;hit.kind=EffectEvent::SkillHit;
        top::historical_skill_preview_binding::qualifyPlayer(hit);
        require(hit.targetActorIndex==-1&&!hit.sourceIsPlayer,"hit preview targets rendered player");
        EffectEvent flight;flight.kind=EffectEvent::Flight;
        top::historical_skill_preview_binding::qualifyPlayer(flight);
        require(flight.targetActorIndex==-2&&flight.sourceIsPlayer,"flight has known player source and no invented target");
        unsetenv("TOP_HISTORICAL_BINDING_TRACE");
        require(!top::effect_binding_trace::enabled(),"trace default off");
        setenv("TOP_HISTORICAL_BINDING_TRACE","1",1);
        require(top::effect_binding_trace::enabled(),"trace explicitly enabled");
        top::effect_binding_trace::Row row;row.resource="child,\"a\".eff";
        std::ostringstream out;top::effect_binding_trace::write(out,row);
        require(out.str().find("\"child,\"\"a\"\".eff\"")!=std::string::npos,"CSV escaping");
        unsetenv("TOP_HISTORICAL_BINDING_TRACE");unsetenv("TOP_HISTORICAL_SKILL_PREVIEW_BIND");
        std::cout<<"SK2.18-J preview identity and read-only trace format OK\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
