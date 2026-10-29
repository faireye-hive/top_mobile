// 05-AO: localizador de missoes; hitboxes devem coincidir com a UI procedural.
#include "top/MapQuestLayout.h"
#include "top/QuestPresentation.h"
#include <cassert>
int main() {
    using top::map_quest_layout::hit;
    const int w=1280,h=720,top=top::map_quest_layout::top(h);
    assert(hit(20,h-30,w,h,false,0,2)==0);
    assert(hit(40,top+68,w,h,false,0,2)==-1);
    assert(hit(40,top+69,w,h,true,0,2)==10);
    assert(hit(40,top+101,w,h,true,0,2)==11);
    assert(hit(40,top+133,w,h,true,0,2)==30); // nao existe terceira oferta
    assert(hit(376,top+12,w,h,true,0,2)==1);
    assert(hit(44,top+328,w,h,true,0,2)==20);
    assert(hit(240,top+328,w,h,true,0,2)==21);
    assert(hit(40,top+69,w,h,true,1,8)==10); // pagina seguinte
    assert(hit(400,top+200,w,h,true,0,2)==30);
    assert(hit(480,top+220,w,h,true,0,2)==-1);
    top::MapQuestView quest{1,"Welcome","Newbie Guide - Senna",2223.f,2785.f,14.f};
    assert(quest.id==1&&quest.npcName.find("Senna")!=std::string::npos);
    return 0;
}
