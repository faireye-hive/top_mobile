#include "top/SocketRules.h"
#include <cstdio>
#include <fstream>
#include <iostream>
int main(){
    using namespace top::socket_rules;
    const char* path="/tmp/top-05aa-socket-fixture.lua";
    {
        std::ofstream out(path);
        out<<"-- Server.Socket.Cost = 1 (comentario ignorado)\n";
        out<<"Server.Socket.Limit = 3   -- limite historico\n";
        out<<"Server.Socket.Cost = 50000  -- ouro por encaixe\n";
    }
    auto rules=load(path);
    std::remove(path);
    if(!rules.valid()||rules.limit!=3||rules.cost!=50000)return 1;
    if(cost(rules,0,3,10000)!=50000||cost(rules,1,3,10000)!=100000||
       cost(rules,2,3,10000)!=150000||cost(rules,3,3,10000)!=-1)return 2;
    if(cost(rules,0,2,10000)!=50000||cost(rules,2,2,10000)!=-1||
       cost(rules,0,0,10000)!=-1||cost(rules,0,3,25000)!=-1)return 3;
    if(!materials(890,891)||!materials(891,890)||materials(890,890)||
       materials(891,892))return 4;
    if(successChancePer10000(0)!=10000||successChancePer10000(1)!=10000||
       successChancePer10000(2)!=10000||successChancePer10000(3)!=0)return 5;
    if(load("/tmp/no-such-top-05aa-sockets.lua").valid())return 6;
    {
        std::ofstream out(path);
        out<<"Server.Socket.Limit = 9\nServer.Socket.Cost = 50000\n";
    }
    if(load(path).valid())return 7;
    std::remove(path);
    std::cout<<"05-AA: regras literais Lua e perfuracao historica OK\n";
    return 0;
}
