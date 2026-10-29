#pragma once

// SkillRecord.cpp reads two actor effect slots and two (id,keyframe) item
// pairs. STAttack.cpp dispatches those channels independently; EffDelay.cpp
// composes target + ground + an additional water effect outside LAND regions.
#include "top/ActionPose.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace top {

enum class SkillEffectSurface { Unknown, Land, Water };
enum class SkillHitScope { TargetAndSurface, TargetOnly, SurfaceOnly };
enum class SkillVisualChannelKind { Actor, Item, ItemExtra, Flight, Target, Ground, Water };

struct SkillVisualChannel {
    SkillVisualChannelKind kind = SkillVisualChannelKind::Actor;
    int effectId = 0;
    int keyFrame = -3; // -1 begin, -2 end, 0 positive pose callback; -3 unknown
    int dummy = -1;    // original dummy number; not a world-space offset
    int itemDummy = -1; // flight may additionally originate on an equipped item
    int effectType = 0; // actor type 1 requests range scaling for self circles
};

struct SkillVisual {
    // Scalar accessors remain for old consumers; event composition uses channels.
    int actor = 0;
    int item = 0;
    int itemExtra = 0;
    int flight = 0;
    int target = 0;
    int ground = 0;
    int water = 0;
    int firstAction = 0; // first historical Play Action; useful for visual captures
    int targetDummy = -1; // also used by STAttack for the projectile destination, even without a target effect
    int actionKeyFrame = -3;
    std::vector<SkillVisualChannel> channels;

    int castId() const { return actor > 0 ? actor : (item > 0 ? item : itemExtra); }
    int flightId() const { return flight; }
    int hitId() const { return target > 0 ? target : (ground > 0 ? ground : water); }

    std::vector<int> castKeyframes() const {
        std::vector<int> keys;
        for (const auto& channel : channels) {
            if (!isCast(channel.kind) || channel.effectId <= 0 ||
                !dispatchableKeyframe(channel.keyFrame)) continue;
            if (std::find(keys.begin(), keys.end(), channel.keyFrame) == keys.end())
                keys.push_back(channel.keyFrame);
        }
        return keys;
    }

    std::vector<SkillVisualChannel> castChannels(int callbackKeyFrame) const {
        // STAttack normalizes positive *callbacks*, not the table's key values.
        if (callbackKeyFrame > 0) callbackKeyFrame = 0;
        std::vector<SkillVisualChannel> selected;
        if (!dispatchableKeyframe(callbackKeyFrame)) return selected;
        for (const auto& channel : channels)
            if (isCast(channel.kind) && channel.effectId > 0 &&
                channel.keyFrame == callbackKeyFrame) selected.push_back(channel);
        return selected;
    }

    std::vector<SkillVisualChannel> hitChannels(SkillEffectSurface surface,
            SkillHitScope scope = SkillHitScope::TargetAndSurface) const {
        std::vector<SkillVisualChannel> selected;
        for (const auto& channel : channels) {
            if (channel.effectId <= 0) continue;
            const bool targetChannel = channel.kind == SkillVisualChannelKind::Target;
            const bool groundChannel = channel.kind == SkillVisualChannelKind::Ground;
            const bool waterChannel = channel.kind == SkillVisualChannelKind::Water &&
                                      surface == SkillEffectSurface::Water;
            if ((targetChannel && scope != SkillHitScope::SurfaceOnly) ||
                ((groundChannel || waterChannel) && scope != SkillHitScope::TargetOnly))
                selected.push_back(channel);
        }
        return selected;
    }

private:
    static bool isCast(SkillVisualChannelKind kind) {
        return kind == SkillVisualChannelKind::Actor || kind == SkillVisualChannelKind::Item ||
               kind == SkillVisualChannelKind::ItemExtra;
    }
    static bool dispatchableKeyframe(int key) { return key >= -2 && key <= 0; }
};

