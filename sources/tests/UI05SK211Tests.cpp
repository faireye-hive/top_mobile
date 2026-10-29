#include "top/EffectSystem.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
namespace fs=std::filesystem;
namespace {
template<class T> void put(std::ofstream& f,T v){f.write(reinterpret_cast<const char*>(&v),sizeof v);}
void name(std::ofstream& f,const char* s){char b[32]{};for(int i=0;s[i]&&i<31;++i)b[i]=s[i];f.write(b,32);}
bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
void eff(const fs::path& path) {
    std::ofstream f(path,std::ios::binary);
    put<unsigned>(f,6);put<int>(f,0);put<unsigned char>(f,0);name(f,"");
    put<unsigned char>(f,0);name(f,"");put<unsigned char>(f,0);
    for(int i=0;i<4;++i)put<float>(f,0);
    put<int>(f,2);
    for(int layer=0;layer<2;++layer) {
        name(f,"layer");for(int v:{2,5,6})put<int>(f,v);
        put<float>(f,.1f);put<unsigned short>(f,1); // stale metadata: timeline must use key durations
        put<float>(f,layer==0?.25f:1.f);
        for(int i=0;i<3;++i)put<float>(f,i==1?2.f:1.f);
        for(int i=0;i<6;++i)put<float>(f,layer==0 && i==2?1.57079632679f:0.f);
        for(int i=0;i<4;++i)put<float>(f,1);
        put<unsigned short>(f,4);put<unsigned short>(f,1);put<float>(f,1);
        for(float uv:{.2f,.7f,.2f,.3f,.8f,.3f,.8f,.7f})put<float>(f,uv);
        put<unsigned short>(f,0);put<float>(f,0);
        name(f,layer==0?"short.tga":"long.tga");name(f,"RectPlane");
        put<unsigned char>(f,1);put<int>(f,1);put<int>(f,8);
        put<float>(f,3);put<float>(f,1);put<float>(f,2);
        put<unsigned short>(f,0);put<float>(f,0);put<float>(f,0);put<int>(f,0);
        put<unsigned char>(f,0);for(int i=0;i<4;++i)put<float>(f,0);put<unsigned char>(f,0);
    }
}
void par(const fs::path& path,int type,const char* model,bool billboard=false) {
    std::ofstream f(path,std::ios::binary);
    put<unsigned>(f,7);name(f,"system");put<int>(f,1);put<float>(f,3);
    put<int>(f,type);name(f,"model");put<int>(f,4); // original forces MODEL/STRIP count=1
    name(f,"native.tga");name(f,model);
    for(float v:{4.f,6.f,8.f})put<float>(f,v);
    put<unsigned short>(f,3);for(float v:{1.f,3.f,0.f})put<float>(f,v);
    for(int i=0;i<9;++i)put<float>(f,0);for(int i=0;i<12;++i)put<float>(f,1);
    put<unsigned char>(f,billboard?1:0);for(int v:{5,6,1,1})put<int>(f,v);
    put<float>(f,3);put<float>(f,100); // MODEL/STRIP do not translate at this speed
    for(int i=0;i<6;++i)put<float>(f,100);
    put<float>(f,.2f);put<unsigned char>(f,0);name(f,"");
    for(float v:{1.f,2.f,3.f,0.f,0.f})put<float>(f,v);
}

}
int main() {
    const auto root=fs::temp_directory_path()/("top_sk211_"+std::to_string(getpid()));
    fs::create_directories(root/"scripts/table");fs::create_directories(root/"effect");
    fs::create_directories(root/"texture/effect");eff(root/"effect/layers.eff");
    for(const char* n:{"native.tga.png","long.tga.png","short.tga.png"}){std::ofstream f(root/"texture/effect"/n);f<<"fixture";}
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");f<<"38\tdefault\t1\tlayers.eff\t10\t0\t0\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt"), scene(root/"scripts/table/sceneffectinfo.txt");
     for(int type=1;type<=21;++type) {
        const int id=100+type;
        std::vector<std::string> c(74,"0");c[0]=std::to_string(id);c[47]="46";c[51]=type==21?"1038":std::to_string(id);
        for(std::size_t i=0;i<c.size();++i)f<<(i?"\t":"")<<c[i];f<<'\n';
        const auto file=std::to_string(type)+".par";
        scene<<id<<'\t'<<file<<"\tfixture\n";
        par(root/"effect"/file,type>=19?4:type,type==19?"missing.eff":type==20?"RectPlane":"layers.eff",type==20);
     }}
    top::MapFile map;
    for(int type=1;type<=21;++type) {
        top::EffectSystem fx;assert(fx.open(root.string()));
        assert(fx.skillVisuals().find(100+type)->firstAction==46);
        fx.emit({top::EffectEvent::Cast,0,0,0,0,100+type});fx.step(.1f);fx.build(map,1,0);
        if(type==20) {
            // Native RIPPLE uses its size keys and billboard flag; no invented
            // ground-plane growth and no direction/velocity translation.
            std::vector<top::MeshVertex> before;
            for(const auto& b:fx.batches())if(!b.verts.empty())before=b.verts;
            assert(before.size()==24);
            const float centerBefore=(before[0].x+before[1].x)*.5f;
            assert(near(before[1].x-before[0].x,.36f));
            fx.step(.4f);fx.build(map,1,0);
            for(const auto& b:fx.batches())if(!b.verts.empty()) {
                assert(near(b.verts[1].x-b.verts[0].x,.6f));
                assert(near((b.verts[0].x+b.verts[1].x)*.5f,centerBefore));
            }
            continue;
        }
        bool model=false;std::size_t vertices=0;
        for(const auto& b:fx.batches()) {
            vertices+=b.verts.size();if(b.verts.empty())continue;
            assert(b.texture.find("native.tga.png")==std::string::npos); // controller mask never substitutes an EFF
            if(b.texture.find("short.tga.png")!=std::string::npos) {
                // At 90 degrees the independent scale must be applied before
                // rotation; the long edge becomes horizontal.
                const float up=b.verts[1].y-b.verts[0].y;
                const float left=b.verts[2].x-b.verts[0].x;
                assert(near(left,-2*up));
            }
            if(b.texture.find("long.tga.png")!=std::string::npos) {
                model=true;
                assert(b.verts.size()>=6);
                const float width=b.verts[1].x-b.verts[0].x;
                const float height=b.verts[2].y-b.verts[0].y;
                assert(near(height,width*2)); // independent EFF X/Y size, nested and direct
                float lo=1,hi=0;
                for(const auto& v:b.verts){lo=std::min(lo,v.u);hi=std::max(hi,v.u);}
                assert(near(lo,.2f)&&near(hi,.8f)); // nested billboard authored UVs
            }
        }
        if(type==19 || type==13)assert(vertices==0);else assert(model);
    }
    fs::remove_all(root);
    std::cout<<"SK2.11: nested EFF geometry in model-rendering particle types, UV crops and no black-mask fallback on missing model OK\n";
}
