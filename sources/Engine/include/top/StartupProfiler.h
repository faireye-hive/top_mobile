#pragma once

// 05-AK.2: cronometro monotono para inicializacao; nao depende de GLX.
// O CSV e atualizado a cada checkpoint para preservar informacoes se o
// processo travar ou for interrompido antes de desenhar o primeiro quadro.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace top::startup {

class Profiler {
public:
    using Clock = std::chrono::steady_clock;
    struct Stage {
        std::string name;
        double deltaMs = 0.0;
        double totalMs = 0.0;
    };

    Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;
    ~Profiler() { close(); }

    void enable(const std::string& path) {
        if (enabled_) return;
        enabled_ = true;
        last_ = began_;
        // Falha de arquivo nao deve impedir o jogo de iniciar.
        file_ = std::fopen(path.c_str(), "w");
        if (file_) {
            std::fputs("etapa,delta_ms,total_ms\n", file_);
            std::fflush(file_);
            std::fprintf(stderr, "[startup] arquivo: %s\n", path.c_str());
        } else {
            std::fprintf(stderr, "[startup] aviso: nao consegui criar CSV: %s\n", path.c_str());
        }
    }

    bool enabled() const { return enabled_; }
    const std::vector<Stage>& stages() const { return stages_; }

    void mark(const char* description) {
        if (!enabled_ || finished_) return;
        const auto now = Clock::now();
        const double deltaMs = std::chrono::duration<double, std::milli>(now - last_).count();
        const double totalMs = std::chrono::duration<double, std::milli>(now - began_).count();
        last_ = now;
        Stage stage{description, deltaMs, totalMs};
        stages_.push_back(stage);
        // stderr e um flush explicito ajudam a identificar onde parou.
        std::fprintf(stderr, "[startup] +%9.2f ms | total %9.2f ms | %s\n",
                     stage.deltaMs, stage.totalMs, stage.name.c_str());
        std::fflush(stderr);
        if (file_) {
            // Escapar aspas mantem o CSV valido para futuros nomes de etapas.
            std::fputc('"', file_);
            for (const char ch : stage.name) {
                if (ch == '"') std::fputc('"', file_);
                std::fputc(ch, file_);
            }
            std::fprintf(file_, "\",%.3f,%.3f\n", stage.deltaMs, stage.totalMs);
            std::fflush(file_);
        }
    }

    void finish() {
        if (!enabled_ || finished_) return;
        finished_ = true;
        std::vector<Stage> ordered = stages_;
        std::sort(ordered.begin(), ordered.end(), [](const Stage& a, const Stage& b) {
            return a.deltaMs > b.deltaMs;
        });
        std::fprintf(stderr, "[startup] ===== etapas mais lentas (ate 5) =====\n");
        for (std::size_t i = 0; i < ordered.size() && i < 5; ++i)
            std::fprintf(stderr, "[startup] %9.2f ms : %s\n",
                         ordered[i].deltaMs, ordered[i].name.c_str());
        if (!stages_.empty())
            std::fprintf(stderr, "[startup] TEMPO ATE PRIMEIRO QUADRO: %.2f ms\n",
                         stages_.back().totalMs);
        std::fflush(stderr);
        close();
    }

private:
    void close() {
        if (file_) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    Clock::time_point began_ = Clock::now();
    Clock::time_point last_ = began_;
    bool enabled_ = false;
    bool finished_ = false;
    std::FILE* file_ = nullptr;
    std::vector<Stage> stages_;
};

} // namespace top::startup
