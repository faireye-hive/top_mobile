#include "top/GfxDevice.h"
#include "top/HistoricalBlend.h"
#include "top/HistoricalTextureFilter.h"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#else
#include <GL/gl.h>
#include <GL/glx.h>
#endif

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <set>
#include <utility>

namespace top {
namespace {

using PFN_glCreateShader = unsigned (*)(unsigned);
using PFN_glShaderSource = void (*)(unsigned, int, const char* const*, const int*);
using PFN_glCompileShader = void (*)(unsigned);
using PFN_glGetShaderiv = void (*)(unsigned, unsigned, int*);
using PFN_glGetShaderInfoLog = void (*)(unsigned, int, int*, char*);
using PFN_glDeleteShader = void (*)(unsigned);
using PFN_glCreateProgram = unsigned (*)();
using PFN_glAttachShader = void (*)(unsigned, unsigned);
using PFN_glLinkProgram = void (*)(unsigned);
using PFN_glGetProgramiv = void (*)(unsigned, unsigned, int*);
using PFN_glGetProgramInfoLog = void (*)(unsigned, int, int*, char*);
using PFN_glUseProgram = void (*)(unsigned);
using PFN_glDeleteProgram = void (*)(unsigned);
using PFN_glGenVertexArrays = void (*)(int, unsigned*);
using PFN_glBindVertexArray = void (*)(unsigned);
using PFN_glDeleteVertexArrays = void (*)(int, const unsigned*);
using PFN_glGenBuffers = void (*)(int, unsigned*);
using PFN_glDeleteBuffers = void (*)(int, const unsigned*);
using PFN_glBindBuffer = void (*)(unsigned, unsigned);
using PFN_glBufferData = void (*)(unsigned, std::ptrdiff_t, const void*, unsigned);
using PFN_glEnableVertexAttribArray = void (*)(unsigned);
using PFN_glVertexAttribPointer = void (*)(unsigned, int, unsigned, unsigned char, int, const void*);
using PFN_glGetUniformLocation = int (*)(unsigned, const char*);
using PFN_glUniformMatrix4fv = void (*)(int, int, unsigned char, const float*);
using PFN_glUniform1i = void (*)(int, int);
using PFN_glUniform3f = void (*)(int, float, float, float);
using PFN_glActiveTexture = void (*)(unsigned);
using PFN_glGenSamplers = void (*)(int, unsigned*);
using PFN_glDeleteSamplers = void (*)(int, const unsigned*);
using PFN_glBindSampler = void (*)(unsigned, unsigned);
using PFN_glSamplerParameteri = void (*)(unsigned, unsigned, int);

PFN_glCreateShader pCreateShader;
PFN_glShaderSource pShaderSource;
PFN_glCompileShader pCompileShader;
PFN_glGetShaderiv pGetShaderiv;
PFN_glGetShaderInfoLog pGetShaderInfoLog;
PFN_glDeleteShader pDeleteShader;
PFN_glCreateProgram pCreateProgram;
PFN_glAttachShader pAttachShader;
PFN_glLinkProgram pLinkProgram;
PFN_glGetProgramiv pGetProgramiv;
PFN_glGetProgramInfoLog pGetProgramInfoLog;
PFN_glUseProgram pUseProgram;
PFN_glDeleteProgram pDeleteProgram;
PFN_glGenVertexArrays pGenVertexArrays;
PFN_glBindVertexArray pBindVertexArray;
PFN_glDeleteVertexArrays pDeleteVertexArrays;
PFN_glGenBuffers pGenBuffers;
PFN_glDeleteBuffers pDeleteBuffers;
PFN_glBindBuffer pBindBuffer;
PFN_glBufferData pBufferData;
PFN_glEnableVertexAttribArray pEnableVertexAttribArray;
PFN_glVertexAttribPointer pVertexAttribPointer;
PFN_glGetUniformLocation pGetUniformLocation;
PFN_glUniformMatrix4fv pUniformMatrix4fv;
PFN_glUniform1i pUniform1i;
PFN_glUniform3f pUniform3f;
PFN_glActiveTexture pActiveTexture;
PFN_glGenSamplers pGenSamplers;
PFN_glDeleteSamplers pDeleteSamplers;
PFN_glBindSampler pBindSampler;
PFN_glSamplerParameteri pSamplerParameteri;

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STATIC_DRAW 0x88E4
#define GL_FLOAT 0x1406
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BLEND_SRC_RGB
#define GL_BLEND_SRC_RGB 0x80C9
#endif
#ifndef GL_BLEND_DST_RGB
#define GL_BLEND_DST_RGB 0x80C8
#endif

template <typename T>
bool load(T& out, const char* name) {
#ifdef __EMSCRIPTEN__
    if (std::strcmp(name, "glCreateShader") == 0) { out = reinterpret_cast<T>(&glCreateShader); return true; }
    if (std::strcmp(name, "glShaderSource") == 0) { out = reinterpret_cast<T>(&glShaderSource); return true; }
    if (std::strcmp(name, "glCompileShader") == 0) { out = reinterpret_cast<T>(&glCompileShader); return true; }
    if (std::strcmp(name, "glGetShaderiv") == 0) { out = reinterpret_cast<T>(&glGetShaderiv); return true; }
    if (std::strcmp(name, "glGetShaderInfoLog") == 0) { out = reinterpret_cast<T>(&glGetShaderInfoLog); return true; }
    if (std::strcmp(name, "glDeleteShader") == 0) { out = reinterpret_cast<T>(&glDeleteShader); return true; }
    if (std::strcmp(name, "glCreateProgram") == 0) { out = reinterpret_cast<T>(&glCreateProgram); return true; }
    if (std::strcmp(name, "glAttachShader") == 0) { out = reinterpret_cast<T>(&glAttachShader); return true; }
    if (std::strcmp(name, "glLinkProgram") == 0) { out = reinterpret_cast<T>(&glLinkProgram); return true; }
    if (std::strcmp(name, "glGetProgramiv") == 0) { out = reinterpret_cast<T>(&glGetProgramiv); return true; }
    if (std::strcmp(name, "glGetProgramInfoLog") == 0) { out = reinterpret_cast<T>(&glGetProgramInfoLog); return true; }
    if (std::strcmp(name, "glUseProgram") == 0) { out = reinterpret_cast<T>(&glUseProgram); return true; }
    if (std::strcmp(name, "glDeleteProgram") == 0) { out = reinterpret_cast<T>(&glDeleteProgram); return true; }
    if (std::strcmp(name, "glGenVertexArrays") == 0) { out = reinterpret_cast<T>(&glGenVertexArrays); return true; }
    if (std::strcmp(name, "glDeleteVertexArrays") == 0) { out = reinterpret_cast<T>(&glDeleteVertexArrays); return true; }
    if (std::strcmp(name, "glBindVertexArray") == 0) { out = reinterpret_cast<T>(&glBindVertexArray); return true; }
    if (std::strcmp(name, "glGenBuffers") == 0) { out = reinterpret_cast<T>(&glGenBuffers); return true; }
    if (std::strcmp(name, "glDeleteBuffers") == 0) { out = reinterpret_cast<T>(&glDeleteBuffers); return true; }
    if (std::strcmp(name, "glBindBuffer") == 0) { out = reinterpret_cast<T>(&glBindBuffer); return true; }
    if (std::strcmp(name, "glBufferData") == 0) { out = reinterpret_cast<T>(&glBufferData); return true; }
    if (std::strcmp(name, "glEnableVertexAttribArray") == 0) { out = reinterpret_cast<T>(&glEnableVertexAttribArray); return true; }
    if (std::strcmp(name, "glVertexAttribPointer") == 0) { out = reinterpret_cast<T>(&glVertexAttribPointer); return true; }
    if (std::strcmp(name, "glGetUniformLocation") == 0) { out = reinterpret_cast<T>(&glGetUniformLocation); return true; }
    if (std::strcmp(name, "glUniformMatrix4fv") == 0) { out = reinterpret_cast<T>(&glUniformMatrix4fv); return true; }
    if (std::strcmp(name, "glUniform1i") == 0) { out = reinterpret_cast<T>(&glUniform1i); return true; }
    if (std::strcmp(name, "glUniform3f") == 0) { out = reinterpret_cast<T>(&glUniform3f); return true; }
    if (std::strcmp(name, "glActiveTexture") == 0) { out = reinterpret_cast<T>(&glActiveTexture); return true; }
    if (std::strcmp(name, "glGenSamplers") == 0) { out = reinterpret_cast<T>(&glGenSamplers); return true; }
    if (std::strcmp(name, "glDeleteSamplers") == 0) { out = reinterpret_cast<T>(&glDeleteSamplers); return true; }
    if (std::strcmp(name, "glBindSampler") == 0) { out = reinterpret_cast<T>(&glBindSampler); return true; }
    if (std::strcmp(name, "glSamplerParameteri") == 0) { out = reinterpret_cast<T>(&glSamplerParameteri); return true; }
    out = nullptr;
#else
    out = reinterpret_cast<T>(glXGetProcAddress(reinterpret_cast<const unsigned char*>(name)));
#endif
    if (!out) std::fprintf(stderr, "OpenGL: falta %s\n", name);
    return out != nullptr;
}

#ifdef __EMSCRIPTEN__
#define TOP_GLSL_VERSION "#version 300 es\nprecision highp float;\nprecision highp int;\n"
#else
#define TOP_GLSL_VERSION "#version 330 core\n"
#endif

const char* kVert = TOP_GLSL_VERSION R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aCol;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec2 aUv2;
layout(location = 4) in vec3 aNormal;
uniform mat4 uMvp;
out vec4 vCol;
out vec2 vUv;
out vec2 vUv2;
out vec3 vNormal;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vCol = aCol;
    vUv = aUv;
    vUv2 = aUv2;
    vNormal = aNormal;
}
)GLSL";

