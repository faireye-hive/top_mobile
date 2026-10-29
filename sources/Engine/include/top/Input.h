#pragma once
#include <string>

namespace top {
struct Input {
    bool quit = false;
    bool key[256] = {};
    bool backspacePressed = false;
    bool replaceText = false; // Mobile native text field replaces its complete name atomically.
    std::string textInput;
    int mouseX = 0;
    int mouseY = 0;
    int mouseDx = 0;
    int mouseDy = 0;
    bool mouseDown = false;
    bool mousePressed = false;
    bool mouseReleased = false;
    bool rightDown = false;
    int wheel = 0;
};

} // namespace top
