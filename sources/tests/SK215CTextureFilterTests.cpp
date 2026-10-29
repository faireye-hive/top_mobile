#include "top/HistoricalTextureFilter.h"
#include <iostream>
#include <stdexcept>
#define REQUIRE(expr) do { if (!(expr)) throw std::runtime_error(#expr); } while (0)
int main() {
    using namespace top::historical_filter;
    REQUIRE(supported(1)==1 && supported(2)==2);
    REQUIRE(supported(0)==0 && supported(3)==0 && supported(999)==0);
    REQUIRE(same(choose(1,1,true), {1,1}));
    REQUIRE(same(choose(2,2,true), {2,2}));
    REQUIRE(same(choose(1,2,true), {1,2}));
    REQUIRE(same(choose(2,1,true), {2,1}));
    REQUIRE(same(choose(7,2,true), {0,2}));
    REQUIRE(same(choose(1,2,false), {0,0}));
    REQUIRE(!same(choose(1,1,true),choose(2,2,true)));
    std::cout << "SK2.15-C: D3D point/linear policy and opt-out OK\n";
}