const char* kFrag = TOP_GLSL_VERSION R"GLSL(
in vec4 vCol;
in vec2 vUv;
in vec2 vUv2;
in vec3 vNormal;
uniform int uMode;
uniform sampler2D uTex;
uniform sampler2D uAlpha;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
out vec4 oCol;
void main() {
    if (uMode == 0 || uMode == 9) {
        // 9: highlight circular de alvo, azul e translúcido.
        oCol = vCol;
    } else if (uMode == 6) {
        // Sombras dos atores usam uma elipse triangulada com UV no centro e
        // na borda. O fade radial evita o aspecto de disco duro do DX9 e
        // mantém a sombra legível sobre terreno claro.
        float edge = 1.0 - smoothstep(0.18, 0.72, distance(vUv, vec2(0.5)));
        if (edge < 0.01) discard;
        oCol = vec4(vCol.rgb, vCol.a * edge);
    } else if (uMode == 1) {
        vec4 t = texture(uTex, vUv);
        vec3 lighting = vec3(1.0);
        if (dot(vNormal, vNormal) > 0.01) {
            float diffuse = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);
            lighting = clamp(uAmbient + uLightColor * diffuse, 0.0, 1.15);
        }
        oCol = vec4(t.rgb * vCol.rgb * lighting, 1.0);
    } else if (uMode == 3) {
        vec4 t = texture(uTex, vUv);
        if (t.a < 0.5) discard;
        vec3 lighting = vec3(1.0);
        if (dot(vNormal, vNormal) > 0.01) {
            float diffuse = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);
            lighting = clamp(uAmbient + uLightColor * diffuse, 0.0, 1.15);
        }
        oCol = vec4(t.rgb * vCol.rgb * lighting, 1.0);
    } else if (uMode == 4) {
        vec4 t = texture(uTex, vUv);
        oCol = vec4(t.rgb * vCol.rgb, t.a * vCol.a);
    } else if (uMode == 5) {
        vec4 t = texture(uTex, vUv);
        oCol = vec4(t.rgb * vCol.rgb, t.a * vCol.a);
    } else if (uMode == 7) {
        vec4 t = texture(uTex, vUv);
        oCol = vec4(t.rgb * vCol.rgb, vCol.a);
    } else if (uMode == 8) {
        vec4 t = texture(uTex, vUv);
        if (t.a < 0.01 || vCol.a < 0.01) discard;
        oCol = vec4(t.rgb * vCol.rgb, t.a * vCol.a);
    } else {
        float a = texture(uAlpha, vUv).a;
        if (a < 0.02) discard;
        oCol = vec4(texture(uTex, vUv2).rgb * vCol.rgb, a);
    }
}
)GLSL";

}  // namespace

