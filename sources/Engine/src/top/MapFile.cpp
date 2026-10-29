#include "top/MapFile.h"

#include <cmath>
#include <cstdio>
#include <utility>
#include <cstring>

namespace top {
namespace {

constexpr int kFlag = 780627;
constexpr int kTileBytes = 15;
constexpr int kSection = 8;

std::uint8_t rgb565Channel(int packed, int shift, int mask, int adjust) {
    int value = (packed & mask) >> shift;
    value <<= adjust;
    if (value > 255) value = 255;
    return static_cast<std::uint8_t>(value);
}

}  // namespace

bool MapFile::open(const std::string& path) {
    close();
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;

    int header[5] = {};
    if (std::fread(header, sizeof(int), 5, file) != 5 || header[0] != kFlag) {
        std::fclose(file);
        return false;
    }
    const int width = header[1];
    const int height = header[2];
    const int sectionW = header[3];
    const int sectionH = header[4];
    if (sectionW != kSection || sectionH != kSection || width <= 0 || height <= 0) {
        std::fclose(file);
        return false;
    }
    // Mapas pequenos como room.map (52x52) terminam com uma secao parcial.
    // A tabela de offsets usa ceil, embora cada bloco presente continue com
    // o armazenamento cheio de 8x8 tiles.
    const int sectionsX = width / kSection + (width % kSection != 0);
    const int sectionsY = height / kSection + (height % kSection != 0);
    const std::uint64_t count = static_cast<std::uint64_t>(sectionsX) * sectionsY;
    const std::uint64_t dataOffset = sizeof(header) + count * sizeof(std::uint32_t);
    // Uma tabela incompleta nao e uma lista de secoes vazias. Valida antes
    // de alocar, inclusive quando as dimensoes do cabecalho sao corrompidas.
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long fileSize = std::ftell(file);
    if (fileSize < 0 || dataOffset > static_cast<std::uint64_t>(fileSize) ||
        std::fseek(file, sizeof(header), SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    offsets_.assign(static_cast<std::size_t>(count), 0);
    if (std::fread(offsets_.data(), sizeof(std::uint32_t), static_cast<std::size_t>(count), file) !=
        static_cast<std::size_t>(count)) {
        std::fclose(file);
        offsets_.clear();
        return false;
    }
    width_ = width;
    height_ = height;
    sectionsX_ = sectionsX;
    sectionsY_ = sectionsY;
    dataOffset_ = dataOffset;
    fileSize_ = static_cast<std::uint64_t>(fileSize);
    file_ = file;
    return true;
}

void MapFile::close() {
    if (file_) std::fclose(static_cast<FILE*>(file_));
    file_ = nullptr;
    offsets_.clear();
    cache_.clear();
    width_ = height_ = sectionsX_ = sectionsY_ = 0;
    dataOffset_ = fileSize_ = 0;
}

void MapFile::swap(MapFile& other) noexcept {
    using std::swap;
    swap(file_, other.file_);
    swap(width_, other.width_);
    swap(height_, other.height_);
    swap(sectionsX_, other.sectionsX_);
    swap(sectionsY_, other.sectionsY_);
    swap(dataOffset_, other.dataOffset_);
    swap(fileSize_, other.fileSize_);
    offsets_.swap(other.offsets_);
    cache_.swap(other.cache_);
}

MapFile::Section& MapFile::section(int sx, int sy) {
    const std::uint64_t key = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sy)) << 32) |
                              static_cast<std::uint32_t>(sx);
    Section& slot = cache_[key];
    if (slot.state != Section::State::Unknown) return slot;
    slot.state = Section::State::Invalid;
    if (!file_ || sx < 0 || sy < 0 || sx >= sectionsX_ || sy >= sectionsY_) return slot;
    const std::uint32_t offset = offsets_[static_cast<std::size_t>(sy) * sectionsX_ + sx];
    if (offset == 0) {
        slot.state = Section::State::Empty;
        return slot;
    }
    constexpr std::uint64_t sectionBytes = kSection * kSection * kTileBytes;
    if (offset < dataOffset_ || static_cast<std::uint64_t>(offset) + sectionBytes > fileSize_) return slot;
    FILE* file = static_cast<FILE*>(file_);
    if (std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0) return slot;
    slot.raw.resize(static_cast<std::size_t>(kSection * kSection * kTileBytes));
    if (std::fread(slot.raw.data(), 1, slot.raw.size(), file) != slot.raw.size()) {
        slot.raw.clear();
        return slot;
    }
    slot.state = Section::State::Present;
    return slot;
}

