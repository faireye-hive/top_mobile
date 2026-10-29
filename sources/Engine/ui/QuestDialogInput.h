#pragma once

#include "screens/ModalLayout.h"
#include "top/QuestPresentation.h"

namespace top::ui {

// 05-AT: estado VISUAL minimo para hit-testing (somente leitura).
// O motor grafico nao participa, e nenhuma regra de jogo e aplicada aqui.
struct QuestDialogHitState {
    bool open = false;
    bool trade = false;
    bool forge = false;
    bool offer = false;
    bool ready = false;
    int missionCount = 0;
    int missionSelected = -1;
    int missionPage = 0;
};

// Valores historicos de clickDialog: evita alterar API/saves nos adaptadores.
// -1 fora; 0 fechar; 1 aceitar; 2 voltar; 3 loja; 4 forja; 5 completar;
// 10..13 selecionar linha; 30/31 paginas; 40 consome clique no modal.
class QuestDialogInput {
public:
    static int hit(const QuestDialogHitState& state,
                   int x, int y, int screenW, int screenH) {
        if (!state.open) return -1;
        const auto rect = modal_layout::npc(screenW, screenH);
        const int left = static_cast<int>(rect.x);
        const int top = static_cast<int>(rect.y);
        if (!rect.contains(static_cast<float>(x), static_cast<float>(y))) return -1;
        if (state.missionCount > 0) {
            if (state.missionSelected < 0) {
                const int row = top::npc_quest_layout::rowAt(x, y, left, top);
                if (row >= 0 && state.missionPage * top::npc_quest_layout::kRows + row < state.missionCount)
                    return 10 + row;
                if (y >= top + 233 && y < top + 258) {
                    if (state.trade && x >= left + 18 && x < left + 100) return 3;
                    if (state.forge && x >= left + 112 && x < left + 194) return 4;
                }
                if (y >= top + 261 && y < top + 291) {
                    if (x >= left + 18 && x < left + 95) return 30;
                    if (x >= left + 105 && x < left + 182) return 31;
                    if (x >= left + 202 && x < left + 301) return 0;
                }
                return 40;
            }
            if (y >= top + 261 && y < top + 291) {
                if (x >= left + 18 && x < left + 147) {
                    if (state.ready) return 5;
                    if (state.offer) return 1;
                }
                if (x >= left + 159 && x < left + 301) return 2;
            }
            return 40;
        }
        if (state.trade && y >= top + 260 && y < top + 300 &&
            x >= left + 18 && x < left + 102) return 3;
        if (state.forge) {
            if (state.offer && y >= top + 232 && y < top + 260 &&
                x >= left + 18 && x < left + 102) return 4;
            if (!state.offer && y >= top + 260 && y < top + 300 &&
                x >= left + (state.trade ? 108 : 18) &&
                x < left + (state.trade ? 190 : 100)) return 4;
        }
        if (state.offer && y >= top + 264 && y < top + 302) {
            if (x >= left + 105 && x < left + 195) return 1;
            if (x >= left + 195 && x < left + 285) return 2;
        }
        return 0;
    }
};

} // namespace top::ui
