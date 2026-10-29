#pragma once

#include "top/PresentationTypes.h"
#include "top/Font.h"
#include "HudDrawList.h"
#include "UIRenderer.h"

namespace top::ui {
// 04-H: interface composta por camadas ATOMICAS. Cada janela entrega o painel,
// icones e textos juntos, antes do proximo painel (z-order deterministica).
// Nada aqui exige um contexto OpenGL ou acesso aos arquivos de textura.
struct HudClip { float x, y, w, h; };
class HudComposer {
public:
    HudComposer(std::vector<ActorBatch>& batches, const Font& font,
                const std::string& assetRoot, int width, int height);
    void layer(UIRenderer& shapes, const HudDrawList& cmds, HudClip clip);
    void notes(const std::vector<HudNote>& notes);
    void text(const std::string& value, float x, float y,
              float r, float g, float b);
private:
    std::vector<ActorBatch>& batches_;
    const Font& font_;
    // Own the asset root: callers may pass a temporary (e.g. "Client").
    // A reference here becomes dangling immediately after construction.
    std::string root_;
    float width_, height_;
    ActorBatch& batch(const std::string& texture);
    void coloredRect(float x, float y, float w, float h, float r, float g, float b, HudClip clip);
    void texturedRect(const std::string& texture, float x, float y, float w, float h,
                      float u0, float v0, float u1, float v1, HudClip clip);
    void glyphs(const std::string& value, float x, float y, float r, float g, float b, HudClip clip);
};
} // namespace top::ui