Tile MapFile::at(int x, int y) {
    Tile tile;
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return tile;
    Section& block = section(x / kSection, y / kSection);
    if (block.state == Section::State::Empty) {
        // MPTile::Init + MPMap::_pDefaultTile: regiao/ilha zero, quatro
        // bloqueios zero e branco; MPMap aplica altura e textura submarinas.
        tile.height = -2.f;
        tile.r = tile.g = tile.b = 255;
        tile.layer[0] = {22, 15};
        tile.layer[1].tex = 255; // IsDefault historico; alpha zero, sem overlay.
        tile.blocked = false;
        tile.valid = true;
        return tile;
    }
    if (block.state != Section::State::Present) return tile;
    const int local = (y % kSection) * kSection + (x % kSection);
    const std::uint8_t* raw = block.raw.data() + local * kTileBytes;
    const std::uint16_t color = static_cast<std::uint16_t>(raw[5] | (raw[6] << 8));
    int heightByte = raw[7];
    if (heightByte >= 128) heightByte -= 256;
    tile.height = static_cast<float>(heightByte) * 0.1f;
    tile.r = rgb565Channel(color, 8, 0xf800, 0);
    tile.g = rgb565Channel(color, 3, 0x07e0, 0);
    tile.b = rgb565Channel(color, 0, 0x001f, 3);
    tile.region = static_cast<std::uint16_t>(raw[8] | (raw[9] << 8));
    tile.island = raw[10];
    // R do RGB565 ja vem deslocado 8, que e o byte alto. O macro do motor
    // faz (rgb & 0xf800) >> 8. G e (rgb & 0x7e0) >> 3. B e (rgb & 0x1f) << 3.
    tile.blocked = (raw[11] & 128) || (raw[12] & 128) || (raw[13] & 128) || (raw[14] & 128);
    // TileInfo_5To8: a base e o byte 4. O DWORD guarda tres pares
    // (textura de 6 bits, alpha de 4 bits), do mais alto para o mais baixo.
    const std::uint32_t packed = static_cast<std::uint32_t>(raw[0]) |
                                 (static_cast<std::uint32_t>(raw[1]) << 8) |
                                 (static_cast<std::uint32_t>(raw[2]) << 16) |
                                 (static_cast<std::uint32_t>(raw[3]) << 24);
    tile.layer[0].tex = raw[4];
    tile.layer[0].alpha = 15;
    tile.layer[1].tex = static_cast<std::uint8_t>((packed >> 26) & 63);
    tile.layer[1].alpha = static_cast<std::uint8_t>((packed >> 22) & 15);
    tile.layer[2].tex = static_cast<std::uint8_t>((packed >> 16) & 63);
    tile.layer[2].alpha = static_cast<std::uint8_t>((packed >> 12) & 15);
    tile.layer[3].tex = static_cast<std::uint8_t>((packed >> 6) & 63);
    tile.layer[3].alpha = static_cast<std::uint8_t>((packed >> 2) & 15);
    tile.valid = true;
    return tile;
}

