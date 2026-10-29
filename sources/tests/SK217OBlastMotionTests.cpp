#include "top/HistoricalBlastMotion.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

static bool near(float a, float b, float epsilon = 1e-6f) {
    return std::fabs(a - b) <= epsilon;
}

int main() {
    using namespace top::historical_blast_motion;
    unsetenv("TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION");
    assert(enabled());
    unsetenv("TOP_HISTORICAL_BLAST_ACTUAL_DT");
    assert(actualDtEnabled());
    setenv("TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION", "1", 1);
    assert(enabled());

    constexpr float dt = 1.f / 60.f;
    const V3 acceleration{{2.f, -3.f, .5f}};
    // At 3 ticks DX9 has moved with v0, v0+a*dt, v0+2*a*dt.
    const auto threeTicks = discreteAccelerationCorrection(acceleration, 3.f * dt);
    assert(near(threeTicks[0], 2.f * (3.f * dt * dt - .5f * 9.f * dt * dt)));
    assert(near(threeTicks[1], -1.5f * threeTicks[0]));
    assert(near(threeTicks[2], .25f * threeTicks[0]));

    // A fractional remainder advances at the velocity accumulated so far.
    const auto partial = discreteAccelerationCorrection(acceleration, 3.5f * dt);
    const float expectedScale = dt * dt * (3.f * 2.f / 2.f + 3.f * .5f) -
                                .5f * (3.5f * dt) * (3.5f * dt);
    assert(near(partial[0], acceleration[0] * expectedScale));
    assert(near(discreteAccelerationCorrection(acceleration, 0.f)[0], 0.f));
    assert(near(discreteAccelerationCorrection(acceleration, NAN)[0], 0.f));

    SystemTimeline timeline;
    timeline.advance(.2f, {{true, 4u}});
    timeline.advance(.1f, {{true, 4u}});
    timeline.advance(.2f, {{true, 4u}});
    const auto actual = timeline.correction(acceleration, 0, 4u);
    assert(near(actual[0], -.5f * acceleration[0] * (.2f*.2f+.1f*.1f+.2f*.2f)));
    assert(near(actual[1], -1.5f * actual[0]));
    timeline.advance(.05f, {{true, 5u}});
    const auto recycled = timeline.correction(acceleration, 0, 5u);
    assert(near(recycled[0], -.5f * acceleration[0] * .05f*.05f));
    timeline.advance(.1f, {{false, 5u}});
    assert(near(timeline.correction(acceleration, 0, 5u)[0], 0.f));

    setenv("TOP_HISTORICAL_BLAST_ACTUAL_DT", "1", 1);
    assert(actualDtEnabled());
    unsetenv("TOP_HISTORICAL_BLAST_ACTUAL_DT");
    unsetenv("TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION");
}
