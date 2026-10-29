#include "top/HistoricalStripAlpha.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

int main() {
    using top::historical_strip_alpha::effectiveAlpha;
    unsetenv("TOP_HISTORICAL_STRIP_PARENT_ALPHA");
    assert(top::historical_strip_alpha::enabled());

    setenv("TOP_HISTORICAL_STRIP_PARENT_ALPHA", "1", 1);
    assert(top::historical_strip_alpha::enabled());
    assert(std::abs(effectiveAlpha(6, .47f, 0.f) - .47f) < 1e-6f);
    assert(std::abs(effectiveAlpha(6, .47f, .4f) - .47f) < 1e-6f);
    assert(std::abs(effectiveAlpha(6, 1.f, .4f) - .4f) < 1e-6f);
    assert(std::abs(effectiveAlpha(5, .47f, .4f) - .4f) < 1e-6f);
    assert(std::abs(effectiveAlpha(6, NAN, .4f) - .4f) < 1e-6f);

    unsetenv("TOP_HISTORICAL_STRIP_PARENT_ALPHA");
    assert(top::historical_strip_alpha::enabled());
}
