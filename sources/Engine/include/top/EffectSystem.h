#pragma once

// Efeitos do cliente antigo em OpenGL. A tabela magicsingleinfo escolhe o
// arquivo .eff; o leitor extrai os recursos legados e a simulacao gera
// billboards aditivos sem depender de DirectX ou do servidor.

#include "top/GfxDevice.h"
#include "top/HistoricalMapSceneEffects.h"
#include "top/MapFile.h"
#include "top/SkillVisual.h"
#include "top/MagicEffectCatalog.h"
#include "top/MagicGroupCatalog.h"
#include "top/SceneEffectCatalog.h"
#include "top/LegacyEffFormat.h"
#include "top/ModelFile.h"
#include "top/HistoricalMagicPath.h"
#include "top/HistoricalPartTrace.h"
#include "top/HistoricalMagicMotion.h"
#include "top/LegacyDirectStrip.h"
#include "top/EffectViewBasis.h"
#include "top/HistoricalParticlePool.h"
#include "top/HistoricalEffTimeline.h"
#include "top/HistoricalModelTimeline.h"
#include "top/HistoricalModelEffTimeline.h"
#include "top/HistoricalRipple.h"
#include "top/HistoricalArrawTimeline.h"
#include "top/HistoricalBlast3Timeline.h"
#include "top/HistoricalBlastMotion.h"
#include "top/HistoricalBlastGroupTimeline.h"
#include "top/HistoricalRange2Collision.h"
#include "top/HistoricalRoundTimeline.h"
#include "top/HistoricalDummyController.h"
#include "top/HistoricalShrink.h"
#include <limits>

#include <string>
#include <array>
#include <memory>
#include <vector>
#include <unordered_map>

