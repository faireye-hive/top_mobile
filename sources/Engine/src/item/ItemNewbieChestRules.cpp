#include "top/ItemNewbieChestRules.h"

namespace top::item_newbie_chests {
namespace {
void give(Plan& plan, int id, int count=1, int itemEvent=0, int quality=0) {
    plan.rewards.push_back({id,count,itemEvent,quality});
}
// Essas familias de profissao constam explicitamente no NewbieChest Lua.
int group(int job) {
    if (job==1 || job==8 || job==9) return 1;
    if (job==2 || job==12) return 2;
    if (job==4 || job==16) return 4;
    if (job==5 || job==13 || job==14) return 5;
    return 0;
}
bool equipment(Plan& p, int chest, int groupId, int type) {
    if (groupId==1) {
        switch (chest) {
            case 437: give(p,10);give(p,296);return true;
            case 438: give(p,2);give(p,291);return true;
            case 439: give(p,14);give(p,297);return true;
            case 440: give(p,3);give(p,293);return true;
            case 441: give(p,12);give(p,298);return true;
        }
    } else if (groupId==2) {
        switch (chest) {
            case 437: give(p,32);give(p,311);return true;
            case 438: give(p,26);give(p,306);return true;
            case 439: give(p,33);give(p,313);return true;
            case 440: give(p,27);give(p,307);return true;
            case 441: give(p,34);give(p,314);return true;
        }
    } else if (groupId==4) {
        switch (chest) {
            case 437:
                give(p,80);
                if (type==1 || type==3) give(p,336);
                else if (type==4) {give(p,351);give(p,2202);}
                else return false;
                return true;
            case 438: give(p,74);if(type==1||type==3)give(p,338);else if(type==4)give(p,386);else return false;return true;
            case 439: give(p,81);if(type==1||type==3)give(p,337);else if(type==4)give(p,352);else return false;return true;
            case 440: give(p,75);if(type==1||type==3)give(p,340);else if(type==4)give(p,350);else return false;return true;
            case 441: give(p,82);if(type==1||type==3)give(p,339);else if(type==4)give(p,354);else return false;return true;
        }
    } else if (groupId==5) {
        switch (chest) {
            case 437:
                give(p,104);
                if(type==3)give(p,372);
                else if(type==4){give(p,359);give(p,2205);}
                else return false;
                return true;
            case 438: give(p,98);if(type==3)give(p,366);else if(type==4)give(p,381);else return false;return true;
            case 439: give(p,105);if(type==3)give(p,373);else if(type==4)give(p,360);else return false;return true;
            case 440: give(p,99);if(type==3)give(p,368);else if(type==4)give(p,389);else return false;return true;
            case 441: give(p,106);if(type==3)give(p,374);else if(type==4)give(p,361);else return false;return true;
        }
    }
    return false;
}
} // namespace

bool isHistoricalNewbieChest(int id, const std::string& callback) {
    return callback=="NewbieChest" && id>=436 && id<=443;
}
bool isHistoricalClassChest(int id, const std::string& callback) {
    return isHistoricalNewbieChest(id,callback) ||
        (id==985 && callback=="ItemUse_SANWUBOX") ||
        (id==986 && callback=="ItemUse_SIWUBOX") ||
        (id==987 && callback=="ItemUse_WUWUBOX") ||
        (id==988 && callback=="ItemUse_LIUWUBOX");
}
Result evaluate(int id, const std::string& callback, int level, int job, int type, bool boat) {
    if (!isHistoricalClassChest(id,callback)) return {Failure::NotNewbieChest,{}};
    const bool classicNewbie = isHistoricalNewbieChest(id,callback);
    if (classicNewbie && boat) return {Failure::Boat,{}};
    if (id>=985 && id<=988) {
        static constexpr int requiredLevels[] = {0,10,35,45,55};
        const int requiredLevel = requiredLevels[id-984];
        if (level < requiredLevel) return {Failure::Level,{}};
        Plan plan;
        plan.minimumFreeSlots=4;
        int equipmentId=0;
        int followupId=0;
        if (id==985) {
            if ((job==4 || job==16) && type!=2) equipmentId=803;
            else if (type==2 && job!=0) equipmentId=763;
            else if ((job==2 || job==12) && type!=2 && type!=4) equipmentId=777;
            else if ((job==5 || job==13 || job==14) && type!=1 && type!=2) equipmentId=789;
            else if (job==1 && type==1) equipmentId=1928;
            followupId=986;
        } else if (id==986) {
            if (job==9 && type==1) equipmentId=767;
            else if (job==8 && type==2) equipmentId=764;
            else if (job==12 && (type==1 || type==3)) equipmentId=778;
            else if (job==13 && (type==3 || type==4)) equipmentId=790;
            else if (job==14 && (type==3 || type==4)) equipmentId=796;
            else if (job==16 && (type==1 || type==3 || type==4)) equipmentId=804;
            followupId=987;
        } else if (id==987) {
            if (job==9 && type==1) equipmentId=768;
            else if (job==8 && type==2) equipmentId=765;
            else if (job==12 && (type==1 || type==3)) equipmentId=779;
            else if (job==13 && (type==3 || type==4)) equipmentId=791;
            else if (job==14 && (type==3 || type==4)) equipmentId=797;
            else if (job==16 && (type==1 || type==3 || type==4)) equipmentId=805;
            followupId=988;
        } else {
            if (job==9 && type==1) equipmentId=769;
            else if (job==8 && type==2) equipmentId=766;
            else if (job==12 && (type==1 || type==3)) equipmentId=780;
            else if (job==13 && (type==3 || type==4)) equipmentId=792;
            else if (job==14 && (type==3 || type==4)) equipmentId=798;
            else if (job==16 && (type==1 || type==3 || type==4)) equipmentId=806;
        }
        if (equipmentId<=0) return {job==0 ? Failure::Job : Failure::CharacterType,{}};
        give(plan,equipmentId,1,0,4);
        if (followupId>0) give(plan,followupId,1,0,4);
        return {Failure::None,plan};
    }
    if (boat) return {Failure::Boat,{}};
    const int requiredLevel=5*(id-435);
    if (level<requiredLevel) return {Failure::Level,{}};
    const int g=group(job);
    if (id>=437 && g==0) return {Failure::Job,{}};
    Plan plan;
    switch(id) {
        case 436:
            plan.minimumFreeSlots=4;plan.gold=2500;
            for(int item: {437,9,4308,444,6050})give(plan,item);
            break;
        case 437:
            plan.minimumFreeSlots=9;plan.gold=5000;
            if(!equipment(plan,id,g,type))return {Failure::CharacterType,{}};
            give(plan,438);give(plan,4602,10);give(plan,4603,10);
            give(plan,4604,10);give(plan,445);break;
        case 438:
            plan.minimumFreeSlots=6;plan.gold=7500;
            if(!equipment(plan,id,g,type))return {Failure::CharacterType,{}};
            for(int item:{439,4264,3351,3352,3353,446})give(plan,item);
            break;
        case 439:
            plan.minimumFreeSlots=6;plan.gold=10000;
            if(!equipment(plan,id,g,type))return {Failure::CharacterType,{}};
            for(int item:{447,440,3844})give(plan,item);
            break;
        case 440:
            plan.minimumFreeSlots=5;plan.gold=12500;
            if(!equipment(plan,id,g,type))return {Failure::CharacterType,{}};
            for(int item:{441,448,262})give(plan,item);
            break;
        case 441:
            plan.minimumFreeSlots=5;plan.gold=15000;
            if(!equipment(plan,id,g,type))return {Failure::CharacterType,{}};
            give(plan,442);give(plan,449);break;
        case 442:
            plan.minimumFreeSlots=3;plan.gold=17500;
            give(plan,443);give(plan,451);break;
        case 443: {
            if (job!=8 && job!=9 && job!=12 && job!=13 && job!=14 && job!=16)
                return {Failure::Job,{}};
            plan.minimumFreeSlots=4;plan.gold=200000;
            switch (job) {
                case 8: give(plan,300,1,95);give(plan,15,1,95);give(plan,301,1,95);give(plan,20,1,11);break;
                case 9: give(plan,295,1,95);give(plan,4,1,95);give(plan,302,1,95);give(plan,22,1,11);break;
                case 12: give(plan,39,1,95);give(plan,310,1,95);give(plan,315,1,95);give(plan,44,1,11);break;
                case 13:
                    give(plan,100,1,95);
                    if (type==3) { give(plan,370,1,95);give(plan,378,1,95); }
                    else { give(plan,392,1,95);give(plan,388,1,95); }
                    give(plan,1440,1,11);break;
                case 14:
                    give(plan,101,1,95);
                    if (type==3) { give(plan,367,1,95);give(plan,375,1,95); }
                    else { give(plan,390,1,95);give(plan,362,1,95); }
                    give(plan,107,1,11);break;
                case 16:
                    give(plan,76,1,95);
                    if (type==1 || type==3) { give(plan,341,1,95);give(plan,342,1,95); }
                    else { give(plan,353,1,95);give(plan,356,1,95); }
                    give(plan,83,1,11);break;
            }
            give(plan,450);give(plan,855,49);break;
        }
        default: return {Failure::Unsupported,{}};
    }
    return {Failure::None,plan};
}
} // namespace top::item_newbie_chests
