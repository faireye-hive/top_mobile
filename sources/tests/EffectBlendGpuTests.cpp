// SK2.11: exercise the production shader and actual framebuffer, not just enums.
#include "top/GfxDevice.h"
#include "top/GlxWindow.h"
#include "top/HistoricalBlend.h"
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
namespace {
float factor(int value,int channel,const std::array<float,4>& src,const std::array<float,4>& dst) {
    switch(value) {
        case 1:return 0;case 2:return 1;
        case 3:return src[channel];case 4:return 1-src[channel];
        case 5:return src[3];case 6:return 1-src[3];
        case 7:return dst[3];case 8:return 1-dst[3];
        case 9:return dst[channel];case 10:return 1-dst[channel];
        case 11:return std::min(src[3],1-dst[3]);
    }
    return 0;
}
}
int main() {
    top::GlxWindow window;if(!window.open(64,64,"Effect blend regression"))return 77;
    top::GfxDevice device;if(!device.init())return 1;device.resize(64,64);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    top::MeshVertex v[6]{};
    const float coords[12]={-1,-1,1,-1,-1,1,1,-1,1,1,-1,1};
    for(int i=0;i<6;++i){v[i].x=coords[i*2];v[i].y=coords[i*2+1];v[i].r=v[i].g=v[i].b=v[i].a=1;}
    std::vector<std::uint8_t> pixels(64*64*3);
    auto render=[&](std::array<std::uint8_t,4> texel,int src,int dst) {
        const unsigned tex=device.createTexture(texel.data(),1,1);
        device.beginFrame(.2f,.4f,.6f);
        const auto pair=top::historic_blend::effective(src,dst);
        device.draw(v,6,identity,8,tex,0,pair.source,pair.destination);
        if(!device.readPixels(pixels.data(),64,64))std::exit(1);
        const int offset=(32*64+32)*3;
        return std::array<int,3>{pixels[offset],pixels[offset+1],pixels[offset+2]};
    };
    // Explicit {0,0} exercises the renderer's legacy batch contract. Since
    // SK2.18-L, an environment variable no longer disables valid DX9 pairs.
    auto legacy=render({0,0,0,255},0,0);
    if(legacy[0]>2||legacy[1]>2||legacy[2]>2){std::cerr<<"legacy reproduction failed\n";return 1;}
    ::setenv("TOP_HISTORICAL_BLEND","0",1);
    auto historic=render({0,0,0,255},5,7);
    for(int c=0;c<3;++c)if(std::abs(historic[c]-(c+1)*51)>2){std::cerr<<"black glow background changed destination\n";return 1;}
    // These 19 pairs occur in current assets. Compare RGB against D3D's
    // equation with the known source texel and clear destination alpha=1.
    const int pairs[][2]={{2,2},{2,7},{3,2},{3,4},{3,6},{3,7},{4,6},{5,1},{5,2},{5,4},
                         {5,6},{5,7},{6,2},{6,7},{7,4},{7,6},{7,7},{10,2},{10,7}};
    const std::array<float,4> source={51.f/255,102.f/255,153.f/255,128.f/255};
    const std::array<float,4> dest={.2f,.4f,.6f,1};
    for(const auto& pair:pairs) {
        const auto pixel=render({51,102,153,128},pair[0],pair[1]);
        for(int c=0;c<3;++c) {
            const float expected=std::clamp(source[c]*factor(pair[0],c,source,dest)+dest[c]*factor(pair[1],c,source,dest),0.f,1.f);
            if(std::abs(pixel[c]-static_cast<int>(std::lround(expected*255)))>2) {
                std::cerr<<"GPU mismatch for "<<pair[0]<<'/'<<pair[1]<<" channel "<<c<<'\n';return 1;
            }
        }
    }
    // Alpha=0 still clears destination for SRCALPHA/ZERO. Mode 8's old
    // discard incorrectly kept the background, ignoring the authored factor.
    const auto zero=render({255,255,255,0},5,1);
    if(zero[0]>2||zero[1]>2||zero[2]>2){std::cerr<<"historical shader still discards alpha zero\n";return 1;}
    // The opaque backbuffer policy must preserve shaded RGB, including when
    // obsolete environment options request the former framebuffer-alpha path.
    auto layered=[&](bool raw) {
        if(raw)::setenv("TOP_HISTORICAL_DEST_ALPHA","framebuffer",1);
        else ::unsetenv("TOP_HISTORICAL_DEST_ALPHA");
        const std::uint8_t shadow[4]={0,0,0,128},black[4]={0,0,0,255};
        const auto s=device.createTexture(shadow,1,1),b=device.createTexture(black,1,1);
        device.beginFrame(.2f,.4f,.6f);
        device.draw(v,6,identity,4,s,0,5,6);
        device.readPixels(pixels.data(),64,64);
        const int offset=(32*64+32)*3;
        const std::array<int,3> before={pixels[offset],pixels[offset+1],pixels[offset+2]};
        std::uint8_t rgba[4]{};glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        if(rgba[3]!=255)std::exit(1);
        device.draw(v,6,identity,8,b,0,5,7);
        device.readPixels(pixels.data(),64,64);
        const int difference=before[2]-pixels[offset+2];
        if(std::abs(difference)>2)std::exit(1);
    };
    layered(true);layered(false);
    ::unsetenv("TOP_HISTORICAL_BLEND");
    device.shutdown();window.close();
    std::cout<<"GPU: legacy black rectangle reproduced; historical background preserved; 19 blend pairs, alpha-zero composition and shadow/DESTALPHA buffer regression passed\n";
}
