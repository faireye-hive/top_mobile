#include "top/Image.h"
#include "top/TexturePaths.h"

#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace top {
namespace {

// lwTexEncode::_Decode1: se o arquivo termina em "mp.x", tira esses 4 bytes
// e troca os 44 primeiros com os 44 ultimos. A troca e a propria inversa.
bool decodeMindPower(std::vector<std::uint8_t>& data) {
    if (data.size() < 4) return false;
    const std::uint8_t* tail = data.data() + data.size() - 4;
    if (std::memcmp(tail, "mp.x", 4) != 0) return true;
    data.resize(data.size() - 4);
    constexpr int kSwap = 44;
    if (data.size() <= static_cast<std::size_t>(kSwap)) return false;
    std::uint8_t tmp[kSwap];
    std::memcpy(tmp, data.data(), kSwap);
    std::memcpy(data.data(), data.data() + data.size() - kSwap, kSwap);
    std::memcpy(data.data() + data.size() - kSwap, tmp, kSwap);
    return true;
}

std::uint16_t ru16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t ru32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

std::int32_t ri32(const std::uint8_t* p) {
    return static_cast<std::int32_t>(ru32(p));
}

void paint(std::uint8_t* dst, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;
}

bool loadBmp(const std::uint8_t* data, std::size_t size, Image& out) {
    if (size < 54 || data[0] != 'B' || data[1] != 'M') return false;
    const std::uint32_t pixelOffset = ru32(data + 10);
    const std::uint32_t headerSize = ru32(data + 14);
    if (headerSize < 40 || pixelOffset >= size) return false;
    const int width = ri32(data + 18);
    const std::int32_t rawHeight = ri32(data + 22);
    const int height = rawHeight < 0 ? -rawHeight : rawHeight;
    const int bpp = ru16(data + 28);
    const std::uint32_t compression = ru32(data + 30);
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
    if (compression != 0 || (bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)) return false;

    const bool bottomUp = rawHeight > 0;
    const int stride = ((width * bpp + 31) / 32) * 4;
    if (pixelOffset + static_cast<std::size_t>(stride) * static_cast<std::size_t>(height) > size) return false;

    out.width = width;
    out.height = height;
    out.rgba.assign(static_cast<std::size_t>(width * height * 4), 255);

    const std::uint8_t* palette = data + 14 + headerSize;
    int colors = 0;
    if (bpp == 8) {
        if (pixelOffset < 14 + headerSize) return false;
        colors = static_cast<int>((pixelOffset - (14 + headerSize)) / 4);
        if (colors <= 0) return false;
    }

    for (int y = 0; y < height; ++y) {
        const int srcY = bottomUp ? (height - 1 - y) : y;
        const std::uint8_t* row = data + pixelOffset + static_cast<std::size_t>(srcY) * stride;
        std::uint8_t* dst = out.rgba.data() + static_cast<std::size_t>(y * width * 4);
        if (bpp == 8) {
            for (int x = 0; x < width; ++x) {
                const int index = row[x];
                if (index >= colors) {
                    paint(dst + x * 4, 0, 0, 0, 255);
                    continue;
                }
                const std::uint8_t* c = palette + index * 4;
                paint(dst + x * 4, c[2], c[1], c[0], 255);
            }
        } else if (bpp == 16) {
            for (int x = 0; x < width; ++x) {
                const unsigned p = ru16(row + x * 2);
                paint(dst + x * 4,
                      static_cast<std::uint8_t>(((p >> 10) & 31) << 3),
                      static_cast<std::uint8_t>(((p >> 5) & 31) << 3),
                      static_cast<std::uint8_t>((p & 31) << 3),
                      255);
            }
        } else if (bpp == 24) {
            for (int x = 0; x < width; ++x) {
                paint(dst + x * 4, row[x * 3 + 2], row[x * 3 + 1], row[x * 3], 255);
            }
        } else {
            for (int x = 0; x < width; ++x) {
                paint(dst + x * 4, row[x * 4 + 2], row[x * 4 + 1], row[x * 4], row[x * 4 + 3]);
            }
        }
    }
    return true;
}

bool loadTga(const std::uint8_t* data, std::size_t size, Image& out) {
    if (size < 18) return false;
    const int idLen = data[0];
    const int colorType = data[1];
    const int type = data[2];
    const int width = ru16(data + 12);
    const int height = ru16(data + 14);
    const int bpp = data[16];
    const int desc = data[17];
    // 2 = sem compressao, 10 = RLE. A UI (new3.tga, new.tga) e tipo 10.
    if ((type != 2 && type != 10) || (bpp != 24 && bpp != 32) || colorType != 0) return false;
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
    const int pixelBytes = bpp / 8;
    std::size_t cursor = 18u + static_cast<std::size_t>(idLen);
    const int count = width * height;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(count) * 4, 255);
    auto take = [&](std::uint8_t* dst) -> bool {
        if (cursor + static_cast<std::size_t>(pixelBytes) > size) return false;
        const std::uint8_t* p = data + cursor;
        const std::uint8_t a = pixelBytes == 4 ? p[3] : 255;
        paint(dst, p[2], p[1], p[0], a);
        cursor += static_cast<std::size_t>(pixelBytes);
        return true;
    };
    if (type == 2) {
        if (cursor + static_cast<std::size_t>(count * pixelBytes) > size) return false;
        for (int i = 0; i < count; ++i) {
            if (!take(pixels.data() + static_cast<std::size_t>(i) * 4)) return false;
        }
    } else {
        int filled = 0;
        while (filled < count) {
            if (cursor >= size) return false;
            const int packet = data[cursor++];
            const int n = (packet & 0x7f) + 1;
            if (filled + n > count) return false;
            if (packet & 0x80) {
                std::uint8_t pixel[4] = {0, 0, 0, 255};
                if (!take(pixel)) return false;
                for (int i = 0; i < n; ++i) std::memcpy(pixels.data() + static_cast<std::size_t>(filled + i) * 4, pixel, 4);
            } else {
                for (int i = 0; i < n; ++i) {
                    if (!take(pixels.data() + static_cast<std::size_t>(filled + i) * 4)) return false;
                }
            }
            filled += n;
        }
    }
    const bool topOrigin = (desc & 0x20) != 0;
    out.width = width;
    out.height = height;
    out.rgba.assign(static_cast<std::size_t>(count) * 4, 255);
    for (int y = 0; y < height; ++y) {
        const int srcY = topOrigin ? y : (height - 1 - y);
        std::memcpy(out.rgba.data() + static_cast<std::size_t>(y * width * 4),
                    pixels.data() + static_cast<std::size_t>(srcY * width * 4),
                    static_cast<std::size_t>(width * 4));
    }
    return true;
}

