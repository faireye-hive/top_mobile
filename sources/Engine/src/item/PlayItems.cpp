#include "top/Play.h"
#include "top/JobNames.h"
#include "top/ItemUseRules.h"
#include "top/ItemChestRules.h"
#include "top/ItemTicketRules.h"
#include "top/ItemNewbieChestRules.h"
#include "top/ItemInstantiationRules.h"
#include "top/SavePaths.h"
#include "top/InventoryStackRules.h"
#include "top/InventoryCapacity.h"
#include "top/ProgressionPoints.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <limits>
#include <numeric>
#include <utility>
#include <unordered_set>
#include <string>

namespace top {
namespace {
int localDateKey() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    if (now == static_cast<std::time_t>(-1) || !localtime_r(&now, &local)) return 0;
    return (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
}
int localMonthDayHourKey() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    if (now == static_cast<std::time_t>(-1) || !localtime_r(&now, &local)) return 0;
    return (local.tm_mon + 1) * 10000 + local.tm_mday * 100 + local.tm_hour;
}
}
bool Play::addStack(std::vector<Stack>& stacks, int id, int count, int limit, int slots) {
    return inventory_rules::insert(stacks, id, count, limit, slots);
}
void Play::assignItemUids() {
    // UID unico entre bolsa principal e temporaria; dados corrompidos nunca
    // podem equipar duas instancias usando o mesmo identificador.
    std::unordered_set<int> used;
    auto fix = [&](std::vector<Stack>& stacks) {
        for (Stack& stack : stacks) {
            if (stack.id == 9604) {
                stack.cloakLevel = std::clamp(stack.cloakLevel, 1, 5);
                // Creat_Item assigns +1 to all five primary attributes at Lv1;
                // each successful Cloak_Upgrade adds one more. Also repairs
                // earlier offline saves that had the level field but omitted
                // the native instance bonuses.
                stack.extra.str = std::max(stack.extra.str, stack.cloakLevel);
                stack.extra.con = std::max(stack.extra.con, stack.cloakLevel);
                stack.extra.agi = std::max(stack.extra.agi, stack.cloakLevel);
                stack.extra.sta = std::max(stack.extra.sta, stack.cloakLevel);
                stack.extra.dex = std::max(stack.extra.dex, stack.cloakLevel);
            }
            if (stack.uid <= 0 || stack.uid > 1000000000 || !used.insert(stack.uid).second) {
                while (used.count(nextItemUid_) && nextItemUid_ < 1000000000) ++nextItemUid_;
                stack.uid = nextItemUid_++;
                used.insert(stack.uid);
            }
            if (stack.uid >= nextItemUid_) nextItemUid_ = stack.uid + 1;
        }
    };
    fix(bag_);
    fix(temp_);
}

bool Play::addBag(int id, int count) {
    const auto it=items_.find(id);
    const int cap=it==items_.end()?1:it->second.maxStack;
    if (!addStack(bag_,id,count,cap,bagCapacity_)) return false;
    assignItemUids();
    return true;
}

bool Play::collectInstance(const Stack& instance) {
    const auto it = items_.find(instance.id);
    const int maxStack = it == items_.end() ? 1 : it->second.maxStack;
    if (instance.id <= 0 || instance.count != 1 || instance.uid <= 0 || maxStack > 1)
        return addBag(instance.id, instance.count);
    if (bag_.size() >= static_cast<std::size_t>(bagCapacity_)) return false;
    if (std::any_of(bag_.begin(), bag_.end(), [&](const Stack& s) { return s.uid == instance.uid; }))
        return false;
    bag_.push_back(instance);
    assignItemUids();
    return true;
}

// Associate identical non-stackable items with separate equipped slots.
int Play::equippedSlotAtBagIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(bag_.size())) return -1;
    const Stack& current = bag_[static_cast<std::size_t>(index)];
    for (const auto& entry : wornUid_) {
        if (entry.second == current.uid && worn_.count(entry.first) && worn_.at(entry.first) == current.id)
            return entry.first;
    }
    // Legado: slots que nao possuem UID continuam identificados por ordem.
    const int id=current.id;
    int previous=0;
    for(int i=0;i<index;++i) if(bag_[static_cast<std::size_t>(i)].id==id) ++previous;
    std::vector<int> slots;
    for(const auto& entry:worn_) if(entry.second==id && !wornUid_.count(entry.first)) slots.push_back(entry.first);
    std::sort(slots.begin(),slots.end());
    // Sem fallback se todos os slots desse ID ja tem UID proprio.
    return previous<static_cast<int>(slots.size())?slots[static_cast<std::size_t>(previous)]:-1;
}

int Play::bagCount(int id) const {
    int total = 0;
    for (const Stack& stack : bag_) if (stack.id == id) total += stack.count;
    return total;
}

