#include "top/WeaponPose.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>
int main() {
    using top::weaponPoseMode;
    using top::weaponPoseAction;
    assert(weaponPoseMode(0, 0) == 0);
    assert(weaponPoseMode(0, 1) == 1);
    assert(weaponPoseMode(1, 1) == 3);
    assert(weaponPoseMode(0, 2) == 2);
    assert(weaponPoseMode(3, 0) == 5);
    assert(weaponPoseMode(0, 4) == 4);
    assert(weaponPoseMode(0, 7) == 6);
    assert(weaponPoseMode(11, 1) == 1);
    assert(weaponPoseMode(7, 1) == 0);
    assert(weaponPoseAction(1, 0) == 1);
    assert(weaponPoseAction(5, 1) == 59);
    assert(weaponPoseAction(7, 3) == 169);
    assert(weaponPoseAction(7, 6) == 331);
    assert(weaponPoseAction(55, 3) == 0);
    for (int mode = 0; mode < top::kWeaponPoseGroups; ++mode) {
        for (int action : {1, 5, 7, 16, 17, 54}) {
            const int actual = weaponPoseAction(action, mode);
            assert(top::weaponPoseGroup(actual) == mode);
            assert(top::weaponPoseBase(actual) == action);
        }
    }
    assert(top::weaponPoseGroup(379) == -1);
    assert(top::weaponPoseBase(0) == 0);
    std::puts("05-D: selecao historica de poses por armamento OK");
}
