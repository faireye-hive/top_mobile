#pragma once
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
namespace top::platform {
inline void frameInfo(unsigned draws, unsigned uploaded, unsigned attributes, unsigned uniforms, bool textEntry, bool inWorld) {
#ifdef __EMSCRIPTEN__
    EM_ASM({
        Module.topDraws = $0; Module.topUploaded = $1;
        Module.topAttributes = $2; Module.topUniforms = $3; Module.topTextEntry = !!$4;
        Module.topInWorld = !!$5;
    }, draws, uploaded, attributes, uniforms, textEntry, inWorld);
#endif
}
inline bool saveRequested() {
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({
        const requested = !!Module.topSaveRequested;
        Module.topSaveRequested = false;
        return requested;
    });
#else
    return false;
#endif
}
inline void saveCompleted(bool okay) {
#ifdef __EMSCRIPTEN__
    EM_ASM({ Module.topSaveWritten = !!$0; }, okay);
#endif
}
}
