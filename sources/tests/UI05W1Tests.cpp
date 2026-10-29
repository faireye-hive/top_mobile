#include "top/ScreenPick.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
int main() {
    using namespace top::screen_pick;
    const Ray front{0.f,1.f,0.f,0.f,0.f,20.f};
    const float mob=actor(front,0.f,0.f,8.f,.7f,2.f);
    assert(mob>0.f && mob<1.f);
    assert(!std::isfinite(actor(front,5.f,0.f,8.f,.7f,2.f)));
    const float item=sphere(front,0.f,.6f,5.f,.85f);
    assert(item>0.f && item<mob);
    assert(visible(item,.90f));
    assert(!visible(mob,item+.01f));
    const Ray lookingElsewhere{0.f,1.f,0.f,1.f,0.f,0.f};
    assert(!std::isfinite(actor(lookingElsewhere,0.f,0.f,8.f)));
    const Ray inside{0.f,1.f,8.f,0.f,0.f,2.f};
    assert(sphere(inside,0.f,1.f,8.f,.7f)>=0.f);
    std::puts("05-W1: hit volumes de monstros/NPC/loot e oclusao OK");
    return 0;
}
