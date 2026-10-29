#include "UIRenderer.h"
#include <algorithm>
#include <utility>
namespace top::ui {
UIRenderer::UIRenderer(int w, int h) : sw_(static_cast<float>(std::max(1, w))), sh_(static_cast<float>(std::max(1, h))) {}
void UIRenderer::rect(float x, float y, float w, float h, Color color) {
    if (w <= 0.f || h <= 0.f) return;
    auto v = [&](float px, float py) {
        MeshVertex out{};
        out.x = px * 2.f / sw_ - 1.f;
        out.y = 1.f - py * 2.f / sh_;
        out.z = 0.f;
        out.r = color.r; out.g = color.g; out.b = color.b; out.a = color.a;
        vertices_.push_back(out);
    };
    v(x,y); v(x+w,y); v(x,y+h);
    v(x+w,y); v(x+w,y+h); v(x,y+h);
}
void UIRenderer::border(float x,float y,float w,float h,float t,Color c) {
    if (w <= 0.f || h <= 0.f || t <= 0.f) return;
    t = std::min(t, std::min(w,h)*.5f);
    rect(x,y,w,t,c); rect(x,y+h-t,w,t,c);
    rect(x,y+t,t,h-2*t,c); rect(x+w-t,y+t,t,h-2*t,c);
}
ActorBatch UIRenderer::takeBatch() {
    ActorBatch batch;
    batch.mode = 5;
    batch.texture = "white";
    batch.verts = std::move(vertices_);
    return batch;
}
} // namespace top::ui
