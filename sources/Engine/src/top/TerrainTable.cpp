#include "top/TerrainTable.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace top {

bool TerrainTable::load(const std::string& path) {
    files_.clear();
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::size_t i = 0;
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i]))) continue;
        std::stringstream row(line);
        int id = 0;
        std::string file;
        if (!(row >> id >> file)) continue;
        files_[id] = file;
    }
    return !files_.empty();
}

const std::string* TerrainTable::fileFor(int id) const {
    const auto it = files_.find(id);
    if (it == files_.end()) return nullptr;
    return &it->second;
}

}  // namespace top