namespace {
// Attribute bindings are retained by each VAO, including its source VBO.
void configureVertexAttributes() {
    const int stride = static_cast<int>(sizeof(MeshVertex));
    pEnableVertexAttribArray(0);
    pVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    pEnableVertexAttribArray(1);
    pVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 3));
    pEnableVertexAttribArray(2);
    pVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 7));
    pEnableVertexAttribArray(3);
    pVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 9));
    pEnableVertexAttribArray(4);
    pVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 11));
}
void viewportForCanvas(int width, int height) {
#ifdef __EMSCRIPTEN__
    emscripten_get_canvas_element_size("#canvas", &width, &height);
#endif
    glViewport(0, 0, width, height);
}
}

bool GfxDevice::compile(unsigned kind, const char* source, unsigned* out) {
    unsigned shader = pCreateShader(kind);
    pShaderSource(shader, 1, &source, nullptr);
    pCompileShader(shader);
    int ok = 0;
    pGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        pGetShaderInfoLog(shader, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader: %s\n", log);
        pDeleteShader(shader);
        return false;
    }
    *out = shader;
    return true;
}

bool GfxDevice::init() {
    bool ok = true;
    ok = load(pCreateShader, "glCreateShader") && ok;
    ok = load(pShaderSource, "glShaderSource") && ok;
    ok = load(pCompileShader, "glCompileShader") && ok;
    ok = load(pGetShaderiv, "glGetShaderiv") && ok;
    ok = load(pGetShaderInfoLog, "glGetShaderInfoLog") && ok;
    ok = load(pDeleteShader, "glDeleteShader") && ok;
    ok = load(pCreateProgram, "glCreateProgram") && ok;
    ok = load(pAttachShader, "glAttachShader") && ok;
    ok = load(pLinkProgram, "glLinkProgram") && ok;
    ok = load(pGetProgramiv, "glGetProgramiv") && ok;
    ok = load(pGetProgramInfoLog, "glGetProgramInfoLog") && ok;
    ok = load(pUseProgram, "glUseProgram") && ok;
    ok = load(pDeleteProgram, "glDeleteProgram") && ok;
    ok = load(pGenVertexArrays, "glGenVertexArrays") && ok;
    ok = load(pBindVertexArray, "glBindVertexArray") && ok;
    ok = load(pDeleteVertexArrays, "glDeleteVertexArrays") && ok;
    ok = load(pGenBuffers, "glGenBuffers") && ok;
    ok = load(pDeleteBuffers, "glDeleteBuffers") && ok;
    ok = load(pBindBuffer, "glBindBuffer") && ok;
    ok = load(pBufferData, "glBufferData") && ok;
    ok = load(pEnableVertexAttribArray, "glEnableVertexAttribArray") && ok;
    ok = load(pVertexAttribPointer, "glVertexAttribPointer") && ok;
    ok = load(pGetUniformLocation, "glGetUniformLocation") && ok;
    ok = load(pUniformMatrix4fv, "glUniformMatrix4fv") && ok;
    ok = load(pUniform1i, "glUniform1i") && ok;
    ok = load(pUniform3f, "glUniform3f") && ok;
    ok = load(pActiveTexture, "glActiveTexture") && ok;
    if (!ok) return false;

    unsigned vert = 0;
    unsigned frag = 0;
    if (!compile(GL_VERTEX_SHADER, kVert, &vert) || !compile(GL_FRAGMENT_SHADER, kFrag, &frag)) return false;
    program_ = pCreateProgram();
    pAttachShader(program_, vert);
    pAttachShader(program_, frag);
    pLinkProgram(program_);
    pDeleteShader(vert);
    pDeleteShader(frag);
    int linked = 0;
    pGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        pGetProgramInfoLog(program_, sizeof log, nullptr, log);
        std::fprintf(stderr, "link: %s\n", log);
        return false;
    }
    mvpLoc_ = pGetUniformLocation(program_, "uMvp");
    modeLoc_ = pGetUniformLocation(program_, "uMode");
    texLoc_ = pGetUniformLocation(program_, "uTex");
    alphaLoc_ = pGetUniformLocation(program_, "uAlpha");
    lightDirLoc_ = pGetUniformLocation(program_, "uLightDir");
    lightColorLoc_ = pGetUniformLocation(program_, "uLightColor");
    ambientLoc_ = pGetUniformLocation(program_, "uAmbient");
    pGenVertexArrays(1, &vao_);
    pGenBuffers(1, &vbo_);
    pBindVertexArray(vao_);
    pBindBuffer(GL_ARRAY_BUFFER, vbo_);
    configureVertexAttributes();
    // Sampler objects are per texture unit, unlike glTexParameteri on shared
    // texture objects. Only effect draws use them, so HUD/terrain retain their
    // existing GL_LINEAR behavior even with the same GL texture ID.
    if (historical_filter::enabled()) {
        const bool samplerAPI = load(pGenSamplers, "glGenSamplers") &&
                                load(pDeleteSamplers, "glDeleteSamplers") &&
                                load(pBindSampler, "glBindSampler") &&
                                load(pSamplerParameteri, "glSamplerParameteri");
        if (samplerAPI) {
            pGenSamplers(4, filterSamplers_);
            for (int i = 0; i < 4; ++i) {
                if (!filterSamplers_[i]) continue;
                pSamplerParameteri(filterSamplers_[i], GL_TEXTURE_MIN_FILTER,
                                   i < 2 ? GL_NEAREST : GL_LINEAR);
                pSamplerParameteri(filterSamplers_[i], GL_TEXTURE_MAG_FILTER,
                                   i % 2 == 0 ? GL_NEAREST : GL_LINEAR);
                pSamplerParameteri(filterSamplers_[i], GL_TEXTURE_WRAP_S, GL_REPEAT);
                pSamplerParameteri(filterSamplers_[i], GL_TEXTURE_WRAP_T, GL_REPEAT);
            }
            std::fprintf(stderr, "[filter-sk215c] DX9 per-system texture filters enabled; "
                                 "sampler objects=%s\n", filterSamplers_[0] ? "ready" : "unavailable");
        } else {
            std::fprintf(stderr, "[filter-sk215c] WARNING: sampler objects unavailable; "
                                 "falling back to legacy texture filters\n");
        }
    }
    ready_ = true;
    if (historic_blend::enabled()) {
#ifdef __EMSCRIPTEN__
        // WebWindow explicitly requests an RGBA default framebuffer.
        const GLint alphaBits = 8;
#else
        GLint alphaBits = 0;
        glGetIntegerv(GL_ALPHA_BITS, &alphaBits);
#endif
        std::fprintf(stderr,
            "[blend-sk26] historical DX9 factors ON | GL_ALPHA_BITS=%d | "
            "destination-alpha=%s; scene clear alpha=1.0\n",
            static_cast<int>(alphaBits), historic_blend::opaqueDestination() ? "opaque-rgb" : "framebuffer");
        if (alphaBits == 0 && !historic_blend::opaqueDestination())
            std::fprintf(stderr, "[blend-sk26] WARNING: default framebuffer "
                        "has no destination alpha channel; (5,7) may not match DX9\n");
    }
    return true;
}

