#pragma once
// 05-AJ: NPCs podem listar dezenas de missoes, nem todas disponiveis ao jogador.
// A oferta deve continuar apos IDs ausentes, concluidos ou fora do nivel.
#include <vector>

namespace top {
template<class TryOffer>
int firstAvailableNpcMission(const std::vector<int>& ids, TryOffer&& tryOffer) {
    unsigned inspected = 0;
    for (int id : ids) {
        if (++inspected > 128) break;
        if (id > 0 && tryOffer(id)) return id;
    }
    return 0;
}
} // namespace top
