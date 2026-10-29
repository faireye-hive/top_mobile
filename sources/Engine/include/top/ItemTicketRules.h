#pragma once
#include <optional>
#include <string>

namespace top::item_tickets {
struct Destination {
    int itemId=0;
    const char* city=nullptr;
    const char* map=nullptr;
    float x=0.f,y=0.f;
};
// Static, audited TicketSys + birth_conf mapping. Does not run Lua or use GL.
std::optional<Destination> destinationFor(int id, const std::string& callback);
struct Conditions {
    int level=1;
    float hp=0, maxHp=1, sp=0, maxSp=1;
    bool boat=false;
    std::string currentMap;
};
enum class Rejection { None, Boat, Health, ProhibitedMap, InvalidLevel };
Rejection check(const Conditions& conditions);
} // namespace top::item_tickets
