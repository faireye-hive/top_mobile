#include "top/GameMain.h"
#include <emscripten.h>

EM_ASYNC_JS(void, flushOnExit, (), {
    if (Module.topFlushSaves) await Module.topFlushSaves(true);
});

int main(int argc, char** argv) {
    const int result = top::runGame(argc, argv);
    // Flush while Asyncify can still suspend this main invocation.
    EM_ASM({ Module.topGameEnded = true; });
    flushOnExit();
    return result;
}
