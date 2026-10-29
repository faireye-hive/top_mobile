#pragma once
#include <string>
#include <vector>

namespace top::item_newbie_chests {
// Tabela fechada recuperada de NewbieChest() no ItemEffect.lua historico.
// Nao avalia scripts Lua durante o jogo e nao mistura regras de NPC/UI.
struct Reward { int itemId=0; int quantity=0; int itemEvent=0; int quality=0; };
struct Plan {
    int minimumFreeSlots=0; // GetChaFreeBagGridNum ANTES de consumir a caixa.
    int gold=0;
    std::vector<Reward> rewards;
};
enum class Failure { None, NotNewbieChest, Boat, Level, Job, CharacterType, Unsupported };
struct Result {
    Failure failure=Failure::NotNewbieChest;
    Plan plan;
    explicit operator bool() const { return failure == Failure::None; }
};
// O callback e o ID devem corresponder. A caixa 443 so aceita profissões de
// segunda promoção que tenham uma tabela de recompensas reproduzida.
Result evaluate(int itemId, const std::string& callback, int level, int job,
                int characterType, bool usingBoat);
bool isHistoricalNewbieChest(int itemId, const std::string& callback);
bool isHistoricalClassChest(int itemId, const std::string& callback);
} // namespace top::item_newbie_chests
