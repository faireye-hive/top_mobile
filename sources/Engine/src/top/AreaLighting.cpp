#include "top/AreaLighting.h"

#include <cstdio>
#include <cstdlib>

namespace top {
namespace {

bool triple(const std::string& text, float out[3], float scale) {
    char* end = nullptr;
    const char* cursor = text.c_str();
    for (int i = 0; i < 3; ++i) {
        const float value = std::strtof(cursor, &end);
        if (end == cursor) return false;
        out[i] = value * scale;
        if (i < 2) {
            if (*end != ',') return false;
            cursor = end + 1;
        }
    }
    return true;
}

std::vector<std::string> columns(const std::string& line) {
    std::vector<std::string> out;
    std::size_t begin = 0;
    while (begin <= line.size()) {
        const std::size_t end = line.find('\t', begin);
        out.push_back(line.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return out;
}

}  // namespace

bool AreaLighting::open(const std::string& path) {
    areas_.clear();
    count_ = 0;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    char lineBuffer[1024];
    while (std::fgets(lineBuffer, sizeof lineBuffer, file)) {
        std::string line(lineBuffer);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        if (line.empty() || line[0] == '/') continue;
        const std::vector<std::string> col = columns(line);
        if (col.size() < 8) continue;
        const int id = std::atoi(col[0].c_str());
        if (id <= 0 || id > 255) continue;
        AreaLight area;
        area.id = id;
        area.name = col[1];
        if (!triple(col[4], area.environment, 1.f / 255.f) || !triple(col[5], area.diffuse, 1.f / 255.f) ||
            !triple(col[6], area.direction, 1.f)) {
            continue;
        }
        area.city = std::atoi(col[7].c_str()) != 0;
        area.valid = true;
        if (areas_.size() <= static_cast<std::size_t>(id)) areas_.resize(static_cast<std::size_t>(id) + 1);
        if (!areas_[static_cast<std::size_t>(id)].valid) ++count_;
        areas_[static_cast<std::size_t>(id)] = std::move(area);
    }
    std::fclose(file);
    return count_ > 0;
}

const AreaLight& AreaLighting::at(int id) const {
    if (id > 0 && static_cast<std::size_t>(id) < areas_.size() && areas_[static_cast<std::size_t>(id)].valid)
        return areas_[static_cast<std::size_t>(id)];
    return fallback_;
}

}  // namespace top
