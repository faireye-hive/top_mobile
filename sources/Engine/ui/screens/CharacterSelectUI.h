#pragma once
#include "ModalLayout.h"
#include <string>
#include <utility>
#include <vector>
namespace top::ui {
struct HudDrawList;
struct WindowState;
struct UIContentState;
class CharacterSelectUI {
public:
    void draw(class UIRenderer& r, int width, int height) const;
    void appendSprites(HudDrawList& out, int width, int height) const;
    void appendText(HudDrawList& out, int width, int height) const;
    void setVisible(bool shown) { visible_ = shown; if (!shown) stage_ = 0; }
    bool visible() const { return visible_; }
    void setRoster(std::vector<std::string> names, int selected) {
        names_ = std::move(names); selected_ = selected;
    }
    void beginCreate() { stage_ = 1; race_ = 0; }
    void beginDelete() { if (!names_.empty()) stage_ = 3; }
    void chooseRace(int race) { race_ = race; name_.clear(); stage_ = race >= 1 && race <= 4 ? 2 : 1; }
    void cancelCreate() { stage_ = 0; race_ = 0; }
    int createRace() const { return race_; }
    int createStage() const { return stage_; }
    const std::string& createName() const { return name_; }
    void editCreateName(const std::string& text, bool backspace);
    // Hit results: 1..4 roster slots, -1 create, -2 delete, -3 enter,
    // -4 exit/cancel, 101..104 races, 201..203 birth cities.
    int hit(int x, int y, int w, int h) const;
private:
    bool visible_ = false;
    int stage_ = 0;
    int race_ = 0;
    int selected_ = -1;
    std::vector<std::string> names_;
    std::string name_;
};
} // namespace top::ui
