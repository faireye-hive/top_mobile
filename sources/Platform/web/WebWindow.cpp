#include "top/WebWindow.h"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace {
top::WebWindow* activeWindow = nullptr;
int keyIndex(const EmscriptenKeyboardEvent& event) {
    const char* code = event.code;
    if (std::strncmp(code, "Key", 3) == 0 && code[3] >= 'A' && code[3] <= 'Z')
        return code[3] - 'A' + 'a';
    if (std::strncmp(code, "Digit", 5) == 0 && code[5] >= '0' && code[5] <= '9') return code[5];
    if (std::strncmp(code, "Numpad", 6) == 0 && code[6] >= '0' && code[6] <= '9') return code[6];
    if (code[0] == 'F' && code[1] >= '1' && code[1] <= '9' && !code[2]) return 128 + code[1] - '1';
    if (!std::strcmp(code, "Escape")) return 27;
    if (!std::strcmp(code, "Enter") || !std::strcmp(code, "NumpadEnter")) return 13;
    if (!std::strcmp(code, "ArrowLeft")) return 1;
    if (!std::strcmp(code, "ArrowRight")) return 2;
    if (!std::strcmp(code, "ArrowUp")) return 3;
    if (!std::strcmp(code, "ArrowDown")) return 4;
    if (!std::strncmp(code, "Shift", 5)) return 5;
    if (!std::strncmp(code, "Alt", 3)) return 6;
    if (!std::strcmp(code, "PageUp") || !std::strcmp(code, "Equal") || !std::strcmp(code, "NumpadAdd")) return 7;
    if (!std::strcmp(code, "PageDown") || !std::strcmp(code, "Minus") || !std::strcmp(code, "NumpadSubtract")) return 8;
    if (!std::strcmp(code, "Insert") || !std::strcmp(code, "Numpad0")) return 9;
    return 0;
}

// Asyncify preserves the shared loop's stack, including all gameplay lambdas.
// A timer fallback also lets an inactive tab finish flushing local saves.
EM_ASYNC_JS(void, nextFrame, (), {
    if (!Module.topFirstFrame) {
        Module.topFirstFrame = true;
        if (!Module.topAssetStats.failures) Module.setStatus("");
        if (Module.topReady) Module.topReady();
    }
    const manualSave = Module.topSaveWritten !== undefined;
    if (Module.topFlushSaves) await Module.topFlushSaves(manualSave);
    if (manualSave) {
        const okay = Module.topSaveWritten && !Module.topSaveError;
        delete Module.topSaveWritten;
        if (Module.topSaveResolve) Module.topSaveResolve(okay);
        Module.topSaveResolve = null;
    }
    if (Module.topNextFrame) await Module.topNextFrame();
    else await new Promise(requestAnimationFrame);
});
}

extern "C" {
EMSCRIPTEN_KEEPALIVE void top_web_key(int key, int down) {
    if (activeWindow) activeWindow->mobileKey(key, down != 0);
}
EMSCRIPTEN_KEEPALIVE void top_web_pointer(int phase, int x, int y) {
    if (activeWindow) activeWindow->mobilePointer(phase, x, y);
}
EMSCRIPTEN_KEEPALIVE void top_web_camera(int dx, int dy, int zoom) {
    if (activeWindow) activeWindow->mobileCamera(dx, dy, zoom);
}
EMSCRIPTEN_KEEPALIVE void top_web_text(int code) {
    if (activeWindow) activeWindow->mobileText(code);
}
}

