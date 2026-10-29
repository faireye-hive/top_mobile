#pragma once
// ItemInfo::sItemEffect (column 90), not a guessed character model. The
// original CCharacter::RefreshLink attaches this scene effect for FAIRY=16.
#include <algorithm>
#include <fstream>
#include <filesystem>
#include "top/SceneEffectCatalog.h"
#include <string>
#include <vector>

namespace top {
struct HistoricalPet {
    int itemId=0, effectId=0, dummy=0;
    std::string name;
    bool definitionAvailable=false;
};
inline std::vector<HistoricalPet> loadHistoricalPets(const std::string& root) {
    std::ifstream input(root+"/scripts/table/iteminfo.txt",std::ios::binary);
    std::vector<HistoricalPet> pets;
    SceneEffectCatalog scenes;scenes.load(root+"/scripts/table/sceneffectinfo.txt");
    std::string line;
    auto number=[](const std::string& text,int fallback=0) {
        try {std::size_t end=0;const int value=std::stoi(text,&end);
            return end==text.size()?value:fallback;}catch(...){return fallback;}
    };
    while(std::getline(input,line)) {
        std::vector<std::string> c;std::size_t begin=0;
        for(;;){const auto end=line.find('\t',begin);c.push_back(line.substr(begin,end-begin));
            if(end==std::string::npos)break;begin=end+1;}
        if(c.size()<91 || number(c[10])!=59 || number(c[0])<=0)continue;
        const auto comma=c[90].find(',');
        const int id=number(c[90].substr(0,comma));
        const int dummy=comma==std::string::npos?0:number(c[90].substr(comma+1),-1);
        const auto* definition=scenes.find(id);
        const bool available=definition && std::filesystem::is_regular_file(root+"/effect/"+definition->particle);
        pets.push_back({number(c[0]),id,dummy,c[1],available});
    }
    std::sort(pets.begin(),pets.end(),[](const auto& a,const auto& b){return a.itemId<b.itemId;});
    return pets;
}
}
