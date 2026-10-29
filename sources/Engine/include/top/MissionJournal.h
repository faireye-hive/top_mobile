#pragma once
// 05-AV: dados textuais do diario, independentes de NPC, UI e OpenGL.
// O modulo mission formata o progresso; a UI apenas o desenha.
#include "top/MissionEvents.h"
#include <algorithm>
#include <cstddef>
#include <utility>
#include <string>
#include <vector>
namespace top {
struct MissionJournalView {
    std::vector<std::string> lines;
    bool canAbandon = false;
};
// Garante que a linha de NPC destinatario fique visivel mesmo quando o
// diario tiver muitos objetivos. Uma linha resume os objetivos ocultos.
inline void limitMissionJournalDetails(std::vector<std::string>& lines,
                                       std::size_t limit, std::size_t footerRows) {
    if(lines.size()<=limit || limit<=footerRows+1)return;
    const std::size_t head=limit-footerRows-1;
    const std::size_t omitted=lines.size()-(limit-1);
    std::vector<std::string> result(lines.begin(),lines.begin()+static_cast<std::ptrdiff_t>(head));
    result.push_back("... +"+std::to_string(omitted)+" objetivos");
    if(footerRows>0)
        result.insert(result.end(),lines.end()-static_cast<std::ptrdiff_t>(footerRows),lines.end());
    lines=std::move(result);
}
// Exibe progresso do gatilho historico desde o ACEITE, e nao somente a
// quantidade atual na bolsa. Nomes sao fornecidos pelo catalogo do jogo.
inline std::string missionEventProgress(const MissionEventTrigger& event,
                                        int count, const std::string& itemName = {}) {
    const bool item = event.kind == MissionEventKind::GetItem;
    std::string name = item ? (itemName.empty() ? "Item " + std::to_string(event.targetId) : itemName)
                            : "Monstro " + std::to_string(event.targetId);
    if(name.size()>18)name.resize(18);
    return (item ? "Coletar " : "Derrotar ") + name + " " +
           std::to_string(std::max(0,std::min(count,event.required))) + "/" +
           std::to_string(std::max(0,event.required));
}
} // namespace top
