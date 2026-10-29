#include "top/ForgeRules.h"
#include "top/ForgeInstancePersistence.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>
struct Stack {
    int id=0,count=1,uid=0,forgeLevel=0,stoneLevel=1,socketCount=0;
    std::array<int,3> gemIds{},gemLevels{};
    top::equipment_stats::Bonus extra;
};
int main(){
    using namespace top;
    const char* fixture="/tmp/top-05z-gemvar-literal-fixture.lua";
    {
        std::ofstream out(fixture);
        out<<"GemVar[2] = {ID = 879, Level = 6, Type = 1, Effect = 6, Attribute = ITEMATTR_VAL_MNATK, Equip = {2,3,4,7,9,0}}\n";
        out<<"GemVar[5] = {ID = 882, Level = 6, Type = 2, Effect = 5, Attribute = ITEMATTR_VAL_DEF, Equip = {11,22,27,0}}\n";
        out<<"GemVar[9] = {ID = 860, Level = 3, Type = 4, Effect = 5, Attribute = ITEMATTR_VAL_AGI, Equip = {24,0}}\n";
    }
    const auto gems=forge_rules::load(fixture);
    std::remove(fixture);
    if(gems.size()!=3 || !gems.at(879).compatible(2) || gems.at(879).compatible(22) ||
       forge_rules::effect(gems.at(879),2).minAttack!=12 ||
       forge_rules::effect(gems.at(879),2).maxAttack!=12 ||
       forge_rules::effect(gems.at(882),3).defense!=15 ||
       forge_rules::effect(gems.at(860),2).agi!=10 ||
       forge_rules::effect(gems.at(860),4).agi!=0) return 1;
    if(forge_rules::forgingCost({1,2,0})!=400000 ||
       forge_rules::forgingChancePer10000(1)!=10000 ||
       forge_rules::forgingChancePer10000(4)!=4500 ||
       forge_rules::forgingChancePer10000(9)!=200 ||
       forge_rules::combiningChancePer10000(2,49)!=9000 ||
       forge_rules::combiningChancePer10000(4,50)!=9500)return 2;
    std::vector<Stack> bag(2);
    bag[0].id=bag[1].id=777;
    bag[0].uid=11;bag[1].uid=12;
    bag[0].socketCount=2;bag[0].gemIds={879,882,0};
    bag[0].gemLevels={2,1,0};bag[0].stoneLevel=1;
    bag[1].socketCount=0;
    const auto encoded=forge_instance::encode(bag);
    std::vector<Stack> restored=bag;
    restored[0].socketCount=restored[1].socketCount=0;
    restored[0].gemLevels={};
    forge_instance::decode(encoded.c_str(),restored);
    if(restored[0].socketCount!=2 || restored[0].gemLevels[0]!=2 ||
       restored[0].gemLevels[1]!=1 || restored[1].socketCount!=0)return 3;
    auto invalid=restored;
    invalid[1].socketCount=0;
    forge_instance::decode("777,12,1,3,1,0,0",invalid);
    if(invalid[1].socketCount!=0)return 4;
    invalid[0].socketCount=0;
    forge_instance::decode("777,11,1,2,1,0,0",invalid);
    if(invalid[0].socketCount!=0)return 5;
    invalid[0].socketCount=0;
    forge_instance::decode("777,999,1,2,2,1,0",invalid);
    if(invalid[0].socketCount!=0)return 6;
    std::cout<<"05-Z: catalogo historico, custo, chance e Z1 validados\n";
    return 0;
}