void GfxDevice::shutdown() {
    for (const auto& entry : bufferVaos_) pDeleteVertexArrays(1, &entry.second);
    bufferVaos_.clear();
    if (vao_ && pDeleteVertexArrays) pDeleteVertexArrays(1, &vao_);
    vao_ = 0;
    // VBOs devem ser liberados antes de destruir o contexto OpenGL.
    if (!staticBuffers_.empty() && pDeleteBuffers)
        pDeleteBuffers(static_cast<int>(staticBuffers_.size()), staticBuffers_.data());
    staticBuffers_.clear();
    if (vbo_ && pDeleteBuffers) pDeleteBuffers(1, &vbo_);
    vbo_ = 0;
    if (pDeleteSamplers && filterSamplers_[0]) pDeleteSamplers(4, filterSamplers_);
    for (unsigned& sampler : filterSamplers_) sampler = 0;
    if (!textures_.empty()) glDeleteTextures(static_cast<int>(textures_.size()), textures_.data());
    textures_.clear();
    if (program_ && pDeleteProgram) pDeleteProgram(program_);
    program_ = 0;
    ready_ = false;
}

void GfxDevice::resize(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    viewportForCanvas(width_, height_);
}

void GfxDevice::beginFrame(float r, float g, float b) {
    frameUniformsReady_ = matrixReady_ = false;
    lastMode_ = -1;
    viewportForCanvas(width_, height_);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(r, g, b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Match an RGB display backbuffer without rewriting any blend factor.
    // Otherwise normal shadow blending lowers destination alpha, and the
    // next DESTALPHA glow multiplies/darkens the scene across its rectangle.
    if(historic_blend::opaqueDestination())
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
}

void GfxDevice::setLighting(float dirX, float dirY, float dirZ,
                            float lightR, float lightG, float lightB,
                            float ambientR, float ambientG, float ambientB) {
    frameUniformsReady_ = false;
    lightDir_[0] = dirX;
    lightDir_[1] = dirY;
    lightDir_[2] = dirZ;
    lightColor_[0] = lightR;
    lightColor_[1] = lightG;
    lightColor_[2] = lightB;
    ambient_[0] = ambientR;
    ambient_[1] = ambientG;
    ambient_[2] = ambientB;
}

unsigned GfxDevice::createTexture(const std::uint8_t* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) return 0;
    unsigned id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // A imagem ja vem com a primeira linha no topo. O UV do terreno e o do
    // DirectX (v = 0 no topo), entao a textura entra sem inverter a linha.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    textures_.push_back(id);
    return id;
}

