// SK2.18-L: retired variables cannot change the normal engine policy.
#include "top/HistoricalArrawTimeline.h"
#include "top/HistoricalBlast3Timeline.h"
#include "top/HistoricalBlastGroupTimeline.h"
#include "top/HistoricalBlastMotion.h"
#include "top/HistoricalBlend.h"
#include "top/HistoricalEffTimeline.h"
#include "top/HistoricalFireHit.h"
#include "top/HistoricalMapSceneEffects.h"
#include "top/HistoricalModelEffTimeline.h"
#include "top/HistoricalModelLifetime.h"
#include "top/HistoricalModelPosition.h"
#include "top/HistoricalModelTimeline.h"
#include "top/HistoricalParticleCount.h"
#include "top/HistoricalParticleExtended.h"
#include "top/HistoricalParticleJitter.h"
#include "top/HistoricalParticleMotion.h"
#include "top/HistoricalParticlePool.h"
#include "top/HistoricalParticleRandom.h"
#include "top/HistoricalRange2Collision.h"
#include "top/HistoricalRange2Ground.h"
#include "top/HistoricalRipple.h"
#include "top/HistoricalRoundTimeline.h"
#include "top/HistoricalShadeMesh.h"
#include "top/HistoricalShrink.h"
#include "top/HistoricalStripAlpha.h"
#include "top/HistoricalTextureFilter.h"
#include "top/LegacyDirectStrip.h"
#include "top/LegacyEffFormat.h"
#include "top/LegacyEffectTextureSelection.h"
#include "top/LegacyStripSystem.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#ifdef NDEBUG
#error This regression requires assertions
#endif

int main() {
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_PARTICLE_EXTENDED", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_PARTICLE_EXTENDED")==0);
        assert(top::historical_particle_extended::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_MODEL_FRAME_STEP", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_MODEL_FRAME_STEP")==0);
        assert(top::historical_model_timeline::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_PARTICLE_COUNT", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_PARTICLE_COUNT")==0);
        assert(top::historical_particle_count::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_MAP_SCENE_EFFECTS", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_MAP_SCENE_EFFECTS")==0);
        assert(top::historical_map_scene_effects::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_PARTICLE_MOTION", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_PARTICLE_MOTION")==0);
        assert(top::historical_particle_motion::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_MODEL_EFF_TIMELINE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_MODEL_EFF_TIMELINE")==0);
        assert(top::historical_model_eff_timeline::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_FILTERS", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_FILTERS")==0);
        assert(top::historical_filter::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_MODEL_LIFETIME", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_MODEL_LIFETIME")==0);
        assert(top::historical_model_lifetime::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_STRIP_PARENT_ALPHA", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_STRIP_PARENT_ALPHA")==0);
        assert(top::historical_strip_alpha::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_ARRAW_TIMELINE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_ARRAW_TIMELINE")==0);
        assert(top::historical_arraw_timeline::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_SHADE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_SHADE")==0);
        assert(top::historical_shade::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_SHADE_TERRAIN_GRID")==0);
        assert(top::historical_shade::terrainGridEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_RIPPLE_POOL", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_RIPPLE_POOL")==0);
        assert(top::historical_ripple::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_ROUND_TIMELINE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_ROUND_TIMELINE")==0);
        assert(top::historical_round_timeline::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_MODEL_POSITION", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_MODEL_POSITION")==0);
        assert(top::historical_model_position::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_SHRINK", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_SHRINK")==0);
        assert(top::historical_shrink::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_SHRINK_TIMELINE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_SHRINK_TIMELINE")==0);
        assert(top::historical_shrink::timelineEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_BLAST_GROUP_CYCLE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_BLAST_GROUP_CYCLE")==0);
        assert(top::historical_blast_group::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_PARTICLE_RANDF", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_PARTICLE_RANDF")==0);
        assert(top::historical_particle_random::randfMappingEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_PARTICLE_POOL", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_PARTICLE_POOL")==0);
        assert(top::historical_particle_pool::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_PARTICLE_EMITTER_TIMER", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_PARTICLE_EMITTER_TIMER")==0);
        assert(top::historical_particle_pool::sharedEmitterTimerEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_BLEND", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_BLEND")==0);
        assert(top::historic_blend::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_DEST_ALPHA", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_DEST_ALPHA")==0);
        assert(top::historic_blend::opaqueDestination());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION")==0);
        assert(top::historical_blast_motion::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_BLAST_ACTUAL_DT", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_BLAST_ACTUAL_DT")==0);
        assert(top::historical_blast_motion::actualDtEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_FIRE_HIT_EVENT", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_FIRE_HIT_EVENT")==0);
        assert(top::historical_fire_hit::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_STRIP_EFF_FRAME_STEP", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_STRIP_EFF_FRAME_STEP")==0);
        assert(top::historical_eff_timeline::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_STRIP_EFF_UV_CLOCK", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_STRIP_EFF_UV_CLOCK")==0);
        assert(top::historical_eff_timeline::controllerClocksEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_SNOW_FIRE_EFF_TIMELINE")==0);
        assert(top::historical_eff_timeline::snowFireEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_PARTICLE_FRAME_ACCELERATION", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_PARTICLE_FRAME_ACCELERATION")==0);
        assert(top::historical_particle_jitter::frameAccelerationEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_MODEL_TEXTURE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_MODEL_TEXTURE")==0);
        assert(top::legacy_effect_texture::historicalEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_RANGE2_COLLISION", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_RANGE2_COLLISION")==0);
        assert(top::historical_range2_collision::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_RANGE2_ACTUAL_DT", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_RANGE2_ACTUAL_DT")==0);
        assert(top::historical_range2_collision::actualDtEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT")==0);
        assert(top::historical_range2_ground::subgridHeightEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_BLAST3_TIMELINE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_BLAST3_TIMELINE")==0);
        assert(top::historical_blast3_timeline::enabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_CMPSTRIP_STRICT_STEP", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_CMPSTRIP_STRICT_STEP")==0);
        assert(top::legacy_direct_strip::strictStepEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_HISTORICAL_CMPSTRIP_LIFECYCLE", value, 1)==0);
        else assert(unsetenv("TOP_HISTORICAL_CMPSTRIP_LIFECYCLE")==0);
        assert(top::legacy_direct_strip::lifecycleEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_STRIP_NESTED_LOOP", value, 1)==0);
        else assert(unsetenv("TOP_STRIP_NESTED_LOOP")==0);
        assert(top::legacy_strip_system::historicalCyclesEnabled());
    }
    for (const char* value : {static_cast<const char*>(nullptr), "0", "1"}) {
        if (value) assert(setenv("TOP_EFF_TIME_MODE", value, 1)==0);
        else assert(unsetenv("TOP_EFF_TIME_MODE")==0);
        assert(top::legacy_eff::historicalTimeMode());
    }
    std::cout << "SK2.18-L normal engine policy is independent of retired environment variables\n";
}
