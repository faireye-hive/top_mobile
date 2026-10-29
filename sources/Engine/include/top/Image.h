#pragma once

// Imagem em RGBA, primeira linha no topo, como o DirectX entrega a textura.
// O arquivo pode estar no embaralhamento de lwTexEncode::_Decode1
// (rodape "mp.x" e troca dos 44 bytes das pontas).

#include <cstdint>
#include <string>
#include <vector>

namespace top {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;

    bool load(const std::string& path);
    // BMP, TGA, DDS ou PNG ja em memoria. Nao desfaz o embaralhamento mp.x.
    bool decode(const std::uint8_t* data, std::size_t size);
};

}  // namespace top
