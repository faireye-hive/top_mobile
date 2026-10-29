#include "top/HistoricalShadeMesh.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
    using top::historical_shade::build;

    bool sawEdgeX = false, sawEdgeZ = false;
    const auto mesh = build(.5f, .5f, 1.f, 3.f,
        [&sawEdgeX, &sawEdgeZ](float x, float z, bool edgeX, bool edgeZ) {
            sawEdgeX = sawEdgeX || edgeX;
            sawEdgeZ = sawEdgeZ || edgeZ;
            return 10.f + x + z;
        });
    assert(mesh.size() == 24u); // two grid cells by two, six vertices each
    assert(std::fabs(mesh[0].x) < 1e-6f);
    assert(std::fabs(mesh[0].z) < 1e-6f);
    assert(std::fabs(mesh[0].y - 10.015f) < 1e-5f);
    assert(std::fabs(mesh[0].u - .25f) < 1e-6f);
    assert(std::fabs(mesh[0].v - .25f) < 1e-6f);
    assert(mesh[1].x == 1.f && mesh[2].z == 1.f);
    assert(mesh[3].x == mesh[1].x && mesh[4].x == 1.f && mesh[4].z == 1.f);
    assert(sawEdgeX && sawEdgeZ);

    const auto noRadius = build(0.f, 0.f, 0.f, 3.f,
        [](float, float, bool, bool) { return 0.f; });
    const auto badRadius = build(0.f, 0.f, NAN, 3.f,
        [](float, float, bool, bool) { return 0.f; });
    assert(noRadius.empty() && badRadius.empty());
    const auto fallback = build(.5f, .5f, 1.f, 7.f,
        [](float, float, bool, bool) { return NAN; });
    assert(!fallback.empty() && std::fabs(fallback.front().y - 7.015f) < 1e-5f);

    unsetenv("TOP_HISTORICAL_SHADE");
    assert(top::historical_shade::enabled());
    unsetenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID");
    assert(top::historical_shade::terrainGridEnabled());
    setenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID", "1", 1);
    assert(top::historical_shade::terrainGridEnabled());
    setenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID", "0", 1);
    assert(top::historical_shade::terrainGridEnabled());
    setenv("TOP_HISTORICAL_SHADE", "1", 1);
    assert(top::historical_shade::enabled());
    setenv("TOP_HISTORICAL_SHADE", "0", 1);
    assert(top::historical_shade::enabled());
    unsetenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID");

    std::cout << "SK2.17-M: historical SHADE mesh projection and opt-in OK\n";
}