namespace top {

struct EffectEvent {
    enum Kind { Cast = 0, SkillHit = 1, Hit = 2, Hurt = 3, Wake = 4, Flight = 5,
                CastBegin = 6, CastAction = 7, CastEnd = 8 };
    Kind kind = Hit;
    float fromX = 0.f;
    float fromY = 0.f;
    float toX = 0.f;
    float toY = 0.f;
    int variant = 0;
    float duration = 0.f; // trajectory: seconds until the impact keyframe
    // Default Unknown keeps target + ground, but never guesses water.
    SkillEffectSurface surface = SkillEffectSurface::Unknown;
    // Optional world heights supplied by the scene/dummy bridge. NaN retains
    // the previous terrain anchor for callers without a 3D attachment.
    float fromHeight = std::numeric_limits<float>::quiet_NaN();
    float toHeight = std::numeric_limits<float>::quiet_NaN();
    bool sourceIsPlayer = false; // Explicit origin qualification; never bind a monster to player bones.
    int targetActorIndex = -2; // -2 unknown, -1 player, >=0 Play/ActorSkin monster slot.
    int sourceActorIndex = -2; // -2 unknown, -1 player, >=0 monster slot; explicit owner for casts/flight
    bool isCast() const { return kind==Cast||kind==CastBegin||kind==CastAction||kind==CastEnd; }
    int castCallbackKeyFrame() const {
        return kind==CastBegin?-1:kind==CastAction?0:kind==CastEnd?-2:-3;
    }
};

struct EffectBatch {
    int mode = 4;
    int sourceBlend = 0; // 0 => legacy GL factors; else original D3DBLEND
    int destBlend = 0;
    int minFilter = 0; // D3DTEXF from .par, 0 => legacy GL_LINEAR
    int magFilter = 0;
    std::string texture;
    std::vector<MeshVertex> verts;
};

struct EffectReport {
    bool tableLoaded = false;
    bool legacyLoaded = false;
    std::string legacyFile;
    std::string textureFile;
    int references = 0;
    int particleFiles = 0;
    int particleSystems = 0;
    int skillVisualRecords = 0;
    int magicGroupRecords = 0; // 05-SK2.2: entries from magicgroupinfo.txt
    int magicEffectRecords = 0; // 05-G: entries from magicsingleinfo.txt
    int sceneEffectRecords = 0; // sceneffectinfo cast/target controller IDs
};

class EffectSystem {
public:
    bool open(const std::string& clientRoot);
    static bool selfTest();
    void emit(const EffectEvent& event);
    // Persistent equipment effects, separate from skill IDs and map placements.
    // visualId=0 removes only this slot. Repeating the same request is idempotent.
    bool setPlayerAttachedEffect(int slot, int visualId, int dummy);
    void setMapSceneEffects(const std::vector<MapSceneEffectPlacement>& placements);
    // Matrices sampled AFTER ActorSkin::build from the rendered pose (DX row
    // vectors, Z-up). Actor/target/item consumers select their enabled binding
    // branches; availability does not imply all owner types are supported.
    void setPlayerDummyMatrices(std::unordered_map<int, std::array<float, 16>> matrices);
    void setTargetDummyMatrices(
        std::unordered_map<int, std::unordered_map<int, std::array<float, 16>>> matrices);
    void setTargetActorRoots(std::unordered_map<int, std::array<float, 3>> roots);
    void setActorItemDummyMatrices(
        std::unordered_map<int, std::unordered_map<int, std::array<float, 16>>> matrices);
    // Moving actor effects follow this world-space origin even when dummy
    // sampling is disabled. Ground/target and item channels stay independent.
    void setPlayerCastAnchor(float worldX, float worldY, float feetHeight);
    // SK2.15-A: direct CMPStrip samples two real dummies, never invented offsets.
    // Body dummies are not interchangeable with equipped item model dummies.
    void setPlayerStripBodyMatrices(std::unordered_map<int,std::array<float,16>> matrices);
    void setPlayerStripItemMatrices(std::unordered_map<int,std::array<float,16>> matrices);
    void step(float dt, const MapFile* map = nullptr);
    void build(MapFile& map, float cameraRightX, float cameraRightY);
    void build(MapFile& map, const EffectViewBasis& view);
    const std::vector<EffectBatch>& batches() const { return batches_; }
    const EffectReport& report() const { return report_; }
    std::size_t activeCount() const { return live_.size(); }
    const SkillVisualCatalog& skillVisuals() const { return skillVisuals_; }
    const MagicEffectCatalog& magicEffects() const { return magicEffects_; }
    std::size_t cachedHistoricalEffects() const { return resourceCache_.size(); }

public:
    struct ParticleProfile {
        int type = 0;
        int count = 1;
        int sourceBlend = 0; // CMPPartSys serialized blend pair
        int destBlend = 0;
        int minFilter = 0; // D3DTEXF_POINT=1, LINEAR=2, 0 => fallback
        int magFilter = 0;
        float life = 1.f;
        int randomDivisor = 2; // DX9 CMPPartSys::_iRoadom, version-dependent
        float velocity = 1.f;
        float step = 0.f;
        float range[3]{};
        float direction[3]{};
        float acceleration[3]{};
        float offset[3]{}; // CMPPartSys::_vOffset (DX world X/Y/Z)
        float delayTime = 0.f; // CMPPartSys::_fDelayTime
        float playTime = 0.f;  // CMPPartSys::_fPlayTime: stop time since controller start, NOT duration after delay
        float pathVelocity = 0.f; // CEffPath::_fVel
        std::vector<std::array<float, 3>> pathPoints;
        std::vector<std::array<float, 3>> pathDirections;
        std::vector<float> pathDistances;
        std::vector<float> sizes;
        std::vector<float> rotations;  // 3 floats/keyframe no CMPPartCtrl
        std::vector<float> colors;
        int frameCount = 1;
        bool billboard = false;
        bool mediayEff = false; // v15 PAR: one nested CMPModelEff per particle
        bool modelDirection = false; // v14 PAR: disables MODEL/STRIP bone binding
        // Name of the historical .eff (or mesh) embedded in a .par system.
        // CMPPartSys::Create binds an effect model for type 5/6; the standalone
        // particle texture is NOT a substitute for that model's geometry.
        std::string modelName;
        std::string hitEffect; // CMPPartSys::m_strHitEff (version > 10)
        struct ModelTrack {
            legacy_eff::Subeffect subeffect;
            std::string texture;
            std::vector<std::string> frames;
            // External .lgo inside .par -> .eff. Shared among subeffects
            // referencing the same model, avoiding repeat heavy parses.
            std::shared_ptr<const Model> externalModel;
            float cycleLength = 0.f;
            std::size_t sourcePart = 0; // original EFF index, before unsupported tracks are skipped
        };
        std::vector<ModelTrack> modelTracks;
        std::string texture;
        std::size_t sourceSystem = 0; // original PAR index, before resource filtering
        std::string sourceParticle;
    };
private:
    const std::unordered_map<int,std::array<float,16>>& attachmentMatrices(int actor,bool item) const;
    struct VisualResource {
        std::string texture;
        std::vector<ParticleProfile> particles;
        std::vector<legacy_direct_strip::Definition> directStrips;
        struct BillboardTrack {
            legacy_eff::Subeffect subeffect;
            std::string resolvedTexture;
            std::vector<std::string> resolvedFrames;  // 05-J texture files by frame
            float cycleLength = 0.f; // max duration of this .eff, not this subeffect
        };
        std::vector<BillboardTrack> billboards;
        std::vector<BillboardTrack> nativeMeshes; // 05-K seven MindPower primitive names
        struct ExternalTrack {
            legacy_eff::Subeffect subeffect;
            Model model; // owned decoded .lgo (loaded only once per effect ID)
            std::string fallbackTexture;
            std::vector<std::string> resolvedFrames;
            float cycleLength = 0.f;
        };
        std::vector<ExternalTrack> externalMeshes; // 05-L: model/effect/*.lgo
    };
    const VisualResource& resourceFor(int visualId);
    const VisualResource& resourceForName(const std::string& name);
    // Projectile's Result controller belongs to the magic that launched it,
    // not to the normal skillinfo hit/ground/water channels.
    const VisualResource& resultForMagic(int magicId);
    void resolveNestedModels(VisualResource& resource);