float MapFile::heightAt(float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0.f || y < 0.f || x >= width_ || y >= height_) return 0.f;
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const Tile t0 = const_cast<MapFile*>(this)->at(ix, iy);
    if (!t0.valid) return 0.f;
    const Tile t1 = const_cast<MapFile*>(this)->at(ix + 1, iy);
    const Tile t2 = const_cast<MapFile*>(this)->at(ix, iy + 1);
    const Tile t3 = const_cast<MapFile*>(this)->at(ix + 1, iy + 1);
    const float h0 = t0.height;
    const float h1 = t1.valid ? t1.height : h0;
    const float h2 = t2.valid ? t2.height : h0;
    const float h3 = t3.valid ? t3.height : h0;
    const float fx = x - static_cast<float>(ix);
    const float fy = y - static_cast<float>(iy);
    if (fx + fy <= 1.f) return h0 + fx * (h1 - h0) + fy * (h2 - h0);
    return h3 + (1.f - fx) * (h2 - h3) + (1.f - fy) * (h1 - h3);
}

float MapFile::range2GroundHeightAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0.f;
    Section& block = const_cast<MapFile*>(this)->section(x / kSection, y / kSection);
    // MPMap's default MPTile has btBlock[0] == 0, so the historical
    // GetGridHeight result for an empty section is zero, not cHeight (-2 m).
    if (block.state == Section::State::Empty) return 0.f;
    if (block.state != Section::State::Present) return 0.f;
    const int local = (y % kSection) * kSection + (x % kSection);
    const std::uint8_t encoded = block.raw[static_cast<std::size_t>(local) * kTileBytes + 11];
    float height = static_cast<float>(encoded & 63u);
    height *= 5.0f;
    if (encoded & 64u) return height / 100.0f * -1.0f;
    return height / 100.0f;
}

float MapFile::sceneGridHeightAt(float x, float y) const {
    if (!file_ || !std::isfinite(x) || !std::isfinite(y)) return 0.f;
    const float scaledX = x * 2.f, scaledY = y * 2.f;
    // CGameScene converts after multiplying: small negative coordinates
    // truncate to grid zero. Check limits before converting to an integer.
    if (!std::isfinite(scaledX) || !std::isfinite(scaledY) ||
        scaledX <= -1.f || scaledY <= -1.f ||
        scaledX >= static_cast<double>(width_) * 2.0 ||
        scaledY >= static_cast<double>(height_) * 2.0) return 0.f;
    const auto gridX = static_cast<std::int64_t>(scaledX);
    const auto gridY = static_cast<std::int64_t>(scaledY);
    const int tileX = static_cast<int>(gridX / 2);
    const int tileY = static_cast<int>(gridY / 2);
    Section& block = const_cast<MapFile*>(this)->section(tileX / kSection, tileY / kSection);
    if (block.state != Section::State::Present) return 0.f;
    const int local = (tileY % kSection) * kSection + (tileX % kSection);
    const int quadrant = static_cast<int>((gridY % 2) * 2 + gridX % 2);
    const std::uint8_t encoded = block.raw[static_cast<std::size_t>(local) * kTileBytes + 11 + quadrant];
    float height = static_cast<float>(encoded & 63u);
    height *= 5.f;
    return (encoded & 64u) ? height / 100.f * -1.f : height / 100.f;
}

