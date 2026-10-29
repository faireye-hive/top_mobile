#pragma once

// Fonte de tela no lugar de Client/font/simsun.ttc. O arquivo do jogo e um
// TTC chinês e esta máquina não tem o cabeçalho do FreeType para compilá-lo
// direto. O atlas sai da DejaVu Sans (ou Noto Sans), já instalada, pela
// libfreetype.so.6.

#include <cstdint>
#include <string>
#include <vector>

namespace top {

class Font {
public:
    struct Glyph {
        float u0 = 0.f;
        float v0 = 0.f;
        float u1 = 0.f;
        float v1 = 0.f;
        float w = 0.f;
        float h = 0.f;
        float left = 0.f;
        float top = 0.f;
        float advance = 0.f;
        bool empty = true;
    };

    bool open();
    bool ready() const { return ready_; }
    float ascent() const { return ascent_; }
    float line() const { return line_; }
    int width() const { return width_; }
    int height() const { return height_; }
    const std::uint8_t* pixels() const { return rgba_.data(); }
    bool glyph(char ch, Glyph& out) const {
        const unsigned char code = static_cast<unsigned char>(ch);
        out = glyphs_[code < 32 || code > 126 ? 0 : code - 32];
        return (code >= 32 && code <= 126) || !out.empty || out.advance > 0.f;
    }

private:
    bool ready_ = false;
    float ascent_ = 12.f;
    float line_ = 18.f;
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> rgba_;
    Glyph glyphs_[95];
};

}  // namespace top
