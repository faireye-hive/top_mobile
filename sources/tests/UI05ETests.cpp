#include "top/ActionPose.h"
#include <cassert>
#include <string>
#include <vector>

int main() {
    using namespace top;
    for (unsigned n = 0; n < 12; ++n) assert(cycleNormalAttack(n) == 7 + static_cast<int>(n % 3));
    assert((parsePoseActions("7,8,9") == std::vector<int>{7,8,9}));
    assert((parsePoseActions(" 11;12 ; 15 ") == std::vector<int>{11,12,15}));
    assert((parsePoseActions("-1") == std::vector<int>{}));
    assert((parsePoseActions("0,55,378,11") == std::vector<int>{11}));
    assert((parsePoseActions("7,x,11abc,12") == std::vector<int>{7,12}));
    const auto attack = parsePoseActions("7,8,9");
    assert(choosePoseAction(attack, 0, 7) == 7);
    assert(choosePoseAction(attack, 1, 7) == 8);
    assert(choosePoseAction(attack, 2, 7) == 9);
    assert(choosePoseAction(attack, 3, 7) == 7);
    assert(choosePoseAction({}, 0, 11) == 11);
    assert(!shouldRestartActionPose(2, 2, 14, 14, 7, 7));
    assert(shouldRestartActionPose(2, 2, 14, 14, 8, 7));
    assert(shouldRestartActionPose(1, 0, 0, 0, 8, 8)); // idle -> run starts at its first frame
    assert(!shouldRestartActionPose(1, 1, 0, 0, 8, 8)); // uninterrupted run keeps cycling
    assert(shouldRestartActionPose(0, 1, 0, 0, 8, 8)); // run -> idle resets its frame cursor
    assert(shouldRestartActionPose(1, 2, 14, 14, 8, 8));
    assert(weaponPoseAction(8, 3) == 170);
    assert(weaponPoseBase(weaponPoseAction(9, 6)) == 9);
    assert(weaponPoseGroup(weaponPoseAction(11, 4)) == 4);
    return 0;
}
