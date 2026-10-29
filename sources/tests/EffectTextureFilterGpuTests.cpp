// Optional SK2.15-C GPU check: sample a shared 2x2 texture first with nearest,
// then without the effect override. The latter must return to linear sampling.
#include "top/GfxDevice.h"
#include "top/GlxWindow.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
int main() {
    ::setenv("TOP_HISTORICAL_FILTERS", "1", 1);
    top::GlxWindow window;
    if (!window.open(64,64,"SK2.15-C per-effect sampler")) return 77;
    top::GfxDevice dev;
    if (!dev.init()) return 1;
    dev.resize(64,64);
    // Single checker with diagonally opposed whites: NEAREST samples one
    // texel; LINEAR averages all four at the center (approximately grey).
    const std::uint8_t pixels[] = {
        0,0,0,255, 255,255,255,255,
        255,255,255,255, 0,0,0,255
    };
    const auto texture = dev.createTexture(pixels,2,2);
    if (!texture) return 1;
    top::MeshVertex v[6]{};
    const float xy[] = {-1,-1, 1,-1, -1,1, 1,-1, 1,1, -1,1};
    const float uv[] = {0,0, 1,0, 0,1, 1,0, 1,1, 0,1};
    for(int i=0;i<6;++i) {
        v[i].x=xy[2*i];v[i].y=xy[2*i+1];
        v[i].u=uv[2*i];v[i].v=uv[2*i+1];
        v[i].r=v[i].g=v[i].b=v[i].a=1.f;
    }
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::vector<std::uint8_t> frame(64*64*3);
    auto sample=[&](int min,int mag) {
        dev.beginFrame(0,0,0);
        dev.draw(v,6,identity,8,texture,0,0,0,min,mag);
        if(!dev.readPixels(frame.data(),64,64)) return -1;
        return int(frame[(32*64+32)*3]);
    };
    const int nearest=sample(1,1), linear=sample(2,2), restored=sample(0,0);
    if (nearest<0 || linear<0 || restored<0 ||
        std::abs(linear-128)>16 || std::abs(linear-restored)>4 ||
        std::abs(nearest-linear)<60) {
        std::cerr<<"filter mismatch nearest="<<nearest<<" linear="<<linear
                 <<" legacy="<<restored<<"\n";return 1;
    }
    dev.shutdown();window.close();
    std::cout<<"SK2.15-C GPU nearest="<<nearest<<" linear="<<linear
             <<" legacy="<<restored<<" (restored) OK\n";
}