namespace top {
bool WebWindow::open(int width, int height, const char* title) {
    activeWindow = this;
    width_ = EM_ASM_INT({ return Module.topLogicalWidth || $0; }, width);
    height_ = EM_ASM_INT({ return Module.topLogicalHeight || $0; }, height);
    if (!EM_ASM_INT({ return !!Module.topLogicalWidth; }))
        emscripten_set_canvas_element_size("#canvas", width, height);
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;
    attrs.alpha = true;
    attrs.depth = true;
    attrs.antialias = false;
    context_ = emscripten_webgl_create_context("#canvas", &attrs);
    if (context_ <= 0 || emscripten_webgl_make_context_current(context_) != EMSCRIPTEN_RESULT_SUCCESS) {
        activeWindow = nullptr;
        std::fprintf(stderr, "Nao foi possivel criar contexto WebGL 2.\n");
        return false;
    }
    setTitle(title);
    auto keyboard = [](int type, const EmscriptenKeyboardEvent* event, void* data) -> EM_BOOL {
        auto& self = *static_cast<WebWindow*>(data);
        const bool down = type == EMSCRIPTEN_EVENT_KEYDOWN;
        const int index = keyIndex(*event);
        if (index) {
            self.pending_.key[index] = down;
            // Browser automation and quick taps can press/release between
            // frames. Keep each new key visible to gameplay for one frame.
            if (down && !event->repeat) self.pressed_[index] = true;
        }
        if (down && !std::strcmp(event->code, "Backspace")) self.pending_.backspacePressed = true;
        if (down && !event->ctrlKey && !event->altKey && event->key[0] >= 32 &&
            event->key[0] <= 126 && event->key[1] == '\0') self.pending_.textInput += event->key;
        return index || !std::strcmp(event->code, "Backspace");
    };
    emscripten_set_keydown_callback("#canvas", this, true, keyboard);
    emscripten_set_keyup_callback("#canvas", this, true, keyboard);
    emscripten_set_blur_callback("#canvas", this, true,
        [](int, const EmscriptenFocusEvent*, void* data) -> EM_BOOL {
            auto& self = *static_cast<WebWindow*>(data);
            const bool quit = self.pending_.quit;
            self.pending_ = Input{};
            self.pending_.quit = quit;
            std::fill_n(self.pressed_, 256, false);
            return false;
        });
    auto mouse = [](int type, const EmscriptenMouseEvent* event, void* data) -> EM_BOOL {
        auto& self = *static_cast<WebWindow*>(data);
        double cssW = self.width_, cssH = self.height_;
        emscripten_get_element_css_size("#canvas", &cssW, &cssH);
        const int x = static_cast<int>(event->targetX * self.width_ / std::max(1., cssW));
        const int y = static_cast<int>(event->targetY * self.height_ / std::max(1., cssH));
        if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
            self.pending_.mouseDx += x - self.pending_.mouseX;
            self.pending_.mouseDy += y - self.pending_.mouseY;
        }
        self.pending_.mouseX = x; self.pending_.mouseY = y;
        if (event->button == 0 && type == EMSCRIPTEN_EVENT_MOUSEDOWN) {
            self.pending_.mouseDown = true; self.pending_.mousePressed = true;
        }
        if (event->button == 0 && type == EMSCRIPTEN_EVENT_MOUSEUP) {
            self.pending_.mouseDown = false; self.pending_.mouseReleased = true;
        }
        if (event->button == 2 && type != EMSCRIPTEN_EVENT_MOUSEMOVE)
            self.pending_.rightDown = type == EMSCRIPTEN_EVENT_MOUSEDOWN;
        return true;
    };
    emscripten_set_mousedown_callback("#canvas", this, true, mouse);
    emscripten_set_mouseup_callback("#canvas", this, true, mouse);
    emscripten_set_mousemove_callback("#canvas", this, true, mouse);
    emscripten_set_mouseleave_callback("#canvas", this, true,
        [](int, const EmscriptenMouseEvent*, void* data) -> EM_BOOL {
            auto& input = static_cast<WebWindow*>(data)->pending_;
            input.mouseReleased = input.mouseDown;
            input.mouseDown = input.rightDown = false;
            return false;
        });
    emscripten_set_wheel_callback("#canvas", this, true,
        [](int, const EmscriptenWheelEvent* event, void* data) -> EM_BOOL {
            static_cast<WebWindow*>(data)->pending_.wheel += event->deltaY < 0 ? 1 : event->deltaY > 0 ? -1 : 0;
            return true;
        });
    emscripten_set_webglcontextlost_callback("#canvas", this, true,
        [](int, const void*, void* data) -> EM_BOOL {
            static_cast<WebWindow*>(data)->pending_.quit = true;
            EM_ASM({ Module.setStatus('Contexto grafico perdido. Recarregue a pagina para continuar.'); });
            return true;
        });
    return true;
}

