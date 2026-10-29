#pragma once

// Tabela scripts/table/terraininfo.txt. O id e o btTexNo do tile.
// O caminho e o mesmo que MPTerrainSet grava em szDataName.

#include <string>
#include <unordered_map>

namespace top {

class TerrainTable {
public:
    bool load(const std::string& path);
    const std::string* fileFor(int id) const;
    int count() const { return static_cast<int>(files_.size()); }

private:
    std::unordered_map<int, std::string> files_;
};

}  // namespace top
