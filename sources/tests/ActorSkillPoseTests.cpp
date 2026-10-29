#include "top/ActorSkin.h"
#include "top/WeaponPose.h"
#include <iostream>
int main() {
    for(int slot:{6,9}) {
        if(top::weaponAttachmentDummy(slot,21,false,0)!=21)return 1;
        if(top::weaponAttachmentDummy(slot,21,true,0)!=slot)return 1;
        for(int pose:{2,5})if(top::weaponAttachmentDummy(slot,21,false,pose)!=slot)return 1;
        if(top::weaponAttachmentDummy(slot,-1,false,0)!=slot)return 1;
    }
    if(!top::ActorSkin::selfTest())return 1;
    std::cout<<"Actor: cast action 12 binds both weapons to animated hands in safe areas, advances with LAB frames and returns to sheathed idle\n";
}