class SkillVisualCatalog {
public:
    bool load(const std::string& path) {
        entries_.clear();
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::string row;
        while (std::getline(file, row)) {
            if (row.empty() || row[0] == '/' || row[0] == '#') continue;
            const auto cols = split(row, '\t');
            if (cols.size() <= 66) continue;
            const int id = parseId(cols[0]);
            if (id <= 0) continue;
            SkillVisual visual;
            const auto actions=parsePoseActions(cols[47]);
            if(!actions.empty()) visual.firstAction=actions.front();
            visual.actionKeyFrame = parseInt(cols[48], -3);
            const auto actors = split(cols[51], ',');
            const auto dummies = split(cols[50], ',');
            const auto types = split(cols[52], ',');
            // The historical reader clamps these lists to defSKILL_ITEM_EFFECT
            // (2), even though SkillRecord.h declares three actor slots.
            for (std::size_t i = 0; i < actors.size() && i < 2; ++i) {
                const int effect = parseId(actors[i]);
                if (i == 0) visual.actor = effect;
                if (effect <= 0) break; // STAttack stops at the first empty slot
                visual.channels.push_back({SkillVisualChannelKind::Actor, effect,
                    visual.actionKeyFrame, i < dummies.size() ? parseInt(dummies[i], -1) : 0,
                    -1, i < types.size() ? parseInt(types[i], 0) : 0});
            }
            const int itemDummy = parseInt(cols[53], -1);
            auto readItem = [&](std::size_t column, SkillVisualChannelKind kind, int& scalar) {
                const auto pair = split(cols[column], ',');
                scalar = parseId(pair[0]);
                const int key = pair.size() > 1 ? parseInt(pair[1], -3) : 0;
                if (scalar > 0) visual.channels.push_back({kind, scalar, key, itemDummy});
            };
            readItem(54, SkillVisualChannelKind::Item, visual.item);
            readItem(55, SkillVisualChannelKind::ItemExtra, visual.itemExtra);
            visual.flight = parseId(cols[59]);
            if (visual.flight > 0) visual.channels.push_back({SkillVisualChannelKind::Flight,
                visual.flight, parseInt(cols[56], -3), parseInt(cols[57], -1), parseInt(cols[58], -1)});
            visual.target = parseId(cols[63]);
            visual.ground = parseId(cols[65]);
            visual.water = parseId(cols[66]);
            const int hitKey = parseInt(cols[64], -3);
            visual.targetDummy=parseInt(cols[62],-1);
            if (visual.target > 0) visual.channels.push_back({SkillVisualChannelKind::Target,
                visual.target, hitKey, visual.targetDummy});
            if (visual.ground > 0) visual.channels.push_back({SkillVisualChannelKind::Ground,
                visual.ground, hitKey});
            if (visual.water > 0) visual.channels.push_back({SkillVisualChannelKind::Water,
                visual.water, hitKey});
            entries_[id] = std::move(visual);
        }
        return !entries_.empty();
    }

    const SkillVisual* find(int skillId) const {
        const auto it = entries_.find(skillId);
        return it == entries_.end() ? nullptr : &it->second;
    }
    std::size_t size() const { return entries_.size(); }

private:
    static std::vector<std::string> split(const std::string& text, char delimiter) {
        std::vector<std::string> fields;
        std::size_t from = 0;
        do {
            const auto to = text.find(delimiter, from);
            fields.push_back(text.substr(from, to == std::string::npos ? to : to - from));
            if (to == std::string::npos) break;
            from = to + 1;
        } while (from <= text.size());
        return fields;
    }
    static int parseInt(const std::string& text, int fallback) {
        if (text.empty()) return fallback;
        const char* begin = text.c_str();
        if (*begin != '-' && (*begin < '0' || *begin > '9')) return fallback;
        errno = 0;
        char* end = nullptr;
        const long value = std::strtol(begin, &end, 10);
        if (end == begin || errno == ERANGE || value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max()) return fallback;
        while (*end == '\r' || *end == '\n') ++end;
        return *end ? fallback : static_cast<int>(value);
    }
    static int parseId(const std::string& value) {
        const int id = parseInt(value, 0);
        return id > 0 && id <= 10000000 ? id : 0;
    }
    std::unordered_map<int, SkillVisual> entries_;
};

} // namespace top
