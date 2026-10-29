#pragma once

// 05-X4: CPU skinning cache, independent of OpenGL.  The pose-space result
// survives consecutive render frames with the same .lab keyframe; the
// world-space result is refreshed once per render pass (movement/yaw/height
// can change even when the animation frame does not).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace top::actor_frame_cache {

struct Vertex {
    float x = 0.f, y = 0.f, z = 0.f;
    float nx = 0.f, ny = 0.f, nz = 0.f;
};

class IndexedVertices {
public:
    void invalidate() {
        frame_ = -1;
        poseEpoch_ = worldEpoch_ = 1;
        std::fill(poseStamp_.begin(), poseStamp_.end(), 0u);
        std::fill(worldStamp_.begin(), worldStamp_.end(), 0u);
    }

    void begin(std::size_t vertexCount, int animationFrame) {
        if (local_.size() != vertexCount) {
            local_.resize(vertexCount);
            world_.resize(vertexCount);
            poseStamp_.assign(vertexCount, 0u);
            worldStamp_.assign(vertexCount, 0u);
            frame_ = -1;
        }
        if (frame_ != animationFrame) {
            frame_ = animationFrame;
            advance(poseEpoch_, poseStamp_);
        }
        // Every build may change the node/weapon attachment transform.
        advance(worldEpoch_, worldStamp_);
    }

    template <class SkinFn, class WorldFn>
    const Vertex& get(std::size_t index, SkinFn&& skin, WorldFn&& transform) {
        if (poseStamp_[index] != poseEpoch_) {
            skin(index, local_[index]);
            poseStamp_[index] = poseEpoch_;
        }
        if (worldStamp_[index] != worldEpoch_) {
            transform(local_[index], world_[index]);
            worldStamp_[index] = worldEpoch_;
        }
        return world_[index];
    }

    std::size_t size() const { return local_.size(); }

private:
    static void advance(std::uint32_t& epoch, std::vector<std::uint32_t>& stamps) {
        if (++epoch == 0) {
            std::fill(stamps.begin(), stamps.end(), 0u);
            epoch = 1;
        }
    }
    int frame_ = -1;
    std::uint32_t poseEpoch_ = 1, worldEpoch_ = 1;
    std::vector<Vertex> local_, world_;
    std::vector<std::uint32_t> poseStamp_, worldStamp_;
};

} // namespace top::actor_frame_cache
