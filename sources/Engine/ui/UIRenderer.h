#pragma once
#include "top/PresentationTypes.h"
#include "UITheme.h"
#include <vector>

namespace top::ui {
// Apenas produz triangulos 2D; nao faz chamadas GL, nao carrega texturas.
// Produz lotes de apresentacao POD consumidos pelo adaptador grafico.
class UIRenderer {
public:
    UIRenderer(int screenWidth, int screenHeight);
    void rect(float x, float y, float w, float h, Color color);
    void border(float x, float y, float w, float h, float thickness, Color color);
    bool empty() const { return vertices_.empty(); }
    ActorBatch takeBatch();
private:
    float sw_, sh_;
    std::vector<MeshVertex> vertices_;
};
} // namespace top::ui
