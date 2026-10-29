#include "top/Font.h"

#include <dlfcn.h>

#include <cstdint>
#include <cstring>

namespace top {
namespace {

using FtError = long;
using FtLib = struct FtLibRec_*;
using FtFace = struct FtFaceRec_*;
using InitFn = FtError (*)(FtLib*);
using NewFn = FtError (*)(FtLib, const char*, long, FtFace*);
using SizeFn = FtError (*)(FtFace, unsigned, unsigned);
using LoadFn = FtError (*)(FtFace, unsigned long, int);
using DoneFaceFn = FtError (*)(FtFace);
using DoneLibFn = FtError (*)(FtLib);

// Medido em libfreetype.so.6 (FreeType 2.13, 64 bits) nesta máquina.
// face+152 é o glyph slot. No slot, o bitmap começa em +152.
constexpr int kSlotAt = 152;
constexpr int kBitmapAt = 152;
constexpr int kAdvanceAt = 128;
constexpr int kBearingAt = 192;

struct FtApi {
    void* so = nullptr;
    InitFn init = nullptr;
    NewFn neu = nullptr;
    SizeFn size = nullptr;
    LoadFn load = nullptr;
    DoneFaceFn doneFace = nullptr;
    DoneLibFn doneLib = nullptr;

    bool bind() {
        so = dlopen("libfreetype.so.6", RTLD_NOW);
        if (!so) return false;
        init = reinterpret_cast<InitFn>(dlsym(so, "FT_Init_FreeType"));
        neu = reinterpret_cast<NewFn>(dlsym(so, "FT_New_Face"));
        size = reinterpret_cast<SizeFn>(dlsym(so, "FT_Set_Pixel_Sizes"));
        load = reinterpret_cast<LoadFn>(dlsym(so, "FT_Load_Char"));
        doneFace = reinterpret_cast<DoneFaceFn>(dlsym(so, "FT_Done_Face"));
        doneLib = reinterpret_cast<DoneLibFn>(dlsym(so, "FT_Done_FreeType"));
        return init && neu && size && load && doneFace && doneLib;
    }
};

}  // namespace

bool Font::open() {
    ready_ = false;
    FtApi api;
    if (!api.bind()) return false;
    FtLib lib = nullptr;
    FtFace face = nullptr;
    if (api.init(&lib) != 0) {
        dlclose(api.so);
        return false;
    }
    const char* paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    };
    bool opened = false;
    for (const char* path : paths) {
        if (api.neu(lib, path, 0, &face) == 0) {
            opened = true;
            break;
        }
    }
    // 11 px cabe nas linhas do painel de status (cerca de 14 px).
    if (!opened || api.size(face, 0, 11) != 0) {
        if (face) api.doneFace(face);
        api.doneLib(lib);
        dlclose(api.so);
        return false;
    }

    width_ = 512;
    height_ = 128;
    rgba_.assign(static_cast<std::size_t>(width_ * height_ * 4), 0);
    int penX = 1;
    int penY = 1;
    int rowH = 0;
    ascent_ = 0.f;
    // FT_LOAD_RENDER = 4. O bitmap de 'A' tem de caber numa letra de 11 px.
    if (api.load(face, 'A', 4) != 0) {
        api.doneFace(face);
        api.doneLib(lib);
        dlclose(api.so);
        return false;
    }
    const unsigned char* probe = *reinterpret_cast<unsigned char* const*>(reinterpret_cast<unsigned char*>(face) + kSlotAt);
    unsigned probeW = 0;
    std::memcpy(&probeW, probe + kBitmapAt + 4, 4);
    if (probeW < 4 || probeW > 32) {
        api.doneFace(face);
        api.doneLib(lib);
        dlclose(api.so);
        return false;
    }

    for (int code = 32; code <= 126; ++code) {
        Glyph& g = glyphs_[code - 32];
        g = Glyph{};
        if (api.load(face, static_cast<unsigned long>(code), 4) != 0) continue;
        const unsigned char* slot = *reinterpret_cast<unsigned char* const*>(reinterpret_cast<unsigned char*>(face) + kSlotAt);
        unsigned rows = 0;
        unsigned cols = 0;
        int pitch = 0;
        std::memcpy(&rows, slot + kBitmapAt, 4);
        std::memcpy(&cols, slot + kBitmapAt + 4, 4);
        std::memcpy(&pitch, slot + kBitmapAt + 8, 4);
        unsigned char* buffer = *reinterpret_cast<unsigned char* const*>(slot + kBitmapAt + 16);
        long advance = 0;
        int left = 0;
        int top = 0;
        std::memcpy(&advance, slot + kAdvanceAt, sizeof advance);
        std::memcpy(&left, slot + kBearingAt, 4);
        std::memcpy(&top, slot + kBearingAt + 4, 4);
        g.advance = static_cast<float>(advance) / 64.f;
        if (g.advance < 1.f) g.advance = 4.f;
        // Os offsets acima descrevem a ABI do FreeType da maquina original.
        // Se uma versao diferente retornar campos inesperados, NUNCA use
        // essas dimensoes/pitch para percorrer memoria fora do bitmap.
        // A fonte pedida tem 11px; dimensoes maiores indicam ABI divergente.
        if (rows == 0 || cols == 0 || !buffer || pitch <= 0) continue;
        if (rows > 64 || cols > 64 || pitch > 256 ||
            static_cast<unsigned>(pitch) < cols) {
            api.doneFace(face);
            api.doneLib(lib);
            dlclose(api.so);
            rgba_.clear();
            ready_ = false;
            return false;
        }
        if (penX + static_cast<int>(cols) + 2 >= width_) {
            penX = 1;
            penY += rowH + 2;
            rowH = 0;
        }
        if (penY + static_cast<int>(rows) >= height_) break;
        for (unsigned y = 0; y < rows; ++y) {
            for (unsigned x = 0; x < cols; ++x) {
                const unsigned char a = buffer[static_cast<int>(y) * pitch + static_cast<int>(x)];
                std::uint8_t* dst = rgba_.data() + (static_cast<std::size_t>(penY + y) * static_cast<std::size_t>(width_) + (penX + x)) * 4;
                dst[0] = 255;
                dst[1] = 255;
                dst[2] = 255;
                dst[3] = a;
            }
        }
        g.u0 = static_cast<float>(penX) / static_cast<float>(width_);
        g.v0 = static_cast<float>(penY) / static_cast<float>(height_);
        g.u1 = static_cast<float>(penX + cols) / static_cast<float>(width_);
        g.v1 = static_cast<float>(penY + rows) / static_cast<float>(height_);
        g.w = static_cast<float>(cols);
        g.h = static_cast<float>(rows);
        g.left = static_cast<float>(left);
        g.top = static_cast<float>(top);
        g.empty = false;
        if (g.top > ascent_) ascent_ = g.top;
        penX += static_cast<int>(cols) + 2;
        if (static_cast<int>(rows) > rowH) rowH = static_cast<int>(rows);
    }
    line_ = ascent_ + 4.f;
    api.doneFace(face);
    api.doneLib(lib);
    dlclose(api.so);
    ready_ = ascent_ > 0.f && !glyphs_['A' - 32].empty;
    return ready_;
}

}  // namespace top