void GfxDevice::replaceTexture(unsigned id, const std::uint8_t* rgba, int width, int height) {
    if (id == 0 || !rgba || width <= 0 || height <= 0) return;
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

bool GfxDevice::readPixels(std::uint8_t* rgb, int width, int height) {
    if (!rgb || width <= 0 || height <= 0) return false;
    std::vector<std::uint8_t> bottomUp(static_cast<std::size_t>(width * height * 3));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
#ifdef __EMSCRIPTEN__
    int renderWidth = width, renderHeight = height;
    emscripten_get_canvas_element_size("#canvas", &renderWidth, &renderHeight);
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(renderWidth) * renderHeight * 4);
    glReadPixels(0, 0, renderWidth, renderHeight, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const std::size_t source = (static_cast<std::size_t>(y * renderHeight / height) * renderWidth +
                                    x * renderWidth / width) * 4;
        const std::size_t destination = (static_cast<std::size_t>(y) * width + x) * 3;
        std::copy_n(rgba.data() + source, 3, bottomUp.data() + destination);
    }
#else
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, bottomUp.data());
#endif
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* src = bottomUp.data() + static_cast<std::size_t>((height - 1 - y) * width * 3);
        std::uint8_t* dst = rgb + static_cast<std::size_t>(y * width * 3);
        std::memcpy(dst, src, static_cast<std::size_t>(width * 3));
    }
    return true;
}

