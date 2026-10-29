#pragma once

// Migracao 05-D: equivalente a __wield_state em CharacterModel.cpp (DX9).
// IDs de tipo em iteminfo.txt; slots 6/9 correspondem a esquerda/direita.
// CharacterPoseInfo.txt mapeia cada acao base para 7 grupos de 54 poses.
namespace top {

constexpr int kWeaponPoseGroups = 7;
constexpr int kWeaponPoseStride = 54;

// STAttack::_UseSkill calls SwitchFightPose even in a safe area. Keep the
// weapon on its animated hand dummy during attacks/casts, independently of
// the idle area's sheathing policy. This applies to both equipped hands.
inline int weaponAttachmentDummy(int slot, int backDummy, bool drawn, int pose) {
    const bool acting = pose == 2 || pose == 5;
    return (slot == 6 || slot == 9) && !drawn && !acting && backDummy >= 0
        ? backDummy : slot;
}

// Valores historicos de CharacterModel.cpp: nao confundir slot com tipo.
inline int weaponPoseMode(int leftType, int rightType) {
    struct WieldRule { int mode, left, right; };
    constexpr WieldRule rules[] = {
        {0, 0, 8}, {0, 0, 6}, {0, 11, 0},
        {1, 0, 1}, {1, 0, 5}, {1, 0, 9}, {1, 0, 10}, {1, 11, 1},
        {2, 0, 2}, {3, 1, 1}, {5, 3, 0}, {4, 0, 4},
        {6, 0, 7}, {1, 0, 18}, {1, 0, 19}
    };
    for (const auto& rule : rules) {
        if (rule.left == leftType && rule.right == rightType) return rule.mode;
    }
    return 0; // mesmo fallback de _BigPose2SmallPose do original
}

inline int weaponPoseGroup(int action) {
    return action >= 1 && action <= kWeaponPoseStride * kWeaponPoseGroups
        ? (action - 1) / kWeaponPoseStride : -1;
}

inline int weaponPoseBase(int action) {
    return action >= 1 && action <= kWeaponPoseStride * kWeaponPoseGroups
        ? (action - 1) % kWeaponPoseStride + 1 : 0;
}

inline int weaponPoseAction(int baseAction, int mode) {
    if (baseAction < 1 || baseAction > kWeaponPoseStride ||
        mode < 0 || mode >= kWeaponPoseGroups) return 0;
    return baseAction + mode * kWeaponPoseStride;
}

} // namespace top
