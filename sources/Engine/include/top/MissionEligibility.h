#pragma once

// Condicoes historicas de inicio que podemos verificar localmente sem executar
// arbitrariamente o Lua do GameServer. IDs aqui sao IDs visiveis da missao,
// resolvidos a partir dos IDs de registro usados nos scripts.
#include <string>
#include <vector>

namespace top {
struct MissionGate {
    struct Item { int id=0; int count=0; };
    std::vector<int> needsCompleted;
    std::vector<int> needsNotCompleted;
    std::vector<int> needsActive;
    std::vector<int> needsNotActive;
    std::vector<Item> needsItems;
    int jobEqual=-1;
    int jobNotEqual=-1;
    // Falha fechada: nao autorizar missoes cujas regras do servidor nao
    // podem ser verificadas localmente. O motivo aparece na UI.
    std::string unsupported;
};
}
