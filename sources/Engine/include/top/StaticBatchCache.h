#pragma once
// 05-X2: cache de geometria imutavel entre atualizacoes de streaming.
// Nao depende de OpenGL: a politica pode ser exercitada por testes headless.
#include <cstddef>
#include <limits>
#include <vector>

namespace top {
class StaticBatchCache {
public:
    struct Slot { unsigned buffer = 0; int vertices = 0; };

    template <class Batches, class Upload>
    void update(const Batches& batches, Upload upload) {
        // Se houver menos batches, a GPU so deve liberar os handles no clear()
        // ou explicitamente no releaseExcess(); mantemos a capacidade para
        // evitar criar/deletar VBOs a cada frame da animacao do cenario.
        if (slots_.size() < batches.size()) slots_.resize(batches.size());
        used_ = batches.size();
        for (std::size_t i = 0; i < used_; ++i) {
            const auto& vertices = batches[i].verts;
            Slot& slot = slots_[i];
            if (vertices.empty() || vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                slot.vertices = 0;
                continue;
            }
            const unsigned buffer = upload(slot.buffer, vertices.data(), static_cast<int>(vertices.size()));
            slot.buffer = buffer;
            slot.vertices = buffer ? static_cast<int>(vertices.size()) : 0;
        }
    }

    template <class Release>
    void clear(Release release) {
        for (Slot& slot : slots_) if (slot.buffer) release(slot.buffer);
        slots_.clear();
        used_ = 0;
    }
    Slot at(std::size_t index) const { return index < used_ ? slots_[index] : Slot{}; }
    std::size_t size() const { return used_; }
    std::size_t retained() const { return slots_.size(); }
private:
    std::vector<Slot> slots_;
    std::size_t used_ = 0;
};
} // namespace top
