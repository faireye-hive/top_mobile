#pragma once
#include "top/EffectSystem.h"
#include <cstdlib>

namespace top::historical_skill_preview_binding {
inline void qualifyPlayer(EffectEvent& event) {
    // Preview cast originates at the rendered player. Hit preview is emitted
    // at that same player's root; -1 is its existing scene bridge identity.
    // Do not invent an actor for the artificial flight endpoint.
    if(event.isCast() || event.kind==EffectEvent::Flight)event.sourceIsPlayer=true;
    if(event.kind==EffectEvent::SkillHit)event.targetActorIndex=-1;
}
}
