#include "top/EquipmentStats.h"
#include "top/ItemInstancePersistence.h"
#include <array>
#include <iostream>
#include <unordered_map>
#include <vector>

struct Stack {
    int id = 0;
    int count = 0;
    int uid = 0;
    int forgeLevel = 0;
    std::array<int,3> gemIds{};
    top::equipment_stats::Bonus extra;
    int quality = 0, durability = 0, maxDurability = 0, energy = 0, maxEnergy = 0;
    int itemValStaTenths = 0;
    std::array<int,3> blueprintParts{};
    bool tradable = true;
    std::array<int,6> fairySkills{};
    std::array<int,5> fairyStats{};
    int fairySystemTick=0,expDisableFlag=0,altarSacrifice=0,cloakLevel=0,fuseId=0;
    int equipmentUpgradeLevel=0;
};
int main() {
    using namespace top::item_instance;
    std::vector<Stack> bag(2);
    bag[0].id = bag[1].id = 1;
    bag[0].count=bag[1].count=1;
    bag[0].uid=41; bag[1].uid=42;
    bag[0].forgeLevel=3; bag[0].gemIds={681,682,0};
    bag[0].extra.str=4; bag[0].extra.aspdCoefficient=70;
    bag[0].extra.minAttackCoefficient=35; bag[0].extra.maxAttackCoefficient=55;
    bag[1].extra.str=7; bag[1].extra.defense=11;
    bag[0].itemValStaTenths=15; bag[0].blueprintParts={7,4,6};
    const auto encoded=encodeStacks(bag);
    std::vector<Stack> restored(2);
    restored[0].id=restored[1].id=1;
    restored[0].count=restored[1].count=1;
    decodeStacks(encoded.c_str(), restored);
    if (restored[0].uid!=41 || restored[1].uid!=42 ||
        restored[0].forgeLevel!=3 || restored[0].gemIds[0]!=681 ||
        restored[0].gemIds[1]!=682 || restored[0].extra.aspdCoefficient!=70 ||
        restored[0].extra.minAttackCoefficient!=35 || restored[0].extra.maxAttackCoefficient!=55 ||
        restored[0].extra.str!=4 || restored[1].extra.str!=7 ||
        restored[1].extra.defense!=11) {
        std::cerr << "Metadados da mesma espada se misturaram\n"; return 1;
    }
    if (restored[0].itemValStaTenths!=15 || restored[0].blueprintParts!=std::array<int,3>{7,4,6}) {
        std::cerr << "Atributos fracionarios de blueprint nao persistiram\n"; return 3;
    }
    auto mismatched=restored;
    mismatched[1].id=8; mismatched[1].uid=0; mismatched[1].extra={};
    decodeStacks(encoded.c_str(),mismatched);
    if (mismatched[1].uid!=0 || mismatched[1].extra.str!=0) {
        std::cerr << "ID divergente recebeu bonus\n"; return 1;
    }
    auto damaged=restored;
    damaged[0].uid=0; damaged[0].extra={};
    decodeStacks("1,1,999,2,0,0,0,2147483648;", damaged);
    if (damaged[0].uid!=0) {
        std::cerr << "Metadata invalida foi aceita\n"; return 1;
    }
    std::unordered_map<int,int> legacy{{9,1},{6,1}}, worn{{9,41},{6,42}}, reloaded;
    const auto wornString=encodeWorn(worn);
    decodeWorn(wornString.c_str(),legacy,reloaded,restored);
    if (reloaded!=worn) {
        std::cerr << "UIDs das duas maos nao foram restaurados\n"; return 1;
    }
    reloaded.clear();
    decodeWorn("9,41;6,41",legacy,reloaded,restored);
    if (reloaded.size()!=1 || !reloaded.count(9)) {
        std::cerr << "UID duplicado usado em mais de uma mao\n"; return 1;
    }
    std::cout << "05-Y: metadados de instancias e slots validados\n";
}
