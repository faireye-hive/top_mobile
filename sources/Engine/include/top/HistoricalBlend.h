#pragma once
// SK2.6 — D3DBLEND (D3D9) -> OpenGL blend factor translation.
// Numeric GL enumerants are stable core API values; kept here so that the
// mapping can be tested headlessly, without GL headers or a GPU.
#include <cstdlib>
#include <cstring>

namespace top { namespace historic_blend {
struct Pair { int source = 0, destination = 0; }; // {0,0}: legacy mode

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }
// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool opaqueDestination() { return true; }
// D3DBLEND_SRCALPHASAT is only valid as a source factor.
constexpr bool validSource(int f) { return f >= 1 && f <= 11; }
constexpr bool validDestination(int f) { return f >= 1 && f <= 10; }
constexpr bool valid(int src,int dst) { return validSource(src) && validDestination(dst); }
inline Pair effective(int src,int dst) {
    return enabled() && valid(src,dst) ? Pair{src,dst} : Pair{};
}
// Return OpenGL core enum values (GL_ZERO, GL_ONE, GL_SRC_COLOR, etc.).
// D3DBLEND: 1 ZERO, 2 ONE, 3 SRCCOLOR, 4 INVSRCCOLOR, 5 SRCALPHA,
// 6 INVSRCALPHA, 7 DESTALPHA, 8 INVDESTALPHA, 9 DESTCOLOR,
// 10 INVDESTCOLOR, 11 SRCALPHASAT.
constexpr unsigned glFactor(int d3d) {
    switch(d3d) {
        case 1: return 0;       // GL_ZERO
        case 2: return 1;       // GL_ONE
        case 3: return 0x0300;  // GL_SRC_COLOR
        case 4: return 0x0301;  // GL_ONE_MINUS_SRC_COLOR
        case 5: return 0x0302;  // GL_SRC_ALPHA
        case 6: return 0x0303;  // GL_ONE_MINUS_SRC_ALPHA
        case 7: return 0x0304;  // GL_DST_ALPHA
        case 8: return 0x0305;  // GL_ONE_MINUS_DST_ALPHA
        case 9: return 0x0306;  // GL_DST_COLOR
        case 10:return 0x0307;  // GL_ONE_MINUS_DST_COLOR
        case 11:return 0x0308;  // GL_SRC_ALPHA_SATURATE (source only)
        default:return 0;      // use only when valid() returns true
    }
}
}}
