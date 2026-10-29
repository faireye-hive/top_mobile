#include "top/EffectSystem.h"
#include "top/HistoricalPartTrace.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs=std::filesystem;
namespace {
using namespace top;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(float a,float b,float epsilon=1e-4f){return std::fabs(a-b)<epsilon;}
template<class T>void put(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof value);}
void name32(std::ofstream& out,const char* value){char text[32]{};for(int i=0;value[i]&&i<31;++i)text[i]=value[i];out.write(text,32);}

void writeMap(const fs::path& path,int heightDecimeters){
    std::ofstream out(path,std::ios::binary);
    for(int value:{780627,8,8,8,8})put(out,value);
    put<std::uint32_t>(out,24u);
    for(int i=0;i<64;++i){
        std::array<std::uint8_t,15> tile{};
        tile[7]=static_cast<std::uint8_t>(static_cast<std::int8_t>(heightDecimeters));
        if(i==1*8+2){ // Four unequal quadrants in world tile (2,1).
            tile[11]=20;tile[12]=64|10;tile[13]=128|30;tile[14]=63;
        }
        out.write(reinterpret_cast<const char*>(tile.data()),tile.size());
    }
}

void writeNativeEff(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,6);put<std::int32_t>(out,0);put<std::uint8_t>(out,0);name32(out,"");
    put<std::uint8_t>(out,0);name32(out,"");put<std::uint8_t>(out,0);
    for(int i=0;i<4;++i)put<float>(out,0.f);
    put<std::int32_t>(out,1);name32(out,"trace mesh");
    for(int value:{1,5,7})put<std::int32_t>(out,value);
    put<float>(out,1.f);put<std::uint16_t>(out,2);
    put<float>(out,0.f);put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,1.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<6;++i)put<float>(out,0.f);
    for(int i=0;i<8;++i)put<float>(out,1.f);
    put<std::uint16_t>(out,0);put<std::uint16_t>(out,0);put<float>(out,0.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);name32(out,"model.tga");name32(out,"RectPlane");
    put<std::uint8_t>(out,0);put<std::int32_t>(out,1);put<std::int32_t>(out,8);
    put<float>(out,3.f);put<float>(out,1.f);put<float>(out,2.f);
    put<std::uint16_t>(out,0);put<float>(out,0.f);put<float>(out,0.f);put<std::int32_t>(out,0);
    put<std::uint8_t>(out,0);for(int i=0;i<4;++i)put<float>(out,0.f);put<std::uint8_t>(out,0);
}

void writePar(const fs::path& path){
    std::ofstream out(path,std::ios::binary);
    put<std::uint32_t>(out,2);name32(out,"impact");put<std::int32_t>(out,1);
    put<std::int32_t>(out,1);name32(out,"impact");put<std::int32_t>(out,1);name32(out,"impact.tga");name32(out,"");
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<std::uint16_t>(out,1);put<float>(out,1.f);
    for(int i=0;i<3;++i)put<float>(out,0.f);
    for(int i=0;i<4;++i)put<float>(out,1.f);
    put<std::uint8_t>(out,1);put<std::int32_t>(out,5);put<std::int32_t>(out,6);
    put<std::int32_t>(out,1);put<std::int32_t>(out,1);
    put<float>(out,.5f);put<float>(out,0.f);
    for(float value:std::array<float,3>{{0.f,0.f,0.f}})put(out,value);
    for(int i=0;i<3;++i)put<float>(out,0.f);
    put<float>(out,.1f);
}

void testTimeline(){
    using namespace historical_part_trace;
    historic_magic::Path path{2,{0.f,5.f,0.f},{8.f,5.f,0.f},4.f,0.f};
    Timeline timeline;timeline.reset(path);
    require(!timeline.advance(.5f,{8.f,5.f,0.f},nullptr,4.f),"stationary target keeps trace alive");
    require(near(timeline.position.x,1.5f),"DX9 caps each trace step at 1.5 units");
    require(!timeline.advance(.5f,{8.f,5.f,4.f},nullptr,4.f),"moving target retargets instead of ending at old time");
    require(near(timeline.position.x,3.f)&&near(timeline.position.z,.375f),
            "retarget correction follows DX9 squared-distance ratio");
    require(near(timeline.renderPosition.x,1.5f)&&near(timeline.renderPosition.z,0.f),
            "current frame renders before applying the current update");
    const auto midpoint=timeline.positionAt(.75f);
    require(near(midpoint.x,2.25f)&&near(midpoint.z,.1875f),"sample history interpolates within a step");

    historic_magic::Path overshoot{2,{0.f,2.f,0.f},{1.f,2.f,0.f},10.f,0.f};
    Timeline shortFlight;shortFlight.reset(overshoot);
    require(shortFlight.advance(.2f,{1.f,2.f,0.f},nullptr,10.f),"overshoot stops Part_trace");
    require(near(shortFlight.position.x,1.f),"overshoot snaps to previous target");
    require(inPlanarRange({0.f,0.f,0.f},{.99f,50.f,.99f}),
            "DX9 range ignores vertical height and maps X/Y to GL X/Z");
}

