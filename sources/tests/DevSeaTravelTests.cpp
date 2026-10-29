#include "top/DevSeaTravel.h"
#include "top/MapFile.h"
#include <fstream>
#include <vector>
#include <unistd.h>
#include <stdexcept>
#include <string>
#include <cmath>
#include <cstdio>

namespace {
void need(bool value) {
    if (!value) throw std::runtime_error("SEA-03: assertion failed");
}
struct MockMap {
    int width() const { return 500; }
    int height() const { return 300; }
    bool blockedAt(float x, float) const { return x >= 100.f; }
    bool sailableAt(float x, float) const { return x >= 100.f && x < 200.f; }
};
}
int main() {
    using namespace top::dev_sea;
    Destination d;
    need(parse("magicsea 123.25 200.50 barco", d));
    need(d.location.map=="magicsea" && d.mode==Mode::Boat);
    need(std::fabs(d.location.x-123.25f)<0.0001f);
    need(resolve(MockMap{},d) && d.location.boat);
    need(parse("garner 7 8",d) && d.mode==Mode::Auto);
    need(resolve(MockMap{},d) && !d.location.boat);
    need(parse("magicsea 104 18",d) && resolve(MockMap{},d) && d.location.boat);
    need(parse("magicsea 104 18 terra",d) && !resolve(MockMap{},d));
    need(parse("magicsea 5 18 barco",d) && !resolve(MockMap{},d));
    need(parse("magicsea 250 18",d) && !resolve(MockMap{},d));
    need(parse("magicsea 501 18",d) && !resolve(MockMap{},d));
    need(!parse("../../path 5 5",d));
    need(!parse("garner -1 5",d));
    need(!parse("garner 5 5 barco foo",d));
    need(!parse("garner foo bar",d));
    need(!parse("garner 5 5 navio",d));
    need(!parse("garner 5",d));
    need(!parse("garner 5 99999",d));
    struct RemoteWater {
        int width() const { return 900; }
        int height() const { return 900; }
        bool sailableAt(float x,float y) const {
            return x>=700.f && x<=760.f && y>=800.f && y<=850.f;
        }
    } remote;
    float foundX=0,foundY=0;
    need(nearestWater(remote,2.f,2.f,foundX,foundY));
    need(remote.sailableAt(foundX,foundY));
    // .map real (versao 780627), costa de duas regioes: LAND e SEA.
    // Os bits sao lidos por MapFile, nao simulados pelo mock acima.
    char pathname[]="/tmp/top-sea03-map-XXXXXX";
    const int fd=::mkstemp(pathname);
    need(fd>=0);
    ::close(fd);
    std::vector<unsigned char> bytes;
    auto u32=[&](unsigned value){
        for (int i=0;i<4;++i) bytes.push_back(static_cast<unsigned char>(value>>(i*8)));
    };
    for (unsigned v : {780627u,16u,8u,8u,8u,28u,0u}) u32(v);
    for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
        unsigned char tile[15]={};
        tile[4]=5;
        tile[5]=tile[6]=255;
        tile[7]=static_cast<unsigned char>(x<4?10:-10);
        tile[8]=static_cast<unsigned char>(x<4?1:0);
        for (unsigned char ch:tile) bytes.push_back(ch);
    }
    {
        std::ofstream f(pathname,std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        need(f.good());
    }
    top::MapFile actual;
    need(actual.open(pathname));
    need(parse("testmap 1.25 1.25",d));
    need(resolve(actual,d) && !d.location.boat);
    need(parse("testmap 4.75 2.25",d));
    need(resolve(actual,d) && d.location.boat);
    need(parse("testmap 8.25 1.25 barco",d));
    need(resolve(actual,d) && d.location.boat); // secao vazia historica
    need(parse("testmap 1.25 1.25 barco",d));
    need(!resolve(actual,d));
    need(parse("testmap 16 1",d));
    need(!resolve(actual,d));
    actual.close();
    std::remove(pathname);
    std::puts("SEA-03: parser / modos / mascara de mapa OK");
}
