#include "top/LegacyParAudit.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace top::par_audit;
namespace {
template<class T> void add(std::vector<unsigned char>& v,T x){
    const auto* p=reinterpret_cast<const unsigned char*>(&x);
    v.insert(v.end(),p,p+sizeof(T));
}
void name(std::vector<unsigned char>& v,const std::string& text){
    for(std::size_t i=0;i<32;++i)v.push_back(i<text.size()?static_cast<unsigned char>(text[i]):0);
}
std::vector<unsigned char> fixture(uint32_t version=6,bool billboard=false,bool mediayEff=false,bool modelDirection=false){
    std::vector<unsigned char> b;
    add(b,version); name(b,"Example");add<int32_t>(b,1); if(version>=3)add<float>(b,1.f);
    add<int32_t>(b,6);name(b,"strip");add<int32_t>(b,1);
    name(b,"1.tga");name(b,"01010003.eff");
    for(int i=0;i<3;++i)add<float>(b,0.f);
    add<uint16_t>(b,2);
    for(int i=0;i<2;++i)add<float>(b,1.f);
    for(int i=0;i<6;++i)add<float>(b,0.f);
    for(int i=0;i<8;++i)add<float>(b,1.f);
    add<uint8_t>(b,billboard?1:0);
    for(int i=0;i<4;++i)add<int32_t>(b,0);
    add<float>(b,4.5f);add<float>(b,1.f);
    for(int i=0;i<7;++i)add<float>(b,0.f); // direction[3], acceleration[3], step[1]
    if(version>3){add<uint8_t>(b,0);name(b,"");}
    if(version>4)for(int i=0;i<3;++i)add<float>(b,0.f);
    if(version>5){add<float>(b,0.f);add<float>(b,.5f);}
    if(version>8){add<uint8_t>(b,1);add<int32_t>(b,2);add<float>(b,1.f);
        for(int i=0;i<6;++i)add<float>(b,0.f);
        for(int i=0;i<3;++i)add<float>(b,0.f); // path segment direction
        for(int i=0;i<3;++i)add<float>(b,i==0?1.f:0.f); // distance vector
    }
    if(version>9)add<uint8_t>(b,0);
    if(version>10)b.insert(b.end(),32,0);
    if(version>12)add<int32_t>(b,0);
    if(version>13)add<uint8_t>(b,modelDirection?1:0);
    if(version>14)add<uint8_t>(b,mediayEff?1:0);
    return b;
}
}
int main(){
    for(uint32_t version: {2u,3u,6u,9u,10u,11u,12u,13u,14u,15u}){
        auto b=fixture(version,false,version==15,version>=14);auto r=parse(b);
        if(!r.ok){std::cerr<<"Version "<<version<<" failed: "<<r.error<<" at "<<r.errorOffset<<"\n";return 1;}
        assert(r.document.version==version && r.document.declaredSystems==1);
        assert(r.document.consumed==b.size() && r.document.systems.size()==1);
        const auto& s=r.document.systems.front();
        assert(s.type==6 && s.frames==2 && s.count==1);
        assert(s.model=="01010003.eff" && s.texture=="1.tga");
        assert(s.hasPath==(version>8) && s.pathPoints==(version>8?2:0));
        assert(s.mediayEff==(version==15));
        assert(s.modelDirection==(version>=14));
    }
    const auto billboardFixture=parse(fixture(6,true));
    assert(billboardFixture.ok && billboardFixture.document.systems.front().billboard);
    const auto mediayFixture=parse(fixture(15,false,true));
    assert(mediayFixture.ok && mediayFixture.document.systems.front().mediayEff);
    const auto unboundDirectionFixture=parse(fixture(14));
    assert(unboundDirectionFixture.ok && !unboundDirectionFixture.document.systems.front().modelDirection);
    auto b=fixture();b.pop_back();assert(!parse(b).ok);
    b=fixture(14,false,false,true);b.pop_back();assert(!parse(b).ok);
    b=fixture(15,false,true);b.pop_back();assert(!parse(b).ok);
    b=fixture();b[0]=255;assert(!parse(b).ok);
    b=fixture();
    // frame count at 4+32+4+4+4+32+4+32+32+12 = 160
    const auto frameAt=4+32+4+4+4+32+4+32+32+12;
    assert(frameAt+1<static_cast<int>(b.size()));
    b[frameAt]=0;b[frameAt+1]=0;
    assert(!parse(b).ok);
    b=fixture();b.insert(b.end(),{0xCA,0xFE});auto r=parse(b);
    assert(r.ok && r.document.bytes==r.document.consumed+2);
    std::cout<<"SK2.3: 10 versions, nested reference, paths, bounds, truncation, trailing bytes OK\n";
}