void rgb565(unsigned packed, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    r = static_cast<std::uint8_t>(((packed >> 11) & 31) * 255 / 31);
    g = static_cast<std::uint8_t>(((packed >> 5) & 63) * 255 / 63);
    b = static_cast<std::uint8_t>((packed & 31) * 255 / 31);
}

void putPixel(Image& out, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    if (x < 0 || y < 0 || x >= out.width || y >= out.height) return;
    paint(out.rgba.data() + static_cast<std::size_t>((y * out.width + x) * 4), r, g, b, a);
}

void decodeColorBlock(const std::uint8_t* block, bool punchThrough, std::uint8_t colors[4][4]) {
    const unsigned c0 = ru16(block);
    const unsigned c1 = ru16(block + 2);
    std::uint8_t r0, g0, b0, r1, g1, b1;
    rgb565(c0, r0, g0, b0);
    rgb565(c1, r1, g1, b1);
    colors[0][0] = r0;
    colors[0][1] = g0;
    colors[0][2] = b0;
    colors[0][3] = 255;
    colors[1][0] = r1;
    colors[1][1] = g1;
    colors[1][2] = b1;
    colors[1][3] = 255;
    if (!punchThrough || c0 > c1) {
        colors[2][0] = static_cast<std::uint8_t>((2 * r0 + r1) / 3);
        colors[2][1] = static_cast<std::uint8_t>((2 * g0 + g1) / 3);
        colors[2][2] = static_cast<std::uint8_t>((2 * b0 + b1) / 3);
        colors[2][3] = 255;
        colors[3][0] = static_cast<std::uint8_t>((r0 + 2 * r1) / 3);
        colors[3][1] = static_cast<std::uint8_t>((g0 + 2 * g1) / 3);
        colors[3][2] = static_cast<std::uint8_t>((b0 + 2 * b1) / 3);
        colors[3][3] = 255;
    } else {
        colors[2][0] = static_cast<std::uint8_t>((r0 + r1) / 2);
        colors[2][1] = static_cast<std::uint8_t>((g0 + g1) / 2);
        colors[2][2] = static_cast<std::uint8_t>((b0 + b1) / 2);
        colors[2][3] = 255;
        colors[3][0] = 0;
        colors[3][1] = 0;
        colors[3][2] = 0;
        colors[3][3] = 0;
    }
}

