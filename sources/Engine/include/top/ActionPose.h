#pragma once

// Migracao 05-E: o campo "Play Action" (coluna 47, base zero) de skillinfo.txt
// contem a ID de uma ou mais acoes de CharacterAction.tx, separadas por virgula.
// As acoes 7, 8 e 9 sao os tres ataques normais do cliente historico.
#include "top/WeaponPose.h"

#include <cctype>
#include <string>
#include <vector>

namespace top {

inline std::vector<int> parsePoseActions(const std::string& actions) {
    std::vector<int> result;
    std::size_t at = 0;
    while (at < actions.size() && result.size() < 8) {
        while (at < actions.size() &&
               (actions[at] == ',' || actions[at] == ';' || std::isspace(static_cast<unsigned char>(actions[at])))) ++at;
        if (at >= actions.size()) break;
        if (!std::isdigit(static_cast<unsigned char>(actions[at]))) {
            while (at < actions.size() && actions[at] != ',' && actions[at] != ';') ++at;
            continue;
        }
        int value = 0;
        while (at < actions.size() && std::isdigit(static_cast<unsigned char>(actions[at]))) {
            value = value * 10 + (actions[at] - '0');
            // Valores fora de 1..54 sao descartados sem risco de overflow.
            if (value > kWeaponPoseStride) {
                while (at < actions.size() && actions[at] != ',' && actions[at] != ';') ++at;
                break;
            }
            ++at;
        }
        if (value < 1 || value > kWeaponPoseStride) continue;
        // Uma acao isolada seguida de texto nao representa uma ID valida.
        if (at < actions.size() && actions[at] != ',' && actions[at] != ';' &&
            !std::isspace(static_cast<unsigned char>(actions[at]))) {
            while (at < actions.size() && actions[at] != ',' && actions[at] != ';') ++at;
            continue;
        }
        result.push_back(value);
    }
    return result;
}

inline int cycleNormalAttack(unsigned sequence) {
    return 7 + static_cast<int>(sequence % 3u);
}

inline int choosePoseAction(const std::vector<int>& actions, unsigned sequence, int fallback) {
    if (actions.empty()) return fallback;
    const int action = actions[sequence % actions.size()];
    return action >= 1 && action <= kWeaponPoseStride ? action : fallback;
}

inline bool shouldRestartActionPose(int wantedPose, int currentPose, int action,
                                    int activeAction, unsigned long long cycle,
                                    unsigned long long activeCycle) {
    return wantedPose != currentPose ||
           ((wantedPose == 2 || wantedPose == 5) &&
            (action != activeAction || cycle != activeCycle));
}

} // namespace top