unsigned GfxDevice::uploadStaticMesh(unsigned reuse, const MeshVertex* vertices, int vertexCount) {
    if (!ready_ || !vertices || vertexCount <= 0) return 0;
    unsigned buffer = reuse;
    if (!buffer) {
        pGenBuffers(1, &buffer);
        if (!buffer) return 0;
        staticBuffers_.push_back(buffer);
    }
    pBindBuffer(GL_ARRAY_BUFFER, buffer);
    const auto bytes = static_cast<std::size_t>(vertexCount) * sizeof(MeshVertex);
    pBufferData(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(bytes), vertices, GL_STATIC_DRAW);
    frameStats_.staticBytes += bytes;
    return buffer;
}

unsigned GfxDevice::uploadActorMesh(unsigned reuse, const MeshVertex* vertices, int vertexCount) {
    if (!ready_ || !vertices || vertexCount <= 0) return 0;
    unsigned buffer = reuse;
    if (!buffer) {
        pGenBuffers(1, &buffer);
        if (!buffer) return 0;
        staticBuffers_.push_back(buffer); // same context-safe cleanup path
    }
    pBindBuffer(GL_ARRAY_BUFFER, buffer);
    const auto bytes = static_cast<std::size_t>(vertexCount) * sizeof(MeshVertex);
    // Orphan previous storage if another frame still references it; avoids
    // synchronization stalls on fast-changing character animations.
    pBufferData(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(bytes), vertices, GL_DYNAMIC_DRAW);
    frameStats_.dynamicBytes += bytes;
    return buffer;
}