float MapFile::shadeGroundHeightAt(float x, float y, bool edgeX, bool edgeY) const {
    if (!std::isfinite(x) || !std::isfinite(y) || !file_ ||
        x < 0.f || y < 0.f || x >= width_ || y >= height_) return 0.f;
    const int tileX = static_cast<int>(x);
    const int tileY = static_cast<int>(y);
    Tile tile = const_cast<MapFile*>(this)->at(tileX, tileY);
    if (!tile.valid) return 0.f;
    const float tileHeight = tile.height;

    std::int64_t gridX = static_cast<std::int64_t>(x * 2.0);
    std::int64_t gridY = static_cast<std::int64_t>(y * 2.0);
    if (edgeX) --gridX;
    if (edgeY) --gridY;
    // MPMap::IsGridBlock returns blocked outside its loaded grid window;
    // GetGridHeight and GetTileHeight both return zero there.
    if (gridX < 0 || gridY < 0 || gridX >= static_cast<std::int64_t>(width_) * 2 ||
        gridY >= static_cast<std::int64_t>(height_) * 2)
        return tileHeight;

    const int subTileX = static_cast<int>(gridX / 2);
    const int subTileY = static_cast<int>(gridY / 2);
    Section& block = const_cast<MapFile*>(this)->section(subTileX / kSection,
                                                        subTileY / kSection);
    std::uint8_t encoded = 0;
    if (block.state == Section::State::Present) {
        const int local = (subTileY % kSection) * kSection + (subTileX % kSection);
        const int quadrant = static_cast<int>((gridY % 2) * 2 + (gridX % 2));
        encoded = block.raw[static_cast<std::size_t>(local) * kTileBytes + 11 + quadrant];
    } else if (block.state != Section::State::Empty) {
        return tileHeight;
    }

    float gridHeight = static_cast<float>(encoded & 63u) * .05f;
    if (encoded & 64u) gridHeight = -gridHeight;
    const bool gridBlocked = (encoded & 128u) != 0;
    return gridHeight > tileHeight && !gridBlocked ? gridHeight : tileHeight;
}

std::uint16_t MapFile::regionAt(float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0.f || y < 0.f || x >= width_ || y >= height_) return 0;
    const Tile tile = const_cast<MapFile*>(this)->at(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
    return tile.valid ? tile.region : 0;
}

bool MapFile::blockedAt(float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0.f || y < 0.f || x >= width_ || y >= height_) return true;
    const auto cmX = static_cast<std::int64_t>(x * 100.f);
    const auto cmY = static_cast<std::int64_t>(y * 100.f);
    const auto gridX = cmX / 50;
    const auto gridY = cmY / 50;
    const int tileX = static_cast<int>(gridX / 2);
    const int tileY = static_cast<int>(gridY / 2);
    Tile tile = const_cast<MapFile*>(this)->at(tileX, tileY);
    // g_IsMoveAble(player, LAND): somente LAND ou BRIDGE, sem usar altura.
    constexpr std::uint16_t kLand = 0x0001;
    constexpr std::uint16_t kBridge = 0x0008;
    if (!tile.valid || (tile.region & (kLand | kBridge)) == 0) return true;
    const int sub = (gridY % 2) * 2 + (gridX % 2);
    const int sx = tileX / kSection;
    const int sy = tileY / kSection;
    Section& block = const_cast<MapFile*>(this)->section(sx, sy);
    if (block.state != Section::State::Present) return true;
    const int local = (tileY % kSection) * kSection + (tileX % kSection);
    const std::uint8_t* raw = block.raw.data() + local * kTileBytes;
    return (raw[11 + sub] & 128) != 0;
}

bool MapFile::sailableAt(float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0.f || y < 0.f || x >= width_ || y >= height_) return false;
    const auto cmX = static_cast<std::int64_t>(x * 100.f);
    const auto cmY = static_cast<std::int64_t>(y * 100.f);
    const auto gridX = cmX / 50;
    const auto gridY = cmY / 50;
    const int tileX = static_cast<int>(gridX / 2);
    const int tileY = static_cast<int>(gridY / 2);
    const Tile tile = const_cast<MapFile*>(this)->at(tileX, tileY);
    constexpr std::uint16_t kLand = 0x0001;
    if (!tile.valid || (tile.region & kLand) != 0) return false;
    Section& block = const_cast<MapFile*>(this)->section(tileX / kSection, tileY / kSection);
    if (block.state == Section::State::Empty) return true;
    if (block.state != Section::State::Present) return false;
    const int local = (tileY % kSection) * kSection + (tileX % kSection);
    const std::uint8_t* raw = block.raw.data() + local * kTileBytes;
    const int sub = (gridY % 2) * 2 + (gridX % 2);
    return (raw[11 + sub] & 128) == 0;
}

}  // namespace top
