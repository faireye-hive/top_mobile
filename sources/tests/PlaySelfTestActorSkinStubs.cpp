#include "top/ActorSkin.h"

// The Play::selfTest target is deliberately headless. Its gameplay checks pass
// no ActorSkin instance to summon code, so these graphics-only entry points
// have inert definitions solely to satisfy the linker for that target.
namespace top {

int ActorSkin::appendMonsterModel(int, bool) { return -1; }

ActorActionTiming ActorSkin::monsterActionTiming(int, int) const { return {}; }

} // namespace top
