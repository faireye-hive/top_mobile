#pragma once
namespace top::ui {
struct Color { float r, g, b, a; };
struct UITheme {
    Color panel{0.10f, 0.17f, 0.25f, 0.88f};
    Color border{0.52f, 0.65f, 0.74f, 0.95f};
    Color slot{0.06f, 0.10f, 0.16f, 0.94f};
    Color slotBorder{0.32f, 0.46f, 0.57f, 0.96f};
    Color shine{0.77f, 0.83f, 0.73f, 0.80f};
};
} // namespace top::ui
