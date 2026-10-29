#include "top/EffectSystem.h"
#include "top/HistoricalModelLifetime.h"
#include "top/FxCoverageAudit.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs=std::filesystem;
using namespace top;

template<class T> void put(std::ofstream& out,T value) {
    out.write(reinterpret_cast<const char*>(&value),sizeof(value));
}
void name(std::ofstream& out,const char* value) {
    char bytes[32]{};
    for(unsigned i=0;value[i]&&i<31;++i)bytes[i]=value[i];
    out.write(bytes,sizeof(bytes));
}
void defaultEff(const fs::path& path,float frameTime) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1);name(out,"base");
    for(int value:{1,5,7})put<std::int32_t>(out,value);
    put<float>(out,frameTime);put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,4);put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(out,uv);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name(out,"base.tga");
    name(out,"RectPlane");put<std::uint8_t>(out,1);
    put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);
    put<std::int32_t>(out,0);put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);
}
void modelChildEff(const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);name(out,"");put<std::uint8_t>(out,0);
    name(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1);name(out,"child");
    for(int value:{1,5,7})put<std::int32_t>(out,value);
    put<float>(out,.3f);put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,4);put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(float uv:{0.f,1.f,0.f,0.f,1.f,0.f,1.f,1.f})put<float>(out,uv);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name(out,"child.tga");
    name(out,"RectPlane");put<std::uint8_t>(out,0);
    put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    for(int i=0;i<3;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);
    put<std::int32_t>(out,0);put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::uint8_t>(out,0);
}
void timedModelPar(const fs::path& path) {
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);name(out,"model-lifetime");put<std::int32_t>(out,1);
    put<float>(out,2.f);
    put<std::int32_t>(out,5);name(out,"timed-model");put<std::int32_t>(out,1);
    name(out,"controller.tga");name(out,"child.eff");
    for(float value:{0.f,0.f,0.f})put<float>(out,value);
    put<std::uint16_t>(out,2);
    for(int i=0;i<2;++i)put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,1);
    for(int value:{5,6,1,1})put<std::int32_t>(out,value);
    put<float>(out,.4f);put<float>(out,0.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    put<float>(out,0.f);
    put<std::uint8_t>(out,0);name(out,"");
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<float>(out,.2f);put<float>(out,1.5f);
}

std::size_t activeAfter(float age,bool historical) {
    if(historical)setenv("TOP_HISTORICAL_MODEL_LIFETIME","1",1);
    else unsetenv("TOP_HISTORICAL_MODEL_LIFETIME");
    const fs::path root=fs::temp_directory_path()/
        (std::string("sk217bs-model-life-")+std::to_string(getpid())+
         (historical?"-on":"-off"));
    fs::remove_all(root);
    fs::create_directories(root/"effect");
    fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    for(const char* file:{"base.tga.png","controller.tga.png","child.tga.png"})
        std::ofstream(root/"texture/effect"/file)<<"fixture";
    defaultEff(root/"effect/default.eff",.1f);
    modelChildEff(root/"effect/child.eff");
    timedModelPar(root/"effect/model.par");
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");
     f<<"600\tmodel.par\tMODEL lifetime fixture\n";}
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tdefault\t1\tdefault.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     std::array<std::string,74> columns;columns.fill("0");
     columns[0]="600";columns[51]="600";
     for(std::size_t i=0;i<columns.size();++i)f<<(i?"\t":"")<<columns[i];
     f<<'\n';}
    EffectSystem system;
    assert(system.open(root.string()));
    EffectEvent event;event.kind=EffectEvent::Cast;event.variant=600;
    event.sourceIsPlayer=true;system.emit(event);system.step(age);
    const auto count=system.activeCount();
    fs::remove_all(root);
    return count;
}

int main() {
    using top::historical_model_lifetime::expiry;
    unsetenv("TOP_HISTORICAL_MODEL_LIFETIME");
    assert(top::historical_model_lifetime::enabled());
    assert(std::fabs(expiry(2.1f,5,.2f,1.5f)-1.5f)<.0001f);
    setenv("TOP_HISTORICAL_MODEL_LIFETIME","1",1);
    assert(std::fabs(expiry(2.1f,5,.2f,1.5f)-1.5f)<.0001f);
    assert(std::fabs(expiry(2.1f,6,.2f,1.5f)-1.5f)<.0001f);
    assert(std::fabs(expiry(2.1f,3,.2f,1.5f)-2.1f)<.0001f);
    assert(std::fabs(expiry(2.1f,5,.2f,0.f)-2.1f)<.0001f);

    const auto modelFeature=std::find_if(fx_coverage::features().begin(),fx_coverage::features().end(),
        [](const auto& feature){return feature.first=="model_controller_lifetime";});
    assert(modelFeature!=fx_coverage::features().end());
    par_audit::System system;system.type=5;system.play=1.f;
    assert(modelFeature->second(system));
    system.type=6;assert(modelFeature->second(system));
    system.type=5;system.play=0.f;assert(!modelFeature->second(system));
    std::vector<fx_coverage::TableRow> rules;std::string error;
    assert(fx_coverage::readTable("sources/auditoria-historica/fx_port_coverage.csv",rules,error));
    const auto lifecycleRule=std::find_if(rules.begin(),rules.end(),[](const auto& row){
        return row.kind=="feature"&&row.key=="model_controller_lifetime";
    });
    assert(lifecycleRule!=rules.end());
    assert(lifecycleRule->token=="historical_model_lifetime::expiry");

    assert(activeAfter(1.51f,false)==0);
    assert(activeAfter(1.51f,true)==0);
    unsetenv("TOP_HISTORICAL_MODEL_LIFETIME");
}
