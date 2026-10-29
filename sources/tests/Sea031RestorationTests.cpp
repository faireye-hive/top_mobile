#include "top/DevSeaTravel.h"

#include <cstdlib>
#include <cmath>
#include <cstdio>

namespace {

// Replica o early-return de ActorSkin::setBoatMode: com modo ja ativo,
// uma nova chamada com outro ID troca o ID logico mas NAO o casco visual.
struct MockBoatActor {
    bool boat = false;
    int selectedId = 1;
    int visibleHullId = 0;
    int calls = 0;
    int modelLoads = 0;
    bool setBoatMode(bool enabled, int id = 0) {
        ++calls;
        if (id > 0) selectedId = id;
        if (enabled == boat) return true;
        if (enabled) {
            visibleHullId = selectedId;
            ++modelLoads;
        } else {
            visibleHullId = 0;
        }
        boat = enabled;
        return true;
    }
};

struct Actor {
    float x = 2232.5f;
    float y = 2787.5f;
    float yaw = 0.f;
};

void need(bool value) {
    if (!value) std::abort();
}

void assertEqual(float got, float expected) {
    need(std::fabs(got - expected) < 0.000001f);
}

} // namespace

int main() {
    // Caso relatado: localizacao TOPLOC1 indica barco; configuracao indica ID 4.
    MockBoatActor boat;
    need(top::dev_sea::restoreInitialBoat(boat, true, 4));
    need(boat.boat && boat.selectedId == 4 && boat.visibleHullId == 4);
    need(boat.modelLoads == 1 && boat.calls == 1);

    // Modelo diagnostico: o fluxo antigo monta o casco 1 antes de ler ID 4.
    MockBoatActor formerSequence;
    need(formerSequence.setBoatMode(true));
    need(formerSequence.setBoatMode(true, 4));
    need(formerSequence.selectedId == 4 && formerSequence.visibleHullId == 1);

    // Com checkpoint em terra o barco nao liga, mas o ID configurado fica
    // pronto para a primeira embarcacao posterior.
    MockBoatActor onLand;
    need(top::dev_sea::restoreInitialBoat(onLand, false, 11));
    need(!onLand.boat && onLand.selectedId == 11 && onLand.modelLoads == 0);
    need(onLand.setBoatMode(true));
    need(onLand.visibleHullId == 11);

    // A reinicializacao de viagem comum continua gerando yaw=0.
    Actor oldActor{};
    oldActor.x = 100.25f;
    oldActor.y = 200.75f;
    oldActor.yaw = 1.325f;
    Actor portal = top::dev_sea::arrivalActor(oldActor, false);
    assertEqual(portal.yaw, 0.f);
    assertEqual(portal.x, 2232.5f);
    assertEqual(portal.y, 2787.5f);

    // Apenas o teleporte F9 entre mapas recebe a rotacao da origem.
    Actor manual = top::dev_sea::arrivalActor(oldActor, true);
    assertEqual(manual.yaw, oldActor.yaw);
    assertEqual(manual.x, 2232.5f);
    assertEqual(manual.y, 2787.5f);
    manual.x = 124.5f;
    manual.y = 88.5f;
    assertEqual(manual.yaw, oldActor.yaw);  // depois de atribuir coordenadas
    std::puts("SEA-03.1: casco na primeira ativacao e yaw exclusivo do F9 OK");
}