void GfxDevice::deleteStaticMesh(unsigned buffer) {
    if (!buffer || !pDeleteBuffers) return;
    const auto vao = bufferVaos_.find(buffer);
    if (vao != bufferVaos_.end()) {
        pDeleteVertexArrays(1, &vao->second);
        bufferVaos_.erase(vao);
    }
    pDeleteBuffers(1, &buffer);
    auto it = std::find(staticBuffers_.begin(), staticBuffers_.end(), buffer);
    if (it != staticBuffers_.end()) staticBuffers_.erase(it);
}

void GfxDevice::drawStaticMesh(unsigned buffer, int vertexCount, const float* mvp,
                               int mode, unsigned texture, unsigned alpha) {
    if (!ready_ || !buffer || vertexCount <= 0) return;
    auto [found, inserted] = bufferVaos_.try_emplace(buffer, 0);
    if (inserted) pGenVertexArrays(1, &found->second);
    pBindVertexArray(found->second);
    if (inserted) {
        pBindBuffer(GL_ARRAY_BUFFER, buffer);
        configureVertexAttributes();
        ++frameStats_.attributeSetups;
    }
    drawBound(vertexCount, mvp, mode, texture, alpha);
}

void GfxDevice::draw(const MeshVertex* vertices, int vertexCount, const float* mvp,
                     int mode, unsigned texture, unsigned alpha,
                     int sourceBlend, int destBlend, int minFilter, int magFilter) {
    if (!ready_ || !vertices || vertexCount <= 0) return;
    pBindVertexArray(vao_);
    pBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const auto bytes = static_cast<std::size_t>(vertexCount) * sizeof(MeshVertex);
    pBufferData(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(bytes), vertices, GL_DYNAMIC_DRAW);
    frameStats_.dynamicBytes += bytes;
    drawBound(vertexCount, mvp, mode, texture, alpha, sourceBlend, destBlend,
              minFilter, magFilter);
}