void paintColorBlock(Image& out, int x0, int y0, const std::uint8_t colors[4][4], std::uint32_t bits, const std::uint8_t* alpha) {
    for (int py = 0; py < 4; ++py) {
        for (int px = 0; px < 4; ++px) {
            const int index = static_cast<int>(bits & 3u);
            bits >>= 2;
            const std::uint8_t* c = colors[index];
            std::uint8_t a = c[3];
            if (alpha) a = alpha[py * 4 + px];
            putPixel(out, x0 + px, y0 + py, c[0], c[1], c[2], a);
        }
    }
}

bool decodeDxt(const std::uint8_t* data, std::size_t size, int width, int height, int kind, Image& out) {
    const int blocksX = (width + 3) / 4;
    const int blocksY = (height + 3) / 4;
    const int blockBytes = kind == 1 ? 8 : 16;
    if (static_cast<std::size_t>(blocksX * blocksY * blockBytes) > size) return false;
    out.width = width;
    out.height = height;
    out.rgba.assign(static_cast<std::size_t>(width * height * 4), 255);
    const std::uint8_t* cursor = data;
    for (int by = 0; by < blocksY; ++by) {
        for (int bx = 0; bx < blocksX; ++bx) {
            const std::uint8_t* block = cursor;
            cursor += blockBytes;
            std::uint8_t alpha[16];
            const std::uint8_t* color = block;
            if (kind == 3) {
                for (int py = 0; py < 4; ++py) {
                    unsigned row = ru16(block + py * 2);
                    for (int px = 0; px < 4; ++px) {
                        alpha[py * 4 + px] = static_cast<std::uint8_t>((row & 15u) * 17u);
                        row >>= 4;
                    }
                }
                color = block + 8;
            } else if (kind == 5) {
                const unsigned a0 = block[0];
                const unsigned a1 = block[1];
                unsigned alphas[8] = {a0, a1, 0, 0, 0, 0, 0, 0};
                if (a0 > a1) {
                    alphas[2] = (6 * a0 + a1) / 7;
                    alphas[3] = (5 * a0 + 2 * a1) / 7;
                    alphas[4] = (4 * a0 + 3 * a1) / 7;
                    alphas[5] = (3 * a0 + 4 * a1) / 7;
                    alphas[6] = (2 * a0 + 5 * a1) / 7;
                    alphas[7] = (a0 + 6 * a1) / 7;
                } else {
                    alphas[2] = (4 * a0 + a1) / 5;
                    alphas[3] = (3 * a0 + 2 * a1) / 5;
                    alphas[4] = (2 * a0 + 3 * a1) / 5;
                    alphas[5] = (a0 + 4 * a1) / 5;
                    alphas[6] = 0;
                    alphas[7] = 255;
                }
                std::uint64_t bits = 0;
                for (int i = 0; i < 6; ++i) bits |= static_cast<std::uint64_t>(block[2 + i]) << (8 * i);
                for (int i = 0; i < 16; ++i) {
                    alpha[i] = static_cast<std::uint8_t>(alphas[bits & 7u]);
                    bits >>= 3;
                }
                color = block + 8;
            }
            std::uint8_t colors[4][4];
            decodeColorBlock(color, kind == 1, colors);
            const std::uint32_t bits = ru32(color + 4);
            paintColorBlock(out, bx * 4, by * 4, colors, bits, kind == 1 ? nullptr : alpha);
        }
    }
    return true;
}

