#pragma once
// Formato aditivo Z1 / ZT1. A chave id+uid impede remapeamento apos reordenacao.
#include "top/ItemInstancePersistence.h"
#include "top/ForgeRules.h"
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
namespace top::forge_instance {
template<class Stack> std::string encode(const std::vector<Stack>& stacks){
    std::ostringstream out;
    for(std::size_t i=0;i<stacks.size();++i){
        const auto& s=stacks[i];
        if(i)out<<';';
        out<<s.id<<','<<s.uid<<','<<s.stoneLevel<<','<<s.socketCount;
        for(int l:s.gemLevels)out<<','<<l;
    }
    return out.str();
}
template<class Stack> void decode(const char* ptr,std::vector<Stack>& stacks){
    if(!ptr)return;
    const std::string line(ptr,std::strcspn(ptr,"|\r\n"));
    std::unordered_map<int,Stack*> byUid;
    for(auto& s:stacks)if(s.uid>0)byUid[s.uid]=&s;
    std::size_t begin=0;
    while(begin<line.size()){
        const std::size_t end=line.find(';',begin);
        const auto vals=top::item_instance::parseRecord(line.substr(begin,end==std::string::npos?end:end-begin));
        if(vals.size()==7 && vals[1]>0 && vals[1]<=1000000000 && vals[2]>=1 && vals[2]<=9){
            const auto it=byUid.find(vals[1]);
            if(it!=byUid.end()&&it->second->id==vals[0]){
                Stack& s=*it->second;
                const std::array<int,3> levels{vals[4],vals[5],vals[6]};
                if(top::forge_rules::validSockets(vals[3],s.gemIds,levels)){
                    s.stoneLevel=vals[2];s.socketCount=vals[3];s.gemLevels=levels;
                }
            }
        }
        if(end==std::string::npos)break;
        begin=end+1;
    }
}
} // namespace top::forge_instance
