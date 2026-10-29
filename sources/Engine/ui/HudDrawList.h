#pragma once
#include <string>
#include <utility>
#include <vector>

namespace top::ui {
// 04-F: comandos de apresentacao independentes de GL e de ActorSkin.
// Camadas preservadas: sprites -> formas procedurais -> icones -> badges/texto.
struct HudSprite {
    const char* file;
    float x, y, w, h;
    float su, sv, sw, sh;
    int texW, texH;
};
struct HudIcon {
    std::string file;
    float x=0.f, y=0.f, w=28.f, h=28.f;
};
struct HudLabel {
    enum class Kind { Text, CountBadge } kind = Kind::Text;
    std::string value;
    float x=0.f, y=0.f;
    float r=0.f, g=0.f, b=0.f;
    int count=0;
};
struct HudDrawList {
    std::vector<HudSprite> sprites;
    std::vector<HudIcon> icons;
    std::vector<HudLabel> labels;
    void addText(std::string str, float x, float y, float r, float g, float b) {
        labels.push_back({HudLabel::Kind::Text, std::move(str), x, y, r, g, b, 0});
    }
    void addBadge(int count, float x, float y) {
        if (count > 1) labels.push_back({HudLabel::Kind::CountBadge, {}, x, y, 0.f, 0.f, 0.f, count});
    }
};
} // namespace top::ui
