#include "top/HistoricalParticlePool.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)

int main() {
    setenv("TOP_HISTORICAL_PARTICLE_EMITTER_TIMER", "1", 1);
    using namespace top::historical_particle_pool;
    constexpr float hz = 60.f;
    constexpr float tick = 1.f / hz;

    const auto beforeFirstFrame = snowFireSystemAt(2, 20, 0.f, 2.f, .15f,
        -1.f, 7, 2, 4, true, hz);
    CHECK(beforeFirstFrame.slots.size() == 20);
    for (const auto& slot : beforeFirstFrame.slots) CHECK(!slot.alive);

    float timer = 0.f;
    int expectedBirths = 0;
    for (int slot = 0; slot < 20; ++slot) {
        timer += tick;
        if (timer >= .15f) {
            timer = 0.f;
            ++expectedBirths;
        }
    }
    const auto firstFrame = snowFireSystemAt(2, 20, tick, 2.f, .15f,
        -1.f, 7, 2, 4, true, hz);
    int firstFrameBirths = 0;
    for (const auto& slot : firstFrame.slots) if (slot.alive) ++firstFrameBirths;
    CHECK(firstFrameBirths == expectedBirths);
    CHECK(firstFrameBirths > 1 && firstFrameBirths < 20);

    int oldAnalyticBirths = 0;
    for (int slot = 0; slot < 20; ++slot)
        if (at(2, slot, 20, tick, 2.f, .15f, -1.f, 7, 2).alive)
            ++oldAnalyticBirths;
    CHECK(oldAnalyticBirths == 1);
    CHECK(firstFrameBirths > oldAnalyticBirths);

    const auto zeroStep = snowFireSystemAt(1, 20, tick, 2.f, 0.f,
        -1.f, 7, 2, 4, true, hz);
    for (const auto& slot : zeroStep.slots) CHECK(slot.alive);

    const auto rejectedSnow = snowFireSystemAt(1, 20, tick, 2.f, 0.f,
        -1.f, 7, 2, 4, false, hz);
    for (const auto& slot : rejectedSnow.slots) CHECK(!slot.alive);

    const auto recycledBeforeDeath = snowFireSystemAt(2, 1, 6.f*tick, .1f,
        0.f, -1.f, 11, 1, 1, true, hz);
    const auto deadFrame = snowFireSystemAt(2, 1, 7.f*tick, .1f,
        0.f, -1.f, 11, 1, 1, true, hz);
    const auto recycledNextFrame = snowFireSystemAt(2, 1, 8.f*tick, .1f,
        0.f, -1.f, 11, 1, 1, true, hz);
    CHECK(recycledBeforeDeath.slots[0].alive);
    CHECK(!deadFrame.slots[0].alive);
    CHECK(recycledNextFrame.slots[0].alive && recycledNextFrame.slots[0].generation == 1);

    const auto stopped = snowFireSystemAt(2, 1, 20.f*tick, 1.f,
        0.f, 2.f*tick, 11, 1, 1, true, hz);
    CHECK(stopped.slots[0].alive && stopped.slots[0].generation == 0);
    std::cout << "SK2.17-J shared SNOW/FIRE emission timer OK\n";
    unsetenv("TOP_HISTORICAL_PARTICLE_EMITTER_TIMER");
}
