#pragma once
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace top::performance {
// RSS do processo, NAO VRAM. Zero quando /proc nao estiver disponivel.
inline long residentMiB() {
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return 0;
    char line[160]; long kb = 0;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "VmRSS:", 6) == 0) {
            if (std::sscanf(line + 6, "%ld", &kb) != 1) kb = 0;
            break;
        }
    }
    std::fclose(f);
    return std::max(0L, kb / 1024);
}
struct Sample { double fps = 0; double ms = 0; long ramMiB = 0; };
class Meter {
public:
    using Clock = std::chrono::steady_clock;
    bool frame(Clock::time_point end, Sample& out) {
        if (!started_) { begin_ = end; started_ = true; return false; }
        ++frames_;
        const double elapsed = std::chrono::duration<double>(end - begin_).count();
        if (elapsed < 1.) return false;
        out.fps = frames_ / elapsed;
        out.ms = 1000. * elapsed / frames_;
        out.ramMiB = residentMiB();
        begin_ = end; frames_ = 0;
        return true;
    }
private:
    Clock::time_point begin_{};
    int frames_ = 0;
    bool started_ = false;
};
} // namespace top::performance
