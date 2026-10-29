#include "top/HistoricalSkillExecution.h"
#include "top/Play.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace top::historical_skill;
int main() {
    const Descriptor slash{1,0,4,800};
    const Descriptor secondary{1,0,4,400};
    assert(inputMode(slash) == InputMode::SelectEnemy);
    assert(inputMode(secondary) == InputMode::SelectEnemy);
    assert(std::fabs(reachMeters(slash,.4f,.4f)-8.8f) < .0001f);
    assert(targetStep(slash,false,4.f,.4f,.4f) == TargetStep::AwaitSelection);
    assert(targetStep(slash,true,20.f,.4f,.4f) == TargetStep::Approach);
    assert(targetStep(slash,true,8.f,.4f,.4f) == TargetStep::Ready);
    assert(inputMode({2,1,1,0}) == InputMode::Passive);
    assert(inputMode({1,1,5,1000}) == InputMode::Direct);
    assert(inputMode({1,0,3,1000}) == InputMode::Direct);
    assert(approachStopMeters(8.8f) < 8.8f);
    if (!top::Play::selfTestHistoricalSkillExecution()) {
        std::fprintf(stderr, "05-BL: end-to-end historical skill FAILED\n");
        return 1;
    }
    std::puts("05-BL: generic historical skill execution passed");
    return 0;
}
