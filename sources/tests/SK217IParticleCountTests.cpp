#include "top/HistoricalParticleCount.h"

#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)

int main() {
    using top::historical_particle_count::resolve;
    CHECK(resolve(1, 100, false, false) == 24);
    CHECK(resolve(2, 50, false, true) == 50);
    CHECK(resolve(1, 80, false, true) == 80);
    CHECK(resolve(3, 80, false, true) == 24);
    CHECK(resolve(2, 80, true, true) == 1);
    CHECK(resolve(1, 24, false, true) == 24);
    CHECK(resolve(1, 0, false, true) == 1);
    std::cout << "SK2.17-I authored SNOW/FIRE count policy OK\n";
}