bool loadDds(const std::uint8_t* data, std::size_t size, Image& out) {
    if (size < 128 || std::memcmp(data, "DDS ", 4) != 0) return false;
    if (ru32(data + 4) != 124) return false;
    const int height = ri32(data + 12);
    const int width = ri32(data + 16);
    const std::uint32_t flags = ru32(data + 8);
    const std::uint32_t pitch = ru32(data + 20);
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
    const std::uint32_t pfFlags = ru32(data + 80);
    const std::uint32_t fourcc = ru32(data + 84);
    const int bitCount = static_cast<int>(ru32(data + 88));
    const std::uint8_t* pixels = data + 128;
    const std::size_t pixelBytes = size - 128;
    auto cc = [](char a, char b, char c, char d) -> std::uint32_t {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(a) | (static_cast<unsigned char>(b) << 8) |
                                          (static_cast<unsigned char>(c) << 16) | (static_cast<unsigned char>(d) << 24));
    };
    if ((pfFlags & 0x4) != 0) {
        int kind = 0;
        if (fourcc == cc('D', 'X', 'T', '1')) kind = 1;
        else if (fourcc == cc('D', 'X', 'T', '2') || fourcc == cc('D', 'X', 'T', '3')) kind = 3;
        else if (fourcc == cc('D', 'X', 'T', '4') || fourcc == cc('D', 'X', 'T', '5')) kind = 5;
        if (kind == 0) return false;
        return decodeDxt(pixels, pixelBytes, width, height, kind, out);
    }
    if (bitCount != 32 && bitCount != 24 && bitCount != 16) return false;
    const int pixelSize = bitCount / 8;
    const int rowPitch = (flags & 0x8) != 0 && pitch >= static_cast<std::uint32_t>(width * pixelSize)
                             ? static_cast<int>(pitch)
                             : width * pixelSize;
    if (pixelBytes < static_cast<std::size_t>(rowPitch) * static_cast<std::size_t>(height)) return false;
    const std::uint32_t rmask = ru32(data + 92);
    const std::uint32_t gmask = ru32(data + 96);
    const std::uint32_t bmask = ru32(data + 100);
    const std::uint32_t amask = ru32(data + 104);
    out.width = width;
    out.height = height;
    out.rgba.assign(static_cast<std::size_t>(width * height * 4), 255);
    auto channel = [](std::uint32_t pixel, std::uint32_t mask) -> std::uint8_t {
        if (mask == 0) return 255;
        unsigned shift = 0;
        std::uint32_t m = mask;
        while ((m & 1u) == 0) {
            m >>= 1;
            ++shift;
        }
        const unsigned value = (pixel & mask) >> shift;
        const unsigned maxValue = m;
        if (maxValue == 0) return 0;
        return static_cast<std::uint8_t>(value * 255u / maxValue);
    };
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row = pixels + static_cast<std::size_t>(y) * rowPitch;
        std::uint8_t* dst = out.rgba.data() + static_cast<std::size_t>(y * width * 4);
        for (int x = 0; x < width; ++x) {
            const std::uint8_t* p = row + x * pixelSize;
            std::uint32_t pixel = p[0];
            if (pixelSize > 1) pixel |= static_cast<std::uint32_t>(p[1]) << 8;
            if (pixelSize > 2) pixel |= static_cast<std::uint32_t>(p[2]) << 16;
            if (pixelSize > 3) pixel |= static_cast<std::uint32_t>(p[3]) << 24;
            if (bitCount == 24 && rmask == 0) {
                paint(dst + x * 4, p[2], p[1], p[0], 255);
                continue;
            }
            const std::uint8_t a = amask == 0 ? 255 : channel(pixel, amask);
            paint(dst + x * 4, channel(pixel, rmask), channel(pixel, gmask), channel(pixel, bmask), a);
        }
    }
    return true;
}

