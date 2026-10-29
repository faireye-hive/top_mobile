#pragma once
#include <string>
#include <vector>
namespace top {
// Estado passivo para o HUD. A regra de aceitar/abandonar permanece em Play.
struct NpcQuestView {
    enum Status { Unavailable=0, Available=1, Active=2, Complete=3, Ready=4 };
    int id=0;
    std::string title, description, objective;
    std::vector<std::string> requirements;
    int rewardGold=0, rewardExp=0;
    Status status=Unavailable;
};
// Lista publica somente de missoes elegiveis do mapa corrente.
struct MapQuestView {
    int id=0;
    std::string title, npcName;
    float x=0.f,y=0.f, distance=0.f;
};
// Coordenadas compartilhadas entre hit-testing do Play e pintura da UI.
namespace npc_quest_layout {
constexpr int kRows=4;
constexpr int kFirstRowY=105;
constexpr int kRowPitch=29;
constexpr int kRowHeight=25;
inline int rowAt(int x,int y,int left,int top) {
    if(x<left+17 || x>=left+302 || y<top+kFirstRowY) return -1;
    int i=(y-top-kFirstRowY)/kRowPitch;
    if(i<0||i>=kRows || (y-top-kFirstRowY)%kRowPitch>=kRowHeight) return -1;
    return i;
}
} // namespace npc_quest_layout
} // namespace top
