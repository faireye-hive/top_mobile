#pragma once
// Geometria unica do localizador de missoes: hit test e desenho compartilham
// coordenadas. Nenhuma dependencia do OpenGL, testavel em headless.
#include <algorithm>
namespace top::map_quest_layout {
inline int top(int screenHeight) { return std::max(8,screenHeight-411); }
inline int hit(int x,int y,int screenWidth,int screenHeight,bool opened,int page,int count) {
    (void)screenWidth;
    if(x>=12&&x<148&&y>=screenHeight-41&&y<screenHeight-10)return 0;
    if(!opened)return -1;
    const int oy=top(screenHeight);
    if(x<12||x>=402||y<oy||y>=oy+354)return -1;
    if(x>=374&&y<oy+34)return 1;
    if(x>=28&&x<385&&y>=oy+68&&y<oy+68+7*32) {
        const int row=(y-oy-68)/32;
        if(row>=0&&row<7&&page*7+row<count)return 10+row;
    }
    if(y>=oy+315&&y<oy+347) {
        if(x>=30&&x<162)return 20;
        if(x>=227&&x<385)return 21;
    }
    return 30; // Bloqueia clique no chao por tras do painel.
}
} // namespace top::map_quest_layout
