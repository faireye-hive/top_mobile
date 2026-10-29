#include "top/HistoricalFireHit.h"
#include "top/HistoricalParticlePool.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)

int main() {
    using top::historical_fire_hit::at;
    const std::vector<float> distances{6.f, 9.f, 15.f};
    const float wrap = .2f + 30.f / 15.f;

    CHECK(!at(wrap - .001f, .2f, 0.f, 15.f, distances, true, true, false).trigger);
    const auto first = at(wrap, .2f, 0.f, 15.f, distances, true, true, false);
    CHECK(first.trigger && std::fabs(first.stopAt - 2.f) < .0001f);
    const auto late = at(wrap + .025f, .2f, 0.f, 15.f, distances, true, true, false);
    CHECK(late.trigger && std::fabs(late.stopAt - 2.f) < .0001f);
    const auto existing = top::historical_particle_pool::at(
        2, 10, 20, 1.8f, .8f, .15f, first.stopAt, 7, 2);
    const auto afterStop = top::historical_particle_pool::at(
        2, 15, 20, 2.3f, .8f, .15f, first.stopAt, 7, 2);
    CHECK(existing.alive); // particles born before Stop continue
    CHECK(!afterStop.alive); // dead slots cannot emit after Stop

    CHECK(!at(wrap, .2f, 2.f, 15.f, distances, true, true, false).trigger);
    CHECK(!at(wrap, .2f, 0.f, 15.f, distances, true, false, false).trigger);
    CHECK(!at(wrap, .2f, 0.f, 15.f, distances, false, true, false).trigger);
    CHECK(!at(wrap, .2f, 0.f, 15.f, {}, true, true, false).trigger);
    CHECK(!at(wrap, .2f, 0.f, 15.f, distances, true, true, true).trigger);
    CHECK(!at(wrap, .2f, 0.f, 0.f, distances, true, true, false).trigger);

    std::cout << "SK2.17-H FIRE path-end event rules OK\n";
}
