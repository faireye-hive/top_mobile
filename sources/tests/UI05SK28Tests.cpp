#include "top/HistoricalMagicMotion.h"
#include "top/MagicEffectCatalog.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using namespace top;
static bool near(float a,float b){return std::fabs(a-b)<1e-4f;}
int main(){
 namespace hm=top::historic_magic;
 assert(hm::shadeAt(3,0.f).mainScale==1.f);
 assert(hm::shadeAt(3,.05f).mainScale==3.f);
 assert(hm::shadeAt(3,.59f).copies==1);
 assert(hm::shadeAt(3,.60f).copies==1);
 assert(hm::shadeAt(3,.61f).copies==2);
 assert(hm::shadeAt(3,1.21f).copies==3);
 hm::ShadeTimeline frameCount;
 frameCount.advance(.61f);
 assert(frameCount.copies==2 && near(frameCount.accumulated,0.f));
 frameCount.advance(1.21f); // DX9 drops overshoot and increments once per call.
 assert(frameCount.copies==3 && near(frameCount.accumulated,0.f));
 frameCount.advance(.3f);
 frameCount.advance(.3f); // DX9 uses strictly greater than .6 s.
 assert(frameCount.copies==3 && near(frameCount.accumulated,.6f));
 frameCount.advance(.001f);
 assert(frameCount.copies==4 && near(frameCount.accumulated,0.f));
 assert(hm::shadeAt(2,1.5f).mainScale==1.f && hm::shadeAt(2,1.5f).copies==0);
 assert(near(hm::shadeAlpha(0,2),.5f));
 assert(near(hm::shadeAlpha(1,2),0.f));
 assert(near(hm::shadeAlpha(0,3),2.f/3.f));
 assert(near(hm::shadeAlpha(1,3),1.f/3.f));
 std::vector<float> xs,alphas;
 hm::forEachModelCopy(3,1.21f,3.f,5.f,0.f,5.f,6.f,5.f,
  [&](float x,float,float scale,float alpha){xs.push_back(x);alphas.push_back(alpha);assert(near(scale,3.f));});
 assert(xs.size()==3);
 assert(near(xs[0],3.f)&&near(xs[1],2.5f)&&near(xs[2],2.f));
 assert(near(alphas[0],1.f)&&near(alphas[1],2.f/3.f)&&near(alphas[2],1.f/3.f));
 xs.clear();
 hm::forEachModelCopy(2,2.f,4.f,5.f,1.f,5.f,6.f,5.f,
  [&](float x,float,float,float){xs.push_back(x);});
 assert(xs.size()==1&&near(xs[0],4.f));
 const auto path=std::filesystem::temp_directory_path()/"top_sk28_magic_table.txt";
 {std::ofstream f(path);
  f<<"18\tIllusion Slash\t1\t04000009.eff\t15\t0\t0\t-1\t3\t0\t0\n";
  f<<"20\tFrozen Arrow\t2\ticearrowf.eff,icearrowf(a).eff\t10\t1\ticearrowf\t-1\t2\t0\ticearrowh\n";
  f<<"90\tBad mode\t1\tbad.eff\t10\t0\t0\t-1\tbad\t0\t0\n";
 }
 MagicEffectCatalog cat; assert(cat.load(path.string()));
 assert(cat.find(18)->renderMode==3&&near(cat.find(18)->velocity,15.f));
 assert(cat.find(20)->renderMode==2&&cat.find(20)->result=="icearrowh");
 assert(cat.find(90)->renderMode==-1);
 std::filesystem::remove(path);
 std::cout<<"SK2.8 DX9 Part_fshade state, trailing copies and mode lookup OK\n";
}
