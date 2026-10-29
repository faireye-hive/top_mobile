#pragma once

// 05-X5: keep actor vertex batches on the GPU when their exact contents do
// not change. Deliberately compare the full vertex bytes: a hash collision
// must never freeze a sword, animation, HP bar, or character position.
// This class performs no GL calls; upload/release are injected for tests.
#include "top/GfxDevice.h"

#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace top {
class ActorGpuCache {
public:
    struct Slot { unsigned buffer = 0; int vertices = 0; };
    struct Stats {
        std::size_t uploadedBytes = 0;
        std::size_t reusedBytes = 0;
        std::size_t uploadedBatches = 0;
        std::size_t reusedBatches = 0;
    };
    static_assert(std::is_trivially_copyable<MeshVertex>::value, "Actor vertex must be POD");

    template<class Batches, class Upload, class Release>
    void update(const Batches& batches, Upload upload, Release release) {
        stats_ = {};
        // Reclaim VBOs immediately when a map/population contains fewer
        // materials; otherwise long teleport sessions retain old GPU memory.
        if (slots_.size() > batches.size()) {
            for (std::size_t i = batches.size(); i < slots_.size(); ++i)
                if (slots_[i].buffer) release(slots_[i].buffer);
            slots_.resize(batches.size());
        }
        if (slots_.size() < batches.size()) slots_.resize(batches.size());
        for (std::size_t i = 0; i < batches.size(); ++i) {
            const auto& batch = batches[i];
            Entry& entry = slots_[i];
            const auto count = batch.verts.size();
            if (count == 0 || count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                entry.vertices = 0;
                entry.snapshot.clear();
                continue;
            }
            const auto bytes = count * sizeof(MeshVertex);
            const bool same = entry.buffer && entry.mode == batch.mode && entry.texture == batch.texture &&
                              entry.snapshot.size() == count &&
                              std::memcmp(entry.snapshot.data(), batch.verts.data(), bytes) == 0;
            if (same) {
                ++stats_.reusedBatches;
                stats_.reusedBytes += bytes;
                entry.vertices = static_cast<int>(count);
                continue;
            }
            // When a material changes at the same index, reusing its VBO is
            // safe only because upload replaces the entire vertex payload.
            const unsigned updated = upload(entry.buffer, batch.verts.data(), static_cast<int>(count));
            if (!updated) {
                // The previous buffer may still exist; do not draw stale data.
                entry.vertices = 0;
                entry.snapshot.clear();
                continue;
            }
            entry.buffer = updated;
            entry.mode = batch.mode;
            entry.texture = batch.texture;
            entry.vertices = static_cast<int>(count);
            entry.snapshot.assign(batch.verts.begin(), batch.verts.end());
            ++stats_.uploadedBatches;
            stats_.uploadedBytes += bytes;
        }
    }

    template<class Release>
    void clear(Release release) {
        for (const Entry& entry : slots_) if (entry.buffer) release(entry.buffer);
        slots_.clear();
        stats_ = {};
    }
    Slot at(std::size_t index) const {
        if (index >= slots_.size()) return {};
        return {slots_[index].buffer, slots_[index].vertices};
    }
    const Stats& stats() const { return stats_; }
    std::size_t retained() const { return slots_.size(); }
private:
    struct Entry {
        unsigned buffer = 0;
        int vertices = 0;
        int mode = -1;
        std::string texture;
        std::vector<MeshVertex> snapshot;
    };
    std::vector<Entry> slots_;
    Stats stats_{};
};
} // namespace top
