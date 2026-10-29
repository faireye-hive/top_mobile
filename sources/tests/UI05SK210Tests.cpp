#include "top/EffectSystem.h"
#include "top/LegacyParticleCurves.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
        name(f,"layer");for(int v:{1,5,6})put<int>(f,v);
        put<float>(f,.1f);put<unsigned short>(f,1); // stale metadata: timeline must use key durations
        put<float>(f,layer==0?.25f:1.f);
        for(int i=0;i<3;++i)put<float>(f,1);
        for(int i=0;i<6;++i)put<float>(f,0);
        for(int i=0;i<4;++i)put<float>(f,1);
        put<unsigned short>(f,0);put<unsigned short>(f,0);put<float>(f,0);
        put<unsigned short>(f,0);put<float>(f,0);
        name(f,layer==0?"short.tga":"long.tga");name(f,"RectPlane");
        put<unsigned char>(f,0);put<int>(f,1);put<int>(f,8);
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
const top::EffectBatch* batch(const top::EffectSystem& fx,const std::string& texture) {
    for(const auto& b:fx.batches())if(b.texture.find(texture)!=std::string::npos&&!b.verts.empty())return &b;
    return nullptr;
}
}
int main() {
    using top::legacyfx::sample;
    assert(near(sample({0,3,9},{},{},1.f/6).size,1.5f));
    assert(near(sample({0,3,9},{},{},1.f/3).size,3));
    assert(near(sample({0,3,9},{},{},.5f).size,6));
    for(float age:{2.f/3,.9f,1.f})assert(near(sample({0,3,9},{},{},age).size,9));
    assert(near(sample({0},{},{},.5f).size,0));
    assert(near(sample({2},{},{},std::numeric_limits<float>::quiet_NaN()).size,2));
    const auto color=sample({1,1},{0,0,0,0,1,1,1,1},{0,0,0,0,0,2},.25f);
    assert(near(color.color[3],.5f)&&near(color.rotation,1));
    const auto root=fs::temp_directory_path()/("top_sk210_"+std::to_string(getpid()));
    fs::create_directories(root/"scripts/table");fs::create_directories(root/"effect");
    fs::create_directories(root/"texture/effect");eff(root/"effect/layers.eff");
    for(const char* texture:{"native.tga.png","short.tga.png","long.tga.png"}) {
        std::ofstream f(root/"texture/effect"/texture);f<<"fixture";
    }
    par(root/"effect/model.par",5,"RectPlane");par(root/"effect/strip.par",6,"RectPlane");
    par(root/"effect/nested.par",5,"layers.eff");
    par(root/"effect/billboard.par",5,"RectPlane",true);
    {std::ofstream f(root/"scripts/table/magicsingleinfo.txt");
     f<<"38\tdefault\t1\tlayers.eff\t10\t0\t0\t-1\t1\t0\t0\n";
     f<<"20\tnested\t0\t0\t10\t1\tnested\t-1\t1\t0\t0\n";}
    {std::ofstream f(root/"scripts/table/sceneffectinfo.txt");f<<"42\tmodel.par\tmodel\n43\tstrip.par\tstrip\n44\tnested.par\tnested\n46\tbillboard.par\tbillboard\n";}
    {std::ofstream f(root/"scripts/table/skillinfo.txt");
     for(int id:{42,43,44,45,46,47}) {
        std::vector<std::string> c(74,"0");c[0]=std::to_string(id);
        if(id==45)c[59]="1020";else c[51]=id==47?"1038":std::to_string(id);
        for(std::size_t i=0;i<c.size();++i)f<<(i?"\t":"")<<c[i];f<<'\n';
     }}
    top::MapFile map;
    for(int id:{42,43}) {
        top::EffectSystem fx;assert(fx.open(root.string()));
        fx.emit({top::EffectEvent::Cast,1,2,1,2,id});fx.step(.5f);fx.build(map,1,0);
        const auto* b=batch(fx,"native.tga.png");assert(b&&b->verts.size()==6);
        float x=0,y=0,z=0,minX=1e9f,maxX=-1e9f;
        for(const auto& v:b->verts){x+=v.x;y+=v.y;z+=v.z;minX=std::min(minX,v.x);maxX=std::max(maxX,v.x);}
        assert(near(x/6,id==42?2:0));assert(near(z/6,id==42?4:1));
        assert(near(y/6,id==42?3.9f:-.1f)); // exact MoveTo + MODEL/STRIP range convention
        for(const auto& v:b->verts)assert(near(v.y,y/6)); // authored horizontal RectPlane, not camera quad
        assert(near(maxX-minX,id==42?.6f:.3f)); // MODEL animates, STRIP holds key zero
    }
    // Serialized length metadata must not truncate actual key durations.
    top::EffectSystem authoredLife;assert(authoredLife.open(root.string()));
    authoredLife.emit({top::EffectEvent::Cast,0,0,0,0,47});authoredLife.step(.8f);authoredLife.build(map,1,0);
    assert(authoredLife.activeCount()==1&&batch(authoredLife,"long.tga.png"));
    // Billboard mode retains the primitive's width; helper uses half-size.
    top::EffectSystem billboard;assert(billboard.open(root.string()));
    billboard.emit({top::EffectEvent::Cast,1,2,1,2,46});billboard.step(.5f);billboard.build(map,1,0);
    const auto* bb=batch(billboard,"native.tga.png");assert(bb&&bb->verts.size()==6);
    float minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f;
    for(const auto& v:bb->verts){minX=std::min(minX,v.x);maxX=std::max(maxX,v.x);minY=std::min(minY,v.y);maxY=std::max(maxY,v.y);}
    assert(near(maxX-minX,.6f)&&near(maxY-minY,.6f));
    // Short nested layers must stop until the entire controller loops.
    for(bool flight:{false,true}) {
        top::EffectSystem fx;assert(fx.open(root.string()));
        fx.emit({flight?top::EffectEvent::Flight:top::EffectEvent::Cast,0,0,0,20,flight?45:44,5});
        fx.step(.1f);fx.build(map,1,0);assert(batch(fx,"short.tga.png"));
        fx.step(.3f);fx.build(map,1,0);
        assert(!batch(fx,"short.tga.png"));assert(batch(fx,"long.tga.png"));
        fx.step(.7f);fx.build(map,1,0);
        assert(bool(batch(fx,"short.tga.png"))==flight);
        assert(bool(batch(fx,"long.tga.png"))==flight);
    }
    fs::remove_all(root);
    std::cout<<"SK2.10 PAR N intervals, zero size, native MODEL/STRIP anchors and grouped EFF cycles OK\n";
}