std::uint32_t be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// PNG 8 bits, sem entrelaçamento. A primeira linha do arquivo já é o topo.
bool loadPng(const std::uint8_t* data, std::size_t size, Image& out) {
    static const std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8 || std::memcmp(data, kSig, 8) != 0) return false;
    int width = 0;
    int height = 0;
    int depth = 0;
    int color = -1;
    std::vector<std::uint8_t> idat;
    std::vector<std::uint8_t> palette;
    std::vector<std::uint8_t> trans;
    std::size_t cursor = 8;
    bool sawEnd = false;
    while (cursor + 12 <= size) {
        const std::uint32_t len = be32(data + cursor);
        if (cursor + 12u + len > size) return false;
        const char* kind = reinterpret_cast<const char*>(data + cursor + 4);
        const std::uint8_t* chunk = data + cursor + 8;
        if (std::memcmp(kind, "IHDR", 4) == 0) {
            if (len < 13) return false;
            width = static_cast<int>(be32(chunk));
            height = static_cast<int>(be32(chunk + 4));
            depth = chunk[8];
            color = chunk[9];
            if (chunk[12] != 0) return false;
        } else if (std::memcmp(kind, "PLTE", 4) == 0) {
            palette.assign(chunk, chunk + len);
        } else if (std::memcmp(kind, "tRNS", 4) == 0) {
            trans.assign(chunk, chunk + len);
        } else if (std::memcmp(kind, "IDAT", 4) == 0) {
            idat.insert(idat.end(), chunk, chunk + len);
        } else if (std::memcmp(kind, "IEND", 4) == 0) {
            sawEnd = true;
            break;
        }
        cursor += 12u + len;
    }
    if (!sawEnd || width <= 0 || height <= 0 || width > 4096 || height > 4096 || depth != 8) return false;
    int channels = 0;
    if (color == 0) channels = 1;
    else if (color == 2) channels = 3;
    else if (color == 3) channels = 1;
    else if (color == 4) channels = 2;
    else if (color == 6) channels = 4;
    else return false;
    if (color == 3 && palette.size() < 3) return false;
    const int stride = width * channels;
    const std::size_t rawSize = static_cast<std::size_t>(height) * static_cast<std::size_t>(stride + 1);
    std::vector<std::uint8_t> raw(rawSize);
    z_stream strm;
    std::memset(&strm, 0, sizeof strm);
    strm.next_in = idat.data();
    strm.avail_in = static_cast<unsigned>(idat.size());
    strm.next_out = raw.data();
    strm.avail_out = static_cast<unsigned>(raw.size());
    if (inflateInit(&strm) != Z_OK) return false;
    const int zok = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if (zok != Z_STREAM_END || strm.total_out < rawSize) return false;
    out.width = width;
    out.height = height;
    out.rgba.assign(static_cast<std::size_t>(width * height * 4), 255);
    std::vector<std::uint8_t> prev(static_cast<std::size_t>(stride), 0);
    std::vector<std::uint8_t> row(static_cast<std::size_t>(stride), 0);
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* src = raw.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride + 1);
        const int filter = src[0];
        const std::uint8_t* in = src + 1;
        for (int i = 0; i < stride; ++i) {
            const int left = i >= channels ? row[static_cast<std::size_t>(i - channels)] : 0;
            const int up = prev[static_cast<std::size_t>(i)];
            const int ul = i >= channels ? prev[static_cast<std::size_t>(i - channels)] : 0;
            int value = in[i];
            if (filter == 1) value += left;
            else if (filter == 2) value += up;
            else if (filter == 3) value += (left + up) / 2;
            else if (filter == 4) value += paeth(left, up, ul);
            else if (filter != 0) return false;
            row[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value & 255);
        }
        std::uint8_t* dst = out.rgba.data() + static_cast<std::size_t>(y * width * 4);
        for (int x = 0; x < width; ++x) {
            const std::uint8_t* p = row.data() + static_cast<std::size_t>(x * channels);
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            std::uint8_t a = 255;
            if (color == 6) {
                r = p[0];
                g = p[1];
                b = p[2];
                a = p[3];
            } else if (color == 2) {
                r = p[0];
                g = p[1];
                b = p[2];
            } else if (color == 0) {
                r = g = b = p[0];
            } else if (color == 4) {
                r = g = b = p[0];
                a = p[1];
            } else {
                const int index = p[0];
                if (index * 3 + 2 >= static_cast<int>(palette.size())) return false;
                r = palette[static_cast<std::size_t>(index * 3)];
                g = palette[static_cast<std::size_t>(index * 3 + 1)];
                b = palette[static_cast<std::size_t>(index * 3 + 2)];
                if (index < static_cast<int>(trans.size())) a = trans[static_cast<std::size_t>(index)];
            }
            paint(dst + x * 4, r, g, b, a);
        }
        prev.swap(row);
    }
    return true;
}

}  // namespace

bool Image::load(const std::string& path) {
    width = 0;
    height = 0;
    rgba.clear();
    const std::string actualPath = resolveTexturePath(path);
    if (actualPath.empty()) return false;
    FILE* file = std::fopen(actualPath.c_str(), "rb");
    if (!file) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long length = std::ftell(file);
    if (length <= 0 || length > 32 * 1024 * 1024) {
        std::fclose(file);
        return false;
    }
    std::fseek(file, 0, SEEK_SET);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    if (std::fread(data.data(), 1, data.size(), file) != data.size()) {
        std::fclose(file);
        return false;
    }
    std::fclose(file);
    if (!decodeMindPower(data)) return false;
    return decode(data.data(), data.size());
}

bool Image::decode(const std::uint8_t* data, std::size_t size) {
    width = 0;
    height = 0;
    rgba.clear();
    if (!data || size == 0) return false;
    if (size >= 8 && data[0] == 137 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        return loadPng(data, size, *this);
    }
    if (size >= 4 && std::memcmp(data, "DDS ", 4) == 0) return loadDds(data, size, *this);
    if (size >= 2 && data[0] == 'B' && data[1] == 'M') return loadBmp(data, size, *this);
    return loadTga(data, size, *this);
}

}  // namespace top
