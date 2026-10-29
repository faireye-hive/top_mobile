#include "top/Font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace top {
bool Font::open() {
    ready_ = false;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    if (FT_Init_FreeType(&library)) return false;
    if (FT_New_Face(library, "/web-font.ttf", 0, &face)) {
        FT_Done_FreeType(library);
        return false;
    }
    if (FT_Set_Pixel_Sizes(face, 0, 11)) {
        FT_Done_Face(face); FT_Done_FreeType(library);
        return false;
    }
    width_ = 512; height_ = 128;
    rgba_.assign(static_cast<std::size_t>(width_ * height_ * 4), 0);
    int penX = 1, penY = 1, rowHeight = 0;
    ascent_ = 0.f;
    for (int code = 32; code <= 126; ++code) {
        Glyph& glyph = glyphs_[code - 32];
        glyph = Glyph{};
        if (FT_Load_Char(face, code, FT_LOAD_RENDER)) continue;
        const FT_GlyphSlot slot = face->glyph;
        const FT_Bitmap& bitmap = slot->bitmap;
        glyph.advance = std::max(1.f, slot->advance.x / 64.f);
        if (!bitmap.buffer || !bitmap.width || !bitmap.rows) continue;
        if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.width > 64 || bitmap.rows > 64) continue;
        if (penX + static_cast<int>(bitmap.width) + 2 >= width_) {
            penX = 1; penY += rowHeight + 2; rowHeight = 0;
        }
        if (penY + static_cast<int>(bitmap.rows) >= height_) break;
        for (unsigned y = 0; y < bitmap.rows; ++y) {
            const unsigned row = bitmap.pitch < 0 ? bitmap.rows - 1 - y : y;
            for (unsigned x = 0; x < bitmap.width; ++x) {
                auto* pixel = rgba_.data() + ((penY + y) * width_ + penX + x) * 4;
                pixel[0] = pixel[1] = pixel[2] = 255;
                pixel[3] = bitmap.buffer[row * std::abs(bitmap.pitch) + x];
            }
        }
        glyph.u0 = static_cast<float>(penX) / width_;
        glyph.v0 = static_cast<float>(penY) / height_;
        glyph.u1 = static_cast<float>(penX + bitmap.width) / width_;
        glyph.v1 = static_cast<float>(penY + bitmap.rows) / height_;
        glyph.w = bitmap.width; glyph.h = bitmap.rows;
        glyph.left = slot->bitmap_left; glyph.top = slot->bitmap_top;
        glyph.empty = false;
        ascent_ = std::max(ascent_, glyph.top);
        penX += bitmap.width + 2;
        rowHeight = std::max(rowHeight, static_cast<int>(bitmap.rows));
    }
    FT_Done_Face(face); FT_Done_FreeType(library);
    line_ = ascent_ + 4.f;
    ready_ = ascent_ > 0.f && !glyphs_['A' - 32].empty;
    return ready_;
}
}
