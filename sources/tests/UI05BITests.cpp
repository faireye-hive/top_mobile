#include "top/Play.h"
#include <cstdio>
int main() {
    if (!top::Play::selfTestExplicitCombat()) {
        std::fprintf(stderr, "05-BI: explicit targeting regression FAILED\n");
        return 1;
    }
    std::puts("05-BI: skill->target, ranged range, one-shot melee PASSED");
    return 0;
}
