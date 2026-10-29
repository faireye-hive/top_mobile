#pragma once

// Leitor do .map MindPower versao 780627.
// O layout do tile e o de SNewFileTile em Libraries/Util/include/MPMapDef.h
// (15 bytes, pack 1). Altura em metros = int8 * 0.1. Cor e RGB565.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace top {

struct Layer {
    std::uint8_t tex = 0;
    std::uint8_t alpha = 0;
};

struct Tile {
    float height = 0.f;
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    // Quatro camadas, na ordem de MPTile::TexLayer.
    // layer[0] e btTileInfo. As outras tres saem de dwTileInfo,
    // o mesmo empacotamento de TileInfo_5To8 em MPMapDef.h.
    Layer layer[4];
    // EAreaMask historico (LAND=1, BRIDGE=8, etc.). Movimento nao pode ser
    // inferido pela altura: praias e pontes podem ficar no nivel do mar.
    std::uint16_t region = 0;
    std::uint8_t island = 0;
    bool blocked = true;
    bool valid = false;
};

class MapFile {
public:
    bool open(const std::string& path);
    void close();
    // Troca o estado carregado sem copiar o FILE*; usado para confirmar
    // transicoes somente depois de validar o mapa/posicao de chegada.
    void swap(MapFile& other) noexcept;

    int width() const { return width_; }
    int height() const { return height_; }
    bool ok() const { return file_ != nullptr; }

    // Offset zero dentro do mapa representa o fundo padrao historico (-2 m,
    // textura 22, regiao mar e sem bloqueios). Falha de leitura e fora do mapa
    // continuam invalidos, para nao liberar navegacao em dados corrompidos.
    Tile at(int x, int y);
    // Mesmo corte em dois triangulos de MPMap::GetHeight.
    float heightAt(float x, float y) const;
    // Altura object/subgrid armazenada em btBlock[0], consultada por RANGE2
    // no DX9 em GetGridHeight(int(x) * 2, int(y) * 2).
    float range2GroundHeightAt(int x, int y) const;
    // CGameScene::GetGridHeight: int(world * 2), then MPMap's four
    // btBlock height quadrants; no triangle interpolation or block filtering.
    float sceneGridHeightAt(float x, float y) const;
    // Altura do vértice SHADE: compara cHeight com o quadrante btBlock e
    // preserva o ajuste de amostra nas bordas da grade CMPShadeMap.
    float shadeGroundHeightAt(float x, float y, bool edgeX, bool edgeY) const;
    std::uint16_t regionAt(float x, float y) const;
    bool blockedAt(float x, float y) const;
    bool sailableAt(float x, float y) const;

private:
    struct Section {
        enum class State { Unknown, Empty, Present, Invalid };
        State state = State::Unknown;
        std::vector<std::uint8_t> raw;
    };

    Section& section(int sx, int sy);

    void* file_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int sectionsX_ = 0;
    int sectionsY_ = 0;
    std::uint64_t dataOffset_ = 0;
    std::uint64_t fileSize_ = 0;
    std::vector<std::uint32_t> offsets_;
    std::unordered_map<std::uint64_t, Section> cache_;
};

}  // namespace top
