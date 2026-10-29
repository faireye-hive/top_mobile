#pragma once

// Dados POD comuns a geometria 3D e comandos de apresentacao 2D.
// Este arquivo NAO depende de OpenGL, X11 ou ActorSkin.
namespace top {
struct MeshVertex {
    float x, y, z;
    float r, g, b, a;
    float u, v;
    float u2, v2;
    // Normal ja transformada para o mundo OpenGL (x, altura, y).
    // Zero mantem UI, terreno auxiliar, efeitos e marcadores sem luz.
    float nx, ny, nz;
};

} // namespace top