void GfxDevice::drawBound(int vertexCount, const float* mvp, int mode,
                          unsigned texture, unsigned alpha, int sourceBlend, int destBlend,
                          int minFilter, int magFilter) {
    ++frameStats_.draws;
    if (!frameUniformsReady_) {
        pUseProgram(program_);
        pUniform1i(texLoc_, 0);
        pUniform1i(alphaLoc_, 1);
        pUniform3f(lightDirLoc_, lightDir_[0], lightDir_[1], lightDir_[2]);
        pUniform3f(lightColorLoc_, lightColor_[0], lightColor_[1], lightColor_[2]);
        pUniform3f(ambientLoc_, ambient_[0], ambient_[1], ambient_[2]);
        frameStats_.uniformUpdates += 5;
        frameUniformsReady_ = true;
    }
    if (!matrixReady_ || std::memcmp(lastMvp_, mvp, sizeof lastMvp_) != 0) {
        pUniformMatrix4fv(mvpLoc_, 1, GL_FALSE, mvp);
        std::memcpy(lastMvp_, mvp, sizeof lastMvp_);
        matrixReady_ = true;
        ++frameStats_.uniformUpdates;
    }
    // Historical effects must not inherit mode 8's alpha-test discard. Some
    // authored destination factors modify the framebuffer even at alpha=0.
    // Mode 4 uses the same RGBA modulation without discarding fragments;
    // the actual blend pair is still applied below by the existing mapper.
    const bool historicalEffect = historic_blend::enabled() && historic_blend::valid(sourceBlend, destBlend);
    const int shaderMode = historicalEffect && (mode == 4 || mode == 8) ? 4 : mode;
    if (shaderMode != lastMode_) {
        pUniform1i(modeLoc_, shaderMode);
        lastMode_ = shaderMode;
        ++frameStats_.uniformUpdates;
    }
    pActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    // 0: unspecified, 1: D3DTEXF_POINT, 2: D3DTEXF_LINEAR.
    // The source is already sanitized by EffectSystem; default remains linear.
    unsigned appliedSampler = 0;
    if (pBindSampler && filterSamplers_[0] && (minFilter || magFilter)) {
        const int minIndex = minFilter == 1 ? 0 : 2;
        const int magIndex = magFilter == 1 ? 0 : 1;
        appliedSampler = filterSamplers_[minIndex + magIndex];
    }
    if (pBindSampler && filterSamplers_[0]) pBindSampler(0, appliedSampler);
    if (appliedSampler) {
        if (const char* trace = std::getenv("TOP_TRACE_EFFECT_FILTERS")) {
            if (trace[0] == '1' && trace[1] == '\0') {
                static std::set<std::pair<int,int>> reported;
                if (reported.size() < 10 && reported.emplace(minFilter,magFilter).second)
                    std::fprintf(stderr,"[effect-filter] min=%d mag=%d sampler=%u "
                                        "GL_min=%s GL_mag=%s\n",
                                 minFilter,magFilter,appliedSampler,
                                 minFilter==1 ? "NEAREST":"LINEAR",
                                 magFilter==1 ? "NEAREST":"LINEAR");
            }
        }
    }
    if (mode == 5 && texture != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    pActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, alpha);
    // 1 e 3 escrevem profundidade sem mistura. 4 e aditivo (bandeira, brilho).
    // 5 e a UI: textura com alpha, por cima da cena. 7 e o mar com alpha.
    if (mode == 5) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    } else if (mode == 1 || mode == 3) {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    } else if (mode == 4) {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
    } else if (mode == 8) {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    } else if (mode == 6 || mode == 9) {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    } else {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
    }
    // SK2.6: D3D9 SRCBLEND/DESTBLEND factor mapping for opt-in effect draws.
    // Other draw calls always pass 0/0 and retain their original GL state.
    // Source/destination are never inverted: 7 = DESTALPHA (not INVSRCA).
    if (historic_blend::valid(sourceBlend, destBlend)) {
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBlendFunc(historic_blend::glFactor(sourceBlend),
                    historic_blend::glFactor(destBlend));
    }
    // SK2.6.1: confirm the *actual GPU blend state* when historical
    // effect draw calls reach this point. A catalog trace alone cannot do it.
    const char* blendDrawTrace = std::getenv("TOP_TRACE_BLEND_DRAWS");
    if (blendDrawTrace && blendDrawTrace[0] == '1' &&
        historic_blend::valid(sourceBlend, destBlend)) {
        static std::set<std::pair<int,int>> reportedPairs;
        if (reportedPairs.size() < 20 && reportedPairs.emplace(sourceBlend, destBlend).second) {
            GLint actualSrc = -1, actualDst = -1;
            glGetIntegerv(GL_BLEND_SRC_RGB, &actualSrc);
            glGetIntegerv(GL_BLEND_DST_RGB, &actualDst);
            std::fprintf(stderr,
                "[blend-gpu] requested_dx9=(%d,%d) requested_gl=(0x%X,0x%X) "
                "active_gl=(0x%X,0x%X)\n",
                sourceBlend, destBlend,
                historic_blend::glFactor(sourceBlend), historic_blend::glFactor(destBlend),
                static_cast<unsigned>(actualSrc), static_cast<unsigned>(actualDst));
        }
    }
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    // Restore sampler binding: subsequent actor/terrain/HUD draw calls use
    // the sampling stored in their original texture objects.
    if (pBindSampler && filterSamplers_[0]) pBindSampler(0, 0);
}

}  // namespace top