void WebWindow::mobileKey(int code, bool down) {
    if (code <= 0 || code >= 256) return;
    if (down && !mobileHeld_[code]) pressed_[code] = true;
    mobileHeld_[code] = down;
}
void WebWindow::mobilePointer(int phase, int x, int y) {
    if (phase == 1) {
        pending_.mouseDx += x - pending_.mouseX;
        pending_.mouseDy += y - pending_.mouseY;
    }
    pending_.mouseX = x; pending_.mouseY = y;
    if (phase == 0) { pending_.mouseDown = true; pending_.mousePressed = true; }
    if (phase == 2 || phase == 3) { pending_.mouseDown = false; pending_.mouseReleased = true; }
}
void WebWindow::mobileCamera(int dx, int dy, int zoom) {
    cameraDx_ += dx; cameraDy_ += dy;
    pending_.wheel += zoom;
}
void WebWindow::mobileText(int code) {
    if (code == -1) { pending_.replaceText = true; pending_.textInput.clear(); }
    else if (code == 8) pending_.backspacePressed = true;
    else if (code >= 32 && code <= 126) pending_.textInput += static_cast<char>(code);
}
void WebWindow::pump(Input& input) {
    EM_ASM({ if (Module.topBeforeInput) Module.topBeforeInput(); });
    width_ = EM_ASM_INT({ return Module.topLogicalWidth || $0; }, width_);
    height_ = EM_ASM_INT({ return Module.topLogicalHeight || $0; }, height_);
    input = pending_;
    for (int i = 0; i < 256; ++i) input.key[i] = input.key[i] || pressed_[i] || mobileHeld_[i];
    if (cameraDx_ || cameraDy_) {
        input.rightDown = true;
        input.mouseDx = cameraDx_; input.mouseDy = cameraDy_;
    }
    cameraDx_ = cameraDy_ = 0;
    std::fill_n(pressed_, 256, false);
    pending_.mouseDx = pending_.mouseDy = pending_.wheel = 0;
    pending_.mousePressed = pending_.mouseReleased = pending_.backspacePressed = false;
    pending_.replaceText = false;
    pending_.textInput.clear();
}
void WebWindow::swap() { nextFrame(); }
void WebWindow::setTitle(const char* title) {
    EM_ASM({ document.title = UTF8ToString($0).replace('OpenGL', 'Web'); }, title);
}
void WebWindow::hidePointer() { EM_ASM({ Module.canvas.style.cursor = Module.topMobile ? 'default' : 'none'; }); }
void WebWindow::close() {
    activeWindow = nullptr;
    // Remove callbacks before this stack-allocated window is destroyed.
    emscripten_set_keydown_callback("#canvas", nullptr, true, nullptr);
    emscripten_set_keyup_callback("#canvas", nullptr, true, nullptr);
    emscripten_set_blur_callback("#canvas", nullptr, true, nullptr);
    emscripten_set_mousedown_callback("#canvas", nullptr, true, nullptr);
    emscripten_set_mouseup_callback("#canvas", nullptr, true, nullptr);
    emscripten_set_mousemove_callback("#canvas", nullptr, true, nullptr);
    emscripten_set_mouseleave_callback("#canvas", nullptr, true, nullptr);
    emscripten_set_wheel_callback("#canvas", nullptr, true, nullptr);
    emscripten_set_webglcontextlost_callback("#canvas", nullptr, true, nullptr);
    if (context_ > 0) emscripten_webgl_destroy_context(context_);
    context_ = 0;
}
}
