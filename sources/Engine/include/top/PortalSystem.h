#pragma once

// Portais offline lidos dos *swhmap.txt do GameServer antigo. Os arquivos
// sao apenas dados: nenhuma parte do servidor e executada.

#include <string>
#include <vector>

namespace top {

struct Portal {
    int id = 0;
    float x = 0.f;
    float y = 0.f;
    std::string targetMap;
    float targetX = 0.f;
    float targetY = 0.f;
    std::string name;
};

class PortalSystem {
public:
    bool load(const std::string& mapName);
    bool near(float x, float y, float radius, Portal& out) const;
    static bool selfTest();
    const std::vector<Portal>& portals() const { return portals_; }

private:
    static bool parse(const std::string& line, Portal& out);
    std::vector<Portal> portals_;
};

}  // namespace top
