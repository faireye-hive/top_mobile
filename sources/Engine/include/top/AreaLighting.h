#pragma once

// Iluminacao ambiental por setor. O byte island de cada tile referencia a
// mesma ficha de scripts/table/areaset.txt usada por CGameScene no cliente DX9.

#include <string>
#include <vector>

namespace top {

struct AreaLight {
    int id = 0;
    std::string name = "Area desconhecida";
    float environment[3] = {128.f / 255.f, 128.f / 255.f, 128.f / 255.f};
    float diffuse[3] = {1.f, 1.f, 1.f};
    float direction[3] = {-1.f, -1.f, -1.f};
    bool city = false;
    bool valid = false;
};

class AreaLighting {
public:
    bool open(const std::string& path);
    const AreaLight& at(int id) const;
    int count() const { return count_; }

private:
    AreaLight fallback_;
    std::vector<AreaLight> areas_;
    int count_ = 0;
};

}  // namespace top
