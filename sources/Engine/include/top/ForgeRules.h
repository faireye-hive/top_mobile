#pragma once
// 05-Z: regras historicas declarativas do Client/scripts/offline/calculate/variable.lua
// + probabilidades e custo de Client/scripts/offline/script/calculate/forge.lua.
// Nao executa Lua do GameServer. Toda regra ausente e recusada por seguranca.
#include "top/EquipmentStats.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace top::forge_rules {
struct Gem {
    int id = 0, maxLevel = 0, family = 0, effect = 0;
    std::string attribute;
    std::vector<int> equipmentTypes;
    bool compatible(int itemType) const {
        return itemType > 0 && std::find(equipmentTypes.begin(),equipmentTypes.end(),itemType) != equipmentTypes.end();
    }
};
using Catalog = std::unordered_map<int,Gem>;
inline Catalog load(const std::string& path) {
    std::ifstream input(path);
    Catalog gems;
    // Apenas atribuicoes GemVar literais; nunca executar codigo Lua fornecido pelo cliente.
    static const std::regex entry(R"(GemVar\[\d+\]\s*=\s*\{ID\s*=\s*(\d+),\s*Level\s*=\s*(\d+),\s*Type\s*=\s*(\d+),\s*Effect\s*=\s*(\d+),\s*Attribute\s*=\s*(ITEMATTR_VAL_[A-Z0-9_]+),\s*Equip\s*=\s*\{([\d,\s]+)\}\})");
    std::string line;
    while(std::getline(input,line)) {
        std::smatch m;
        if (!std::regex_search(line,m,entry)) continue;
        Gem g;
        try {
            g.id=std::stoi(m[1]); g.maxLevel=std::stoi(m[2]); g.family=std::stoi(m[3]);
            g.effect=std::stoi(m[4]); g.attribute=m[5].str();
            const std::string list=m[6].str();
            static const std::regex number(R"((\d+))");
            for(auto it=std::sregex_iterator(list.begin(),list.end(),number);it!=std::sregex_iterator();++it){
                const int type=std::stoi((*it)[1]);
                if(type!=0 && type<=100) g.equipmentTypes.push_back(type);
            }
        } catch(...) { continue; }
        if(g.id>0 && g.id<=1000000 && g.maxLevel>=1 && g.maxLevel<=9 &&
           g.effect>0 && g.effect<=10000 && !g.equipmentTypes.empty())
            gems[g.id]=g;
    }
    return gems;
}
inline equipment_stats::Bonus effect(const Gem& g,int level) {
    equipment_stats::Bonus b;
    if(level<=0 || level>g.maxLevel) return b;
    const int v=g.effect*level;
    const std::string& a=g.attribute;
    if(a=="ITEMATTR_VAL_MNATK") b.minAttack=b.maxAttack=v;
    else if(a=="ITEMATTR_VAL_DEF") b.defense=v;
    else if(a=="ITEMATTR_VAL_HIT") b.hit=v;
    else if(a=="ITEMATTR_VAL_FLEE") b.dodge=v;
    else if(a=="ITEMATTR_VAL_CRT") b.critical=v;
    else if(a=="ITEMATTR_VAL_MXHP") b.hp=v;
    else if(a=="ITEMATTR_VAL_MXSP") b.sp=v;
    else if(a=="ITEMATTR_VAL_PDEF") b.resistance=v;
    else if(a=="ITEMATTR_VAL_STR") b.str=v;
    else if(a=="ITEMATTR_VAL_AGI") b.agi=v;
    else if(a=="ITEMATTR_VAL_DEX") b.dex=v;
    else if(a=="ITEMATTR_VAL_CON") b.con=v;
    else if(a=="ITEMATTR_VAL_STA") b.sta=v;
    return b;
}
inline int forgingCost(const std::array<int,3>& levels){
    int sum=0;
    for(int l:levels){if(l<0||l>9)return -1;sum+=l;}
    // forge.lua: (1 + sum(niveis das gemas)) * 100000.
    return (1+sum)*100000;
}
inline int forgingChancePer10000(int nextGemLevel) {
    // forge.lua Forging_Success_Rate (sem STATE_JLGLJB / bonus de frutas).
    static constexpr int rates[]={0,10000,10000,10000,4500,3500,2500,1500,500,200};
    return nextGemLevel>=1&&nextGemLevel<=9?rates[nextGemLevel]:0;
}
inline int forgingChanceWithFruitPer10000(int nextGemLevel,int bonusFruitLevel) {
    const int base=forgingChancePer10000(nextGemLevel);
    if(base<=0)return 0;
    return std::min(10000,base+std::max(0,bonusFruitLevel)*1000); // Lua: stateLv * 0.1
}
inline int combiningChancePer10000(int currentLevel,int type) {
    // forge.lua Combine_Success_Rate: tipo 49 normal; tipo 50 refiner.
    static constexpr int common[]={10000,9000,8000,7000,6000,5000,4000,3000,2000};
    static constexpr int refiner[]={10000,10000,10000,9500,9000,8500,8000,7500,7000};
    if(currentLevel<1||currentLevel>9)return 0;
    return type==49?common[currentLevel-1]:type==50?refiner[currentLevel-1]:0;
}
inline int combiningChanceWithFruitPer10000(int currentLevel,int type,int bonusFruitLevel) {
    const int base=combiningChancePer10000(currentLevel,type);
    if(base<=0)return 0;
    return std::min(10000,base+std::max(0,bonusFruitLevel)*1000); // Lua: stateLv * 0.1
}
inline bool validSockets(int count,const std::array<int,3>& gems,const std::array<int,3>& levels){
    if(count<0||count>3)return false;
    for(int k=0;k<3;++k){
        if(levels[k]<0||levels[k]>9)return false;
        if((gems[k]==0)!=(levels[k]==0))return false;
        if(k>=count && gems[k]!=0)return false;
    }
    return true;
}
} // namespace top::forge_rules
