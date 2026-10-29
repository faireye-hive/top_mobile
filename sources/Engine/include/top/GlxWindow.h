#pragma once

#include <string>
#include "top/Input.h"

namespace top {


class GlxWindow {
public:
    bool open(int width, int height, const char* title);
    void close();
    void pump(Input& input);
    void swap();
    // Query driver GLX swap-control; optional explicit VSync A/B switch.
    std::string swapControlInfo() const;
    bool setSwapInterval(int interval);
    void setTitle(const char* title);
    void hidePointer();
    int width() const { return width_; }
    int height() const { return height_; }
    bool ok() const { return display_ != nullptr; }

private:
    void* display_ = nullptr;
    unsigned long window_ = 0;
    void* context_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int lastMouseX_ = 0;
    int lastMouseY_ = 0;
    bool haveMouse_ = false;
    unsigned long blankCursor_ = 0;
    unsigned long wmDeleteWindow_ = 0;
};

}  // namespace top