void Play::removeBag(int id, int count) {
    for (std::size_t i = 0; i < bag_.size() && count > 0;) {
        Stack& stack = bag_[i];
        if (stack.id != id) {
            ++i;
            continue;
        }
        const int used = std::min(count, stack.count);
        stack.count -= used;
        count -= used;
        if (stack.count == 0) bag_.erase(bag_.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }
}

bool Play::hasBag(int id) const {
    for (const Stack& stack : bag_) {
        if (stack.id == id && stack.count > 0) return true;
    }
    return false;
}

bool Play::hasBankItem(int id) const {
    return std::any_of(bank_.begin(), bank_.end(), [id](const Stack& stack) {
        return stack.id == id && stack.count > 0;
    });
}

Play::StarActionResult Play::grantSennaUnityChest() {
    if (player_.level >= 41 || hasBag(1033) || hasBag(1034) ||
        hasBankItem(1033) || hasBankItem(1034) || isWorn(1033) || isWorn(1034)) {
        say("Senna: voce nao atende aos requisitos do evento Star of Unity.");
        return StarActionResult::Ineligible;
    }
    const auto item = items_.find(1033);
    if (item == items_.end()) {
        say("Senna: dados locais do Star of Unity Chest indisponiveis.");
        return StarActionResult::DataMissing;
    }
    std::vector<Stack> preview = bag_;
    if (!inventory_rules::insert(preview, 1033, 1, item->second.maxStack, bagCapacity_)) {
        say("Senna: libere espaco na bolsa para receber o Star of Unity Chest.");
        return StarActionResult::NoSpace;
    }
    auto reward = std::find_if(preview.rbegin(), preview.rend(), [](const Stack& stack) { return stack.id == 1033; });
    if (reward == preview.rend()) return StarActionResult::DataMissing;
    reward->quality = 4; // TriggerAction(1, GiveItem, 1033, 1, 4)
    bag_.swap(preview);
    assignItemUids();
    onHistoricalEvent(MissionEventKind::GetItem, 1033, 1);
    say("Senna entregou um Star of Unity Chest.");
    save(savePath());
    return StarActionResult::Success;
}

Play::StarActionResult Play::redeemTeamStar() {
    // Transfer_TeamStar in calculate/forge.lua: this is an explicit second-
    // advancement exchange, not a general-purpose script callback.
    if (player_.level < 41 || bagCount(1034) != 1 || hasBankItem(1034) || isWorn(1034)) {
        say("Resgate exige nivel 41+, exatamente 1 Star of Unity na bolsa e nenhum no cofre/equipamento.");
        return StarActionResult::Ineligible;
    }
    static const std::unordered_map<int, int> rewards = {
        {8, 1382}, {9, 1392}, {12, 1409}, {13, 1433}, {14, 1467}, {16, 1419}
    };
    const auto rewardId = rewards.find(job_);
    if (rewardId == rewards.end()) {
        say("Star of Unity so pode ser resgatado por uma classe de segunda promocao.");
        return StarActionResult::UnsupportedClass;
    }
    const auto item = items_.find(rewardId->second);
    if (item == items_.end() || item->second.maxStack != 1) {
        say("Arma historica desta classe indisponivel nos dados locais.");
        return StarActionResult::DataMissing;
    }
    std::vector<Stack> preview = bag_;
    for (std::size_t i = 0; i < preview.size(); ++i) {
        if (preview[i].id == 1034) { preview.erase(preview.begin() + static_cast<std::ptrdiff_t>(i)); break; }
    }
    if (!inventory_rules::insert(preview, rewardId->second, 1, item->second.maxStack, bagCapacity_)) {
        say("Bolsa sem espaco para a arma resgatada; Star of Unity preservado.");
        return StarActionResult::NoSpace;
    }
    auto reward = std::find_if(preview.rbegin(), preview.rend(), [&](const Stack& stack) {
        return stack.id == rewardId->second;
    });
    if (reward == preview.rend()) return StarActionResult::DataMissing;
    reward->quality = 22; // GiveItem(..., itemId, 1, 22) no callback original.
    bag_.swap(preview);
    assignItemUids();
    onHistoricalEvent(MissionEventKind::GetItem, rewardId->second, 1);
    say("Star of Unity trocado pela arma historica da classe.");
    save(savePath());
    return StarActionResult::Success;
}

bool Play::isWorn(int id) const {
    for (const auto& worn : worn_) {
        if (worn.second == id) return true;
    }
    return false;
}


std::optional<Play::TicketTravel> Play::takeTicketTravelRequest() {
    auto result = pendingTicketTravel_;
    pendingTicketTravel_.reset();
    return result;
}
bool Play::consumeTicketAfterTravel(int itemUid,int itemId) {
    if (itemUid <= 0 || itemId <= 0) return false;
    const auto it = std::find_if(bag_.begin(),bag_.end(),[&](const Stack& s) {
        return s.uid == itemUid && s.id == itemId && s.count > 0;
    });
    if (it == bag_.end()) return false;
    const auto definition = items_.find(itemId);
    if (definition == items_.end() || definition->second.scriptAction != "Ticket" ||
        !item_tickets::destinationFor(itemId, definition->second.scriptAction)) return false;
    if (--it->count == 0) bag_.erase(it);
    say("Ticket utilizado. Chegada confirmada.");
    save(savePath());
    return true;
}
void Play::reportTicketTravelFailed() {
    pendingTicketTravel_.reset();
    say("Viagem indisponivel: mapa de destino ausente ou invalido. Ticket preservado.");
}

void Play::clickBag(int index, ActorSkin* actors) {
    if (index < 0 || index >= static_cast<int>(bag_.size())) return;
    Stack& stack = bag_[static_cast<std::size_t>(index)];
    const auto it = items_.find(stack.id);
    if (it == items_.end()) return;
    const ItemInfo& item = it->second;

    if (pendingApparelConverterUid_ > 0) {
        const int converterUid = pendingApparelConverterUid_;
        pendingApparelConverterUid_ = 0;
        const auto converter = std::find_if(bag_.begin(), bag_.end(), [converterUid](const Stack& value) {
            return value.uid == converterUid && value.id == 9602 && value.count > 0;
        });
        if (converter == bag_.end()) {
            say("Apparel Converter cancelado: conversor nao esta mais na bolsa.");
            return;
        }
        if (stack.uid == converterUid) {
            say("Conversao de apparel cancelada; conversor preservado.");
            return;
        }
        if (item.maxDurability == 25000 || stack.maxDurability == 25000) {
            say("Apparel Converter: o equipamento ja e apparel; ambos os itens foram preservados.");
            return;
        }
        static constexpr std::pair<int,int> kApparelByEquipmentType[] = {
            {1,5291}, {2,5292}, {3,5296}, {4,5293}, {7,5295}, {9,5294},
            {11,5297}, {20,5287}, {22,5288}, {23,5289}, {24,5290}, {27,5288}
        };
        const auto conversion = std::find_if(std::begin(kApparelByEquipmentType),
            std::end(kApparelByEquipmentType), [&](const auto& entry) { return entry.first == item.type; });
        if (conversion == std::end(kApparelByEquipmentType) || item.slot < 0 || item.equipSlots.empty()) {
            say("Invalid equipment. Conversor e equipamento foram preservados.");
            return;
        }
        if (!ensureApparelInfo(conversion->second, item.id)) {
            say("Apparel Converter: ficha historica de apparel indisponivel; ambos os itens foram preservados.");
            return;
        }
        const auto apparelInfo = items_.find(conversion->second);
        const int sourceEquipmentId = stack.id;
        const int equippedSlot = equippedSlotAtBagIndex(index);
        const int converterIndex = static_cast<int>(std::distance(bag_.begin(), converter));
        std::vector<Stack> preview = bag_;
        const int targetUid = stack.uid;
        auto consumeOne = [&](int uid) {
            const auto source = std::find_if(preview.begin(), preview.end(), [uid](const Stack& value) {
                return value.uid == uid && value.count > 0;
            });
            if (source == preview.end()) return false;
            if (--source->count == 0) preview.erase(source);
            return true;
        };
        const int resolvedConverterUid = bag_[static_cast<std::size_t>(converterIndex)].uid;
        if (!consumeOne(resolvedConverterUid) || !consumeOne(targetUid) ||
            preview.size() >= static_cast<std::size_t>(bagCapacity_)) {
            say("Apparel Converter: bolsa sem espaco ou itens alterados; conversor e equipamento preservados.");
            return;
        }
        const std::size_t beforeOutput = preview.size();
        if (!inventory_rules::insert(preview, conversion->second, 1, apparelInfo->second.maxStack, bagCapacity_) ||
            preview.size() != beforeOutput + 1) {
            say("Apparel Converter: nao foi possivel criar apparel; ambos os itens foram preservados.");
            return;
        }
        Stack& apparel = preview.back();
        apparel.maxDurability = 25000;
        apparel.durability = 25000;
        apparel.fuseId = sourceEquipmentId;
        bag_.swap(preview);
        if (equippedSlot >= 0) {
            worn_.erase(equippedSlot);
            wornUid_.erase(equippedSlot);
            if (sheetReady_) applyAttrs(false); else recomputeGear();
        }
        assignItemUids();
        say("Equipamento " + std::to_string(sourceEquipmentId) + " convertido em apparel; aparencia preservada" +
            (equippedSlot >= 0 ? " e item desequipado." : "."));
        save(savePath());
        return;
    }
    if (item.id == 9602 && item.scriptAction == "ApparelConverter") {
        pendingApparelConverterUid_ = stack.uid;
        say("Apparel Converter: clique no equipamento da bolsa para converte-lo; cancelar clicando de novo.");
        return;
    }

    if (pendingCloakUpgradeUid_ > 0) {
        const int toolUid = pendingCloakUpgradeUid_;
        pendingCloakUpgradeUid_ = 0;
        auto tool = std::find_if(bag_.begin(), bag_.end(), [toolUid](const Stack& value) {
            return value.uid == toolUid && value.id == 9605 && value.count > 0;
        });
        if (tool == bag_.end()) {
            say("Admiral Cloak Upgrade cancelado: dispositivo nao esta mais na bolsa.");
            return;
        }
        if (stack.uid == toolUid) {
            say("Admiral Cloak Upgrade cancelado; dispositivo preservado.");
            return;
        }
        if (usingTicketBoat_) {
            say("Admiral Cloak Upgrade nao pode ser usado navegando; dispositivo preservado.");
            return;
        }
        if (item.type != 88) {
            say("Admiral Cloak Upgrade so pode ser aplicado a uma capa; ambos os itens foram preservados.");
            return;
        }
        if (stack.cloakLevel == 0 && stack.id == 9604 && stack.quality == 88) stack.cloakLevel = 1;
        if (stack.cloakLevel < 1 || stack.cloakLevel >= 5) {
            say(stack.cloakLevel >= 5 ? "Admiral Cloak ja atingiu o nivel maximo; dispositivo preservado."
                                      : "Nivel da Admiral Cloak invalido; dispositivo preservado.");
            return;
        }

        static constexpr int successBasisPoints[] = {0, 10000, 9000, 8000, 7000, 6000};
        const int level = stack.cloakLevel;
        const bool success = roll(10000) < successBasisPoints[level];
        const int toolIndex = static_cast<int>(std::distance(bag_.begin(), tool));
        if (success) {
            ++stack.cloakLevel; // SetItemAttr(Target, 55, Level + 1) / SetItemLv.
            ++stack.extra.str;
            ++stack.extra.agi;
            ++stack.extra.dex;
            ++stack.extra.con;
            ++stack.extra.sta;
            if (sheetReady_ && wornUid_.end() != std::find_if(wornUid_.begin(), wornUid_.end(),
                    [&](const auto& worn) { return worn.second == stack.uid; })) applyAttrs(false);
            say("Admiral Cloak aprimorada para o nivel " + std::to_string(stack.cloakLevel) +
                "; +1 em cada atributo principal.");
        } else {
            say("Falha na melhoria da Admiral Cloak; capa preservada sem dano.");
        }
        --bag_[static_cast<std::size_t>(toolIndex)].count;
        if (bag_[static_cast<std::size_t>(toolIndex)].count <= 0)
            bag_.erase(bag_.begin() + toolIndex);
        save(savePath());
        return;
    }

    const auto isBlackDragonMaterial = [](int id, const std::string& action) {
        return (action == "BlackDragon_Gem" && id >= 864 && id <= 866) ||
               (action == "BlackDragon_Power" && id >= 267 && id <= 270) ||
               (action == "BlackDragon_Dice" && id == 271);
    };
    if (item.id == 266 && item.scriptAction == "BlackDragon_Altar") {
        if (pendingAltarMaterialUid_ > 0) {
            const auto sourceIt = std::find_if(bag_.begin(), bag_.end(), [this](const Stack& value) {
                return value.uid == pendingAltarMaterialUid_ && value.count > 0;
            });
            if (sourceIt == bag_.end()) {
                pendingAltarMaterialUid_ = 0;
                say("Black Dragon Altar: item selecionado nao esta mais na bolsa.");
                return;
            }
            const int sourceIndex = static_cast<int>(std::distance(bag_.begin(), sourceIt));
            const auto sourceInfo = items_.find(sourceIt->id);
            if (sourceInfo == items_.end() || !isBlackDragonMaterial(sourceIt->id, sourceInfo->second.scriptAction)) {
                pendingAltarMaterialUid_ = 0;
                say("Black Dragon Altar: material selecionado indisponivel.");
                return;
            }
            if (usingTicketBoat_) {
                pendingAltarMaterialUid_ = 0;
                say("Black Dragon Altar: materiais so podem ser aplicados em terra.");
                return;
            }
            Stack& altar = bag_[static_cast<std::size_t>(index)];
            Stack& material = bag_[static_cast<std::size_t>(sourceIndex)];
            int tribute = 0;
            if (material.id == 866) tribute = 1;
            else if (material.id == 865) tribute = 2;
            else if (material.id == 864) tribute = 3;
            if (tribute != 0) {
                if (altar.altarSacrifice == tribute) {
                    pendingAltarMaterialUid_ = 0;
                    say("Black Dragon Altar ja possui esse sacrificio; material preservado.");
                    return;
                }
                altar.altarSacrifice = tribute;
                say("Sacrificio aplicado ao Black Dragon Altar; proximo equipamento historico " +
                    std::to_string(tribute == 1 ? 845 : tribute == 2 ? 847 : 846) + ".");
            } else if (material.id >= 267 && material.id <= 270) {
                int* attribute = material.id == 267 ? &altar.extra.str :
                    material.id == 268 ? &altar.extra.agi : material.id == 269 ? &altar.extra.dex : &altar.extra.con;
                if (*attribute >= 5) {
                    pendingAltarMaterialUid_ = 0;
                    say("Black Dragon Altar: esse atributo ja atingiu o limite 5; material preservado.");
                    return;
                }
                if (roll(100) < 50) { // historical Percentage_Random(0.5), local RNG sequence differs
                    if (altar.durability >= 50) altar.durability -= 50;
                    ++*attribute;
                    say(sourceInfo->second.name + " ativado; atributo elevado em 1.");
                } else {
                    ++altar.maxEnergy;
                    ++altar.energy;
                    say("A maldicao do Black Dragon aumentou em 1; o poder foi consumido.");
                }
            } else { // Angelic Dice (271)
                if (altar.energy <= 0) {
                    pendingAltarMaterialUid_ = 0;
                    say("Black Dragon Altar sem maldicoes para remover; dado preservado.");
                    return;
                }
                int chance = 0;
                if (altar.energy <= 4) chance = 20;
                else if (altar.energy <= 8) chance = 40;
                else if (altar.energy <= 12) chance = 60;
                else if (altar.energy <= 16) chance = 80;
                else if (altar.energy <= 100) chance = 100;
                if (roll(100) < chance) {
                    --altar.energy;
                    if (altar.maxEnergy > 0) --altar.maxEnergy;
                    say("O Angelic Dice removeu uma maldicao do Black Dragon Altar.");
                } else {
                    say("O Angelic Dice nao removeu a maldicao; dado consumido.");
                }
            }
            pendingAltarMaterialUid_ = 0;
            --material.count;
            if (material.count <= 0) bag_.erase(bag_.begin() + sourceIndex);
            save(savePath());
            return;
        }
        if (stack.durability != 0 || stack.energy > 8 || stack.altarSacrifice < 1 || stack.altarSacrifice > 3) {
            say("Black Dragon Altar: quebre o selo, reduza as maldicoes a 8 ou menos e aplique um sacrificio.");
            return;
        }
        // BlackDragon_Altar in historical ItemEffect.lua is event-gated on
        // the server. The offline client exposes the same literal exchange
        // locally so its recipe can be exercised; activation is PORT_APPROX.
        if (static_cast<int>(bag_.size()) >= bagCapacity_) {
            say("Black Dragon Altar: libere um espaco na bolsa; o altar foi preservado.");
            return;
        }
        static constexpr int rewards[] = {0, 845, 847, 846};
        const int rewardId = rewards[stack.altarSacrifice];
        const auto rewardInfo = items_.find(rewardId);
        if (rewardInfo == items_.end()) {
            say("Black Dragon Altar: equipamento historico indisponivel nos dados locais.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& altar = preview[static_cast<std::size_t>(index)];
        if (altar.count > 1) --altar.count;
        else preview.erase(preview.begin() + index);
        const std::size_t oldSize = preview.size();
        if (!inventory_rules::insert(preview, rewardId, 1, rewardInfo->second.maxStack, bagCapacity_)) {
            say("Black Dragon Altar: nao foi possivel receber o equipamento; altar preservado.");
            return;
        }
        auto reward = std::find_if(preview.rbegin(), preview.rend(), [rewardId](const Stack& value) {
            return value.id == rewardId;
        });
        if (reward == preview.rend() || preview.size() <= oldSize) {
            say("Black Dragon Altar: recompensa historica indisponivel; altar preservado.");
            return;
        }
        reward->quality = 20 - stack.energy;
        const int quality = reward->quality;
        bag_.swap(preview);
        assignItemUids();
        pendingAltarMaterialUid_ = 0;
        say("Black Dragon Altar aberto: equipamento " + std::to_string(rewardId) +
            " recebido com qualidade " + std::to_string(quality) + ".");
        save(savePath());
        return;
    }

    if (isBlackDragonMaterial(item.id, item.scriptAction)) {
        if (pendingAltarMaterialUid_ == stack.uid) {
            pendingAltarMaterialUid_ = 0;
            say("Uso no Black Dragon Altar cancelado; item preservado.");
            return;
        }
        if (pendingAltarMaterialUid_ > 0 && item.id != 266) pendingAltarMaterialUid_ = 0;
        if (usingTicketBoat_) {
            say(item.name + ": nao pode ser aplicado navegando; item preservado.");
            return;
        }
        pendingAltarMaterialUid_ = stack.uid;
        say("Selecione o Black Dragon Altar na bolsa para aplicar " + item.name + ".");
        return;
    }

    if (pendingAltarMaterialUid_ > 0) {
        pendingAltarMaterialUid_ = 0;
        say("Uso no Black Dragon Altar cancelado; o item clicado foi preservado.");
        return;
    }
    if (item.id == 9605 && item.scriptAction == "Cloak_Upgrade") {
        if (usingTicketBoat_) {
            say("Admiral Cloak Upgrade nao pode ser usado navegando; dispositivo preservado.");
            return;
        }
        pendingCloakUpgradeUid_ = stack.uid;
        say("Selecione na bolsa a capa tipo 88 para aplicar o Admiral Cloak Upgrade.");
        return;
    }
    if (item.id == 3335 && item.scriptAction == "ItemUse_MarchElf") {
        if (usingTicketBoat_) {
            say("Fairy March nao pode ser usado navegando; item preservado.");
            return;
        }
        if (player_.id != 3 && player_.id != 4) {
            say("Fairy March so pode ser usado por personagens historicos tipo 3 ou 4; item preservado.");
            return;
        }
        itemFairyMarchState_ = {50, 600.f}; // State_MarchElf_Add: DEF +50, SRec +20, 10 minutes.
        if (sheetReady_) applyAttrs(false);
        else recomputeGear();
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Fairy March ativo: DEF +50 e recuperacao de SP +20 por 10 minutos.");
        save(savePath());
        return;
    }
    if (item.id == 2224 && item.scriptAction == "ItemUse_MOLIBOX") {
        // Historical ItemUse_MOLIBOX always reserves four free slots, then
        // chooses one of twelve fixed three/four-piece apparel sets.
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 4) {
            say("Modern Apparel Chest exige quatro espacos livres; baú preservado.");
            return;
        }
        static const std::array<std::array<int, 4>, 12> sets{{
            {{5107,5108,5109,0}}, {{5111,5112,5113,0}}, {{5115,5116,5117,0}},
            {{5119,5120,5121,0}}, {{5123,5124,5125,0}}, {{5127,5128,5129,0}},
            {{5130,5131,5132,5133}}, {{5134,5135,5136,5137}},
            {{5138,5139,5140,5141}}, {{5143,5144,5145,0}},
            {{5147,5148,5149,0}}, {{5151,5152,5153,0}}
        }};
        const auto& selected = sets[static_cast<std::size_t>(roll(static_cast<int>(sets.size())))];
        std::vector<Stack> preview = bag_;
        int rewardCount = 0;
        bool complete = true;
        for (int rewardId : selected) {
            if (rewardId == 0) continue;
            const auto rewardInfo = items_.find(rewardId);
            if (rewardInfo == items_.end() ||
                !inventory_rules::insert(preview, rewardId, 1, rewardInfo->second.maxStack, bagCapacity_)) {
                complete = false;
                break;
            }
            auto reward = std::find_if(preview.rbegin(), preview.rend(), [rewardId](const Stack& value) {
                return value.id == rewardId;
            });
            if (reward == preview.rend()) { complete = false; break; }
            reward->quality = 4;
            ++rewardCount;
        }
        if (!complete || rewardCount < 3) {
            say("Modern Apparel Chest: dados de um conjunto historico indisponiveis; bau preservado.");
            return;
        }
        bag_.swap(preview);
        Stack& chest = bag_[static_cast<std::size_t>(index)];
        --chest.count;
        if (chest.count <= 0) bag_.erase(bag_.begin() + index);
        assignItemUids();
        say("Modern Apparel Chest aberto: conjunto historico de " + std::to_string(rewardCount) + " pecas recebido.");
        save(savePath());
        return;
    }
    if (item.id == 3896 && item.scriptAction == "ItemUse_XEZJ") {
        if (usingTicketBoat_) {
            say("Malicious Horn nao pode ser usado navegando; item preservado.");
            return;
        }
        itemMaliciousHornState_ = {30, 1800.f}; // State_PKZDYS_Add, statelv=1.
        if (sheetReady_) applyAttrs(false);
        else recomputeGear();
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Malicious Horn ativo: ataque +30 por 30 minutos.");
        save(savePath());
        return;
    }
    if (item.id == 2904 && item.scriptAction == "ItemUse_JRDQBox") {
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 10) {
            say("Gift of the Beauty exige dez espacos livres; bau preservado.");
            return;
        }
        if (player_.id == 1 || player_.id == 2) {
            say("Gift of the Beauty so pode ser aberto por personagens historicos femininos; bau preservado.");
            return;
        }
        const int draw = roll(100) + 1;
        std::vector<std::pair<int,int>> rewards;
        if (draw <= 20) rewards = {{3343,1}};
        else if (draw <= 40) rewards = {{3354,1},{3355,1},{3356,1},{3357,1},{3358,1},{3359,1}};
        else if (draw <= 45) rewards = {{937,1}};
        else if (draw <= 65) {
            for (int id=4264; id<=4273; ++id) rewards.emplace_back(id,1);
        } else if (draw <= 75) rewards = {{3094,3}};
        else if (draw <= 85) rewards = {{855,10}};
        else if (draw == 86) rewards = {{1012,1}};
        else if (draw <= 88 && player_.id == 3) rewards = {{5244,1},{5245,1},{5246,1},{5247,1}};
        else if (draw >= 89 && draw <= 90 && player_.id == 4)
            rewards = {{5252,1},{5253,1},{5254,1},{5255,1}};
        else rewards = {{1808 + roll(4),1}};

        std::vector<Stack> preview = bag_;
        bool complete = true;
        for (const auto& reward : rewards) {
            const auto info = items_.find(reward.first);
            if (info == items_.end() ||
                !inventory_rules::insert(preview, reward.first, reward.second, info->second.maxStack, bagCapacity_)) {
                complete = false;
                break;
            }
            auto granted = std::find_if(preview.rbegin(), preview.rend(), [&](const Stack& value) {
                return value.id == reward.first;
            });
            if (granted == preview.rend()) { complete = false; break; }
            granted->quality = 4;
        }
        if (!complete) {
            say("Gift of the Beauty: recompensa historica ausente ou sem espaco; bau preservado.");
            return;
        }
        bag_.swap(preview);
        Stack& chest = bag_[static_cast<std::size_t>(index)];
        --chest.count;
        if (chest.count <= 0) bag_.erase(bag_.begin() + index);
        assignItemUids();
        say("Gift of the Beauty aberto; recompensas historicas recebidas.");
        save(savePath());
        return;
    }
    if (item.id == 2905 && item.scriptAction == "ItemUse_CJDQBox") {
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 10) {
            say("Gift of the Hunk exige dez espacos livres; bau preservado.");
            return;
        }
        if (player_.id == 3 || player_.id == 4) {
            say("Gift of the Hunk so pode ser aberto por personagens historicos masculinos; bau preservado.");
            return;
        }
        const int draw = roll(100) + 1;
        std::vector<std::pair<int,int>> rewards;
        if (draw <= 20) rewards = {{3077,1}};
        else if (draw <= 40) rewards = {{3354,1},{3355,1},{3356,1},{3357,1},{3358,1},{3359,1}};
        else if (draw <= 45) rewards = {{936,1}};
        else if (draw <= 65) {
            for (int id=4264; id<=4273; ++id) rewards.emplace_back(id,1);
        } else if (draw <= 75) rewards = {{3094,3}};
        else if (draw <= 85) rewards = {{855,10}};
        else if (draw == 86) rewards = {{862,1}};
        else if (draw <= 88 && player_.id == 1) rewards = {{5221,1},{5222,1},{5223,1}};
        else if (draw >= 89 && draw <= 90 && player_.id == 2) rewards = {{5238,1},{5239,1},{5240,1}};
        else rewards = {{1808 + roll(4),1}};

        std::vector<Stack> preview = bag_;
        bool complete = true;
        for (const auto& reward : rewards) {
            const auto info = items_.find(reward.first);
            if (info == items_.end() ||
                !inventory_rules::insert(preview, reward.first, reward.second, info->second.maxStack, bagCapacity_)) {
                complete = false;
                break;
            }
            auto granted = std::find_if(preview.rbegin(), preview.rend(), [&](const Stack& value) {
                return value.id == reward.first;
            });
            if (granted == preview.rend()) { complete = false; break; }
            granted->quality = 4;
        }
        if (!complete) {
            say("Gift of the Hunk: recompensa historica ausente ou sem espaco; bau preservado.");
            return;
        }
        bag_.swap(preview);
        Stack& chest = bag_[static_cast<std::size_t>(index)];
        --chest.count;
        if (chest.count <= 0) bag_.erase(bag_.begin() + index);
        assignItemUids();
        say("Gift of the Hunk aberto; recompensas historicas recebidas.");
        save(savePath());
        return;
    }
    if (item.id == 3086 && item.scriptAction == "ItemUse_SuanmingWork") {
        if (usingTicketBoat_) {
            say(item.name + ": nao pode ser usado navegando; lote preservado.");
            return;
        }
        // ItemEffect.lua::ItemUse_SuanmingWork checks the historical level
        // ceiling and requires CEXP to be above CLEXP before consuming.
        constexpr int historicalLevelLimit = 80; // Server.Level.Limit in variable.lua
        if (player_.level >= historicalLevelLimit) {
            say("Career Lot: limite historico de nivel atingido; item preservado.");
            return;
        }
        if (exp_ <= 0 || expNext_ <= 0) {
            say("Career Lot: EXP atual insuficiente; item preservado.");
            return;
        }

        const auto clover = std::find_if(bag_.begin(), bag_.end(), [](const Stack& value) {
            return value.id == 3336 && value.count > 0;
        });
        const bool blessed = clover != bag_.end();
        const int level = player_.level;
        const int lot = roll(100) + 1;
        std::string result;
        auto addHistoricalExp = [&](double amount, bool levelFraction) {
            long double gained = std::floor(levelFraction ?
                static_cast<long double>(expNext_) * amount : static_cast<long double>(amount));
            if (!levelFraction && level >= historicalLevelLimit)
                gained = std::floor(gained / 50.0L);
            long double total = static_cast<long double>(exp_) + gained;
            if (total > expNext_)
                total = expNext_ + std::floor((total - expNext_) / 50.0L);
            exp_ = static_cast<int>(std::clamp(total, 0.0L, 1000000000.0L));
            // Historical SetChaAttrI(ATTR_CEXP) calls CFightAble::CountLevel
            // immediately. The save stores current-level EXP, so mirror that
            // rollover here after applying QianAddExp's divide-by-50 overflow.
            char levelNotice[96];
            while (expNext_ > 0 && exp_ >= expNext_ && player_.level < 100) {
                exp_ -= expNext_;
                ++player_.level;
                const int gainedPoints = progression::attrPointAt(player_.level);
                ap_ += gainedPoints;
                tp_ += progression::skillPointAt(player_.level);
                if (sheetReady_) applyAttrs(false);
                player_.hp = player_.maxHp;
                player_.homeY = player_.homeX;
                expNext_ = experience_.needed(player_.level);
                std::snprintf(levelNotice, sizeof levelNotice, "Nivel %d. +%d pontos.", player_.level, gainedPoints);
                say(levelNotice);
            }
        };
        auto addState = [&](int state, int minutes) {
            const float duration = static_cast<float>(minutes * 60);
            bool applied = false;
            switch (state) {
            case 0:
                if (itemPotionAttackState_.remaining <= 0.f) {
                    itemPotionAttackState_ = {150, duration}; applied = true;
                }
                result = applied ? "ataque aumentado por " + std::to_string(minutes) + " min" : "bonus de ataque ja ativo; duracao nao renovada";
                break;
            case 1:
                if (itemLotteryDefenseState_.remaining <= 0.f) {
                    itemLotteryDefenseState_ = {150, duration}; applied = true;
                }
                result = applied ? "defesa aumentada por " + std::to_string(minutes) + " min" : "bonus de defesa ja ativo; duracao nao renovada";
                break;
            case 2:
                if (itemPotionMaxHpState_.remaining <= 0.f) {
                    itemPotionMaxHpState_ = {1000, duration}; applied = true;
                }
                result = applied ? "HP maximo aumentado por " + std::to_string(minutes) + " min" : "bonus de HP maximo ja ativo; duracao nao renovada";
                break;
            case 3:
                if (itemPotionHitState_.remaining <= 0.f) {
                    itemPotionHitState_ = {30, duration}; applied = true;
                }
                result = applied ? "precisao aumentada por " + std::to_string(minutes) + " min" : "bonus de precisao ja ativo; duracao nao renovada";
                break;
            default:
                if (itemPotionAspdState_.remaining <= 0.f) {
                    itemPotionAspdState_ = {140, duration}; applied = true;
                }
                result = applied ? "velocidade de ataque aumentada por " + std::to_string(minutes) + " min" : "bonus de velocidade de ataque ja ativo; duracao nao renovada";
                break;
            }
            if (applied) {
                if (sheetReady_) applyAttrs(false);
                else recomputeGear();
            }
        };
        auto addExperienceBlessing = [&](int minutes) {
            if (experienceBoost_.remaining <= 0.f) {
                experienceBoost_ = {200, static_cast<float>(minutes * 60), 3086};
                result = "bonus de EXP por " + std::to_string(minutes) + " min";
            } else result = "bonus de EXP ja ativo; duracao nao renovada";
        };
        auto randomState = [&](int maxMinutes) {
            addState(roll(5), roll(maxMinutes) + 1);
        };
        if (!blessed) {
            if (lot <= 10) {
                const int outcome = roll(100) + 1;
                if (outcome <= 20) {
                    const int percent = roll(5) + 1;
                    addHistoricalExp(percent / 100.0, true);
                    result = "+" + std::to_string(percent) + "% da EXP do nivel";
                } else if (outcome <= 50) addExperienceBlessing(roll(60) + 1);
                else randomState(60);
            } else if (lot <= 35) {
                const int amount = level * (roll(451) + 50);
                addHistoricalExp(amount, false);
                result = "+" + std::to_string(amount) + " EXP";
            } else if (lot <= 55) {
                result = "nada aconteceu";
            } else if (lot <= 85) {
                const int amount = -level * (roll(451) + 50);
                addHistoricalExp(amount, false);
                result = std::to_string(amount) + " EXP";
            } else {
                const int outcome = roll(100) + 1;
                if (outcome <= 30) {
                    const int percent = roll(2) + 1;
                    addHistoricalExp(-percent / 100.0, true);
                    result = "perda de " + std::to_string(percent) + "% da EXP do nivel";
                } else if (outcome <= 70) {
                    const int amount = -level * (roll(901) + 100);
                    addHistoricalExp(amount, false);
                    result = std::to_string(amount) + " EXP";
                } else {
                    const int damage = static_cast<int>(static_cast<long long>(player_.hp) * 90 / 100);
                    player_.hp = std::max(1, player_.hp - damage);
                    result = "dano de 90% do HP atual";
                }
            }
        } else {
            if (lot <= 30) {
                const int outcome = roll(100) + 1;
                if (outcome <= 30) {
                    const int factor = roll(91) + 10;
                    const double amount = static_cast<double>(level) * level * factor /
                        static_cast<double>(std::max(1, 50 - level));
                    addHistoricalExp(amount, false);
                    result = "+" + std::to_string(static_cast<int>(std::floor(amount))) + " EXP";
                } else if (outcome <= 60) addExperienceBlessing(roll(60) + 1);
                else randomState(60);
            } else if (lot <= 80) {
                const int outcome = roll(100) + 1;
                if (outcome <= 50) {
                    const int factor = roll(46) + 5;
                    const double amount = static_cast<double>(level) * level * factor /
                        static_cast<double>(std::max(1, 50 - level));
                    addHistoricalExp(amount, false);
                    result = "+" + std::to_string(static_cast<int>(std::floor(amount))) + " EXP";
                } else {
                    randomState(30);
                }
            } else {
                result = "nada aconteceu";
            }
        }

        if (blessed) {
            const int cloverIndex = static_cast<int>(std::distance(bag_.begin(), clover));
            --bag_[static_cast<std::size_t>(index)].count;
            --bag_[static_cast<std::size_t>(cloverIndex)].count;
            std::array<int, 2> eraseIndices{{index, cloverIndex}};
            std::sort(eraseIndices.begin(), eraseIndices.end(), [](int a, int b) { return a > b; });
            for (int eraseIndex : eraseIndices)
                if (bag_[static_cast<std::size_t>(eraseIndex)].count <= 0)
                    bag_.erase(bag_.begin() + eraseIndex);
        } else {
            --stack.count;
            if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        }
        say(item.name + (blessed ? " (Mystic Clover): " : ": ") + result + ".");
        save(savePath());
        return;
    }
    if (item.id == 3087 && item.scriptAction == "ItemUse_SuanmingMoney") {
        if (usingTicketBoat_) {
            say(item.name + ": nao pode ser usado navegando; item preservado.");
            return;
        }
        const auto clover = std::find_if(bag_.begin(), bag_.end(), [](const Stack& value) {
            return value.id == 3336 && value.count > 0;
        });
        const bool blessed = clover != bag_.end();
        const int lot = roll(100) + 1;
        int goldDelta = 0;
        double goldFraction = 0.0;
        int hpDamagePercent = 0;
        int rewardId = 0;
        int dropMinutes = 0;
        std::string result;

        // ItemEffect.lua::Suanming_Money and SuanmingTeshu_Money. The
        // thresholds and nested rolls below mirror the historical scripts.
        if (!blessed) {
            if (lot <= 10) {
                const int resultRoll = roll(100) + 1;
                if (resultRoll <= 10) {
                    goldFraction = static_cast<double>(roll(5) + 1) / 100.0;
                    result = "bonus de " + std::to_string(static_cast<int>(goldFraction * 100)) + "% de ouro";
                } else if (resultRoll <= 50) {
                    dropMinutes = roll(60) + 1;
                    result = "bonus de drop por " + std::to_string(dropMinutes) + " min";
                } else {
                    rewardId = 1092;
                    result = "pergaminho 1092";
                }
            } else if (lot <= 35) {
                const int resultRoll = roll(100) + 1;
                if (resultRoll <= 20) {
                    goldFraction = static_cast<double>(roll(9) + 1) / 1000.0;
                    result = "bonus proporcional de ouro";
                } else if (resultRoll <= 60) {
                    goldDelta = (roll(15) + 1) * 1000;
                    result = "+" + std::to_string(goldDelta) + " ouro";
                } else {
                    hpDamagePercent = 90;
                    goldDelta = roll(10001) + 10000;
                    result = "+" + std::to_string(goldDelta) + " ouro com dano de 90% do HP atual";
                }
            } else if (lot <= 55) {
                result = "nada aconteceu";
            } else if (lot <= 85) {
                if (roll(100) + 1 <= 20) {
                    goldDelta = -(roll(4901) + 100);
                    result = std::to_string(goldDelta) + " ouro";
                } else {
                    goldFraction = -static_cast<double>(roll(9) + 1) / 1000.0;
                    result = "perda de ouro proporcional";
                }
            } else {
                if (roll(100) + 1 <= 20) {
                    goldDelta = -(roll(20001) + 10000);
                    result = std::to_string(goldDelta) + " ouro";
                } else {
                    goldFraction = -static_cast<double>(roll(2) + 1) / 100.0;
                    result = "perda de " + std::to_string(static_cast<int>(-goldFraction * 100)) + "% de ouro";
                }
            }
        } else {
            if (lot <= 30) {
                const int resultRoll = roll(100) + 1;
                if (resultRoll <= 5) {
                    goldDelta = (roll(91) + 10) * 10000;
                    result = "+" + std::to_string(goldDelta) + " ouro";
                } else if (resultRoll <= 60) {
                    dropMinutes = roll(60) + 1;
                    result = "bonus de drop por " + std::to_string(dropMinutes) + " min";
                } else {
                    rewardId = 1092;
                    result = "pergaminho 1092";
                }
            } else if (lot <= 80) {
                if (roll(100) + 1 <= 60) {
                    goldDelta = (roll(20) + 1) * 1000;
                    result = "+" + std::to_string(goldDelta) + " ouro";
                } else {
                    hpDamagePercent = 90;
                    goldDelta = roll(20001) + 10000;
                    result = "+" + std::to_string(goldDelta) + " ouro com dano de 90% do HP atual";
                }
            } else {
                result = "nada aconteceu";
            }
        }

        if (rewardId > 0) {
            const auto reward = items_.find(rewardId);
            if (reward == items_.end() || reward->second.maxStack <= 0) {
                say(item.name + ": premio historico ausente do catalogo; lote e trevo preservados.");
                return;
            }
            std::vector<Stack> preview = bag_;
            auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
                return value.uid == stack.uid && value.id == item.id;
            });
            if (source == preview.end()) return;
            if (--source->count <= 0) preview.erase(source);
            if (blessed) {
                auto usedClover = std::find_if(preview.begin(), preview.end(), [](const Stack& value) {
                    return value.id == 3336 && value.count > 0;
                });
                if (usedClover == preview.end()) return;
                if (--usedClover->count <= 0) preview.erase(usedClover);
            }
            if (!inventory_rules::insert(preview, rewardId, 1, reward->second.maxStack, bagCapacity_)) {
                say(item.name + ": premio nao cabe na bolsa; lote e trevo preservados.");
                return;
            }
            bag_.swap(preview);
            assignItemUids();
            onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        } else {
            if (blessed) {
                const int lotIndex = index;
                const int cloverIndex = static_cast<int>(std::distance(bag_.begin(), clover));
                --bag_[static_cast<std::size_t>(lotIndex)].count;
                --bag_[static_cast<std::size_t>(cloverIndex)].count;
                std::array<int, 2> eraseIndices{{lotIndex, cloverIndex}};
                std::sort(eraseIndices.begin(), eraseIndices.end(), [](int a, int b) { return a > b; });
                for (int eraseIndex : eraseIndices)
                    if (bag_[static_cast<std::size_t>(eraseIndex)].count <= 0)
                        bag_.erase(bag_.begin() + eraseIndex);
            } else {
                --stack.count;
                if (stack.count <= 0) bag_.erase(bag_.begin() + index);
            }
        }

        if (goldFraction != 0.0) {
            const long long delta = static_cast<long long>(std::floor(static_cast<double>(gold_) * goldFraction));
            gold_ = static_cast<int>(std::clamp(static_cast<long long>(gold_) + delta, 0LL,
                                                 static_cast<long long>(std::numeric_limits<int>::max())));
        }
        if (goldDelta != 0) {
            gold_ = static_cast<int>(std::clamp(static_cast<long long>(gold_) + goldDelta, 0LL,
                                                 static_cast<long long>(std::numeric_limits<int>::max())));
        }
        if (hpDamagePercent > 0) {
            const int damage = static_cast<int>(static_cast<long long>(player_.hp) * hpDamagePercent / 100);
            player_.hp = std::max(1, player_.hp - damage);
        }
        if (dropMinutes > 0 && dropRateBoost_.remaining <= 0.f) {
            dropRateBoost_ = {200, static_cast<float>(dropMinutes * 60), 3087};
        }
        say(item.name + (blessed ? " (Mystic Clover): " : ": ") + result + ".");
        save(savePath());
        return;
    }
    if (item.id == 2962 && item.scriptAction == "SummonAries") {
        if (usingTicketBoat_) {
            say("Cannot summon Aries Guardian while sailing; scroll preserved.");
            return;
        }
        if (hereSafeZone_) {
            say("Unable to summon monster in Safe Zone; scroll preserved.");
            return;
        }
        if (!summonAriesGuardian(actors)) {
            say("Summon Scroll - Aries: summon unavailable; scroll preserved.");
            return;
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Aries Guardian summoned for 3600 seconds.");
        save(savePath());
        return;
    }
    int summonLifetime = 0;
    const char* summonName = nullptr;
    bool (Play::*summonHistorical)(ActorSkin*) = nullptr;
    if (item.id == 1014 && item.scriptAction == "ItemUse_NSSN") {
        summonLifetime = 3600;
        summonName = "Goddess Guardian";
        summonHistorical = &Play::summonGoddessGuardian;
    } else if (item.id == 1027 && item.scriptAction == "ItemUse_MHDYSD") {
        summonLifetime = 180;
        summonName = "Medical Report creature";
        summonHistorical = &Play::summonMedicalReportCreature;
    }
    if (summonHistorical) {
        if (usingTicketBoat_) {
            say("Cannot summon while sailing; scroll preserved.");
            return;
        }
        if (hereSafeZone_) {
            say("Unable to summon monster in Safe Zone; scroll preserved.");
            return;
        }
        if (!(this->*summonHistorical)(actors)) {
            say(item.name + ": summon unavailable; scroll preserved.");
            return;
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(std::string(summonName) + " summoned for " + std::to_string(summonLifetime) + " seconds.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_MonsterSummon" && item.id == 319) {
        if (!summonHistoricalMonster(actors)) {
            say(item.name + ": nao foi possivel invocar agora; pergaminho preservado.");
            return;
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": monstro invocado por 40 segundos.");
        save(savePath());
        return;
    }
    int fixedSummonId = 0;
    int fixedSummonRespawn = 9000;
    int fixedSummonLifetime = 9000;
    bool fixedFollowerSummon = false;
    if (item.id == 2888 && item.scriptAction == "ItemUse_HJZHQ") {
        fixedSummonId = 930; fixedSummonRespawn = 1800; fixedSummonLifetime = 1800;
        fixedFollowerSummon = true;
    } else if (item.id == 2889 && item.scriptAction == "ItemUse_MLZHQ") {
        fixedSummonId = 931; fixedSummonRespawn = 3600; fixedSummonLifetime = 3600;
        fixedFollowerSummon = true;
    } else if (item.id == 2890 && item.scriptAction == "ItemUse_XRZHQ") {
        fixedSummonId = 932; fixedSummonRespawn = 7200; fixedSummonLifetime = 7200;
        fixedFollowerSummon = true;
    } else if (item.id == 2891 && item.scriptAction == "ItemUse_SDZHQ") {
        fixedSummonId = 929; fixedSummonRespawn = 10800; fixedSummonLifetime = 10800;
        fixedFollowerSummon = true;
    }
    if (fixedSummonId == 0 && item.id == 3012 && item.scriptAction == "ItemUse_XYZSTL") fixedSummonId = 786;
    else if (item.id == 3014 && item.scriptAction == "ItemUse_HDCZ") fixedSummonId = 757;
    else if (item.id == 3015 && item.scriptAction == "ItemUse_HY") fixedSummonId = 761;
    else if (item.id == 3016 && item.scriptAction == "ItemUse_HLHT") fixedSummonId = 952;
    else if (item.id == 2990 && item.scriptAction == "ItemUse_BYSHJZ") {
        fixedSummonId = 1009;
        fixedSummonRespawn = 3700;
        fixedSummonLifetime = 3600;
    }
    else if (item.id == 3036 && item.scriptAction == "ItemUse_JNSHZ") {
        fixedSummonId = 1038;
        fixedSummonRespawn = 10900;
        fixedSummonLifetime = 3600;
    }
    if (fixedSummonId > 0) {
        if (usingTicketBoat_) {
            say(item.name + ": nao pode ser usado navegando; pergaminho preservado.");
            return;
        }
        if (fixedFollowerSummon) {
            const int activePets = static_cast<int>(std::count_if(foes_.begin(), foes_.end(),
                [](const Foe& foe) { return foe.followsPlayer && foe.summonRemaining > 0.f && foe.hp > 0; }));
            if (activePets >= 5) {
                say(item.name + ": limite historico de cinco pets atingido; pergaminho preservado.");
                return;
            }
        }
        if (!fixedFollowerSummon && hereSafeZone_) {
            say(item.name + ": nao pode invocar monstros em zona segura; pergaminho preservado.");
            return;
        }
        if (!createHistoricalSummon(fixedSummonId, fixedSummonRespawn, fixedSummonLifetime,
                                    actors, fixedFollowerSummon)) {
            say(item.name + ": invocacao indisponivel; pergaminho preservado.");
            return;
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        if (fixedFollowerSummon)
            say(item.name + ": pet historico convocado por " + std::to_string(fixedSummonLifetime) + " segundos.");
        else if (item.id == 3036)
            say(item.name + ": criatura historica 1038 invocada por 3600 segundos.");
        else say(item.name + ": monstro historico invocado por 9000 segundos.");
        save(savePath());
        return;
    }
    int evilGeneratorTier=0;
    if(item.id==1144 && item.scriptAction=="ItemUse_FaSheng1") evilGeneratorTier=1;
    else if(item.id==2728 && item.scriptAction=="ItemUse_FaSheng2") evilGeneratorTier=2;
    else if(item.id==2752 && item.scriptAction=="ItemUse_FaSheng3") evilGeneratorTier=3;
    else if(item.id==2776 && item.scriptAction=="ItemUse_FaSheng4") evilGeneratorTier=4;
    if(evilGeneratorTier>0) {
        if(usingTicketBoat_) {say(item.name+": nao pode ser usado navegando; item preservado.");return;}
        if(hereSafeZone_) {say(item.name+": nao pode invocar monstros em zona segura; item preservado.");return;}
        static constexpr int tier1[]={841,842,843,843,845,846,229,274};
        static constexpr int tier2[]={847,848,849,850,851,852,211,706,673,690,691,692,693,106,289};
        static constexpr int tier3[]={757,679,678,707,708,776,74};
        static constexpr int tier4[]={952,805,807,786,788};
        int monsterId=0, refresh=0, lifetime=0;
        switch(evilGeneratorTier) {
            case 1: monsterId=tier1[roll(static_cast<int>(std::size(tier1)))];refresh=3700;lifetime=3600;break;
            case 2: monsterId=tier2[roll(static_cast<int>(std::size(tier2)))];refresh=3700;lifetime=3600;break;
            case 3: monsterId=tier3[roll(static_cast<int>(std::size(tier3)))];refresh=7300;lifetime=7200;break;
            case 4:
                monsterId=tier4[roll(static_cast<int>(std::size(tier4)))];
                refresh=monsterId==952?10900:7300;
                lifetime=monsterId==952?10800:7200;
                break;
        }
        // ItemEffect.lua offsets CreateChaX by five map units (0.05 m).
        if(!createHistoricalSummon(monsterId,refresh,lifetime,actors,false,.05f,.05f)) {
            say(item.name+": monstro historico indisponivel; item preservado.");return;
        }
        --stack.count;
        if(stack.count<=0)bag_.erase(bag_.begin()+index);
        say(item.name+": monstro historico "+std::to_string(monsterId)+
            " invocado por "+std::to_string(lifetime)+" segundos.");
        save(savePath());
        return;
    }
    if (item.id == 2907 && item.scriptAction == "ItemUse_HQBOX") {
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 4) {
            say(item.name + ": exige quatro espacos livres; pacote preservado.");
            return;
        }
        // Literal Lua ranges leave result 60 without a reward. Keep that
        // historical one-percent outcome instead of widening a neighboring range.
        const int result = roll(100) + 1;
        int rewardId = 0;
        if (result >= 1 && result <= 20) rewardId = 1012;
        else if (result <= 40) rewardId = 1016;
        else if (result >= 41 && result < 60) rewardId = 333;
        else if (result >= 61) rewardId = 273;
        if (rewardId > 0) {
            const auto reward = items_.find(rewardId);
            if (reward == items_.end() || reward->second.maxStack <= 0) {
                say(item.name + ": resultado historico ausente do catalogo; pacote preservado.");
                return;
            }
        }
        std::vector<Stack> preview = bag_;
        auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == 2907 && value.count > 0;
        });
        if (source == preview.end()) return;
        if (--source->count <= 0) preview.erase(source);
        if (rewardId > 0) {
            const std::size_t oldSize = preview.size();
            if (!inventory_rules::insert(preview, rewardId, 1, items_.at(rewardId).maxStack, bagCapacity_)) {
                say(item.name + ": premio nao cabe na bolsa; pacote preservado.");
                return;
            }
            if (preview.size() > oldSize) preview.back().quality = 4;
        }
        bag_.swap(preview);
        assignItemUids();
        if (rewardId > 0) {
            onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
            say(item.name + ": item " + std::to_string(rewardId) + " recebido, qualidade 4.");
        } else say(item.name + ": resultado historico sem premio (resultado 60).");
        save(savePath());
        return;
    }
    if (item.id == 2909 && item.scriptAction == "ItemUse_XYPIGBOX") {
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 1) {
            say(item.name + ": exige um espaco livre; bau preservado.");
            return;
        }
        const int result = roll(100) + 1;
        int rewardId = 0;
        if (result <= 4) rewardId = 1012;
        else if (result == 5) rewardId = 1016;
        else if (result <= 30) rewardId = 861;
        else if (result <= 60) rewardId = 885;
        else if (result <= 75) rewardId = 860;
        else if (result <= 88) rewardId = 862;
        else rewardId = 863;
        const auto reward = items_.find(rewardId);
        if (reward == items_.end() || reward->second.maxStack <= 0) {
            say(item.name + ": premio historico ausente do catalogo; bau preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == 2909 && value.count > 0;
        });
        if (source == preview.end()) return;
        if (--source->count <= 0) preview.erase(source);
        const std::size_t oldSize = preview.size();
        if (!inventory_rules::insert(preview, rewardId, 1, reward->second.maxStack, bagCapacity_)) {
            say(item.name + ": premio nao cabe na bolsa; bau preservado.");
            return;
        }
        if (preview.size() > oldSize) preview.back().quality = 4;
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        say(item.name + ": item " + std::to_string(rewardId) + " recebido, qualidade 4.");
        save(savePath());
        return;
    }
    if (item.id == 1035 && item.scriptAction == "ItemUse_MoHuanBox") {
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 1) {
            say(item.name + ": exige um espaco livre; bau preservado.");
            return;
        }
        // ItemEffect.lua::ItemUse_MoHuanBox first rolls 1..4663. Three rare
        // ranges award fixed cards; all other outcomes roll uniformly 3850..3875,
        // redirecting the three already-covered rare cards to Card A (3850).
        const int rareRoll = roll(4663) + 1;
        int rewardId = 0;
        if (rareRoll <= 3) rewardId = 3866;
        else if (rareRoll <= 14) rewardId = 3864;
        else if (rareRoll <= 65) rewardId = 3858;
        else {
            rewardId = roll(26) + 3850;
            if (rewardId == 3858 || rewardId == 3864 || rewardId == 3866)
                rewardId = 3850;
        }
        const auto reward = items_.find(rewardId);
        if (reward == items_.end() || reward->second.maxStack <= 0) {
            say(item.name + ": carta historica ausente do catalogo; bau preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == 1035 && value.count > 0;
        });
        if (source == preview.end()) return;
        if (--source->count <= 0) preview.erase(source);
        const std::size_t oldSize = preview.size();
        if (!inventory_rules::insert(preview, rewardId, 1, reward->second.maxStack, bagCapacity_)) {
            say(item.name + ": premio nao cabe na bolsa; bau preservado.");
            return;
        }
        if (preview.size() > oldSize) preview.back().quality = 4;
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        say(item.name + ": recebeu " + reward->second.name + " (qualidade 4).");
        save(savePath());
        return;
    }
    if (item.id == 3901 && item.scriptAction == "ItemUse_HYBOX") {
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 1) {
            say(item.name + ": exige um espaco livre; pacote preservado.");
            return;
        }
        const auto paperInfo = items_.find(3904);
        const std::uint64_t cardWeight = itemRandomRewards_.totalWeight(3901);
        if (paperInfo == items_.end() || paperInfo->second.maxStack <= 0 || cardWeight == 0 ||
            cardWeight > static_cast<std::uint64_t>(INT_MAX) || !experience_.valid() ||
            player_.level < 1 || player_.level > 100 || exp_ < 0 || gold_ < 0) {
            say(item.name + ": dados historicos incompletos; pacote preservado.");
            return;
        }

        // ItemUse_HYBOX grants gold and CEXP, has a 1/500 chance of a card
        // from BaoXiang_HYBOX, and always grants Red Paper (3904).
        const bool jackpot = roll(1000) + 1 == 1000;
        int expGain = jackpot ? 66600 : (roll(5) + 2) * 50;
        int goldGain = jackpot ? 8888 : 8;
        if (player_.level >= 80) expGain = jackpot ? 1332 : expGain / 50;

        item_random_rewards::Reward bonusCard;
        const bool hasBonusCard = roll(500) + 1 == 500;
        if (hasBonusCard && !itemRandomRewards_.rewardForRoll(3901,
                static_cast<std::uint64_t>(roll(static_cast<int>(cardWeight))) + 1u, bonusCard)) {
            say(item.name + ": tabela historica de cartas indisponivel; pacote preservado.");
            return;
        }
        const auto cardInfo = hasBonusCard ? items_.find(bonusCard.itemId) : items_.end();
        if (hasBonusCard && (cardInfo == items_.end() || cardInfo->second.maxStack <= 0)) {
            say(item.name + ": carta historica ausente no catalogo; pacote preservado.");
            return;
        }

        std::vector<Stack> preview = bag_;
        auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == 3901 && value.count > 0;
        });
        if (source == preview.end()) return;
        if (--source->count <= 0) preview.erase(source);
        auto insertReward = [&](int id, int count, int quality, int maxStack) {
            const std::size_t oldSize = preview.size();
            if (!inventory_rules::insert(preview, id, count, maxStack, bagCapacity_)) return false;
            if (preview.size() > oldSize) preview.back().quality = quality;
            return true;
        };
        if ((hasBonusCard && !insertReward(bonusCard.itemId, bonusCard.quantity,
                                           bonusCard.quality, cardInfo->second.maxStack)) ||
            !insertReward(3904, 1, 5, paperInfo->second.maxStack)) {
            say(item.name + ": recompensas nao cabem na bolsa; pacote preservado.");
            return;
        }

        const int originalLevel = player_.level;
        const std::int64_t rawExp = static_cast<std::int64_t>(exp_) + expGain;
        if (rawExp > 1000000000LL) {
            say(item.name + ": experiencia excede o limite local; pacote preservado.");
            return;
        }
        std::int64_t remainingExp = rawExp;
        int resultingLevel = player_.level;
        while (resultingLevel < 100) {
            const int needed = experience_.needed(resultingLevel);
            if (needed <= 0 || remainingExp < needed) break;
            remainingExp -= needed;
            ++resultingLevel;
        }
        if (remainingExp > 1000000000LL) {
            say(item.name + ": experiencia excede o limite local; pacote preservado.");
            return;
        }

        bag_.swap(preview);
        gold_ = static_cast<int>(std::min<std::int64_t>(1000000000,
            static_cast<std::int64_t>(gold_) + goldGain));
        for (int reached = originalLevel + 1; reached <= resultingLevel; ++reached) {
            ap_ += progression::attrPointAt(reached);
            tp_ += progression::skillPointAt(reached);
        }
        player_.level = resultingLevel;
        exp_ = static_cast<int>(remainingExp);
        expNext_ = experience_.needed(player_.level);
        if (resultingLevel != originalLevel && sheetReady_) applyAttrs(false);
        if (resultingLevel != originalLevel) {
            player_.hp = player_.maxHp;
            player_.homeY = player_.homeX;
        }
        assignItemUids();
        if (hasBonusCard) onHistoricalEvent(MissionEventKind::GetItem, bonusCard.itemId, bonusCard.quantity);
        onHistoricalEvent(MissionEventKind::GetItem, 3904, 1);
        say(item.name + ": +" + std::to_string(goldGain) + " ouro, +" +
            std::to_string(expGain) + " EXP e Red Paper" +
            (hasBonusCard ? "; carta bonus " + cardInfo->second.name : "") +
            (jackpot ? " (jackpot)" : "") + ".");
        save(savePath());
        return;
    }
    if (item.id == 2916 && item.scriptAction == "ItemUse_LoveBox") {
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 1) {
            say(item.name + ": exige um espaco livre; bau preservado.");
            return;
        }
        if (player_.id < 1 || player_.id > 4) {
            say(item.name + ": tipo de personagem sem resultado historico; bau preservado.");
            return;
        }
        const bool female = player_.id == 3 || player_.id == 4;
        int rewardId = 0;
        int specialStrength = 0;
        // ItemUse_LoveBox draws this first roll for either character sex.
        const int firstRoll = roll(35) + 1;
        if (female) {
            if (firstRoll == 35) {
                rewardId = 2902;
                specialStrength = roll(200) + 1;
            } else {
                const int giftRoll = roll(12) + 1;
                if (giftRoll == 1) rewardId = 3343;
                else if (giftRoll == 2) rewardId = 3077;
                else rewardId = 4264 + roll(10);
            }
            // The historical callback always draws this unused value after
            // the female branch, including its rare amulet result; preserve
            // its local RNG call count.
            (void)(roll(7) + 1);
        } else {
            const int giftRoll = roll(7) + 1;
            if (giftRoll == 7) {
                rewardId = 2903;
                specialStrength = roll(1000) + 1;
            } else {
                const int commonGiftRoll = roll(12) + 1;
                if (commonGiftRoll == 1) rewardId = 3343;
                else if (commonGiftRoll == 2) rewardId = 3077;
                else rewardId = 4264 + roll(10);
            }
        }
        const auto reward = items_.find(rewardId);
        if (rewardId <= 0 || reward == items_.end() || reward->second.maxStack <= 0) {
            say(item.name + ": recompensa historica ausente; bau preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == 2916 && value.count > 0;
        });
        if (source == preview.end()) return;
        if (--source->count <= 0) preview.erase(source);
        const std::size_t oldSize = preview.size();
        if (!inventory_rules::insert(preview, rewardId, 1, reward->second.maxStack, bagCapacity_)) {
            say(item.name + ": premio nao cabe na bolsa; bau preservado.");
            return;
        }
        if (preview.size() > oldSize) {
            Stack& granted = preview.back();
            granted.quality = 4;
            if (rewardId == 2902 || rewardId == 2903) granted.extra.str = specialStrength;
        }
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        say(item.name + ": recebeu " + reward->second.name + " (qualidade 4)." +
            (specialStrength > 0 ? " FOR " + std::to_string(specialStrength) + "." : ""));
        save(savePath());
        return;
    }
    if ((item.id == 2955 && item.scriptAction == "ItemUse_CZBox") ||
        (item.id == 3035 && item.scriptAction == "ItemUse_JNCZBox")) {
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 4) {
            say(item.name + ": exige quatro espacos livres; bau preservado.");
            return;
        }
        std::vector<int> rewardIds;
        if (item.id == 2955) {
            rewardIds = {5525, 5526, 5527};
            if (player_.id == 4) rewardIds.push_back(5528);
        } else {
            static const std::array<std::array<int, 4>, 4> taurusSets{{
                {{5356, 5357, 5358, 5359}},
                {{5360, 5361, 5362, 5363}},
                {{5364, 5365, 5366, 5367}},
                {{5368, 5369, 5370, 5371}},
            }};
            if (player_.id < 1 || player_.id > static_cast<int>(taurusSets.size())) {
                say(item.name + ": tipo de personagem sem conjunto historico; bau preservado.");
                return;
            }
            const auto& set = taurusSets[static_cast<std::size_t>(player_.id - 1)];
            rewardIds.assign(set.begin(), set.end());
        }
        for (int rewardId : rewardIds) {
            const auto reward = items_.find(rewardId);
            if (reward == items_.end() || reward->second.maxStack != 1) {
                say(item.name + ": uma peca historica nao existe no catalogo; bau preservado.");
                return;
            }
        }
        std::vector<Stack> preview = bag_;
        auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == item.id && value.count > 0;
        });
        if (source == preview.end()) return;
        if (--source->count <= 0) preview.erase(source);
        for (int rewardId : rewardIds) {
            const std::size_t oldSize = preview.size();
            if (!inventory_rules::insert(preview, rewardId, 1, items_.at(rewardId).maxStack, bagCapacity_) ||
                preview.size() != oldSize + 1 || preview.back().id != rewardId) {
                say(item.name + ": recompensas nao cabem na bolsa; bau preservado.");
                return;
            }
            preview.back().quality = 4;
        }
        bag_.swap(preview);
        assignItemUids();
        for (int rewardId : rewardIds) onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        say(item.name + ": conjunto historico recebido, qualidade 4.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_Admiral" && item.id == 9603) {
        if (usingTicketBoat_) {
            say("Nao abra o Admiral Cloak Chest enquanto navega; bau preservado.");
            return;
        }
        const int freeSlots = bagCapacity_ - static_cast<int>(bag_.size());
        if (freeSlots < 2) {
            say("Admiral Cloak Chest exige 2 espacos livres; bau preservado.");
            return;
        }
        const auto cloak = items_.find(9604);
        if (cloak == items_.end() || cloak->second.maxStack != 1) {
            say("Admiral Cloak historico indisponivel no catalogo; bau preservado.");
            return;
        }
        // ItemUse_Admiral: GiveItem(..., Server.Cloak.Item.ID, 1, 88).
        // Lock/battle-state APIs belong to the online server and are absent
        // from this offline client; the two-slot and sailing guards are kept.
        std::vector<Stack> preview = bag_;
        auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == 9603 && value.count > 0;
        });
        if (source == preview.end()) return;
        if (--source->count <= 0) preview.erase(source);
        if (!inventory_rules::insert(preview, 9604, 1, cloak->second.maxStack, bagCapacity_)) {
            say("Sem espaco para a capa; Admiral Cloak Chest preservado.");
            return;
        }
        auto reward = std::find_if(preview.rbegin(), preview.rend(), [](const Stack& value) {
            return value.id == 9604;
        });
        if (reward == preview.rend()) return;
        reward->quality = 88;
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, 9604, 1);
        say("Admiral Cloak Chest aberto: Admiral Cloak recebido. (PORT_APPROX)");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_Zhenheilong" && item.id == 2843) {
        if (usingTicketBoat_) {
            say("Rightful Chest of Black Dragon nao pode ser aberto navegando; bau preservado.");
            return;
        }
        // Lua checks for five free slots even though it gives three pieces,
        // or four for cha_type 4.
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 5) {
            say("Rightful Chest of Black Dragon exige cinco espacos livres; bau preservado.");
            return;
        }
        std::vector<int> rewardIds{2367, 2368, 2369};
        // Offline character IDs identify Ami as 4; the original callback
        // branches on cha_type == 4 rather than race explicitly.
        if (player_.id == 4) rewardIds.push_back(2370);
        for (int rewardId : rewardIds) {
            const auto reward = items_.find(rewardId);
            if (reward == items_.end() || reward->second.maxStack != 1) {
                say("Rightful Chest of Black Dragon: falta uma peca historica no catalogo; bau preservado.");
                return;
            }
        }
        std::vector<Stack> preview = bag_;
        auto source = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == 2843 && value.count > 0;
        });
        if (source == preview.end()) return;
        if (--source->count <= 0) preview.erase(source);
        for (int rewardId : rewardIds) {
            const std::size_t oldSize = preview.size();
            if (!inventory_rules::insert(preview, rewardId, 1, items_.at(rewardId).maxStack, bagCapacity_)) {
                say("Rightful Chest of Black Dragon nao cabe na bolsa; bau preservado.");
                return;
            }
            if (preview.size() != oldSize + 1 || preview.back().id != rewardId) return;
            preview.back().quality = 16;
        }
        bag_.swap(preview);
        assignItemUids();
        for (int rewardId : rewardIds) onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        say("Rightful Chest of Black Dragon aberto: tres pecas recebidas" +
            std::string(player_.id == 4 ? " e uma peca adicional." : "."));
        save(savePath());
        return;
    }
    if (item.id == 2988 && item.scriptAction == "ItemUse_NiceCake") {
        const auto medal = std::find_if(bag_.begin(), bag_.end(), [](const Stack& value) {
            return value.id == 3849 && value.count > 0;
        });
        if (medal == bag_.end()) {
            say("Famous Cake exige um Medal of Valor na bolsa; bolo preservado.");
            return;
        }
        const int honor = medal->extra.str;
        if (honor < 0 || honor >= 27000 || fame_ >= 99990001) {
            say("Honor ou reputacao acima do limite do Famous Cake; bolo preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        auto previewMedal = std::find_if(preview.begin(), preview.end(), [](const Stack& value) {
            return value.id == 3849 && value.count > 0;
        });
        auto previewCake = std::find_if(preview.begin(), preview.end(), [&](const Stack& value) {
            return value.uid == stack.uid && value.id == 2988 && value.count > 0;
        });
        if (previewMedal == preview.end() || previewCake == preview.end()) {
            say("Famous Cake: estado da bolsa mudou; bolo preservado.");
            return;
        }
        previewMedal->extra.str += 3000;
        if (--previewCake->count <= 0) preview.erase(previewCake);
        bag_.swap(preview);
        fame_ += 9999;
        assignItemUids();
        say("Famous Cake: +3000 honor e +9999 reputacao.");
        save(savePath());
        return;
    }
    if (item.id == 2898 && item.scriptAction == "ItemUse_SDDC") {
        if (usingTicketBoat_) {
            say(item.name + ": nao pode ser usada enquanto navega; item preservado.");
            return;
        }
        const int level = player_.level;
        const std::int64_t currentLevelStart = experience_.cumulative(level);
        const std::int64_t nextLevelStart = experience_.cumulative(level + 1);
        if (level < 1 || level >= 100 || currentLevelStart < 0 || nextLevelStart <= currentLevelStart) {
            say(item.name + ": tabela historica de EXP indisponivel para este nivel; item preservado.");
            return;
        }
        const std::int64_t currentTotal = currentLevelStart + std::max(0, exp_);
        const std::int64_t nextLevelDelta = nextLevelStart - currentLevelStart;
        double targetTotal = static_cast<double>(currentTotal);
        if (level <= 9) {
            targetTotal = static_cast<double>(experience_.cumulative(level + 5));
        } else if (level <= 29) {
            targetTotal = static_cast<double>(experience_.cumulative(level + 3));
        } else if (level <= 59) {
            targetTotal = static_cast<double>(nextLevelStart);
        } else if (level <= 75) {
            targetTotal += static_cast<double>(nextLevelDelta) * 0.5 + 10.0;
        } else if (level <= 85) {
            targetTotal += static_cast<double>(nextLevelDelta) * 0.3 + 10.0;
            if (level == 79 && targetTotal > static_cast<double>(nextLevelStart)) {
                targetTotal = static_cast<double>(nextLevelStart) +
                    (targetTotal - static_cast<double>(nextLevelStart)) * 0.02;
            }
        } else {
            targetTotal += static_cast<double>(nextLevelDelta) * 0.01;
        }
        const std::int64_t target = static_cast<std::int64_t>(std::floor(targetTotal));
        const std::int64_t gain = std::clamp<std::int64_t>(target - currentTotal, 0, INT_MAX);
        if (gain > 0) grantItemExperience(static_cast<int>(gain));
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": EXP avancada conforme a tabela historica; nivel " +
            std::to_string(player_.level) + ".");
        save(savePath());
        return;
    }
    if (item.id == 1860 && item.scriptAction == "0444") {
        // The historical Blessed Potion is skill 0444 / STATE_PKWD (level 10,
        // 5 s). Keep it item-only so this does not alter the skill executor.
        itemBlessedPotionFor_ = 5.f;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": invulneravel por 5 s.");
        save(savePath());
        return;
    }
    int historicalItemExp = 0;
    if (item.id == 2895 && item.scriptAction == "ItemUse_MWHJ")
        historicalItemExp = player_.level >= 80 ? 100 : 5000;
    else if (item.id == 3915 && item.scriptAction == "ItemUse_YB")
        historicalItemExp = player_.level >= 80 ? 6 : 300;
    if (historicalItemExp > 0) {
        grantItemExperience(historicalItemExp);
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": +" + std::to_string(historicalItemExp) + " EXP; nivel " +
            std::to_string(player_.level) + ".");
        save(savePath());
        return;
    }
    const bool expiredDumpling =
        (item.id == 3037 && item.scriptAction == "ItemUse_WQZZ") ||
        (item.id == 3043 && item.scriptAction == "ItemUse_DHZ") ||
        (item.id == 3044 && item.scriptAction == "ItemUse_DSZ") ||
        (item.id == 3045 && item.scriptAction == "ItemUse_NMZ");
    if (expiredDumpling) {
        // The historical callbacks only display this notice. Because they do
        // not call UseItemFailed, CharacterCmd consumes one item on success.
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("The item has gone rusty and has lost its effect, don't be too sad");
        save(savePath());
        return;
    }
    const bool forgeCatalyst =
        (item.scriptAction == "ItemUse_LSDZG" && (item.id == 3074 || item.id == 3883));
    const bool combineCatalyst =
        (item.scriptAction == "ItemUse_HSDZG" && (item.id == 3075 || item.id == 3884));
    if (forgeCatalyst || combineCatalyst) {
        if (usingTicketBoat_) {
            say(item.name + ": nao pode ser usado enquanto navega; item preservado.");
            return;
        }
        if (forgeCatalyst) {
            itemForgeBoostFor_ = 60.f;
            itemForgeBoostLevel_ = 4; // STATE_JLGLJB: +0.4 to Forging_Success_Rate
            say(item.name + ": chance de forja +40 pontos percentuais por 60 s.");
        } else {
            itemCombineBoostFor_ = 60.f;
            itemCombineBoostLevel_ = 2; // STATE_HCGLJB: +0.2 to Combine_Success_Rate
            say(item.name + ": chance de combinacao +20 pontos percentuais por 60 s.");
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_SanJiaoFan" && (item.id == 3046 || item.id == 3340)) {
        if (!usingTicketBoat_) {
            say(item.name + ": so pode ser usado enquanto navega; item preservado.");
            return;
        }
        // STATE_YSBoatMspd_Add historico: statelv 1 acrescenta 0.15 a MspdSa.
        // O estado local dura os mesmos 900 segundos; nao altera skilleffect.lua.
        itemBoatSpeedFor_ = 900.f;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": velocidade do barco +15% por 900 s.");
        save(savePath());
        return;
    }
    if (item.id == 4551 && item.scriptAction == "ItemUse_Fish") {
        if (!usingTicketBoat_) {
            say(item.name + ": so pode ser usada enquanto navega; item preservado.");
            return;
        }
        if (player_.homeX <= 0.f) {
            say(item.name + ": capacidade de SP do barco indisponivel; item preservado.");
            return;
        }
        // The historical callback restores 50 SP, capped at ATTR_MXSP, while
        // aboard. In this offline client homeY/homeX hold current/max SP.
        player_.homeY = std::min(player_.homeX, std::max(0.f, player_.homeY) + 50.f);
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": SP do barco +50 (" + std::to_string(static_cast<int>(player_.homeY)) + "/" +
            std::to_string(static_cast<int>(player_.homeX)) + ").");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ExpCheckSys_Toggle" && item.id == 9621) {
        stack.expDisableFlag = stack.expDisableFlag == 1 ? 0 : 1;
        say(stack.expDisableFlag == 1 ?
            "Ganho de EXP de monstros desativado." : "Ganho de EXP de monstros ativado.");
        save(savePath());
        return;
    }
    if (item.id == 1021 && item.scriptAction == "ItemUse_kala") {
        // ItemUse_kala applies STATE_KALA level 1 for 20 seconds. Its
        // historical Add callback copies current ATTR_STA into STATEV_STA;
        // keep this as an item-specific attribute state without touching the
        // skill effect executor or its formulas.
        if (itemKaraVictoryState_.remaining > 0.f) {
            say(item.name + ": STATE_KALA ja esta ativo; item preservado.");
            return;
        }
        const auto gear = gearBonuses();
        const int currentSta = std::max(0, bsta_ + gear.sta + foodStates_[2].level * 5 +
            itemSpiritState_.bonus + itemPotionSpiritState_.bonus + itemInnocenceState_.bonuses[4]);
        itemKaraVictoryState_ = {currentSta, 20.f};
        if (sheetReady_) applyAttrs(false);
        else recomputeGear();
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": STA +" + std::to_string(currentSta) + " por 20 s.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_PKJSYS" && item.id == 1857) {
        // State_PKJSYS_Add in historical skilleffect.lua adds 30 STA; the
        // item callback uses state level 10 for 180 seconds. AddState's
        // historical ADD_LARGER mode does not refresh an equal-level state.
        if (itemSpiritState_.remaining <= 0.f) {
            itemSpiritState_ = {30, 180.f};
            if (sheetReady_) applyAttrs(false);
            else recomputeGear();
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + (itemSpiritState_.remaining > 0.f
            ? ": ESP +30 por " + std::to_string(static_cast<int>(std::ceil(itemSpiritState_.remaining))) + " s."
            : ": efeito historico indisponivel."));
        save(savePath());
        return;
    }
    if ((item.scriptAction == "ItemUse_MspdYS" && item.id == 3339) ||
        (item.scriptAction == "ItemUse_MspdYSA" && item.id == 3105)) {
        if (usingTicketBoat_) {
            say(item.name + ": nao pode ser usada enquanto navega; item preservado.");
            return;
        }
        // Both callbacks apply STATE_YSMspd level 1; only their durations differ.
        // Equal-level AddState does not refresh an already active state.
        if (itemPotionMspdState_.remaining <= 0.f) {
            itemPotionMspdState_ = {15, item.id == 3339 ? 900.f : 1800.f};
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": velocidade de movimento +15% por " +
            std::to_string(static_cast<int>(std::ceil(itemPotionMspdState_.remaining))) + " s.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_PKZDYS" && item.id == 1855) {
        // State_PKZDYS_Add: statelv 10 adds 150 to both attack bounds.
        if (itemPotionAttackState_.remaining <= 0.f) {
            itemPotionAttackState_ = {150, 150.f};
            if (sheetReady_) applyAttrs(false);
            else recomputeGear();
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": ATAQ +150 por " +
            std::to_string(static_cast<int>(std::ceil(itemPotionAttackState_.remaining))) + " s.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_PKKBYS" && item.id == 1856) {
        // State_PKKBYS_Add: statelv 10 adds 140 to historical AspdSb for 20 s.
        // Equal-level AddState consumes the potion without refreshing the timer.
        if (itemPotionAspdState_.remaining <= 0.f) {
            itemPotionAspdState_ = {140, 20.f};
            if (sheetReady_) applyAttrs(false);
            else recomputeGear();
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": velocidade de ataque aumentada por " +
            std::to_string(static_cast<int>(std::ceil(itemPotionAspdState_.remaining))) + " s.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_PKJZYS" && item.id == 1859) {
        // State_PKJZYS_Add: statelv 10 adds 30 hit. Equal-level AddState
        // consumes the item without replacing/restarting an active state.
        if (itemPotionHitState_.remaining <= 0.f) {
            itemPotionHitState_ = {30, 900.f};
            if (sheetReady_) applyAttrs(false);
            else recomputeGear();
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": acerto +30 por " +
            std::to_string(static_cast<int>(std::ceil(itemPotionHitState_.remaining))) + " s.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_PKMNYS" && item.id == 1854) {
        // State_PKMNYS_Add: statelv 10 adds 1000 maximum HP for 1800 s.
        if (itemPotionMaxHpState_.remaining <= 0.f) {
            itemPotionMaxHpState_ = {1000, 1800.f};
            if (sheetReady_) applyAttrs(false);
            else recomputeGear();
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": HP maximo +1000 por " +
            std::to_string(static_cast<int>(std::ceil(itemPotionMaxHpState_.remaining))) + " s.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_XiDianBook" && item.id == 2216) {
        if (usingTicketBoat_) {
            say("Nao use Book for Regaining Ability enquanto navega; item preservado.");
            return;
        }
        // The historical callback requires two free slots before clearing any
        // fight skill, even when the character has no rebirth skills.
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 2) {
            say("Book for Regaining Ability exige dois espacos livres; nenhum estado alterado.");
            return;
        }

        int rebirthSkillCount = 0;
        for (int id = 453; id <= 458; ++id) {
            const auto learned = learnedSkills_.find(id);
            if (learned != learnedSkills_.end() && learned->second >= 1) ++rebirthSkillCount;
        }

        std::vector<int> historicalRewards;
        if (rebirthSkillCount > 0) {
            int rebirthBook = 0;
            switch (job_) {
                case 8: rebirthBook = 2957; break;  // Champion
                case 9: rebirthBook = 2956; break;  // Crusader
                case 12: rebirthBook = 2961; break; // Sharpshooter
                case 13: rebirthBook = 2959; break; // Cleric
                case 14: rebirthBook = 2958; break; // Seal Master
                case 16: rebirthBook = 2960; break; // Voyager
                default: break;
            }
            // The historical Lua also grants item 1572 here, but that ID has
            // no iteminfo record in the shipped offline data. Refuse before
            // clearing skills instead of creating an unusable phantom item.
            if (rebirthBook == 0 || !items_.count(rebirthBook) || !items_.count(1572)) {
                say("Reset preservado: faltam dados locais dos itens de retorno historicos.");
                return;
            }
            historicalRewards = {rebirthBook, 1572};
        }

        long long refund = 0;
        for (const auto& learned : learnedSkills_) {
            const auto rule = skillRules_.find(learned.first);
            if (rule == skillRules_.end() || learned.second <= 0) {
                say("Reset preservado: falta a regra historica de uma skill aprendida.");
                return;
            }
            refund += std::max(0, rule->second.points) + std::max(0, learned.second - 1);
        }

        std::vector<Stack> preview = bag_;
        const int usedUid = stack.uid;
        auto used = std::find_if(preview.begin(), preview.end(), [usedUid](const Stack& candidate) {
            return candidate.uid == usedUid;
        });
        if (used == preview.end()) return;
        if (--used->count <= 0) preview.erase(used);
        for (int rewardId : historicalRewards) {
            const ItemInfo& reward = items_.at(rewardId);
            const std::size_t oldSize = preview.size();
            if (!inventory_rules::insert(preview, rewardId, 1, reward.maxStack, bagCapacity_)) {
                say("Reset preservado: os itens historicos nao cabem na bolsa.");
                return;
            }
            if (preview.size() > oldSize) preview.back().quality = 4;
        }

        learnedSkills_.clear();
        tp_ = static_cast<int>(std::clamp(static_cast<long long>(tp_) + refund -
            (rebirthSkillCount > 0 ? 2 : 0), 0LL, 1000000LL));
        for (SkillDef& hotbarSkill : skills_) hotbarSkill = {};
        std::fill(std::begin(skillCd_), std::end(skillCd_), 0.f);
        requestedSkill_ = armedSkill_ = -1;
        pendingBasicStrike_ = false;
        bag_.swap(preview);
        say("Skills de combate redefinidas. TP recuperados: " + std::to_string(refund) + ".");
        save(savePath());
        return;
    }
    if (item.scriptAction == "Book_SkillReset" && item.id == 996) {
        if (usingTicketBoat_) {
            say("Nao redefina skills enquanto navega; livro preservado.");
            return;
        }
        long long refund = 0;
        for (const auto& learned : learnedSkills_) {
            const auto rule = skillRules_.find(learned.first);
            if (rule == skillRules_.end() || learned.second <= 0) {
                say("Reset cancelado: nao foi possivel recuperar os pontos historicos de todas as skills.");
                return;
            }
            // The book's historical reset refunds the points spent to learn
            // the skill plus one TP for each manually purchased level.
            refund += std::max(0, rule->second.points) + std::max(0, learned.second - 1);
        }
        if (learnedSkills_.empty()) {
            say("Nenhuma skill de combate aprendida para redefinir; livro preservado.");
            return;
        }
        learnedSkills_.clear();
        tp_ = static_cast<int>(std::min<long long>(1000000LL, static_cast<long long>(tp_) + refund));
        requestedSkill_ = armedSkill_ = -1;
        pendingBasicStrike_ = false;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Skills de combate redefinidas. TP recuperados: " + std::to_string(refund) + ".");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_DSSDM" && item.id == 3888) {
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto navega; item preservado.");
            return;
        }
        // ItemUse_DSSDM applies STATE_PKSFYS level 1 for 300 seconds;
        // State_PKSFYS_Add gives that level the same +30 DEF as Festive Wine.
        if (1 >= (itemHardenFor_ > 0.f ? (itemHardenDefense_ >= 150 ? 10 : 1) : 0)) {
            itemHardenFor_ = 300.f;
            itemHardenDefense_ = 30;
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": DEF +" + std::to_string(itemHardenDefense_) + " por " +
            std::to_string(static_cast<int>(std::ceil(itemHardenFor_))) + " s.");
        save(savePath());
        return;
    }
    if ((item.scriptAction == "ItemUse_PKSFYS" && item.id == 1858) ||
        (item.scriptAction == "ItemUse_XiongHJ" && item.id == 264)) {
        if (item.scriptAction == "ItemUse_XiongHJ" && usingTicketBoat_) {
            say("Nao use Festive Wine enquanto navega; item preservado.");
            return;
        }
        const bool festiveWine = item.id == 264;
        const int level = festiveWine ? 1 : 10;
        const int duration = festiveWine ? 1800 : 300;
        const int defense = level == 1 ? 30 : 150; // skilleff STATE_PKSFYS_Add
        if (level >= (itemHardenFor_ > 0.f ? (itemHardenDefense_ >= 150 ? 10 : 1) : 0)) {
            itemHardenFor_ = static_cast<float>(duration);
            itemHardenDefense_ = defense;
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Harden Potion: DEF +" + std::to_string(itemHardenDefense_) + " por " +
            std::to_string(static_cast<int>(std::ceil(itemHardenFor_))) + " s.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_IDBOX" && item.id == 24) {
        if (player_.level < 60) {
            say("Chest of Cabal: exige nivel 60; baú preservado.");
            return;
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Chest of Cabal: o evento foi encerrado. Nenhum premio historico esta disponivel.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_AiCao" && item.id == 265) {
        if (usingTicketBoat_) {
            say("Ether Clover nao pode ser usado enquanto navega; item preservado.");
            return;
        }
        itemEtherCloverFor_ = 180.f;
        itemEtherCloverDodge_ = 10; // STATE_PKSBYS_Add, statelv 1
        player_.dodge = flee_ + itemEtherCloverDodge_;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Ether Clover: evasao +10 por 180 s.");
        save(savePath());
        return;
    }
    if ((item.scriptAction == "ItemUse_Crbbt" && item.id == 584) ||
        (item.scriptAction == "ItemUse_Jrqkl" && item.id == 585)) {
        const bool strength = item.id == 584;
        ItemAttributeState& state = strength ? itemStrengthState_ : itemConstitutionState_;
        if (state.remaining <= 0.f) {
            const auto gear = gearBonuses();
            state.bonus = std::max(0, strength ? bstr_ + gear.str : bcon_ + gear.con);
        }
        state.remaining = 20.f;
        if (sheetReady_) applyAttrs(false);
        else recomputeGear();
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + (strength ? ": FOR temporariamente dobrada por 20 s." :
                                  ": CON temporariamente dobrada por 20 s."));
        save(savePath());
        return;
    }
    ItemAttributeState* attributePotionState = nullptr;
    const char* attributePotionName = nullptr;
    int attributePotionDuration = 3600;
    const bool longAttributePotion = item.scriptAction == "ItemUse_SPXsyjA" ||
        item.scriptAction == "ItemUse_SPLhyjA" || item.scriptAction == "ItemUse_SPMnyjA" ||
        item.scriptAction == "ItemUse_SPYyyjA" || item.scriptAction == "ItemUse_SPSlyjA";
    if (longAttributePotion) attributePotionDuration = 5400;
    if ((item.scriptAction == "ItemUse_SPXsyj" && item.id == 897) ||
        (item.scriptAction == "ItemUse_SPXsyjA" && item.id == 3100)) {
        attributePotionState = &itemPotionStrengthState_; attributePotionName = "FOR";
    } else if ((item.scriptAction == "ItemUse_SPLhyj" && item.id == 893) ||
               (item.scriptAction == "ItemUse_SPLhyjA" && item.id == 3101)) {
        attributePotionState = &itemPotionAgilityState_; attributePotionName = "AGI";
    } else if ((item.scriptAction == "ItemUse_SPMnyj" && item.id == 895) ||
               (item.scriptAction == "ItemUse_SPMnyjA" && item.id == 3102)) {
        attributePotionState = &itemPotionConstitutionState_; attributePotionName = "CON";
    } else if ((item.scriptAction == "ItemUse_SPYyyj" && item.id == 894) ||
               (item.scriptAction == "ItemUse_SPYyyjA" && item.id == 3103)) {
        attributePotionState = &itemPotionAccuracyState_; attributePotionName = "DES";
    } else if ((item.scriptAction == "ItemUse_SPSlyj" && item.id == 896) ||
               (item.scriptAction == "ItemUse_SPSlyjA" && item.id == 3104)) {
        attributePotionState = &itemPotionSpiritState_; attributePotionName = "ESP";
    }
    if (attributePotionState) {
        const std::array<const ItemAttributeState*, 5> attributePotionStates{{
            &itemPotionStrengthState_, &itemPotionAgilityState_, &itemPotionAccuracyState_,
            &itemPotionConstitutionState_, &itemPotionSpiritState_}};
        const bool anotherPotionActive = std::any_of(
            attributePotionStates.begin(), attributePotionStates.end(),
            [&](const ItemAttributeState* active) {
                return active != attributePotionState && active->remaining > 0.f;
            });
        if (anotherPotionActive) {
            say("Poicoes de atributo nao podem ser combinadas; item preservado.");
            return;
        }
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto navega; item preservado.");
            return;
        }
        attributePotionState->bonus = 5;
        attributePotionState->remaining = static_cast<float>(attributePotionDuration);
        if (sheetReady_) applyAttrs(false);
        else recomputeGear();
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": " + attributePotionName + " +5 por " +
            std::to_string(attributePotionDuration / 60) + " min.");
        save(savePath());
        return;
    }
    if ((item.id == 3107 && item.scriptAction == "ItemUse_DenglongA") ||
        (item.id == 3342 && item.scriptAction == "ItemUse_Denglong")) {
        if (usingTicketBoat_) {
            say(item.name + ": nao pode ser usada enquanto navega; item preservado.");
            return;
        }
        // Both callbacks add STATE_DENGLONG level 1 (+50 DEF). Historical
        // AddState does not refresh the timer for an equal-level state.
        if (itemLanternDefenseState_.remaining <= 0.f) {
            itemLanternDefenseState_ = {50, item.id == 3107 ? 900.f : 600.f};
            if (sheetReady_) applyAttrs(false);
            else recomputeGear();
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": DEF +50 por " +
            std::to_string(static_cast<int>(std::ceil(itemLanternDefenseState_.remaining))) + " s.");
        save(savePath());
        return;
    }
    if ((item.scriptAction == "ItemUse_MspdYSB" && item.id == 850) ||
        (item.scriptAction == "ItemUse_XEZJB" && item.id == 851) ||
        (item.scriptAction == "ItemUse_DenglongB" && item.id == 852)) {
        if (usingTicketBoat_) {
            say("Este consumivel nao pode ser usado enquanto navega; item preservado.");
            return;
        }
        itemBodyStateId_ = item.id == 850 ? 1 : item.id == 851 ? 2 : 3;
        itemBodyStateFor_ = 900.f;
        itemBodyMoveSpeedBonus_ = item.id == 850 ? 0.3f : 0.f;
        itemBodyAttackBonus_ = item.id == 851 ? 50 : item.id == 852 ? -30 : 0;
        itemBodyDefenseBonus_ = item.id == 850 ? -50 : item.id == 851 ? -25 : 70;
        if (sheetReady_) applyAttrs(false);
        else recomputeGear();
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": estado aplicado por 15 min.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_65BBBox" && item.id == 241) {
        if (job_ == 0) {
            say("Level 65 Boss Equipment Box exige uma promocao de classe; caixa preservada.");
            return;
        }
        int rewardId = 0;
        if (job_ == 12 || job_ == 2) rewardId = 780;
        else if (job_ == 9) rewardId = 769;
        else if (job_ == 16 || job_ == 4) rewardId = 806;
        else if (job_ == 8) rewardId = 766;
        else if (job_ == 13 || job_ == 5) rewardId = 792;
        else if (job_ == 14) rewardId = 798;
        else if (job_ == 1) {
            if (player_.id == 1) rewardId = 769;
            else if (player_.id == 2) rewardId = 766;
        }
        const auto definition = items_.find(rewardId);
        if (rewardId <= 0 || definition == items_.end()) {
            say("Classe/raça sem recompensa historica mapeada; caixa preservada.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumedBox = preview[static_cast<std::size_t>(index)];
        if (--consumedBox.count <= 0) preview.erase(preview.begin() + index);
        const std::size_t firstRewardSlot = preview.size();
        if (!inventory_rules::insert(preview, rewardId, 1, definition->second.maxStack, bagCapacity_) ||
            preview.size() <= firstRewardSlot || preview[firstRewardSlot].id != rewardId) {
            say("Bolsa sem espaco para a recompensa; caixa preservada.");
            return;
        }
        preview[firstRewardSlot].quality = 4;
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        say("Abriu Level 65 Boss Equipment Box: recebeu " + definition->second.name +
            " (qualidade 4; atributos aleatorios do MakeItem historico sao aproximados).");
        save(savePath());
        return;
    }
    const int newCapacity = inventory_rules::upgradeTarget(item.id);
    if (newCapacity > 0) {
        if (!inventory_rules::canUpgrade(item.id,bagCapacity_)) {
            say("Expansao exige bolsa com " + std::to_string(newCapacity-4) + " espacos.");
            return;
        }
        bagCapacity_ = newCapacity;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin()+index);
        say("Bolsa ampliada para " + std::to_string(bagCapacity_) + " espacos.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_HonorPoint" || item.scriptAction == "ItemUse_FightingPoint") {
        // ItemEffect.lua increments VAL_STR (honor) or MAXENERGY (chaos)
        // on the Medal of Valor (3849), and leaves both untouched if absent.
        auto medal = std::find_if(bag_.begin(), bag_.end(), [](const Stack& value) {
            return value.id == 3849 && value.count > 0;
        });
        if (medal == bag_.end()) {
            say("Voce precisa carregar um Medal of Valor para usar este bonus.");
            return;
        }
        const bool honor = item.scriptAction == "ItemUse_HonorPoint";
        const int current = honor ? medal->extra.str : medal->maxEnergy;
        // The offline save decoder bounds item attributes to 1,000,000.
        // Keep the book and medal unchanged if +100 would make this record
        // unreadable after the next load.
        if (current < 0 || current > 999900) {
            say("O Medal of Valor atingiu o limite de atributo persistido no modo offline.");
            return;
        }
        std::vector<Stack> preview = bag_;
        auto previewMedal = std::find_if(preview.begin(), preview.end(), [](const Stack& value) {
            return value.id == 3849 && value.count > 0;
        });
        if (honor) previewMedal->extra.str += 100; // ITEMATTR_VAL_STR
        else previewMedal->maxEnergy += 100;       // ITEMATTR_MAXENERGY
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        bag_.swap(preview);
        assignItemUids();
        say(std::string("Medal of Valor: +100 ") + (honor ? "honor." : "chaos."));
        save(savePath());
        return;
    }
    if (((item.id >= 1000 && item.id <= 1003) || item.id == 8909) &&
        item.scriptAction == "Blueprint") {
        using item_production::Kind;
        // PORT_APPROX: conserva a distribuição do math.random historico, mas
        // Play::roll usa o gerador offline atual e nao a mesma sequencia DX9.
        const int kindRoll = roll(100) + 1;
        const Kind kind = kindRoll <= 40 ? Kind::Manufacturing :
                          kindRoll <= 61 ? Kind::Crafting : Kind::Cooking;
        const int energyRoll = roll(100) + 1;
        int energy = 1;
        if (item.id == 1001) energy = energyRoll <= 5 ? 4 : energyRoll <= 30 ? 3 : 2;
        else if (item.id == 1002) energy = energyRoll <= 5 ? 6 : energyRoll <= 30 ? 5 : 4;
        else if (item.id == 1003) {
            if (energyRoll <= 5 && kind != Kind::Crafting) energy = 8;
            else if (energyRoll >= 6 && energyRoll <= 30) energy = 7;
            else energy = 6;
        }
        const int totalWeight = itemProductionRules_.totalWeight(kind, energy);
        const auto* recipe = totalWeight > 0 ?
            itemProductionRules_.recipeForRoll(kind, energy, roll(totalWeight) + 1) : nullptr;
        const int blueprintId = kind == Kind::Manufacturing ? 2300 :
                                kind == Kind::Crafting ? 2301 : 2302;
        const auto blueprintInfo = items_.find(blueprintId);
        if (!recipe || blueprintInfo == items_.end() || blueprintInfo->second.maxStack != 1) {
            say(item.name + ": tabela ou item de blueprint historico indisponivel; item preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        const std::size_t generatedSlot = preview.size();
        if (!inventory_rules::insert(preview, blueprintId, 1, blueprintInfo->second.maxStack, bagCapacity_) ||
            preview.size() <= generatedSlot || preview[generatedSlot].id != blueprintId) {
            say("Libere espaco na bolsa para gerar o blueprint; item preservado.");
            return;
        }
        Stack& output = preview[generatedSlot];
        output.extra.str = recipe->materials[0]; // ITEMATTR_VAL_STR
        output.extra.con = recipe->materials[1]; // ITEMATTR_VAL_CON
        output.extra.dex = recipe->materials[2]; // ITEMATTR_VAL_DEX
        output.extra.agi = item_production::canonicalOutputItemId(*recipe); // ITEMATTR_VAL_AGI
        output.maxDurability = energy <= 3 ? energy : roll(energy * 10 - 9) + 10; // MAXURE
        output.durability = energy; // URE
        output.maxEnergy = recipe->itemLevel / 10 + 100; // MAXENERGY
        output.energy = 10 - recipe->itemLevel / 10; // ENERGY
        int sta = roll(10) + 1;
        if (kind == Kind::Crafting) sta = std::max(10, sta * 5); // 0.5 increments, tenths stored
        else sta *= 10;
        output.itemValStaTenths = sta;
        const int part2 = roll(18) + 3; // SetNum_Part2(3..20)
        const int part4 = roll(8) + 2;  // SetNum_Part4(2..9)
        const int part6 = (roll(3) + 1) * std::max(1, energy - 2);
        output.blueprintParts = {part2, part4, part6};
        bag_.swap(preview);
        assignItemUids();
        const char* kindName = kind == Kind::Manufacturing ? "Manufacturing" :
                               kind == Kind::Crafting ? "Crafting" : "Cooking";
        say(std::string("Blueprint de ") + kindName + " gerado: receita " + std::to_string(recipe->index) + ".");
        save(savePath());
        return;
    }
    if (item.id == 1033 && item.scriptAction == "ItemUse_TunDuiZhiXing") {
        const auto starInfo = items_.find(1034);
        if (starInfo == items_.end() || starInfo->second.maxStack != 1) {
            say("Star of Unity Chest: item 1034 ausente ou invalido; caixa preservada.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        if (!inventory_rules::insert(preview, 1034, 1, starInfo->second.maxStack, bagCapacity_)) {
            say("Libere espaco na bolsa para abrir o Star of Unity Chest.");
            return;
        }
        auto star = std::find_if(preview.rbegin(), preview.rend(), [](const Stack& value) { return value.id == 1034; });
        if (star == preview.rend()) return;
        star->quality = 4;
        star->durability = 100;   // SetItemAttr(ITEMATTR_URE, 100)
        star->maxDurability = 20100; // SetItemAttr(ITEMATTR_MAXURE, 20100)
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, 1034, 1);
        say("Star of Unity Chest aberto: voce recebeu um Star of Unity.");
        save(savePath());
        return;
    }
    const auto lifeBook = lifeSkillBooks_.find(item.id);
    if (lifeBook != lifeSkillBooks_.end()) {
        const LifeSkillBookRule& rule = lifeBook->second;
        if (usingTicketBoat_) {
            say("Nao use livros de habilidade profissional enquanto estiver navegando.");
            return;
        }
        if (shopOpen_) {
            say("Feche a loja antes de usar um livro de habilidade profissional.");
            return;
        }
        const int currentLevel = lifeSkills_.count(rule.skillId) ? lifeSkills_.at(rule.skillId) : 0;
        if (currentLevel != rule.expectedLevel) {
            say("O livro exige nivel profissional atual " + std::to_string(rule.expectedLevel) + ".");
            return;
        }
        if (rule.prerequisiteSkill > 0) {
            const int prerequisiteLevel = lifeSkills_.count(rule.prerequisiteSkill) ? lifeSkills_.at(rule.prerequisiteSkill) : 0;
            if (prerequisiteLevel < rule.prerequisiteLevel) {
                const auto prerequisite = skillRules_.find(rule.prerequisiteSkill);
                const std::string name = prerequisite == skillRules_.end() ?
                    std::to_string(rule.prerequisiteSkill) : prerequisite->second.name;
                say("Aprenda " + name + " nivel " + std::to_string(rule.prerequisiteLevel) + " primeiro.");
                return;
            }
        }
        if (lifeSkillPoints_ < 1) {
            say("Falta 1 ponto de habilidade profissional para ler este livro.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        if (rule.rewardItem > 0) {
            const auto reward = items_.find(rule.rewardItem);
            if (reward == items_.end() || reward->second.maxStack != 1 ||
                !inventory_rules::insert(preview, rule.rewardItem, 1, reward->second.maxStack, bagCapacity_)) {
                say("Nao ha espaco ou dados historicos para o material de iniciante; livro preservado.");
                return;
            }
            auto granted = std::find_if(preview.rbegin(), preview.rend(), [&](const Stack& value) {
                return value.id == rule.rewardItem;
            });
            if (granted == preview.rend()) return;
            granted->extra.str = 1; // ITEMATTR_VAL_STR
            granted->maxEnergy = 10000;
            granted->energy = 1;
        }
        bag_.swap(preview);
        lifeSkills_[rule.skillId] = rule.addLevel;
        --lifeSkillPoints_;
        assignItemUids();
        say("Aprendeu habilidade profissional nivel " + std::to_string(rule.addLevel) + ".");
        save(savePath());
        return;
    }
    const auto book = skillBooks_.find(item.id);
    if (book != skillBooks_.end()) {
        const int skillId = book->second;
        const auto ruleIt = skillRules_.find(skillId);
        if (ruleIt == skillRules_.end()) {
            say("Livro sem regra de skill: " + item.name + ".");
            return;
        }
        const SkillRule& rule = ruleIt->second;
        if (learnedSkills_.count(skillId)) {
            say("Voce ja aprendeu " + rule.name + ".");
            return;
        }
        if (!rule.jobs.empty() && std::find(rule.jobs.begin(), rule.jobs.end(), job_) == rule.jobs.end()) {
            say(rule.name + " nao pertence a classe " + std::string(jobName(job_)) + ".");
            return;
        }
        if (player_.level < rule.level) {
            say(rule.name + " exige nivel " + std::to_string(rule.level) + ".");
            return;
        }
        // find, nao operator[]: o operator[] criava uma entrada de nivel 0 para
        // o pre-requisito, e castSkill (que testa count) passava a tratar a
        // skill como aprendida sem o livro.
        if (rule.prerequisite > 0) {
            const auto have = learnedSkills_.find(rule.prerequisite);
            if (have == learnedSkills_.end() || have->second < rule.prerequisiteLevel) {
                const auto prior = skillRules_.find(rule.prerequisite);
                const std::string priorName = prior == skillRules_.end() ? std::to_string(rule.prerequisite) : prior->second.name;
                say("Primeiro aprenda " + priorName + " nivel " + std::to_string(rule.prerequisiteLevel) + ".");
                return;
            }
        }
        if (tp_ < rule.points) {
            say("Faltam pontos de skill para " + rule.name + ".");
            return;
        }
        tp_ -= rule.points;
        learnedSkills_[skillId] = 1;
        stack.count -= 1;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Aprendeu " + rule.name +
            (inferredSkillBooks_.count(item.id) ? " (mapeamento PORT_APPROX pelo nome historico)." : "."));
        // Learning is separate from equipping a hotkey; the player drags it.
        save(savePath());
        return;
    }
    // 05-AY: ticket = transicao de mapa, nunca se comporta como bau.
    // O item so sai da bolsa APOS o frontend realizar a transicao.
    if (item.scriptAction == "Ticket") {
        const auto destination = item_tickets::destinationFor(item.id,item.scriptAction);
        if (!destination) {
            say("Ticket sem destino historico validado: " + item.name + ".");
            return;
        }
        const item_tickets::Conditions conditions{player_.level,
            static_cast<float>(player_.hp),static_cast<float>(player_.maxHp),
            player_.homeY,player_.homeX,usingTicketBoat_,worldMap_};
        switch (item_tickets::check(conditions)) {
            case item_tickets::Rejection::Boat:
                say("Nao use tickets enquanto estiver no barco."); return;
            case item_tickets::Rejection::Health:
                say("Ticket exige pelo menos 50% de HP e SP."); return;
            case item_tickets::Rejection::ProhibitedMap:
                say("Este mapa nao permite usar tickets."); return;
            case item_tickets::Rejection::InvalidLevel:
                say("Nivel invalido para utilizar ticket."); return;
            case item_tickets::Rejection::None: break;
        }
        pendingTicketTravel_ = TicketTravel{stack.uid,stack.id,
            destination->city,destination->map,destination->x,destination->y};
        say("Ticket: viajando para " + pendingTicketTravel_->city + ".");
        return;
    }
    if (item.scriptAction == "ItemUse_wxlh" && item.id == 240) {
        const auto fairy = items_.find(183); // SI=183, SN=1, SE=15 in variable.lua.
        if (fairy == items_.end() || fairy->second.type != 59 || fairy->second.maxStack != 1) {
            say(item.name + ": Fairy of Life sem dados historicos; item preservado.");
            return;
        }
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 1) {
            say(item.name + ": exige um espaco livre; item preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        const std::size_t fairySlot = preview.size();
        if (!inventory_rules::insert(preview, 183, 1, fairy->second.maxStack, bagCapacity_) ||
            preview.size() <= fairySlot || preview[fairySlot].id != 183) {
            say(item.name + ": nao foi possivel criar a Fairy of Life; item preservado.");
            return;
        }
        Stack& createdFairy = preview[fairySlot];
        createdFairy.quality = 15;
        createdFairy.maxDurability = fairy->second.maxDurability;
        createdFairy.maxEnergy = fairy->second.maxEnergy;
        // ItemUse_wxlh fixes SA1..SA5 (5 each) on the first five nonzero
        // attributes of the MakeItem result. The offline fairy model stores
        // these five life attributes directly.
        createdFairy.fairyStats = {5,5,5,5,5};
        bag_.swap(preview);
        assignItemUids();
        say("Abriu Reincarnation Pagoda e recebeu Fairy of Life com os cinco atributos historicos em 5.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_Egg") {
        const auto fairy = items_.find(item.fairyEggTargetId);
        if (item.fairyEggTargetId <= 0 || fairy == items_.end() || fairy->second.type != 59 ||
            fairy->second.maxStack != 1) {
            say(item.name + ": saida da fada sem dados historicos validos; ovo preservado.");
            return;
        }
        const int freeSlots = bagCapacity_ - static_cast<int>(bag_.size());
        if (freeSlots < 2) {
            say("Precisa de pelo menos dois espacos livres para abrir " + item.name + ".");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumedEgg = preview[static_cast<std::size_t>(index)];
        if (--consumedEgg.count <= 0) preview.erase(preview.begin() + index);
        const std::size_t fairySlot = preview.size();
        if (!inventory_rules::insert(preview, item.fairyEggTargetId, 1,
                                     fairy->second.maxStack, bagCapacity_) ||
            preview.size() <= fairySlot || preview[fairySlot].id != item.fairyEggTargetId) {
            say("Nao foi possivel criar a fada historica; ovo preservado.");
            return;
        }
        Stack& createdFairy = preview[fairySlot];
        createdFairy.quality = 4; // MakeItem(role, fairyId, 1, 4)
        createdFairy.maxDurability = fairy->second.maxDurability;
        createdFairy.maxEnergy = fairy->second.maxEnergy;
        // The Egg callback proves the output ID and quality. The server-side
        // MakeItem initializer is not present in the offline script package;
        // current URE/ENERGY and random fairy attributes stay at zero.
        bag_.swap(preview);
        assignItemUids();
        say("Abriu " + item.name + " e recebeu " + fairy->second.name + ".");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_JNJLD" && item.id == 1015) {
        static constexpr int fairyIds[] = {183,184,185,186,187,188,189,190,191,199};
        const int freeSlots = bagCapacity_ - static_cast<int>(bag_.size());
        const int fairyId = fairyIds[static_cast<std::size_t>(roll(10))];
        const auto fairy = items_.find(fairyId);
        if (freeSlots < 1 || fairy == items_.end() || fairy->second.type != 59 ||
            fairy->second.maxStack != 1) {
            say(item.name + ": falta espaco ou dados historicos da fada; ovo preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumedEgg = preview[static_cast<std::size_t>(index)];
        if (--consumedEgg.count <= 0) preview.erase(preview.begin() + index);
        const std::size_t fairySlot = preview.size();
        if (!inventory_rules::insert(preview, fairyId, 1, fairy->second.maxStack, bagCapacity_) ||
            preview.size() <= fairySlot || preview[fairySlot].id != fairyId) {
            say("Nao foi possivel criar a fada historica; ovo preservado.");
            return;
        }
        Stack& createdFairy = preview[fairySlot];
        createdFairy.quality = 4; // MakeItem(role, fairyId, 1, 4)
        createdFairy.maxDurability = fairy->second.maxDurability;
        createdFairy.maxEnergy = fairy->second.maxEnergy;
        const int skillGroup = roll(3) + 1; // Lua: math.random(1, 3)
        createdFairy.fairySkills = {skillGroup,1,skillGroup+1,1,skillGroup+2,1};
        // MakeItem's randomized base attributes/stamina remain PORT_APPROX;
        // the three historical fairy skill parameter pairs are exact.
        bag_.swap(preview);
        assignItemUids();
        say("Abriu " + item.name + " e recebeu " + fairy->second.name + " (skills historicas registradas).");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_NSDXB" && item.id == 1016) {
        static constexpr int fixedRewards[] = {619,625,628,631,634,674,698,701};
        if (bagCapacity_ - static_cast<int>(bag_.size()) < 1) {
            say("Goddess's Pouch precisa de um espaco livre; item preservado.");
            return;
        }
        const int star = roll(12) + 1; // Lua math.random(1,12)
        const int rewardId = star <= 8 ? fixedRewards[static_cast<std::size_t>(star - 1)] :
                                         2530 + roll(19); // Lua math.random(2530,2548)
        const auto reward = items_.find(rewardId);
        if (reward == items_.end() || reward->second.maxStack != 1) {
            say("Goddess's Pouch: recompensa historica indisponivel; item preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumedPouch = preview[static_cast<std::size_t>(index)];
        if (--consumedPouch.count <= 0) preview.erase(preview.begin() + index);
        const std::size_t rewardSlot = preview.size();
        if (!inventory_rules::insert(preview, rewardId, 1, reward->second.maxStack, bagCapacity_) ||
            preview.size() <= rewardSlot || preview[rewardSlot].id != rewardId) {
            say("Goddess's Pouch: nao foi possivel criar a recompensa; item preservado.");
            return;
        }
        preview[rewardSlot].quality = 4; // MakeItem(role, rewardId, 1, 4)
        preview[rewardSlot].maxDurability = reward->second.maxDurability;
        preview[rewardSlot].maxEnergy = reward->second.maxEnergy;
        bag_.swap(preview);
        assignItemUids();
        say("Goddess's Pouch: recebeu " + reward->second.name + ". Aviso global original nao reproduzido.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_CZZX" && (item.id == 1009 || item.id == 1013)) {
        const int rewardId = item.id == 1009 ? 1010 : 0;
        const auto reward = rewardId > 0 ? items_.find(rewardId) : items_.end();
        if (rewardId > 0 && (reward == items_.end() ||
            bagCapacity_ - static_cast<int>(bag_.size()) < 1)) {
            say(item.name + ": falta espaco ou dados para Withered Heart; item preservado.");
            return;
        }
        const bool refresh = itemInnocenceState_.remaining > 0.f;
        ItemAllAttributeState nextState = itemInnocenceState_;
        if (!refresh) {
            const auto gear = gearBonuses();
            const std::array<int,5> current = {
                bstr_ + gear.str + itemStrengthState_.bonus + itemPotionStrengthState_.bonus,
                bagi_ + gear.agi + itemAgilityState_.bonus + itemPotionAgilityState_.bonus,
                bcon_ + gear.con + itemConstitutionState_.bonus + itemPotionConstitutionState_.bonus,
                bdex_ + gear.dex + itemAccuracyState_.bonus + itemPotionAccuracyState_.bonus,
                bsta_ + gear.sta + foodStates_[2].level * 5 + itemSpiritState_.bonus + itemPotionSpiritState_.bonus
            };
            for (std::size_t stat=0; stat<current.size(); ++stat)
                nextState.bonuses[stat] = static_cast<int>(std::floor(std::max(0,current[stat]) * 0.30));
        }
        nextState.remaining = 30.f;
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        if (rewardId > 0) {
            const std::size_t rewardSlot = preview.size();
            if (!inventory_rules::insert(preview, rewardId, 1, reward->second.maxStack, bagCapacity_) ||
                preview.size() <= rewardSlot || preview[rewardSlot].id != rewardId) {
                say(item.name + ": nao foi possivel entregar Withered Heart; item preservado.");
                return;
            }
            preview[rewardSlot].quality = 4; // GiveItem(role, 0, 1010, 1, 4)
        }
        bag_.swap(preview);
        itemInnocenceState_ = nextState;
        if (sheetReady_) applyAttrs(false);
        else recomputeGear();
        assignItemUids();
        if (rewardId > 0) onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        say(item.name + ": todos os atributos +30% por 30 s." +
            (rewardId > 0 ? " Withered Heart recebido." : ""));
        save(savePath());
        return;
    }
    if (item.scriptAction == "FairyRation") {
        if (usingTicketBoat_) {
            say("Nao alimente a fada enquanto navega.");
            return;
        }
        if (item.type != 57 || item.fairyRationAmount <= 0) {
            say(item.name + ": quantidade de racao historica indisponivel; item preservado.");
            return;
        }
        const auto equippedFairy = worn_.find(16);
        const auto equippedUid = wornUid_.find(16);
        if (equippedFairy == worn_.end() || equippedUid == wornUid_.end()) {
            say("Equipe uma fada no slot de pet antes de usar a ração.");
            return;
        }
        const auto fairyDefinition = items_.find(equippedFairy->second);
        if (fairyDefinition == items_.end() || fairyDefinition->second.type != 59) {
            say("O slot de pet nao contem uma fada reconhecida; ração preservada.");
            return;
        }
        if ((equippedFairy->second == 7125 || equippedFairy->second == 7126) && item.id < 5644) {
            say("Esta fada nao pode receber esta ração; item preservado.");
            return;
        }
        auto fairyStack = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& candidate) {
            return candidate.uid == equippedUid->second && candidate.id == equippedFairy->second &&
                   candidate.count == 1;
        });
        if (fairyStack == bag_.end()) {
            say("A instancia da fada equipada nao foi encontrada; ração preservada.");
            return;
        }
        const int maximum = fairyStack->maxDurability > 0 ? fairyStack->maxDurability :
                            fairyDefinition->second.maxDurability;
        if (maximum <= 0 || fairyStack->durability >= maximum) {
            say("A stamina da fada ja esta cheia ou sem dados historicos; ração preservada.");
            return;
        }
        const int previousStamina = std::max(0, fairyStack->durability);
        const long long restored = static_cast<long long>(item.fairyRationAmount) * 50LL;
        fairyStack->maxDurability = maximum;
        fairyStack->durability = static_cast<int>(std::min<long long>(maximum,
            static_cast<long long>(previousStamina) + restored));
        const int current = fairyStack->durability;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Usou " + item.name + ": stamina da fada " + std::to_string(previousStamina) +
            " -> " + std::to_string(current) + "/" + std::to_string(maximum) + ".");
        save(savePath());
        return;
    }
    if ((item.scriptAction == "AddFairyBody" || item.scriptAction == "AddFunnySkill") && item.type == 34) {
        if (usingTicketBoat_) {
            say("Nao aprenda skills de fada enquanto navega; livro preservado.");
            return;
        }
        if (item.learnedSkillId <= 0 || item.learnedSkillLevel <= 0 ||
            skillRules_.count(item.learnedSkillId) == 0) {
            say(item.name + ": regra de aprendizado historica indisponivel; livro preservado.");
            return;
        }
        const auto learned = learnedSkills_.find(item.learnedSkillId);
        const int currentLevel = learned == learnedSkills_.end() ? 0 : learned->second;
        if (currentLevel >= item.learnedSkillLevel) {
            say("Voce ja possui essa skill no mesmo nivel ou superior; livro preservado.");
            return;
        }
        learnedSkills_[item.learnedSkillId] = item.learnedSkillLevel;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Aprendeu " + skillRules_.at(item.learnedSkillId).name + " nivel " +
            std::to_string(item.learnedSkillLevel) + ".");
        save(savePath());
        return;
    }
    if (item.scriptAction == "AddFairySkill") {
        if (usingTicketBoat_) {
            say("Nao ensine skills da fada enquanto navega; livro preservado.");
            return;
        }
        if (item.type != 58 || item.fairySkillId <= 0 || item.fairySkillLevel <= 0) {
            say(item.name + ": regra historica indisponivel; livro preservado.");
            return;
        }
        const auto equippedFairy = worn_.find(16);
        const auto equippedUid = wornUid_.find(16);
        if (equippedFairy == worn_.end() || equippedUid == wornUid_.end()) {
            say("Equipe uma fada no slot de pet antes de usar o livro.");
            return;
        }
        const auto fairyDefinition = items_.find(equippedFairy->second);
        if (fairyDefinition == items_.end() || fairyDefinition->second.type != 59) {
            say("O slot de pet nao contem uma fada reconhecida; livro preservado.");
            return;
        }
        const auto fairyStack = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& candidate) {
            return candidate.uid == equippedUid->second && candidate.id == equippedFairy->second &&
                   candidate.count == 1;
        });
        if (fairyStack == bag_.end()) {
            say("A instancia da fada equipada nao foi encontrada; livro preservado.");
            return;
        }
        std::array<int, 3> skillIds{fairyStack->fairySkills[0], fairyStack->fairySkills[2],
                                    fairyStack->fairySkills[4]};
        std::array<int, 3> skillLevels{fairyStack->fairySkills[1], fairyStack->fairySkills[3],
                                      fairyStack->fairySkills[5]};
        int targetSlot = -1;
        bool prerequisiteOK = false;
        for (std::size_t slot = 0; slot < skillIds.size(); ++slot) {
            if (skillIds[slot] == item.fairySkillId) {
                if (item.fairySkillLevel <= skillLevels[slot]) {
                    say("A fada ja aprendeu esse nivel; livro preservado.");
                    return;
                }
                if (item.fairySkillLevel != skillLevels[slot] + 1) {
                    say("Aprenda o nivel anterior dessa skill primeiro; livro preservado.");
                    return;
                }
                targetSlot = static_cast<int>(slot);
                prerequisiteOK = true;
                break;
            }
        }
        if (targetSlot < 0) {
            if (item.fairySkillLevel != 1) {
                say("Aprenda o livro de nivel 1 dessa skill primeiro; livro preservado.");
                return;
            }
            const auto empty = std::find(skillIds.begin(), skillIds.end(), 0);
            if (empty == skillIds.end()) {
                say("A fada ja ocupa os tres espacos de skill; livro preservado.");
                return;
            }
            targetSlot = static_cast<int>(std::distance(skillIds.begin(), empty));
            prerequisiteOK = true;
        }
        if (!prerequisiteOK || targetSlot < 0 || targetSlot >= 3) return;
        fairyStack->fairySkills[static_cast<std::size_t>(targetSlot * 2)] = item.fairySkillId;
        fairyStack->fairySkills[static_cast<std::size_t>(targetSlot * 2 + 1)] = item.fairySkillLevel;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Fada aprendeu " + item.name + ".");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_BZ") {
        // The historical callback only rejects use while sailing; on land it
        // has no additional Lua state/effect. Native visual presentation, if
        // any, is deliberately left to the item/renderer systems.
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto navega; item preservado.");
            return;
        }
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + " usado.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "FairySkillRemoval" && item.id >= 9609 && item.id <= 9611) {
        if (item.type != 34) {
            say(item.name + ": livro historico invalido; item preservado.");
            return;
        }
        const auto equippedFairy = worn_.find(16);
        const auto equippedUid = wornUid_.find(16);
        if (equippedFairy == worn_.end() || equippedUid == wornUid_.end()) {
            say("Equipe uma fada no slot de pet antes de apagar uma skill.");
            return;
        }
        const auto fairyDefinition = items_.find(equippedFairy->second);
        if (fairyDefinition == items_.end() || fairyDefinition->second.type != 59) {
            say("O slot de pet nao contem uma fada reconhecida; livro preservado.");
            return;
        }
        auto fairyStack = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& candidate) {
            return candidate.uid == equippedUid->second && candidate.id == equippedFairy->second &&
                   candidate.count == 1;
        });
        if (fairyStack == bag_.end()) {
            say("A instancia da fada equipada nao foi encontrada; livro preservado.");
            return;
        }
        const std::size_t slot = static_cast<std::size_t>(item.id - 9609);
        const std::size_t skillField = slot * 2;
        if (fairyStack->fairySkills[skillField] <= 0) {
            say("O slot " + std::to_string(slot + 1) + " da fada esta vazio; livro preservado.");
            return;
        }
        // FairySkillRemoval.lua clears only the selected skill/level pair in
        // the equipped fairy's forge parameters. Player skill rules/effects
        // remain untouched.
        fairyStack->fairySkills[skillField] = 0;
        fairyStack->fairySkills[skillField + 1] = 0;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Skill removida do slot " + std::to_string(slot + 1) + " da fada.");
        save(savePath());
        return;
    }
    if ((item.scriptAction == "FairyFruit" || item.scriptAction == "FairyFruitImprvd") &&
        item.fairyFruitAttribute >= 0 && item.fairyFruitAttribute < 5 && item.fairyFruitLevels > 0) {
        if (usingTicketBoat_) {
            say("Nao use frutas de fada enquanto navega; item preservado.");
            return;
        }
        const auto equippedFairy = worn_.find(16);
        const auto equippedUid = wornUid_.find(16);
        if (equippedFairy == worn_.end() || equippedUid == wornUid_.end()) {
            say("Equipe uma fada no slot de pet antes de usar a fruta.");
            return;
        }
        const auto fairyDefinition = items_.find(equippedFairy->second);
        if (fairyDefinition == items_.end() || fairyDefinition->second.type != 59) {
            say("O slot de pet nao contem uma fada reconhecida; fruta preservada.");
            return;
        }
        const bool angela = equippedFairy->second == 7125 || equippedFairy->second == 7126;
        auto fairy = std::find_if(bag_.begin(), bag_.end(), [&](const Stack& candidate) {
            return candidate.uid == equippedUid->second && candidate.id == equippedFairy->second &&
                   candidate.count == 1;
        });
        if (fairy == bag_.end()) {
            say("A instancia da fada equipada nao foi encontrada; fruta preservada.");
            return;
        }
        const int fairyLevel = std::accumulate(fairy->fairyStats.begin(), fairy->fairyStats.end(), 0);
        if ((!item.fairyFruitImproved && angela) || fairyLevel >= fairyMaximumLevel_) {
            say("Esta fada nao pode ganhar mais niveis com " + item.name + "; fruta preservada.");
            return;
        }
        if (item.fairyFruitImproved) {
            const bool improvedLevel = fairyLevel >= fairyNormalFruitMaximum_ &&
                                       fairyLevel < fairyImprovedFruitMaximum_;
            if (!improvedLevel && !angela) {
                say("Esta fruta exige a faixa historica de nivel melhorado da fada; item preservado.");
                return;
            }
        } else if (fairyLevel >= fairyNormalFruitMaximum_) {
            say("A fada atingiu o limite de frutas normais; fruta preservada.");
            return;
        }
        if (fairy->maxEnergy <= 0 || fairy->energy < fairy->maxEnergy) {
            say("A fada precisa estar com o crescimento completo antes de usar a fruta.");
            return;
        }

        const int attribute = item.fairyFruitAttribute;
        const int currentAttribute = fairy->fairyStats[static_cast<std::size_t>(attribute)];
        const double roundedGrowth = std::floor((1.0 + std::pow(fairyLevel / 10.0, 3.0)) * 10.0) / 10.0;
        const double divisor = roundedGrowth * std::max(0.01, 1.0 - currentAttribute * 0.05);
        const double successChance = divisor > 0.0 ? 1.0 / divisor : 1.0;
        const long long successThreshold = std::min<long long>(1000000000LL,
            static_cast<long long>(std::floor(successChance * 1000000000.0)));
        // Percentage_Random draws an integer in [0, 1e9] and succeeds when
        // it is <= chance*1e9. Use the full offline LCG state; Play::roll's
        // 16-bit projection is too narrow for this historical 1e9 range.
        const unsigned oldSeed = seed_;
        seed_ = seed_ * 1103515245u + 12345u;
        const long long fairyRoll = static_cast<long long>(seed_ & 0x7fffffffu) % 1000000001LL;
        const bool levelUp = fairyRoll <= successThreshold;
        std::vector<Stack> preview = bag_;
        auto changedFairy = std::find_if(preview.begin(), preview.end(), [&](const Stack& candidate) {
            return candidate.uid == equippedUid->second && candidate.id == equippedFairy->second;
        });
        if (changedFairy == preview.end()) return;
        int newLevel = fairyLevel;
        if (levelUp) {
            int& stat = changedFairy->fairyStats[static_cast<std::size_t>(attribute)];
            if (stat > 99 - item.fairyFruitLevels || fairyLevel + item.fairyFruitLevels > fairyMaximumLevel_) {
                say("O atributo da fada excederia o limite persistido; fruta preservada.");
                return;
            }
            stat += item.fairyFruitLevels;
            newLevel += item.fairyFruitLevels;
            changedFairy->maxEnergy = std::min(6480, 240 * newLevel);
            changedFairy->maxDurability = std::min(32000,
                changedFairy->maxDurability + 1000 * item.fairyFruitLevels);
            if (changedFairy->maxDurability == 25000) changedFairy->maxDurability = 25001;
            changedFairy->energy = 0;
        } else {
            changedFairy->energy /= 2; // FairyLevel(..., False=0): growth is halved on failure.
        }
        Stack& fruit = preview[static_cast<std::size_t>(index)];
        if (--fruit.count <= 0) preview.erase(preview.begin() + index);
        const int oldNextItemUid = nextItemUid_;
        bag_.swap(preview);
        assignItemUids();
        const std::string saveFile = savePath();
        if (!saveFile.empty() && !save(saveFile)) {
            bag_.swap(preview);
            nextItemUid_ = oldNextItemUid;
            seed_ = oldSeed;
            say("Fruta de fada: falha ao salvar; fada, fruta e crescimento foram preservados.");
            return;
        }
        if(levelUp)applyAttrs(false);
        say(levelUp ? "Fada subiu para o nivel " + std::to_string(newLevel) + "; crescimento reiniciado."
                    : "A tentativa falhou; o crescimento da fada caiu pela metade.");
        return;
    }
    if (item.scriptAction == "ItemUse_BirthCake" && (item.id == 3240 || item.id == 3338)) {
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto navega; item preservado.");
            return;
        }
        constexpr int maxAttributePoints = 1000000000;
        // PORT_APPROX: safe offline bound; the historical source does not
        // expose the server-side AP storage ceiling. Revisit for D3 parity.
        if (ap_ > maxAttributePoints - 3) {
            say("Pontos de atributo no limite; " + item.name + " foi preservado.");
            return;
        }
        ap_ += 3; // ItemUse_BirthCake: Attr_ap(role) + 3.
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": +3 pontos de atributo. Total: " + std::to_string(ap_) + ".");
        save(savePath());
        return;
    }
    if (item.scriptAction == "Book_StatReset" && item.id == 997) {
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto navega; item preservado.");
            return;
        }
        if (bstr_ < 5 || bcon_ < 5 || bagi_ < 5 || bdex_ < 5 || bsta_ < 5) {
            say("Atributos abaixo do minimo historico; " + item.name + " foi preservado.");
            return;
        }
        const long long refunded = static_cast<long long>(bstr_) + bcon_ + bagi_ + bdex_ + bsta_ - 25;
        const long long updatedPoints = static_cast<long long>(ap_) + refunded;
        // AP is persisted as a signed int and the offline Birth Cake handler
        // uses this same safe ceiling. Never consume the book on overflow.
        if (refunded < 0 || updatedPoints > 1000000000LL) {
            say("Pontos de atributo fora do limite persistido; " + item.name + " foi preservado.");
            return;
        }
        ap_ = static_cast<int>(updatedPoints);
        bstr_ = bcon_ = bagi_ = bdex_ = bsta_ = 5;
        applyAttrs(false);
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": atributos restaurados ao minimo; " + std::to_string(refunded) +
            " pontos devolvidos.");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_CZHe" && item.id == 854) {
        // ItemEffect.lua rejects at 999,900,000G, then calls AddMoney(1,000,000).
        if (gold_ >= 999900000) {
            say("Seu ouro esta no limite para usar esta nota; item preservado.");
            return;
        }
        gold_ = static_cast<int>(static_cast<long long>(gold_) + 1000000LL);
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": +1.000.000 ouro. Total: " + std::to_string(gold_) + ".");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_HappyBook" && (item.id == 853 || item.id == 2989)) {
        const int level = player_.level;
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto navega; item preservado.");
            return;
        }
        if (level < 1 || level >= 86 || !experience_.valid()) {
            say(level >= 86 ? "Seu nivel e alto demais para usar " + item.name + "." :
                "Tabela historica de experiencia indisponivel; item preservado.");
            return;
        }
        const std::int64_t currentThreshold = experience_.cumulative(level);
        const std::int64_t nextThreshold = experience_.cumulative(level + 1);
        if (currentThreshold < 0 || nextThreshold <= currentThreshold || exp_ < 0) {
            say("Estado de experiencia invalido; " + item.name + " foi preservado.");
            return;
        }
        const std::int64_t currentCumulative = currentThreshold + exp_;
        double targetCumulative = static_cast<double>(currentCumulative);
        if (level <= 9) {
            targetCumulative = static_cast<double>(experience_.cumulative(level + 5));
        } else if (level <= 29) {
            targetCumulative = static_cast<double>(experience_.cumulative(level + 3));
        } else if (level <= 59) {
            targetCumulative = static_cast<double>(nextThreshold);
        } else {
            const double fraction = level <= 75 ? 0.5 : 0.3;
            targetCumulative += static_cast<double>(nextThreshold - currentThreshold) * fraction + 10.0;
            if (level == 79 && targetCumulative > static_cast<double>(nextThreshold)) {
                targetCumulative = static_cast<double>(nextThreshold) +
                    (targetCumulative - static_cast<double>(nextThreshold)) * 0.02;
            }
        }
        if (targetCumulative < static_cast<double>(currentCumulative)) {
            say("Resultado de experiencia invalido; " + item.name + " foi preservado.");
            return;
        }
        // SetChaAttrI historically receives cumulative DEXP; saves in this
        // client store EXP relative to the current level, so translate before
        // applying each historical level-up reward.
        const std::int64_t target = static_cast<std::int64_t>(targetCumulative);
        std::int64_t remaining = target - currentThreshold;
        int resultingLevel = level;
        while (resultingLevel < 100) {
            const int needed = experience_.needed(resultingLevel);
            if (needed <= 0 || remaining < needed) break;
            remaining -= needed;
            ++resultingLevel;
        }
        if (remaining > std::numeric_limits<int>::max()) {
            say("Resultado de experiencia fora do limite local; " + item.name + " foi preservado.");
            return;
        }
        for (int reached = level + 1; reached <= resultingLevel; ++reached) {
            ap_ += progression::attrPointAt(reached);
            tp_ += progression::skillPointAt(reached);
        }
        player_.level = resultingLevel;
        if (resultingLevel != level && sheetReady_) applyAttrs(false);
        if (resultingLevel != level) {
            player_.hp = player_.maxHp;
            player_.homeY = player_.homeX;
        }
        exp_ = static_cast<int>(remaining);
        expNext_ = experience_.needed(player_.level);
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": avancou " + std::to_string(resultingLevel - level) + " nivel(is); agora nivel " +
            std::to_string(player_.level) + ".");
        save(savePath());
        return;
    }
    const bool aprilExperience = item.scriptAction == "ItemUse_Foolish" && item.id == 2953;
    const bool growthFastFood = item.scriptAction == "ItemUse_CZKC" && item.id == 992;
    if (aprilExperience || growthFastFood) {
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto navega; item preservado.");
            return;
        }
        const int level = player_.level;
        const int minimumLevel = growthFastFood ? 86 : 1;
        if (level < minimumLevel || level >= 100 || !experience_.valid()) {
            if (growthFastFood && level < minimumLevel) {
                say(item.name + " exige nivel 86 ou superior; item preservado.");
                return;
            }
            say("Estado ou tabela de experiencia indisponivel; " + item.name + " foi preservado.");
            return;
        }
        const int nextLevelExp = experience_.needed(level);
        if (nextLevelExp <= 0 || exp_ < 0 || exp_ >= nextLevelExp) {
            say("Estado ou tabela de experiencia indisponivel; " + item.name + " foi preservado.");
            return;
        }
        const Foe oldPlayer = player_;
        const int oldExp = exp_, oldExpNext = expNext_, oldAp = ap_, oldTp = tp_;
        const int oldBaseMin = baseMin_, oldBaseMax = baseMax_, oldBaseDef = baseDef_;
        const int oldBaseResist = baseResist_, oldBaseLuck = baseLuck_;
        const float oldBaseMoveSpeed = baseMoveSpeed_;
        const int oldAttrMin = attrMin_, oldAttrMax = attrMax_, oldAttrDef = attrDef_;
        const int oldHrecGain = hrecGain_, oldSrecGain = srecGain_;
        const int oldSheetHp = sheetHp_, oldSheetSp = sheetSp_;
        const std::vector<Stack> oldBag = bag_;
        // Historical callbacks add 3% (April's Fool) or 1% (Growth Fast Food)
        // of the current level's DEXP interval to CEXP. SetChaAttrI stores an
        // integer in the server.
        const double fraction = growthFastFood ? 0.01 : 0.03;
        const std::int64_t increment = static_cast<std::int64_t>(
            std::floor(static_cast<double>(nextLevelExp) * fraction));
        std::int64_t remaining = static_cast<std::int64_t>(exp_) + increment;
        int resultingLevel = level;
        while (resultingLevel < 100) {
            const int needed = experience_.needed(resultingLevel);
            if (needed <= 0 || remaining < needed) break;
            remaining -= needed;
            ++resultingLevel;
        }
        if (remaining < 0 || remaining > std::numeric_limits<int>::max()) {
            say("Resultado de experiencia fora do limite local; " + item.name + " foi preservado.");
            return;
        }
        for (int reached = level + 1; reached <= resultingLevel; ++reached) {
            ap_ += progression::attrPointAt(reached);
            tp_ += progression::skillPointAt(reached);
        }
        player_.level = resultingLevel;
        if (resultingLevel != level && sheetReady_) applyAttrs(false);
        if (resultingLevel != level) {
            player_.hp = player_.maxHp;
            player_.homeY = player_.homeX;
        }
        exp_ = static_cast<int>(remaining);
        expNext_ = experience_.needed(player_.level);
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + (growthFastFood ? ": +1% da experiencia do nivel; agora nivel " :
                         ": +3% da experiencia do nivel; agora nivel ") +
            std::to_string(player_.level) + ".");
        const std::string path = savePath();
        if (!path.empty() && !save(path)) {
            player_ = oldPlayer;
            exp_ = oldExp; expNext_ = oldExpNext; ap_ = oldAp; tp_ = oldTp;
            baseMin_ = oldBaseMin; baseMax_ = oldBaseMax; baseDef_ = oldBaseDef;
            baseResist_ = oldBaseResist; baseLuck_ = oldBaseLuck; baseMoveSpeed_ = oldBaseMoveSpeed;
            attrMin_ = oldAttrMin; attrMax_ = oldAttrMax; attrDef_ = oldAttrDef;
            hrecGain_ = oldHrecGain; srecGain_ = oldSrecGain;
            sheetHp_ = oldSheetHp; sheetSp_ = oldSheetSp;
            bag_ = oldBag;
            say("Falha ao salvar; " + item.name + ", EXP e pontos foram preservados.");
            return;
        }
        return;
    }
    if (item.scriptAction == "AmplifierItem") {
        const item_amplifier::Rule* rule = item_amplifier::find(item.id);
        if (!rule) return;
        if (rule->effect == item_amplifier::Effect::FairyGrowth) {
            if (fairyGrowthBoost_.remaining > 0.f) {
                say(item.name + ": ja existe um amplificador de crescimento de fada ativo; item preservado.");
                return;
            }
            fairyGrowthBoost_.multiplierPercent = rule->multiplierPercent;
            fairyGrowthBoost_.remaining = static_cast<float>(rule->durationSeconds);
            fairyGrowthBoost_.sourceItemId = item.id;
            --stack.count;
            if (stack.count <= 0) bag_.erase(bag_.begin() + index);
            say(item.name + ": crescimento da fada 2x por " +
                std::to_string(rule->durationSeconds / 60) + " min.");
            save(savePath());
            return;
        }
        if (player_.level < rule->minimumLevel ||
            (rule->maximumLevel > 0 && player_.level > rule->maximumLevel)) {
            const std::string range = rule->maximumLevel > 0 ?
                "nivel " + std::to_string(rule->minimumLevel > 0 ? rule->minimumLevel : 1) +
                " a " + std::to_string(rule->maximumLevel) :
                "nivel " + std::to_string(rule->minimumLevel) + " ou mais";
            say(item.name + " exige " + range + "; item preservado.");
            return;
        }
        ItemBoostState& active = rule->effect == item_amplifier::Effect::Experience ?
            experienceBoost_ : dropRateBoost_;
        if (active.remaining <= 0.f || rule->multiplierPercent >= active.multiplierPercent) {
            active.multiplierPercent = std::max(active.multiplierPercent,rule->multiplierPercent);
            active.sourceItemId = item.id;
        }
        active.remaining = std::max(active.remaining,static_cast<float>(rule->durationSeconds));
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        const char* effectName = rule->effect == item_amplifier::Effect::Experience ? "EXP" : "drop";
        const int whole = active.multiplierPercent / 100;
        const int fraction = active.multiplierPercent % 100;
        const std::string multiplier = std::to_string(whole) + (fraction == 50 ? ".5" : "");
        std::string detail = ": " + multiplier + "x " + effectName +
            " por ate " + std::to_string(static_cast<int>(active.remaining / 60.f)) + " min.";
        if (rule->hasLuckyStrikeBonus) detail += " Chance de Lucky Strike nao reproduzida (PORT_APPROX).";
        say(item.name + detail);
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemUse_PartyFruit" &&
        (item.id == 849 || item.id == 6019)) {
        // Historical ItemUse_PartyFruit applies STATE_ZDSBJYGZ level 1 for
        // 900 seconds to every team member (or their boat). This offline
        // client has no team service, so the controlled character receives
        // the same 1.5x EXP state; stronger/equal boosts reject without use.
        if (experienceBoost_.remaining > 0.f &&
            experienceBoost_.multiplierPercent >= 150) {
            say(item.name + ": bonus de EXP igual ou melhor ja esta ativo; item preservado.");
            return;
        }
        experienceBoost_.multiplierPercent = 150;
        experienceBoost_.remaining = 900.f;
        experienceBoost_.sourceItemId = item.id;
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say(item.name + ": EXP x1.5 por 15 minutos (PORT_APPROX: jogador solo offline).");
        save(savePath());
        return;
    }
    // Caixas de progressao historicas 436..443 e caixas por classe 985..988.
    // 443 usa eventos 11/95; mapeamentos de classe sem regra continuam recusados.
    if (item_newbie_chests::isHistoricalClassChest(item.id,item.scriptAction)) {
        const auto result=item_newbie_chests::evaluate(item.id,item.scriptAction,
                                                      player_.level,job_,player_.id,usingTicketBoat_);
        if (!result) {
            using Failure=item_newbie_chests::Failure;
            switch (result.failure) {
                case Failure::Boat: say("Nao abra Newbie Chest enquanto estiver no barco.");break;
                case Failure::Level: say(item.name + ": nivel insuficiente.");break;
                case Failure::Job: say(item.name + ": exige promocao de classe compativel.");break;
                case Failure::CharacterType: say(item.name + ": profissao/tipo nao compativel com recompensa historica.");break;
                default: say(item.name + ": efeito historico especial ainda nao suportado; item preservado.");break;
            }
            return;
        }
        const auto& plan=result.plan;
        const int freeSlots=bagCapacity_-static_cast<int>(bag_.size());
        if (freeSlots<plan.minimumFreeSlots) {
            say(item.name + ": exige ao menos " + std::to_string(plan.minimumFreeSlots) + " espacos livres.");
            return;
        }
        std::vector<Stack> preview=bag_;
        auto& consumed=preview[static_cast<std::size_t>(index)];
        if (--consumed.count<=0) preview.erase(preview.begin()+index);
        std::vector<std::size_t> generatedSlots;
        for (const auto& reward:plan.rewards) {
            const auto found=items_.find(reward.itemId);
            if (found==items_.end() || reward.quantity<=0 ||
                !inventory_rules::insert(preview,reward.itemId,reward.quantity,
                                         found->second.maxStack,bagCapacity_)) {
                say(item.name + ": recompensa indisponivel ou espaco insuficiente; caixa preservada.");
                return;
            }
            if (reward.quality > 0) {
                const auto granted = std::find_if(preview.rbegin(),preview.rend(),[&](const Stack& value) {
                    return value.id==reward.itemId;
                });
                if (granted==preview.rend()) {
                    say(item.name + ": instancia de recompensa ausente; caixa preservada.");
                    return;
                }
                granted->quality=reward.quality;
            }
            if (reward.itemEvent==11 || reward.itemEvent==95) {
                if (reward.quantity!=1 || !found->second.chInstance || found->second.maxStack!=1 ||
                    preview.empty()) {
                    say(item.name + ": instancia historica indisponivel; caixa preservada.");
                    return;
                }
                generatedSlots.push_back(preview.size()-1);
            }
        }
        // Os UIDs temporários são calculados antes do commit para que uma
        // recompensa inválida jamais consuma a caixa. O mesmo UID semeia a
        // geração offline e fica salvo junto dos atributos resultantes.
        std::unordered_set<int> usedUids;
        for (const Stack& stack:preview) if(stack.uid>0) usedUids.insert(stack.uid);
        for (const Stack& stack:temp_) if(stack.uid>0) usedUids.insert(stack.uid);
        int candidateUid=std::max(1,nextItemUid_);
        std::size_t generatedAt=0;
        for (const auto& reward:plan.rewards) {
            if (reward.itemEvent!=11 && reward.itemEvent!=95) continue;
            const std::size_t slot=generatedSlots[generatedAt++];
            Stack& stack=preview[slot];
            while(candidateUid<=1000000000 && usedUids.count(candidateUid)) ++candidateUid;
            if(candidateUid<=0 || candidateUid>1000000000) {
                say(item.name + ": sem UID livre para a recompensa; caixa preservada.");
                return;
            }
            stack.uid=candidateUid++;
            usedUids.insert(stack.uid);
            const ItemInfo& info=items_.at(reward.itemId);
            const auto event=static_cast<item_instantiation::HistoricalEvent>(reward.itemEvent);
            const auto instance=item_instantiation::materialize(event,info.type,info.level,stack.uid,
                                                                 info.stats,info.statsMaximum,
                                                                 info.maxDurability,info.maxDurabilityRange,
                                                                 info.maxEnergy,info.maxEnergyRange);
            if(!instance.valid) {
                say(item.name + ": gerador historico ainda nao cobre uma recompensa; caixa preservada.");
                return;
            }
            stack.quality=instance.quality;
            stack.maxDurability=instance.maxDurability;
            stack.durability=instance.maxDurability;
            stack.maxEnergy=instance.maxEnergy;
            stack.energy=instance.maxEnergy;
            stack.extra+=instance.bonus;
        }
        // A transacao ja foi validada: ouro e itens so mudam juntos.
        bag_.swap(preview);
        assignItemUids();
        gold_=static_cast<int>(std::min(1000000000LL,static_cast<long long>(gold_)+plan.gold));
        for (const auto& reward:plan.rewards)
            onHistoricalEvent(MissionEventKind::GetItem,reward.itemId,reward.quantity);
        say("Abriu " + item.name + " e recebeu " + std::to_string(plan.gold) + " ouro.");
        save(savePath());
        return;
    }
    if (item.id == 1872 && item.scriptAction == "ItemUse_YingbiBox") {
        constexpr int minimumLevel = 41;
        constexpr int initialCoinCost = 20;
        constexpr int dailyCostStep = 5;
        constexpr int requiredFreeSlots = 4;
        const int today = localDateKey();
        const int opened = today == fairyCoinChestDay_ ? fairyCoinChestOpened_ : 0;
        const int coinCost = initialCoinCost + opened;
        if (today == 0) {
            say("Fairy Coin Chest: data local indisponivel; item preservado.");
            return;
        }
        if (player_.level < minimumLevel) {
            say("Fairy Coin Chest exige nivel 41; item preservado.");
            return;
        }
        if (bagCapacity_ - static_cast<int>(bag_.size()) < requiredFreeSlots) {
            say("Fairy Coin Chest exige 4 espacos livres; item preservado.");
            return;
        }
        int coinCount = 0;
        for (const Stack& stackInBag : bag_)
            if (stackInBag.id == 855) coinCount += stackInBag.count;
        if (coinCount < coinCost) {
            say("Fairy Coin Chest exige " + std::to_string(coinCost) + " Fairy Coins; item preservado.");
            return;
        }
        const std::uint64_t totalWeight = itemRandomRewards_.totalWeight(1872);
        item_random_rewards::Reward reward;
        if (totalWeight == 0 || totalWeight > static_cast<std::uint64_t>(INT_MAX) ||
            !itemRandomRewards_.rewardForRoll(1872, static_cast<std::uint64_t>(roll(static_cast<int>(totalWeight))) + 1,
                                               reward)) {
            say("Fairy Coin Chest: tabela historica indisponivel; item preservado.");
            return;
        }
        const auto rewardInfo = items_.find(reward.itemId);
        const auto chestInfo = items_.find(1872);
        if (rewardInfo == items_.end() || rewardInfo->second.maxStack <= 0 ||
            chestInfo == items_.end() || chestInfo->second.maxStack <= 0) {
            say("Fairy Coin Chest: recompensa historica ausente no catalogo; item preservado.");
            return;
        }

        std::vector<Stack> preview = bag_;
        Stack& openedChest = preview[static_cast<std::size_t>(index)];
        if (--openedChest.count <= 0) preview.erase(preview.begin() + index);
        int coinsRemaining = coinCost;
        for (std::size_t i = 0; i < preview.size() && coinsRemaining > 0;) {
            Stack& coin = preview[i];
            if (coin.id != 855) { ++i; continue; }
            const int taken = std::min(coinsRemaining, coin.count);
            coin.count -= taken;
            coinsRemaining -= taken;
            if (coin.count <= 0) preview.erase(preview.begin() + static_cast<std::ptrdiff_t>(i));
            else ++i;
        }
        if (coinsRemaining > 0 ||
            !inventory_rules::insert(preview, reward.itemId, reward.quantity, rewardInfo->second.maxStack, bagCapacity_) ||
            !inventory_rules::insert(preview, 1872, 1, chestInfo->second.maxStack, bagCapacity_)) {
            say("Fairy Coin Chest: nao foi possivel concluir a recompensa; item preservado.");
            return;
        }
        bag_.swap(preview);
        assignItemUids();
        fairyCoinChestDay_ = today;
        fairyCoinChestOpened_ = std::min(1000000, opened + dailyCostStep);
        onHistoricalEvent(MissionEventKind::GetItem, reward.itemId, reward.quantity);
        onHistoricalEvent(MissionEventKind::GetItem, 1872, 1);
        say("Fairy Coin Chest aberto: recebeu " + std::to_string(reward.quantity) + "x " +
            rewardInfo->second.name + "; proxima abertura hoje custa " +
            std::to_string(initialCoinCost + fairyCoinChestOpened_) + " Fairy Coins.");
        save(savePath());
        return;
    }
    if (item.id == 1011 && item.scriptAction == "ItemUse_HHLP") {
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto navega; item preservado.");
            return;
        }
        const int dateHour = localMonthDayHourKey();
        if (dateHour < 10122 || dateHour > 10123) {
            say(item.name + ": disponivel somente em 1 de janeiro, das 22h as 23h; item preservado.");
            return;
        }
    }
    // 05-AX: baus com recompensa deterministica auditada em ItemEffect.lua.
    // A transacao inteira e simulada em uma copia antes de tocar na bolsa real:
    // nao consome bau se faltar espaco, se o item historico nao estiver no
    // catalogo, ou se qualquer uma das recompensas nao couber.
    auto chest = historicalChestRules_.planFor(item.id, item.scriptAction);
    if (!chest && item.id == 7363 && item.scriptAction == "Corsairs_WangChest") {
        // ItemEffect.lua::Corsairs_WangChest selects one of six class belts
        // for levels 50..55, or one of six level-65 belts from level 60 up.
        // Levels 56..59 have no historical branch; keep the item for them.
        if (player_.level >= 50 && player_.level <= 55) {
            chest = item_chests::Plan{1, {},
                {{7294,1,4},{7297,1,4},{7300,1,4},{7303,1,4},{7306,1,4},{7309,1,4}}};
        } else if (player_.level >= 60) {
            chest = item_chests::Plan{1, {},
                {{7312,1,4},{7315,1,4},{7318,1,4},{7321,1,4},{7324,1,4},{7327,1,4}}};
        } else {
            say("Wang Xiao Hu's Chest nao tem resultado historico para este nivel; item preservado.");
            return;
        }
        chest->successMessage = "Wang Xiao Hu's Chest aberto: cinturão historico recebido.";
    }
    if (!chest) chest = item_chests::planFor(item.id, item.scriptAction, player_.id, job_);
    if (chest) {
        if (chest->requiresShore && usingTicketBoat_) {
            say("Nao abra este bau enquanto navega; item preservado.");
            return;
        }
        const int freeSlots = bagCapacity_ - static_cast<int>(bag_.size());
        if (freeSlots < chest->requiredFreeSlots) {
            say("Precisa de " + std::to_string(chest->requiredFreeSlots) + " espacos livres para abrir " + item.name + ".");
            return;
        }
        if (chest->minimumLevel > 0 && player_.level < chest->minimumLevel) {
            say(item.name + " exige nivel " + std::to_string(chest->minimumLevel) + "; item preservado.");
            return;
        }
        if (!chest->allowedJobs.empty() &&
            std::find(chest->allowedJobs.begin(), chest->allowedJobs.end(), job_) == chest->allowedJobs.end()) {
            say(item.name + " exige uma classe de segunda promocao elegivel; item preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        std::vector<item_chests::Grant> grants = chest->rewards;
        if (!chest->randomRewardGroups.empty()) {
            const auto& group = chest->randomRewardGroups[static_cast<std::size_t>(roll(
                static_cast<int>(chest->randomRewardGroups.size())))];
            grants.insert(grants.end(), group.begin(), group.end());
        } else if (!chest->randomChoices.empty()) {
            grants.push_back(chest->randomChoices[static_cast<std::size_t>(roll(
                static_cast<int>(chest->randomChoices.size())))]);
        } else if (!chest->weightedRandomChoices.empty()) {
            int totalWeight = 0;
            for (const auto& weighted : chest->weightedRandomChoices)
                if (weighted.weight > 0) totalWeight += weighted.weight;
            if (totalWeight <= 0) {
                say(item.name + ": tabela historica de sorteio invalida; item preservado.");
                return;
            }
            int outcome = roll(totalWeight);
            for (const auto& weighted : chest->weightedRandomChoices) {
                if (weighted.weight <= 0) continue;
                if (outcome < weighted.weight) {
                    grants.push_back(weighted.grant);
                    break;
                }
                outcome -= weighted.weight;
            }
            if (grants.empty()) {
                say(item.name + ": sorteio historico falhou; item preservado.");
                return;
            }
        }
        for (const auto& grant : grants) {
            const auto found = items_.find(grant.itemId);
            const std::size_t firstNewSlot = preview.size();
            if (found == items_.end() || grant.count <= 0 ||
                !inventory_rules::insert(preview, grant.itemId, grant.count,
                                         found->second.maxStack, bagCapacity_)) {
                say("Nao foi possivel abrir " + item.name + ": recompensa sem dados ou bolsa cheia.");
                return;
            }
            if (grant.quality >= 0) {
                for (std::size_t slot = firstNewSlot; slot < preview.size(); ++slot)
                    preview[slot].quality = grant.quality;
            }
        }
        bag_.swap(preview);
        assignItemUids();
        for (const auto& grant : grants)
            onHistoricalEvent(MissionEventKind::GetItem, grant.itemId, grant.count);
        say(chest->successMessage.empty() ? "Abriu " + item.name + "." : chest->successMessage);
        save(savePath());
        return;
    }
    if (item_chests::characterRestrictedBox(item.id,item.scriptAction)) {
        say(item.name + ": baú Relic incompatível com o tipo deste personagem.");
        return; // Sem consumo e sem recompensas.
    }
    if (item.scriptAction == "ItemUse_XingYunBox" && item.id == 3011) {
        if (bag_.size() >= static_cast<std::size_t>(bagCapacity_)) {
            say("Libere um espaco na bolsa antes de abrir " + item.name + "; item preservado.");
            return;
        }
        // ItemEffect.lua has eight rare outcomes in 241,920 rolls, then a
        // uniform 959..984 card roll where the five rare IDs become 959.
        const int rareRoll = roll(241920) + 1;
        int rewardId = 0;
        if (rareRoll == 1) rewardId = 961;
        else if (rareRoll == 2) rewardId = 969;
        else if (rareRoll <= 4) rewardId = 973;
        else if (rareRoll <= 6) rewardId = 980;
        else if (rareRoll <= 8) rewardId = 979;
        else {
            rewardId = roll(26) + 959;
            if (rewardId == 961 || rewardId == 969 || rewardId == 973 ||
                rewardId == 980 || rewardId == 979) rewardId = 959;
        }
        const auto definition = items_.find(rewardId);
        if (definition == items_.end() || definition->second.maxStack <= 0) {
            say(item.name + ": carta historica indisponivel; item preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        const std::size_t firstNewSlot = preview.size();
        if (!inventory_rules::insert(preview, rewardId, 1,
                                     definition->second.maxStack, bagCapacity_)) {
            say(item.name + ": bolsa sem espaco para a carta; item preservado.");
            return;
        }
        if (preview.size() > firstNewSlot) preview[firstNewSlot].quality = 4;
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        const auto card = items_.find(rewardId);
        say("Abriu " + item.name + ": recebeu " +
            (card == items_.end() ? std::to_string(rewardId) : card->second.name) + ".");
        save(savePath());
        return;
    }
    // 9600 has a separate iteminfo label (MysteryBox), but the historical
    // GiveRandomItems function explicitly maps it to BaoXiang_MysteryBOX.
    // BlackMarket uses the same weighted row schema, with its own `Market`
    // mappings and tables.
    const bool blackMarket = item.scriptAction == "BlackMarket";
    const bool fairyBox = item.id == 262 && item.scriptAction == "ItemUse_JingLingBOX";
    const bool classRewardChest = item.scriptAction == "ChestYmazes" &&
                                  (item.id == 9618 || item.id == 9619 || item.id == 9620);
    if (item.scriptAction == "GiveRandomItems" ||
        (item.id == 9600 && item.scriptAction == "MysteryBox") || blackMarket || fairyBox || classRewardChest) {
        const std::uint64_t weight = classRewardChest ?
            itemRandomRewards_.totalWeight(item.id, job_) : itemRandomRewards_.totalWeight(item.id);
        const bool hasRewardTable = classRewardChest ?
            itemRandomRewards_.supports(item.id, job_) : itemRandomRewards_.supports(item.id);
        if (!hasRewardTable || weight == 0 || weight > static_cast<std::uint64_t>(INT_MAX)) {
            say(item.name + ": tabela histórica de recompensas indisponível; item preservado.");
            return;
        }
        // O callback histórico exige um quadrado livre antes de sortear, mesmo
        // quando o resultado pudesse ser empilhado. Mantemos essa condição e
        // validamos todas as entregas em uma cópia antes de consumir o baú.
        if (bag_.size() >= static_cast<std::size_t>(bagCapacity_)) {
            say("Libere um espaço na bolsa antes de abrir " + item.name + ".");
            return;
        }
        item_random_rewards::Reward reward;
        const std::uint64_t rewardRoll = static_cast<std::uint64_t>(roll(static_cast<int>(weight))) + 1u;
        const bool rewardFound = classRewardChest ?
            itemRandomRewards_.rewardForRoll(item.id, job_, rewardRoll, reward) :
            itemRandomRewards_.rewardForRoll(item.id, rewardRoll, reward);
        if (!rewardFound) {
            say(item.name + ": sorteio histórico indisponível; item preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        std::vector<std::pair<int,int>> rewards{{reward.itemId, reward.quantity}};
        if (!blackMarket && (item.id == 3902 || item.id == 3903)) rewards.emplace_back(3904, 1);
        bool canGrant = true;
        for (const auto& grant : rewards) {
            const auto definition = items_.find(grant.first);
            if (definition == items_.end() || grant.second <= 0) {
                canGrant = false;
                break;
            }
            const std::size_t firstNewSlot = preview.size();
            if (!inventory_rules::insert(preview, grant.first, grant.second,
                                         definition->second.maxStack, bagCapacity_)) {
                canGrant = false;
                break;
            }
            const int quality = grant.first == reward.itemId ? reward.quality : 5;
            for (std::size_t slot = firstNewSlot; slot < preview.size(); ++slot) {
                preview[slot].quality = quality;
                if (fairyBox && grant.first == reward.itemId && definition->second.type == 59) {
                    preview[slot].maxDurability = definition->second.maxDurability;
                    preview[slot].maxEnergy = definition->second.maxEnergy;
                }
            }
        }
        if (!canGrant) {
            say(item.name + ": recompensa ausente ou sem espaço; item preservado.");
            return;
        }
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, reward.itemId, reward.quantity);
        if (!blackMarket && (item.id == 3902 || item.id == 3903))
            onHistoricalEvent(MissionEventKind::GetItem, 3904, 1);
        const auto rewardDefinition = items_.find(reward.itemId);
        const std::string rewardName = rewardDefinition == items_.end() ?
            std::to_string(reward.itemId) : rewardDefinition->second.name;
        say("Abriu " + item.name + ": " + std::to_string(reward.quantity) + "x " + rewardName +
            (reward.announces ? " (recompensa especial; aviso local)." : ".") +
            (fairyBox ? " A inicializacao aleatoria da fada pelo MakeItem original nao foi reproduzida." : ""));
        save(savePath());
        return;
    }
    if (item.scriptAction == "GemVouchers") {
        // PORT_APPROX: preserve the historical reward list and uniform choice,
        // while the offline RNG sequence and server-wide Notice have no local
        // DX9/server equivalent. Match the historical boat and free-slot gates.
        if (usingTicketBoat_) {
            say("Nao use vouchers de gemas enquanto estiver navegando; item preservado.");
            return;
        }
        if (bag_.size() >= static_cast<std::size_t>(bagCapacity_)) {
            say("Libere um espaco na bolsa antes de abrir " + item.name + ".");
            return;
        }
        const std::size_t count = itemGemVoucherRules_.rewardCount(item.id);
        if (count == 0 || !itemGemVoucherRules_.supports(item.id)) {
            say(item.name + ": tabela historica de recompensas indisponivel; item preservado.");
            return;
        }
        int rewardId = 0;
        if (!itemGemVoucherRules_.rewardForRoll(item.id,
                static_cast<std::uint64_t>(roll(static_cast<int>(count)) + 1), rewardId)) {
            say(item.name + ": sorteio historico indisponivel; item preservado.");
            return;
        }
        const auto definition = items_.find(rewardId);
        if (definition == items_.end() || definition->second.maxStack <= 0) {
            say(item.name + ": recompensa sem dados historicos; item preservado.");
            return;
        }
        std::vector<Stack> preview = bag_;
        Stack& consumed = preview[static_cast<std::size_t>(index)];
        if (--consumed.count <= 0) preview.erase(preview.begin() + index);
        const std::size_t firstNewSlot = preview.size();
        if (!inventory_rules::insert(preview, rewardId, 1,
                                     definition->second.maxStack, bagCapacity_)) {
            say(item.name + ": recompensa sem espaco; item preservado.");
            return;
        }
        // GiveItem(..., quality=4) is fixed in GemVouchers, independent of roll.
        for (std::size_t slot = firstNewSlot; slot < preview.size(); ++slot)
            if (preview[slot].id == rewardId) preview[slot].quality = 4;
        bag_.swap(preview);
        assignItemUids();
        onHistoricalEvent(MissionEventKind::GetItem, rewardId, 1);
        say("Abriu " + item.name + ": recebeu " + std::to_string(rewardId) + ".");
        save(savePath());
        return;
    }
    if (item.scriptAction == "ItemStat") {
        item_stat_rules::Rule rule;
        if (!itemStatRules_.find(item.id, rule)) {
            say(item.name + ": regra histórica de atributos indisponível; item preservado.");
            return;
        }
        if (shopOpen_) {
            say("Feche a loja antes de usar " + item.name + ".");
            return;
        }
        if (usingTicketBoat_) {
            say("Não use " + item.name + " enquanto estiver navegando.");
            return;
        }
        int* attribute = nullptr;
        switch (rule.attribute) {
            case item_stat_rules::Attribute::Strength: attribute = &bstr_; break;
            case item_stat_rules::Attribute::Constitution: attribute = &bcon_; break;
            case item_stat_rules::Attribute::Agility: attribute = &bagi_; break;
            case item_stat_rules::Attribute::Accuracy: attribute = &bdex_; break;
            case item_stat_rules::Attribute::Spirit: attribute = &bsta_; break;
        }
        if (!attribute) return;
        const long long next = static_cast<long long>(*attribute) + rule.amount;
        if (next < 5 || next > 130) {
            say("Não é possível alterar " + rule.name + ": o atributo deve permanecer entre 5 e 130.");
            return;
        }
        *attribute = static_cast<int>(next);
        if (rule.amount < 0)
            ap_ = static_cast<int>(std::min(1000000000LL,
                static_cast<long long>(ap_) - static_cast<long long>(rule.amount)));
        applyAttrs(false);
        --stack.count;
        if (stack.count <= 0) bag_.erase(bag_.begin() + index);
        say("Usou " + item.name + ": " + rule.name + " " +
            (rule.amount > 0 ? "+" : "") + std::to_string(rule.amount) + ". Pontos de atributo: " +
            std::to_string(ap_) + ".");
        save(savePath());
        return;
    }
    const bool gear = item_rules::isEquipment(item.slot,item.equipSlots,item.minAtk,item.maxAtk,item.def);
    if (item.healHp > 0 || item.healSp > 0 || item.healHpPercent > 0.0 ||
        item.healSpPercent > 0.0 || item.healStateId > 0) {
        if (usingTicketBoat_) {
            say("Nao use " + item.name + " enquanto estiver navegando.");
            return;
        }
        if (player_.hp <= 0 || player_.homeY <= 0.f) {
            say(item.name + ": nao pode ser usado com HP ou SP zerado.");
            return;
        }
        const auto cooldown = itemCooldownRemaining_.find(item.id);
        if (cooldown != itemCooldownRemaining_.end() && cooldown->second > 0.f) {
            say(item.name + " em recarga por " + std::to_string(static_cast<int>(std::ceil(cooldown->second))) + " s.");
            return;
        }
        const int foodStateIndex = item.healStateId - 165;
        if (item.healStateId > 0 &&
            (foodStateIndex < 0 || foodStateIndex >= static_cast<int>(foodStates_.size()) ||
             item.healStateLevel <= 0 || item.healStateLevel > 10 || item.healStateSeconds <= 0)) {
            say(item.name + ": estado historico " + std::to_string(item.healStateId) +
                " ainda nao possui regra offline; item preservado.");
            return;
        }
        if (item.healStateId > 0 &&
            foodStates_[static_cast<std::size_t>(foodStateIndex)].level >= item.healStateLevel) {
            say(item.name + ": ja existe um estado de comida igual ou superior; item preservado.");
            return;
        }
        const double hpRestore = static_cast<double>(item.healHp) +
            static_cast<double>(player_.maxHp) * item.healHpPercent;
        const double spRestore = static_cast<double>(item.healSp) +
            static_cast<double>(player_.homeX) * item.healSpPercent;
        const int restoreHp = static_cast<int>(std::min<double>(std::numeric_limits<int>::max(), std::floor(hpRestore)));
        const float restoreSp = static_cast<float>(std::min<double>(std::numeric_limits<float>::max(), spRestore));
        player_.hp = static_cast<int>(std::min<long long>(player_.maxHp,
            static_cast<long long>(player_.hp) + std::max(0, restoreHp)));
        player_.homeY = std::min(player_.homeX, player_.homeY + std::max(0.f, restoreSp));
        if (item.healStateId > 0) {
            FoodState& state = foodStates_[static_cast<std::size_t>(foodStateIndex)];
            state.level = item.healStateLevel;
            state.remaining = static_cast<float>(item.healStateSeconds);
            if (sheetReady_) applyAttrs(false);
            else recomputeGear();
            // Os callbacks históricos ATK e SPR também restauram recurso no
            // instante de aplicação, além dos valores de Restore.Life.
            if (item.healStateId == 166)
                player_.hp = std::min(player_.maxHp, player_.hp + item.healStateLevel * 200);
            else if (item.healStateId == 167)
                player_.homeY = std::min(player_.homeX,
                    player_.homeY + static_cast<float>(item.healStateLevel * 100));
        }
        if (item.healCooldownSeconds > 0)
            itemCooldownRemaining_[item.id] = static_cast<float>(item.healCooldownSeconds);
        say("Usou " + item.name + ".");
    } else if (gear) {
        equipBagToSlot(index, -1);
        return;
    } else {
        if (item_rules::hasUnsupportedScriptAction(item.scriptAction))
            say(item.name + ": funcao historica " + item.scriptAction + " ainda nao implementada offline.");
        else
            say("Item guardado. Arraste para loja ou para fora da bolsa e confirme.");
        return;
    }
    stack.count -= 1;
    if (stack.count <= 0) bag_.erase(bag_.begin() + index);
    save(savePath());
}

bool Play::equipBagToSlot(int index, int preferredSlot) {
    if (index < 0 || index >= static_cast<int>(bag_.size())) return false;
    Stack& stack = bag_[static_cast<std::size_t>(index)];
    const auto found = items_.find(stack.id);
    if (found == items_.end()) return false;
    const ItemInfo& item = found->second;
    if (!item_rules::isEquipment(item.slot,item.equipSlots,item.minAtk,item.maxAtk,item.def)) return false;
    // JobType 8 (Champion) explicitly gives up shields for two-handed combat
    // in the historical StringSet. ItemInfo type 11 is the shield category.
    if (job_ == 8 && item.type == 11) {
        say("Champion nao pode usar escudos.");
        return false;
    }
    auto allowed = [](const std::vector<int>& list, int value) {
        return list.empty() || std::find(list.begin(),list.end(),-1)!=list.end() ||
               std::find(list.begin(),list.end(),value)!=list.end();
    };
    if (!allowed(item.races,player_.id)) {
        say(item.name + " nao serve em " + player_.name + "."); return false;
    }
    if (!allowed(item.jobs,job_)) {
        say(item.name + " nao pertence a classe " + std::string(jobName(job_)) + "."); return false;
    }
    if (item.level > player_.level) {
        say("Precisa do nivel " + std::to_string(item.level) + "."); return false;
    }
    const int already=equippedSlotAtBagIndex(index);
    if (already>=0) {
        // A mesma instancia permanece unica, nao migra implicitamente entre maos.
        worn_.erase(already); wornUid_.erase(already);
        say("Tirou " + item.name + ".");
    } else {
        std::vector<int> locations=item.equipSlots;
        if (locations.empty() && item.slot>=0) locations.push_back(item.slot);
        if (locations.empty()) { say(item.name + " nao tem encaixe."); return false; }
        int selected=-1;
        if (preferredSlot >= 0) {
            if (!item_rules::canEquipSlot(item.slot,item.equipSlots,preferredSlot))return false;
            selected=preferredSlot;
        } else if (item.requiredEquipSlots.size() > 1 &&
                   std::find(locations.begin(), locations.end(), item.slot) != locations.end()) {
            // ItemInfo col. 29 marks two-hand equipment (e.g. bows and the
            // Two Handed Sword). Keep its authored primary hand, not whichever
            // hand happens to be empty.
            selected=item.slot;
        } else {
            for(int location:locations)if(!worn_.count(location)){selected=location;break;}
            if(selected<0)selected=locations.front();
        }
        std::vector<int> displaced;
        for (const auto& equipped : worn_) {
            if (equipped.first == selected ||
                std::find(item.requiredEquipSlots.begin(), item.requiredEquipSlots.end(), equipped.first) !=
                    item.requiredEquipSlots.end()) {
                displaced.push_back(equipped.first);
                continue;
            }
            const auto existing = items_.find(equipped.second);
            if (existing != items_.end() &&
                std::find(existing->second.requiredEquipSlots.begin(), existing->second.requiredEquipSlots.end(),
                          selected) != existing->second.requiredEquipSlots.end())
                displaced.push_back(equipped.first);
        }
        for (int slot : displaced) {
            const auto removed = worn_.find(slot);
            if (removed == worn_.end()) continue;
            const auto removedItem = items_.find(removed->second);
            if (removedItem != items_.end()) say("Desequipou " + removedItem->second.name + ".");
            worn_.erase(removed);
            wornUid_.erase(slot);
        }
        worn_[selected]=item.id;
        wornUid_[selected]=stack.uid;
        say("Equipou " + item.name + ".");
    }
    if (sheetReady_) applyAttrs(false); else recomputeGear();
    save(savePath());
    return true;
}

int Play::bagItemUid(int index) const {
    return index<0||index>=static_cast<int>(bag_.size()) ? 0 : bag_[static_cast<std::size_t>(index)].uid;
}
int Play::bagItemId(int index) const {
    return index < 0 || index >= static_cast<int>(bag_.size()) ? 0 : bag_[static_cast<std::size_t>(index)].id;
}
std::string Play::bagItemName(int index) const {
    const int id=bagItemId(index);
    const auto item=items_.find(id);
    return item == items_.end() ? std::to_string(id) : item->second.name;
}
bool Play::sellBag(int index) {
    if(!shopOpen_ || index<0 || index>=static_cast<int>(bag_.size())) return false;
    if(equippedSlotAtBagIndex(index)>=0) { say("Desequipe antes de vender."); return false; }
    Stack& stack=bag_[static_cast<std::size_t>(index)];
    const auto found=items_.find(stack.id);
    if(found==items_.end() || found->second.price<=0) { say("Item sem preco de venda."); return false; }
    const int price=std::max(1,found->second.price/2);
    gold_=std::min(1000000000,gold_+price);
    say("Vendeu "+found->second.name+" por "+std::to_string(price)+" ouro.");
    if(--stack.count<=0) bag_.erase(bag_.begin()+index);
    save(savePath());
    return true;
}

bool Play::discardBag(int index) {
    if(index<0 || index>=static_cast<int>(bag_.size())) return false;
    if(equippedSlotAtBagIndex(index)>=0) { say("Desequipe antes de descartar."); return false; }
    if(loots_.size()>=24) { say("Muito loot no chao."); return false; }
    Stack& stack=bag_[static_cast<std::size_t>(index)];
    const int id=stack.id;
    Stack dropped=stack;
    dropped.count=1; // pilha com 20 unidades descarta so uma; nunca duplica 20 no loot
    loots_.push_back({id,hereX_+.60f,hereY_+.60f,dropped});
    if(--stack.count<=0) bag_.erase(bag_.begin()+index);
    const auto found=items_.find(id);
    say("Descartou "+(found==items_.end()?std::to_string(id):found->second.name)+" no chao.");
    save(savePath());
    return true;
}


void Play::clickEquip(int slot) {
    const auto it = worn_.find(slot);
    if (it == worn_.end()) return;
    const int id=it->second;
    worn_.erase(it);
    wornUid_.erase(slot);
    if (sheetReady_) applyAttrs(false); else recomputeGear();
    const auto found=items_.find(id);
    say("Tirou " + (found==items_.end()?std::to_string(id):found->second.name) + ".");
    save(savePath());
}

void Play::stashBag(int index) {
    if (index < 0 || index >= static_cast<int>(bag_.size())) return;
    const Stack stack = bag_[static_cast<std::size_t>(index)];
    if (equippedSlotAtBagIndex(index)>=0) {
        say("Tira o item antes de guardar na bolsa temporaria.");
        return;
    }
    const auto found = items_.find(stack.id);
    const int cap = found==items_.end()?1:found->second.maxStack;
    if (cap == 1 && stack.count == 1) {
        if (temp_.size() >= 16) { say("Bolsa temporaria cheia."); return; }
        temp_.push_back(stack);
    } else if (!addStack(temp_,stack.id,stack.count,cap,16)) {
        say("Bolsa temporaria cheia.");
        return;
    }
    bag_.erase(bag_.begin() + index);
    assignItemUids();
    say("Guardou na bolsa temporaria.");
    save(savePath());
}

void Play::takeTemp(int index) {
    if (index < 0 || index >= static_cast<int>(temp_.size())) return;
    const Stack stack = temp_[static_cast<std::size_t>(index)];
    const auto found = items_.find(stack.id);
    const int cap = found==items_.end()?1:found->second.maxStack;
    if (cap == 1 && stack.count == 1) {
        if (bag_.size() >= static_cast<std::size_t>(bagCapacity_)) {
            say("Bolsa cheia."); return;
        }
        bag_.push_back(stack);
    } else if (!addBag(stack.id, stack.count)) {
        say("Bolsa cheia."); return;
    }
    temp_.erase(temp_.begin() + index);
    assignItemUids();
    say("Voltou para a bolsa.");
    save(savePath());
}

bool Play::openBank() {
    if (bankOpen_) return true;
    constexpr int accessFee = 200; // NpcScript01.lua: HasMoney/TakeMoney -> OpenBank
    if (gold_ < accessFee) { say("O banco cobra 200G; ouro insuficiente."); return false; }
    const int oldGold = gold_;
    const bool wasOpen = bankOpen_;
    gold_ -= accessFee;
    bankOpen_ = true;
    if (!save(savePath())) {
        gold_ = oldGold;
        bankOpen_ = wasOpen;
        return false;
    }
    say("Cofre aberto. Taxa de acesso: 200G.");
    return true;
}

void Play::depositBank(int bagIndex) {
    if (!bankOpen_ || bagIndex < 0 || bagIndex >= static_cast<int>(bag_.size())) return;
    if (equippedSlotAtBagIndex(bagIndex) >= 0) { say("Desequipe o item antes de depositar."); return; }
    const Stack stack = bag_[static_cast<std::size_t>(bagIndex)];
    const auto info = items_.find(stack.id);
    const int cap = info == items_.end() ? 1 : info->second.maxStack;
    const auto oldBag = bag_;
    const auto oldBank = bank_;
    const auto oldWornUids = wornUid_;
    const int oldNextUid = nextItemUid_;
    if (cap == 1 && stack.count == 1) {
        if (bank_.size() >= 80) { say("Cofre cheio."); return; }
        bank_.push_back(stack);
    } else if (!addStack(bank_, stack.id, stack.count, cap, 80)) {
        say("Cofre cheio."); return;
    }
    bag_.erase(bag_.begin() + bagIndex);
    assignItemUids();
    if (!save(savePath())) {
        bag_ = oldBag;
        bank_ = oldBank;
        wornUid_ = oldWornUids;
        nextItemUid_ = oldNextUid;
        return;
    }
    say("Item depositado no cofre.");
}

void Play::withdrawBank(int bankIndex) {
    if (!bankOpen_ || bankIndex < 0 || bankIndex >= static_cast<int>(bank_.size())) return;
    const Stack stack = bank_[static_cast<std::size_t>(bankIndex)];
    const auto info = items_.find(stack.id);
    const int cap = info == items_.end() ? 1 : info->second.maxStack;
    const auto oldBag = bag_;
    const auto oldBank = bank_;
    const auto oldWornUids = wornUid_;
    const int oldNextUid = nextItemUid_;
    if (cap == 1 && stack.count == 1) {
        if (bag_.size() >= static_cast<std::size_t>(bagCapacity_)) { say("Bolsa cheia."); return; }
        bag_.push_back(stack);
    } else if (!addBag(stack.id, stack.count)) { say("Bolsa cheia."); return; }
    bank_.erase(bank_.begin() + bankIndex);
    assignItemUids();
    if (!save(savePath())) {
        bag_ = oldBag;
        bank_ = oldBank;
        wornUid_ = oldWornUids;
        nextItemUid_ = oldNextUid;
        return;
    }
    say("Item retirado do cofre.");
}

void Play::dragInventory(int source, int target) {
    if (source >= 100 && source < 200) {
        if (target == 250) clickEquip(source - 100);
        return;
    }
    if (source >= 300 && source < 400) {
        if (target == 250) {
            if (bankOpen_) withdrawBank(source - 300);
            else takeTemp(source - 300);
        }
        return;
    }
    if (source < 200 || source >= 300) return;
    const int index = source - 200;
    if (index < 0 || index >= static_cast<int>(bag_.size())) return;
    if (target == 350) {
        if (bankOpen_) depositBank(index);
        else stashBag(index);
        return;
    }
    if (target < 100 || target >= 200) return;
    const auto item = items_.find(bag_[static_cast<std::size_t>(index)].id);
    if (item == items_.end() || !item_rules::canEquipSlot(item->second.slot,item->second.equipSlots,target - 100)) {
        say("Esse item nao encaixa nesse espaco.");
        return;
    }
    equipBagToSlot(index,target - 100);
}


} // namespace top
