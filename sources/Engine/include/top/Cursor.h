#pragma once

// Cursores do cliente antigo: Client/cursor/*.ani (RIFF ACON) e o
// block.ani, que e um .ico parado. O quadro vira RGBA com a mascara AND.

#include <cstdint>
#include <string>
#include <vector>

namespace top {

enum CursorState {
    CurNormal = 0,
    CurActive,
    CurDrag,
    CurSize,
    CurVertical,
    CurLevel,
    CurWait,
    CurAttack,
    CurSkill,
    CurLand,
    CurSail,
    CurForbid,
    CurHover,
    CurPick,
    CurCamera,
    CurChat,
    CurRight,
    CurClick,
    CurHide,
    CurSearch,
    CurBlock,
    CurRepair,
    CurFeed,
    CurCount
};

struct CursorFrame {
    int w = 0;
    int h = 0;
    int hotX = 0;
    int hotY = 0;
    int opaque = 0;
    std::vector<std::uint8_t> rgba;
};

class GameCursor {
public:
    bool load(const std::string& clientRoot);
    static bool selfTest(const std::string& clientRoot);
    void setState(int state);
    void advance(float dt);
    int state() const { return state_; }
    int stepIndex() const { return step_; }
    int fileCount() const { return filesOk_; }
    int framesIn(int state) const;
    const CursorFrame* frame() const;
    int frameSlot() const;
    const std::vector<CursorFrame>& frames() const { return frames_; }

private:
    struct Clip {
        int first = 0;
        int count = 0;
        std::vector<int> seq;
        std::vector<float> hold;
        bool ok = false;
    };

    bool decodeFile(const std::string& path, Clip& clip);
    std::vector<CursorFrame> frames_;
    Clip clips_[CurCount];
    int state_ = CurNormal;
    int step_ = 0;
    float time_ = 0.f;
    int filesOk_ = 0;
};

}  // namespace top
