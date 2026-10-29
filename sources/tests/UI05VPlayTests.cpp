// Integration regression for saves, combat and temporary effects without GL.
#include "top/Play.h"
#include <cstdio>
#include <cstring>
int main(int argc,char** argv) {
    if(argc==2 && std::strcmp(argv[1],"--pet-stats-only")==0)return top::Play::selfTestPetStats()?0:1;
    if (!top::Play::selfTest()) {
        std::fprintf(stderr, "Play::selfTest failed (05-V)\n");
        return 1;
    }
    return 0;
}
