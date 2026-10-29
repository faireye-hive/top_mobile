#pragma once

#include "top/LegacyEffFormat.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace top { namespace historical_eff_timeline {

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool enabled() { return true; }

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool controllerClocksEnabled() { return true; }

// SK2.18-L: historical behavior is part of normal execution.
inline constexpr bool snowFireEnabled() { return true; }

struct ClockState {
    std::size_t index = 0;
    float elapsed = 0.f;
};

// CTexCoordList/CTexList/CTexFrame each keep controller-local state. DX9
// advances once only when elapsed > frameTime and throws away the remainder.
inline void advanceClock(ClockState& clock, float dt, float frameTime,
                         std::size_t count) {
    if (count < 2) { clock = {}; return; }
    clock.elapsed += dt;
    if (clock.elapsed > frameTime) {
        ++clock.index;
        clock.elapsed = 0.f;
    }
    if (clock.index >= count) clock.index = 0;
}

struct TrackState {
    std::size_t frame = 0;
    float frameTime = 0.f;
    bool active = false;
    ClockState coordClock;
    ClockState regionClock;
    ClockState textureClock;
};

// CMPModelEff::FrameMove advances at most one frame per call and resets the
// residual clock to zero. The group restarts only after every child stopped.
struct Group {
    bool initialized = false;
    bool finished = false;
    unsigned generation = 0;
    std::vector<TrackState> tracks;

    template<class ModelTracks>
    void reset(const ModelTracks& source) {
        initialized = true;
        finished = false;
        tracks.assign(source.size(), TrackState{});
        for (std::size_t i = 0; i < source.size(); ++i)
            tracks[i].active = !source[i].subeffect.keys.empty();
    }

    template<class ModelTracks>
    void advance(float dt, const ModelTracks& source, bool loop) {
        if (!std::isfinite(dt) || dt < 0.f || source.empty()) return;
        if (!initialized || tracks.size() != source.size()) reset(source);
        if (finished) return;

        for (std::size_t i = 0; i < tracks.size(); ++i) {
            TrackState& state = tracks[i];
            if (!state.active) continue;
            const auto& keys = source[i].subeffect.keys;
            if (state.frame >= keys.size()) {
                state.active = false;
                continue;
            }
            const float frameDuration = keys[state.frame].time;
            state.frameTime += dt;
            if (state.frameTime >= frameDuration) {
                ++state.frame;
                state.frameTime = 0.f;
                if (state.frame >= keys.size()) state.active = false;
            }
            if (!state.active) continue;
            const auto& part = source[i].subeffect;
            if (part.type == 2)
                advanceClock(state.coordClock, dt, part.coordFrameTime,
                    part.meshCoordFrames.empty() ? part.coordFrames.size() : part.meshCoordFrames.size());
            else if (part.type == 3)
                advanceClock(state.regionClock, dt, part.regionFrameTime,
                    part.meshRegions.empty() ? part.regions.size() : part.meshRegions.size());
            else if (part.type == 1)
                advanceClock(state.textureClock, dt, part.textureFrameTime, part.frames.size());
        }

        const bool anyActive = std::any_of(tracks.begin(), tracks.end(),
            [](const TrackState& state) { return state.active; });
        if (!anyActive) {
            if (loop) {
                reset(source);
                ++generation;
            } else {
                finished = true;
            }
        }
    }

    legacy_eff::FramePhase phase(std::size_t index) const {
        legacy_eff::FramePhase result;
        if (!initialized || index >= tracks.size() || !tracks[index].active) {
            result.finished = true;
            return result;
        }
        result.from = tracks[index].frame;
        result.to = result.from + 1 < tracks.size() ? result.from + 1 : result.from;
        return result;
    }

    template<class ModelTracks>
    legacy_eff::FramePhase phase(std::size_t index, const ModelTracks& source) const {
        auto result = phase(index);
        if (result.finished || index >= source.size()) return result;
        const auto& keys = source[index].subeffect.keys;
        if (result.from >= keys.size()) {
            result.finished = true;
            return result;
        }
        result.to = result.from + 1 < keys.size() ? result.from + 1 : 0;
        const float duration = keys[result.from].time;
        result.fraction = duration > 1e-7f ? std::clamp(
            tracks[index].frameTime / duration, 0.f, 1.f) : 0.f;
        return result;
    }

    std::size_t textureFrame(std::size_t index) const {
        return index < tracks.size() ? tracks[index].textureClock.index : 0;
    }

    legacy_eff::Subeffect::QuadUV sampleQuad(std::size_t index,
                                             const legacy_eff::Subeffect& part) const {
        auto result = legacy_eff::defaultQuadUV();
        if (index >= tracks.size()) return result;
        const auto& state = tracks[index];
        if (part.type == 2 && !part.coordFrames.empty()) {
            const auto frame = std::min(state.coordClock.index, part.coordFrames.size()-1);
            const auto next = std::min(frame+1, part.coordFrames.size()-1);
            const float t = part.coordFrameTime > 0.f ? std::clamp(
                state.coordClock.elapsed / part.coordFrameTime, 0.f, 1.f) : 0.f;
            for (std::size_t v=0; v<4; ++v) for (std::size_t a=0; a<2; ++a)
                result[v][a] = part.coordFrames[frame][v][a]*(1.f-t) +
                               part.coordFrames[next][v][a]*t;
        } else if (part.type == 3 && !part.regions.empty()) {
            result = part.regions[std::min(state.regionClock.index, part.regions.size()-1)];
        }
        return result;
    }

    std::vector<legacy_eff::Subeffect::UV> sampleMesh(
        std::size_t index, const legacy_eff::Subeffect& part,
        const std::vector<legacy_eff::Subeffect::UV>& defaults) const {
        auto result = defaults;
        if (index >= tracks.size()) return result;
        const auto& state = tracks[index];
        if (part.type == 2 && !part.meshCoordFrames.empty()) {
            const auto frame = std::min(state.coordClock.index, part.meshCoordFrames.size()-1);
            const auto next = std::min(frame+1, part.meshCoordFrames.size()-1);
            if (part.meshCoordFrames[frame].size() == result.size() &&
                part.meshCoordFrames[next].size() == result.size()) {
                const float t = part.coordFrameTime > 0.f ? std::clamp(
                    state.coordClock.elapsed / part.coordFrameTime, 0.f, 1.f) : 0.f;
                for (std::size_t v=0; v<result.size(); ++v) for (int a=0; a<2; ++a)
                    result[v][a] = part.meshCoordFrames[frame][v][a]*(1.f-t) +
                                   part.meshCoordFrames[next][v][a]*t;
            }
        } else if (part.type == 3 && !part.meshRegions.empty()) {
            const auto& region = part.meshRegions[std::min(state.regionClock.index,
                                                            part.meshRegions.size()-1)];
            if (region.size() == result.size()) result = region;
        }
        if (result.size() == 4 && part.type == 2 && !part.coordFrames.empty()) {
            const auto quad = sampleQuad(index, part);
            for (std::size_t v=0; v<4; ++v) result[v] = quad[v];
        } else if (result.size() == 4 && part.type == 3 && !part.regions.empty()) {
            const auto quad = sampleQuad(index, part);
            for (std::size_t v=0; v<4; ++v) result[v] = quad[v];
        }
        return result;
    }
};

}} // namespace top::historical_eff_timeline
