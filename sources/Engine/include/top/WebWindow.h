#pragma once
#include "top/Input.h"

namespace top {
class WebWindow {
public:
    bool open(int width, int height, const char* title);
    void close();
    void pump(Input& input);
    void swap();
    std::string swapControlInfo() const { return "WebGL 2 / requestAnimationFrame"; }
    bool setSwapInterval(int interval) { return interval == 1; }
    void setTitle(const char* title);
    void hidePointer();
    int width() const { return width_; }
    int height() const { return height_; }
    bool ok() const { return context_ > 0; }
    void mobileKey(int code, bool down);
    void mobilePointer(int phase, int x, int y);
    void mobileCamera(int dx, int dy, int zoom);
    void mobileText(int code);
private:
    Input pending_;
    bool pressed_[256] = {};
    bool mobileHeld_[256] = {};
    int cameraDx_ = 0, cameraDy_ = 0;
    int context_ = 0;
    int width_ = 0, height_ = 0;
};
}
