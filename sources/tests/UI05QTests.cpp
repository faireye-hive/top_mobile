#include "top/MonsterSkills.h"
#include "top/SkillVisual.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    using namespace top::monster_skills;
    const std::vector<Attack> shadow{{25,50,1.f,0.f}, {138,50,9.f,40.f}};
    assert(valid(shadow[0]) && valid(shadow[1]));
    assert(choose(shadow, 2.0f, .4f, .4f, 0) == 1); // melee fora de alcance
    assert(choose(shadow, 5.0f, .4f, .4f, 1) == 1);
    assert(choose(shadow, 0.8f, .4f, .4f, 0) == 0);
    assert(choose(shadow, 0.8f, .4f, .4f, 49) == 0);
    assert(choose(shadow, 0.8f, .4f, .4f, 50) == 1);
    assert(choose(shadow, 10.f, .4f, .4f, 40) == -1);
    assert(choose(shadow, NAN, .4f, .4f, 3) == -1);
    assert(std::abs(maxReach(shadow, 1.8f, .4f, .4f) - 9.8f) < .0001f);
    assert(std::abs(windup(8.f,40.f) - .2f) < .0001f);
    assert(std::abs(windup(1.f,0.f) - .15f) < .0001f);
    assert(std::abs(windup(100.f,10.f) - .65f) < .0001f);
    assert(!valid({42,-1,8.f,0.f}));
    assert(!valid({42,500,INFINITY,0.f}));
    assert(!valid({42,500,8.f,NAN}));
    assert(choose({{42,0,8.f,0.f},{13,3,2.f,0.f}},1.f,.4f,.4f,2)==1);
    // Rejeitar ataques invalidos e nao promover uma skill acima do peso.
    assert(choose({{42,0,8.f,0.f},{13,0,2.f,0.f}},1.f,.4f,.4f,2)==-1);
    if (argc>1) {
        top::SkillVisualCatalog visuals;
        assert(visuals.load(argv[1]));
        const auto* ranged = visuals.find(138);
        assert(ranged && ranged->flightId()==1036);
        const auto* melee = visuals.find(25);
        assert(melee && melee->flightId()==0);
    }
    std::puts("05-Q: escolhas ponderadas, alcance, projeteis e fallback OK");
}
