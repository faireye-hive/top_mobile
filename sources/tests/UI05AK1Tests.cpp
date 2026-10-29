// 05-AK.1: regressao do indice de missões do GameServer: uma leitura de
// MissionScript*.lua deve servir centenas de NPCs sem varredura por consulta.
#include "top/NpcSystem.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    const fs::path original = fs::current_path();
    const fs::path fixture = fs::temp_directory_path() /
        ("top_ui05ak1_fixture_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(fixture);
    fs::create_directories(fixture / "Client/scripts/offline/script/MisScript");
    fs::create_directories(fixture / "Client/scripts/offline/maps/garner");
    {
        std::ofstream npc(fixture / "Client/scripts/offline/maps/garner/garnernpc.txt");
        npc << "2\tBlacksmith - Goldie\t1\t20\t1\t219312,270612\t219312,270612\t114\tArgent City\t1\t0\tr_trade01\t0\n";
    }
    const auto missionPath = fixture / "Client/scripts/offline/script/MisScript/MissionScript01.lua";
    {
        std::ofstream missions(missionPath);
        missions << "-- DefineMission(1, \"fake\", 9000)\n";
        missions << "DefineMission(1, \"Ignore COMPLETE_SHOW\", 700) -- COMPLETE_SHOW\n";
        missions << "DefineMission(1, \"Primeira missao\", 700)\n";
        missions << "MisBeginTalk(\"Fale com o guarda\")\n";
        missions << "MisNeed(MIS_NEED_KILL, 127, 10)\n";
        missions << "MisPrize(MIS_PRIZE_MONEY, 461)\n";
        missions << "DefineMission(1, \"Bloqueada\", 701)\n";
        missions << "MisBeginCondition(AlwaysFailure)\n";
        missions << "MisBeginTalk(\"Nao oferecer\")\n";
        missions << "DefineMission(1, \"Terceira\", 702)\n";
        missions << "MisBeginTalk(\"Coletar\")\n";
        missions << "MisNeed(MIS_NEED_ITEM, 3910, 1)\n";
        missions << "MisResultAction(AddExp, 9)\n";
    }
    {
        std::ofstream missions(fixture / "Client/scripts/offline/script/MisScript/MissionScript02.lua");
        missions << "DefineMission(1, \"Nao deve sobrescrever\", 700)\n";
        missions << "MisBeginTalk(\"Outro arquivo\")\n";
    }
    fs::current_path(fixture);
    top::NpcSystem system;
    assert(system.load("garner"));
    for (int pass = 0; pass < 250; ++pass) {
        top::HistoricalMission first;
        assert(system.mission(700, first) && first.title == "Primeira missao");
        assert(first.targetId == 127 && first.targetCount == 10 && first.rewardGold == 461);
        top::HistoricalMission third;
        assert(system.mission(702, third) && third.neededItems.size() == 1 && third.neededItems[0].id == 3910);
        assert(third.rewardExp == 9);
        top::HistoricalMission blocked;
        assert(!system.mission(701, blocked));
        assert(!system.mission(123456, blocked));
    }
    // Ao recarregar mapa, descartar o indice antigo e a cache: os scripts
    // podem ser corrigidos ou substituidos na instalacao offline.
    {
        std::ofstream missions(missionPath, std::ios::trunc);
        missions << "DefineMission(1, \"Nova versao\", 700)\n";
        missions << "MisBeginTalk(\"Alterado\")\n";
    }
    assert(system.load("garner"));
    top::HistoricalMission again;
    assert(system.mission(700, again) && again.title == "Nova versao");
    assert(!system.mission(702, again));
    fs::current_path(original);
    fs::remove_all(fixture);
    return 0;
}