void testTerrainStop(){
    using namespace historical_part_trace;
    const auto mapPath=fs::temp_directory_path()/"top_sk218d_trace_map.map";
    // CGameScene::GetGridHeight calls MPMap with int(world * 2).
    // This fixture deliberately
    // gives the visual tile surface and small-grid collision different heights.
    writeMap(mapPath,0);
    MapFile map;require(map.open(mapPath.string()),"open synthetic terrain");
    historic_magic::Path path{2,{1.f,2.5f,1.75f},{6.f,2.5f,1.75f},10.f,0.f};
    Timeline timeline;timeline.reset(path);
    require(near(map.heightAt(2.5f,1.f),0.f),"visual terrain fixture remains at tile height");
    require(near(map.sceneGridHeightAt(2.25f,1.25f),1.f),"scene grid quadrant zero");
    require(near(map.sceneGridHeightAt(2.5f,1.25f),-.5f),"half-tile boundary selects signed quadrant one");
    require(near(map.sceneGridHeightAt(2.25f,1.5f),1.5f),"block bit does not filter quadrant two height");
    require(near(map.sceneGridHeightAt(2.5f,1.75f),3.15f),"scene grid quadrant three");
    require(near(map.sceneGridHeightAt(3.f,1.75f),0.f),"scene sample advances to the next world tile");
    require(near(map.sceneGridHeightAt(8.f,1.f),0.f),"scene grid outside map returns zero");
    require(near(map.sceneGridHeightAt(NAN,1.f),0.f),"nonfinite sample is rejected before integer conversion");
    require(timeline.advance(.3f,{6.f,2.5f,1.75f},&map,10.f),"Part_trace stops below CGameScene GetGridHeight");
    require(near(timeline.position.x,2.5f)&&timeline.position.y<map.sceneGridHeightAt(2.5f,1.75f),
            "collision selects int(world * 2) quadrant, not tile zero or interpolated terrain");
    map.close();fs::remove(mapPath);
}

void testEffectSystemGateAndResult(){
    const fs::path root=fs::temp_directory_path()/("top_sk218d_trace_"+std::to_string(getpid()));
    fs::create_directories(root/"effect");fs::create_directories(root/"scripts/table");
    fs::create_directories(root/"texture/effect");
    writeNativeEff(root/"effect/model.eff");writePar(root/"effect/impact.par");
    {std::ofstream(root/"texture/effect/model.tga.png")<<"fixture";
     std::ofstream(root/"texture/effect/impact.tga.png")<<"fixture";}
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tfixture default\t1\tmodel.eff\t10\t0\t0\t-1\t1\t0\t0\n";
     f<<"20\tFrozen Arrow\t1\tmodel.eff\t10\t0\t0\t-1\t2\t0\timpact\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     std::vector<std::string> columns(74,"0");columns[0]="93";columns[59]="1020";
     for(std::size_t i=0;i<columns.size();++i)f<<(i?"\t":"")<<columns[i];f<<'\n';}
    EffectEvent flight;flight.kind=EffectEvent::Flight;flight.variant=93;
    flight.fromX=1.f;flight.fromY=1.f;flight.toX=6.f;flight.toY=1.f;
    flight.fromHeight=7.f;flight.toHeight=7.f;flight.duration=.5f;flight.targetActorIndex=0;
    unsetenv("TOP_HISTORICAL_PART_TRACE");
    EffectSystem compatible;require(compatible.open(root.string()),"open synthetic client with default mode");
    compatible.emit(flight);compatible.setTargetActorRoots({{0,{6.f,6.f,1.f}}});
    compatible.step(.1f);compatible.setTargetActorRoots({{0,{8.f,6.f,5.f}}});
    compatible.step(.4f);
    require(compatible.activeCount()==1,"default-off path keeps the authored static duration");

    setenv("TOP_HISTORICAL_PART_TRACE","1",1);
    EffectSystem effect;require(effect.open(root.string()),"open synthetic client");
    effect.emit(flight);
    effect.setTargetActorRoots({{0,{6.f,6.f,1.f}}});
    effect.step(.1f);
    require(effect.activeCount()==1,"flight remains live during target tracking");
    effect.setTargetActorRoots({{0,{8.f,6.f,5.f}}});
    effect.step(.1f);
    require(effect.activeCount()==1,"moving target does not trigger the original fixed-duration result");
    for(int i=0;i<30&&effect.activeCount()==1;++i){
        effect.step(.1f);
        effect.setTargetActorRoots({{0,{8.f,6.f,5.f}}});
    }
    require(effect.activeCount()>=2,"tracked flight stops and starts its authored result");
    unsetenv("TOP_HISTORICAL_PART_TRACE");
    fs::remove_all(root);
}
}

int main(){
    try{testTimeline();testTerrainStop();testEffectSystemGateAndResult();}
    catch(const std::exception& error){std::cerr<<"SK2.18-D Part_trace failure: "<<error.what()<<'\n';return 1;}
    std::cout<<"SK2.18-D/E Part_trace target motion, DX9 grid collision and integration OK\n";
}
