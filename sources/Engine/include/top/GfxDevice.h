#pragma once

// Device grafico OpenGL 3.3. Substitui IDirect3DDevice9 aos poucos.
// O primeiro desenho reusa a malha do terreno: X e Y do jogo no chao,
// altura em Z no MindPower vira Y para cima no OpenGL.

#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "top/VertexTypes.h"

namespace top {

class GfxDevice {
public:
    bool init();
    void shutdown();
    void resize(int width, int height);
    void beginFrame(float r, float g, float b);
    void setLighting(float dirX, float dirY, float dirZ,
                     float lightR, float lightG, float lightB,
                     float ambientR, float ambientG, float ambientB);
    // mode 0: cor do vertice. mode 1: textura * cor.
    // mode 2: textura com alpha do atlas.
    // mode 3: objeto da cena, descarta alpha < 0.5 (DXT1 de arvore).
    // mode 4: objeto aditivo.
    // mode 5: interface, alpha da textura, sem teste de profundidade.
    // mode 6: sombra de chao, alpha do vertice e sem escrever profundidade.
    // mode 7: mar historico, textura animada e alpha da costa.
    // mode 8: objeto com opacidade animada, alpha blend normal.
    // Optional D3DBLEND pair for historical *effect* batches only (0 = legacy).
    void draw(const MeshVertex* vertices, int vertexCount, const float* mvp, int mode,
              unsigned texture, unsigned alpha, int sourceBlend = 0, int destBlend = 0,
              int minFilter = 0, int magFilter = 0);
    // Retem terreno e cenario no VBO entre quadros, sem glBufferData repetido.
    unsigned uploadStaticMesh(unsigned reuse, const MeshVertex* vertices, int vertexCount);
    // 05-X5: actor VBOs are retained per material, but change frequently.
    unsigned uploadActorMesh(unsigned reuse, const MeshVertex* vertices, int vertexCount);
    void deleteStaticMesh(unsigned buffer);
    void drawStaticMesh(unsigned buffer, int vertexCount, const float* mvp, int mode, unsigned texture, unsigned alpha);
    struct FrameStats { unsigned draws = 0, attributeSetups = 0, uniformUpdates = 0; std::size_t dynamicBytes = 0, staticBytes = 0; };
    void resetFrameStats() { frameStats_ = {}; }
    FrameStats frameStats() const { return frameStats_; }
    unsigned createTexture(const std::uint8_t* rgba, int width, int height);
    void replaceTexture(unsigned id, const std::uint8_t* rgba, int width, int height);
    bool readPixels(std::uint8_t* rgb, int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }

private:
    bool compile(unsigned kind, const char* source, unsigned* out);
    void drawBound(int vertexCount, const float* mvp, int mode, unsigned texture,
                   unsigned alpha, int sourceBlend = 0, int destBlend = 0,
                   int minFilter = 0, int magFilter = 0);
    unsigned program_ = 0;
    unsigned vao_ = 0;
    unsigned vbo_ = 0;
    unsigned filterSamplers_[4]{}; // point/linear per axis; effect draws only
    int mvpLoc_ = -1;
    int modeLoc_ = -1;
    int texLoc_ = -1;
    int alphaLoc_ = -1;
    int lightDirLoc_ = -1;
    int lightColorLoc_ = -1;
    int ambientLoc_ = -1;
    float lightDir_[3] = {-1.f, -1.f, -1.f};
    float lightColor_[3] = {.42f, .42f, .42f};
    float ambient_[3] = {.58f, .58f, .58f};
    int width_ = 1;
    int height_ = 1;
    bool ready_ = false;
    std::vector<unsigned> textures_;
    std::vector<unsigned> staticBuffers_; // limpeza segura ao fechar o contexto
    std::unordered_map<unsigned, unsigned> bufferVaos_;
    bool frameUniformsReady_ = false, matrixReady_ = false;
    int lastMode_ = -1;
    float lastMvp_[16] = {};
    FrameStats frameStats_{};
};

}  // namespace top
