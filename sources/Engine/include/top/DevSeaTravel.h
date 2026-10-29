#pragma once

// SEA-03: comandos EXCLUSIVOS da bancada offline. Nao criam regras de
// navegacao: o terreno real e consultado em MapFile antes de executar travel.
#include "top/DevWorldPosition.h"
#include <sstream>
#include <algorithm>
#include <limits>
#include <string>

namespace top::dev_sea {

// A configuracao e lida ANTES desta chamada; o primeiro casco visual
// precisa receber o ID correto. Evita duas ativacoes com IDs diferentes.
// Contrato: chamar uma vez, apos ActorSkin::open e antes do primeiro render.
template <typename BoatActor>
inline bool restoreInitialBoat(BoatActor& actor, bool boatWasSaved, int selectedShipId) {
    return actor.setBoatMode(boatWasSaved, selectedShipId);
}

// Actor{} reproduz o inicio das viagens normais (yaw=0); somente a viagem
// solicitada pelo laboratorio F9 carrega a orientacao da origem.
template <typename Actor>
inline Actor arrivalActor(const Actor& current, bool fromDevLab) {
    Actor destination{};
    if (fromDevLab) destination.yaw = current.yaw;
    return destination;
}

enum class Mode { Auto, Land, Boat };
struct Destination {
    dev_world::Location location;
    Mode mode = Mode::Auto;
};

// Digite: nome_do_mapa X Y [terra|barco]. O yaw e preservado do jogador.
inline bool parse(const std::string& input, Destination& output) {
    std::istringstream stream(input);
    Destination result;
    std::string mode, extra;
    if (!(stream >> result.location.map >> result.location.x >> result.location.y))
        return false;
    if (stream >> mode) {
        if (mode == "terra") result.mode = Mode::Land;
        else if (mode == "barco") result.mode = Mode::Boat;
        else return false;
        if (stream >> extra) return false;
    }
    if (!dev_world::valid(result.location)) return false;
    output = result;
    return true;
}

// Leva em conta bloqueio e mascara historica do tile, nao a sua altura.
// O modo Auto escolhe terra quando disponivel; mar caso contrario.
template <typename Map>
inline bool resolve(const Map& map, Destination& command) {
    const auto& p = command.location;
    if (p.x >= map.width() || p.y >= map.height()) return false;
    const bool land = !map.blockedAt(p.x, p.y);
    const bool sea = map.sailableAt(p.x, p.y);
    if (command.mode == Mode::Land && !land) return false;
    if (command.mode == Mode::Boat && !sea) return false;
    if (command.mode == Mode::Auto && !land && !sea) return false;
    command.location.boat = command.mode == Mode::Boat ||
                            (command.mode == Mode::Auto && !land);
    return true;
}

// SEA-03: busca SOMENTE tiles navegaveis do arquivo .map. Primeiro procura
// 480 m ao redor do jogador; se nao encontrar, amostra todo o mapa para
// acesso pratico a regioes maritimas distantes (somente ao clicar F9).
template <typename Map>
inline bool nearestWater(Map& map, float centerX, float centerY, float& outX, float& outY) {
    float best = std::numeric_limits<float>::max();
    auto consider = [&](float x, float y) {
        const float dx=x-centerX,dy=y-centerY;
        const float distance=dx*dx+dy*dy;
        if (distance<best && map.sailableAt(x,y)) {
            best=distance;
            outX=x;
            outY=y;
        }
    };
    for (int dy=-480;dy<=480;dy+=4)
        for (int dx=-480;dx<=480;dx+=4)
            if (dx*dx+dy*dy < best) consider(centerX+dx,centerY+dy);
    if (best != std::numeric_limits<float>::max()) return true;
    const int step=std::max(4,std::max(map.width(),map.height())/160);
    for (int y=step/2;y<map.height();y+=step)
        for (int x=step/2;x<map.width();x+=step)
            consider(static_cast<float>(x)+.5f,static_cast<float>(y)+.5f);
    return best != std::numeric_limits<float>::max();
}

} // namespace top::dev_sea
