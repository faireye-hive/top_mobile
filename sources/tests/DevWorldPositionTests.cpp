#include "top/DevWorldPosition.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <string>
#include <unistd.h>
int main() {
    char directory[] = "/tmp/top-devworld-XXXXXX";
    assert(mkdtemp(directory));
    const std::string file = std::string(directory) + "/location.txt";
    top::dev_world::Location point{"magicsea", 123.75f, 884.5f, 1.75f, true};
    assert(top::dev_world::write(file, point));
    top::dev_world::Location roundTrip;
    assert(top::dev_world::read(file, roundTrip));
    assert(roundTrip.map == point.map && roundTrip.boat);
    assert(std::fabs(roundTrip.x - point.x) < 0.0001f);
    assert(std::fabs(roundTrip.y - point.y) < 0.0001f);
    assert(std::fabs(roundTrip.yaw - point.yaw) < 0.0001f);
    assert(!top::dev_world::valid({"../outside", 1.f, 1.f, 0.f, false}));
    assert(!top::dev_world::valid({"garner", NAN, 1.f, 0.f, false}));
    assert(top::dev_world::path("Client", 9).find("location-9.txt") != std::string::npos);
    assert(!top::dev_world::write(file, {"../bad", 1.f, 1.f, 0.f, false}));
    assert(top::dev_world::read(file, roundTrip)); // failed write retains previous
    { std::ofstream f(file, std::ios::trunc); f << "TOPLOC1 ../bad 5 5 0 0\n"; }
    assert(!top::dev_world::read(file, roundTrip));
    { std::ofstream f(file, std::ios::trunc); f << "TOPLOC1 garner 5 5 0 0 OVERFLOW\n"; }
    assert(!top::dev_world::read(file, roundTrip));
    std::remove(file.c_str());
    rmdir(directory);
    std::puts("dev world position: roundtrip, input validation, preservation OK");
}