    struct Instance {
        EffectEvent::Kind kind = EffectEvent::Hit;
        float age = 0.f;
        float life = 1.f;
        float expiresAfter = 1.f; // particle/effect tail; flight life remains trajectory time
        float fromX = 0.f;
        float fromY = 0.f;
        float toX = 0.f;
        float toY = 0.f;
        int variant = 0;
        int visualId = 0;
        unsigned seed = 1;
        historic_magic::Path path;
        historic_magic::ShadeTimeline fshadeTimeline;
        historical_part_trace::Timeline partTraceTimeline;
        bool partTraceRequested = false;
        bool partTraceFollowing = false;
        bool partTraceFinished = false;
        bool partTraceRetirePending = false;
        bool absoluteHeight = false;
        bool groundAnchored = false; // EffDelay's ground/water CreateEffect, not target dummy
        int actorDummy = -1; // actor cast channel dummy; -1 uses the historic fallback
        int itemDummy = -1; // equipped-item cast channel dummy
        int targetDummy = -1; // SkillVisual target channel dummy
        int targetActorIndex = -2; // -2 unknown, -1 player, >=0 monster slot
        bool targetActorFollow = false;
        bool actorCastFollow = false; // never bind hit/ground/item channels
        bool itemCastFollow = false;
        int sourceActorIndex = -2;
        std::vector<legacy_direct_strip::Trail> directTrails;
        bool directStripLogged=false;
        bool directStripMissingLogged=false;
        bool resultStarted = false; // only once per authored flight instance
        std::string namedResource; // SendResMessage particle controller name
        bool fixedPosition = false;
        historic_magic::Vec3 fixedWorldPosition{};
        bool mapScene = false;
        int attachedSlot = -1;
        std::uint64_t mapPlacementKey = 0;
        float mapSceneYaw = 0.f;
        std::vector<unsigned> range2HitGenerations;
        struct Range2FrameStep { float oldAge=0.f; float newAge=0.f; float dt=0.f; };
        std::vector<Range2FrameStep> range2FrameSteps;
        std::vector<historical_range2_collision::SystemTimeline> range2Timelines;
        std::vector<unsigned char> fireHitReported;
        std::vector<float> fireHitStopTimes;
        std::vector<historical_particle_pool::SystemTimeline> snowFireTimelines;
        std::vector<historical_eff_timeline::Group> snowFireEffTimelines;
        std::vector<historical_shrink::SystemTimeline> shrinkTimelines;
        std::vector<historical_model_timeline::Timeline> modelTimelines;
        std::vector<historical_eff_timeline::Group> modelEffTimelines;
        std::vector<historical_ripple::SystemTimeline> rippleTimelines;
        std::vector<historical_eff_timeline::Group> rippleEffTimelines;
        std::vector<historical_eff_timeline::Group> stripEffTimelines;
        std::vector<historical_eff_timeline::Group> arrawEffTimelines;
        std::vector<historical_blast3_timeline::SystemTimeline> blast3Timelines;
        std::vector<historical_eff_timeline::Group> blast3EffTimelines;
        std::vector<historical_blast_motion::SystemTimeline> blastMotionTimelines;
        std::vector<historical_blast_group::SystemTimeline> blastGroupTimelines;
        std::vector<historical_round_timeline::SystemTimeline> roundTimelines;
        std::vector<historical_eff_timeline::Group> roundEffTimelines;
        mutable std::vector<historical_dummy::Timeline> dummyTimelines;
    };

    std::string root_;
    std::string texture_;
    std::vector<ParticleProfile> castParticles_;
    std::vector<ParticleProfile> hitParticles_;
    EffectReport report_;
    SkillVisualCatalog skillVisuals_;
    MagicEffectCatalog magicEffects_;
    MagicGroupCatalog magicGroups_;
    SceneEffectCatalog sceneEffects_;
    std::unordered_map<int, VisualResource> resourceCache_;
    std::unordered_map<std::string, VisualResource> namedResourceCache_;
    std::vector<Instance> live_;
    std::vector<EffectBatch> batches_;
    std::unordered_map<int, std::array<float, 16>> playerDummyMatrices_;
    std::unordered_map<int, std::unordered_map<int, std::array<float, 16>>> targetDummyMatrices_;
    std::unordered_map<int, std::array<float, 3>> targetActorRoots_;
    std::unordered_map<int, std::unordered_map<int, std::array<float, 16>>> actorItemDummyMatrices_;
    std::unordered_map<int, std::array<float, 16>> stripBodyMatrices_, stripItemMatrices_;
    float playerCastX_ = 0.f, playerCastY_ = 0.f, playerCastFeet_ = 0.f;
    float mapSceneClock_ = 0.f;
    bool playerCastAnchorValid_ = false;
    unsigned serial_ = 1;
};

}  // namespace top
