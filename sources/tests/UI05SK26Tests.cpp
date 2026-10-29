#include "top/HistoricalBlend.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
using top::historic_blend::glFactor;
using top::historic_blend::valid;
int main() {
    assert(::unsetenv("TOP_HISTORICAL_BLEND") == 0);
    assert(top::historic_blend::enabled());
    assert(::unsetenv("TOP_HISTORICAL_DEST_ALPHA")==0);
    assert(top::historic_blend::opaqueDestination());
    assert(::setenv("TOP_HISTORICAL_DEST_ALPHA","framebuffer",1)==0);
    assert(top::historic_blend::opaqueDestination());
    assert(::unsetenv("TOP_HISTORICAL_DEST_ALPHA")==0);
    assert(top::historic_blend::effective(5,7).source == 5);
    // D3DBLEND constants are not 1-to-1 with OpenGL enum values.
    const unsigned expected[] = {0,0,1,0x0300,0x0301,0x0302,0x0303,
                                 0x0304,0x0305,0x0306,0x0307,0x0308};
    for (int f=1; f<=11; ++f) assert(glFactor(f)==expected[f]);
    assert(valid(5,2) && valid(5,6) && valid(5,7));
    assert(valid(11,2)); // SRC_ALPHA_SATURATE is allowed as source
    assert(!valid(5,11)); // not allowed as destination
    assert(!valid(-1,2) && !valid(0,2) && !valid(12,2));
    assert(::setenv("TOP_HISTORICAL_BLEND","0",1)==0);
    assert(top::historic_blend::effective(5,6).source == 5);
    assert(top::historic_blend::opaqueDestination());
    assert(::setenv("TOP_HISTORICAL_BLEND","1",1)==0);
    const auto conventional=top::historic_blend::effective(5,6);
    assert(conventional.source==5 && conventional.destination==6);
    const auto destinationAlpha=top::historic_blend::effective(5,7);
    assert(destinationAlpha.source==5 && destinationAlpha.destination==7);
    assert(glFactor(conventional.destination)==0x0303);
    assert(glFactor(destinationAlpha.destination)==0x0304);
    assert(::setenv("TOP_HISTORICAL_BLEND","true",1)==0);
    assert(top::historic_blend::effective(5,7).source==5); // only explicit 0 requests compatibility
    assert(::unsetenv("TOP_HISTORICAL_BLEND")==0);
    std::cout << "SK2.6: DX9 -> GL 11 source factors, destination validation, "
                 "5/2 5/6 5/7 differentiation, historical default with 0 rollback OK\n";
}
