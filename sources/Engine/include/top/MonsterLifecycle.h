#pragma once

// 05-T: estados de spawns entre as atualizacoes do streaming local.
// Nada aqui executa scripts Lua; o relogio avanca apenas na simulacao offline.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace top { namespace monster_lifecycle {

inline float respawnDelay(int luaSeconds) {
    // ChaSpn e CreateCha sem tempo explicito mantem os 8 s do porte anterior.
    return luaSeconds > 0 ? static_cast<float>(std::min(luaSeconds, 864000)) : 8.f;
}

struct State {
    int hp = 0;
    float x = 0.f, y = 0.f, yaw = 0.f;
    double deadline = 0.;
    bool dead = false;
};

class Registry {
public:
    void clear() { maps_.clear(); }
    void save(const std::string& map, std::uint64_t key, double now,
              int hp, float x, float y, float yaw, float deathElapsed, int luaSeconds) {
        if (map.empty() || key == 0 || !std::isfinite(now) ||
            !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(yaw)) return;
        auto& entries = maps_[map];
        // Limite de memoria; protege contra arquivos e mapas arbitrarios.
        if (entries.size() >= 30000 && entries.find(key) == entries.end()) return;
        State state;
        state.hp = std::max(0, hp);
        state.x = x; state.y = y; state.yaw = yaw;
        state.dead = hp <= 0;
        if (state.dead) {
            const float delay = respawnDelay(luaSeconds);
            state.deadline = now + std::max(0.f, delay - std::max(0.f, deathElapsed));
        }
        entries[key] = state;
    }

    // Retorna false se o spawn ainda nao tem historico. Quando o temporizador
    // expira, o monstro retorna com HP completo nas coordenadas de nascimento.
    bool restore(const std::string& map, std::uint64_t key, double now,
                 int maxHp, float birthX, float birthY, int luaSeconds,
                 int& hp, float& x, float& y, float& yaw, float& deathElapsed) const {
        const auto mit = maps_.find(map);
        if (mit == maps_.end() || key == 0) return false;
        const auto it = mit->second.find(key);
        if (it == mit->second.end()) return false;
        const State& state = it->second;
        if (state.dead) {
            if (now >= state.deadline) {
                hp = std::max(1, maxHp);
                x = birthX; y = birthY;
                deathElapsed = 0.f;
            } else {
                hp = 0;
                x = state.x; y = state.y;
                yaw = state.yaw;
                deathElapsed = std::max(.01f, respawnDelay(luaSeconds) -
                    static_cast<float>(state.deadline - now));
            }
        } else {
            hp = std::clamp(state.hp, 1, std::max(1, maxHp));
            x = state.x; y = state.y; yaw = state.yaw;
            deathElapsed = 0.f;
        }
        return true;
    }

    std::size_t sizeFor(const std::string& map) const {
        const auto it = maps_.find(map);
        return it == maps_.end() ? 0 : it->second.size();
    }

private:
    std::unordered_map<std::string, std::unordered_map<std::uint64_t, State>> maps_;
};

} } // namespace top::monster_lifecycle
