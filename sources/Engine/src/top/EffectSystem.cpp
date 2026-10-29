#include "top/EffectSystem.h"
#include "top/HistoricalRange2Ground.h"
#include "top/HistoricalShrink.h"
#include "top/TexturePaths.h"
#include "top/LegacyParticleCurves.h"
#include "top/LegacyEffectInspector.h"
#include "top/LegacyEffectGeometry.h"
#include "top/LegacyExternalEffectMesh.h"
#include "top/HistoricalBlend.h"
#include "top/HistoricalTextureFilter.h"
#include "top/HistoricalMagicMotion.h"
#include "top/LegacyStripSystem.h"
#include "top/HistoricalParticlePool.h"
#include "top/HistoricalParticleMotion.h"
#include "top/HistoricalParticleRandom.h"
#include "top/HistoricalParticleExtended.h"
#include "top/HistoricalParticleJitter.h"
#include "top/HistoricalRange2Collision.h"
#include "top/HistoricalFireHit.h"
#include "top/HistoricalParticleCount.h"
#include "top/HistoricalShadeMesh.h"
#include "top/HistoricalStripAlpha.h"
#include "top/HistoricalBlastMotion.h"
#include "top/HistoricalModelPosition.h"
#include "top/HistoricalModelLifetime.h"
#include "top/LegacyEffectTextureSelection.h"
#include "top/EffectBindingTrace.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <tuple>
#include <utility>

namespace top {
namespace {

constexpr float kPi = 3.14159265358979323846f;

// Legacy shader/mode selection for the explicit compatibility path. With
// historical rendering enabled, the authored factor pair is applied by GL;
// mode 8 is not a translation of D3D factor 7 (which is DESTALPHA).
int historicBlendMode(const legacy_eff::Subeffect& part) {
    return part.sourceBlend == 5 && part.destBlend == 7 ? 8 : 4;
}

// Historical factors affect only effect batches. UI, terrain, actor and
// transparent selection-ring state remain untouched.
void setEffectBlend(EffectBatch& batch, int source, int destination) {
    const auto effective = historic_blend::effective(source, destination);
    batch.sourceBlend = effective.source;
    batch.destBlend = effective.destination;
}
bool sameEffectBlend(const EffectBatch& batch, int source, int destination) {
    const auto effective = historic_blend::effective(source, destination);
    return batch.sourceBlend == effective.source && batch.destBlend == effective.destination;
}
// Sampling is per .par system, not per texture. The same image can be
// referenced from two systems with different point/linear settings.
void setEffectFilter(EffectBatch& batch, int min, int mag) {
    const auto pair = historical_filter::effective(min, mag);
    batch.minFilter = pair.min;
    batch.magFilter = pair.mag;
}
bool sameEffectFilter(const EffectBatch& batch, int min, int mag) {
    return historical_filter::same({batch.minFilter, batch.magFilter},
                                   historical_filter::effective(min, mag));
}
bool historicalRawEffModelParams() {
    const char* value = std::getenv("TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS");
    return value && value[0] == '1' && value[1] == '\0';
}

constexpr bool historicalItemDummyBindingEnabled() { return true; }

constexpr bool historicalItemEffectRequireDummyEnabled() { return true; }

constexpr bool historicalTargetDummyBindingEnabled() { return true; }

constexpr bool historicalPartTraceEnabled() { return true; }

constexpr bool historicalBoundBillboardCenterEnabled() { return true; }

constexpr bool historicalBoundModelMatrixEnabled() { return true; }

constexpr bool historicalMagicResultStartsOnArrival() { return true; }

constexpr bool historicalFshadeFrameCountEnabled() { return true; }

// Temporary visual calibration for Frozen Arrow's historical flight meshes.
// Applies to all its authored models, not to unrelated spells. The asset's
// authored size/animation remain intact; this only changes world units.
float frozenArrowMultiplier(int skillId, int effectId) {
    if (skillId != 93 || (effectId != 1020 && effectId != 296)) return 1.f;
    float multiplier=1.f; // original-sized Frozen Arrow; do not mask missing result FX
    if (const char* raw=std::getenv("TOP_FROZEN_ARROW_FX_SCALE")) {
        char* end=nullptr;
        const float value=std::strtof(raw,&end);
        if (end!=raw && *end=='\0' && std::isfinite(value) && value>=.5f && value<=6.f)
            multiplier=value;
    }
    return multiplier;
}

bool effectModelName(const std::string& name) {
    if(name.size()<5) return false;
    std::string suffix=name.substr(name.size()-4);
    std::transform(suffix.begin(),suffix.end(),suffix.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return suffix==".eff";
}

bool readLegacy(const std::string& path, std::vector<std::string>& names) {
    LegacyEffectSummary parsed;
    if (!inspectLegacyEff(path, parsed, historicalRawEffModelParams())) return false;
    if (parsed.structured) {
        names = parsed.verifiedResources;
    } else {
        // Conservative fallback for historical variants not yet decoded:
        // keep only recognizable filenames, never random binary fragments.
        for (const auto& item : parsed.candidates) {
            const std::string& candidate = item.candidate;
            if (candidate.size() < 4 || !MagicEffectCatalog::safeName(candidate) ||
                !std::isalnum(static_cast<unsigned char>(candidate.front()))) continue;
            const bool hexOnly = std::all_of(candidate.begin(), candidate.end(), [](unsigned char c) {
                return std::isxdigit(c) != 0;
            });
            if (hexOnly) continue;
            const auto dot = candidate.rfind('.');
            if (dot != std::string::npos) {
                const std::string ext = candidate.substr(dot);
                if (ext != ".eff" && ext != ".par" && ext != ".tga" &&
                    ext != ".dds" && ext != ".png") continue;
            }
            names.push_back(candidate);
        }
    }
    return true;
}

struct BinaryReader {
    const std::vector<unsigned char>& data;
    std::size_t at = 0;
    template <typename T> bool get(T& out) {
        if (at + sizeof(T) > data.size()) return false;
        std::memcpy(&out, data.data() + at, sizeof(T));
        at += sizeof(T);
        return true;
    }
    bool skip(std::size_t bytes) {
        if (at + bytes > data.size()) return false;
        at += bytes;
        return true;
    }
    bool name(std::string& out) {
        if (at + 32 > data.size()) return false;
        const char* text = reinterpret_cast<const char*>(data.data() + at);
        std::size_t size = 0;
        while (size < 32 && text[size]) ++size;
        out.assign(text, size);
        at += 32;
        return true;
    }
};

bool loadParticles(const std::string& path, const std::string& root, std::vector<EffectSystem::ParticleProfile>& out,
                   std::vector<legacy_direct_strip::Definition>* direct = nullptr) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (length < 44 || length > 8 * 1024 * 1024) { std::fclose(file); return false; }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    const bool read = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    if (!read) return false;
    BinaryReader in{bytes};
    unsigned version = 0;
    std::string controller;
    int systems = 0;
    float controllerLife = 0.f;
    if (!in.get(version) || version < 2 || version > 15 || !in.name(controller) || !in.get(systems) ||
        systems < 0 || systems > 100 || (version >= 3 && !in.get(controllerLife))) return false;
    std::vector<EffectSystem::ParticleProfile> decoded;
    for (int system = 0; system < systems; ++system) {
        EffectSystem::ParticleProfile profile;
        profile.sourceSystem=static_cast<std::size_t>(system);
        profile.sourceParticle=path.substr(path.find_last_of("/\\")+1);
        std::string systemName, textureName, modelName, unusedModelRangeName;
        unsigned short frames = 0;
        if (!in.get(profile.type) || !in.name(systemName) || !in.get(profile.count) || !in.name(textureName) ||
            !in.name(modelName) || !in.get(profile.range[0]) || !in.get(profile.range[1]) || !in.get(profile.range[2]) ||
            !in.get(frames) || frames == 0 || frames > 256) return false;
        profile.sizes.resize(frames);
        profile.frameCount = static_cast<int>(frames);
        for (float& size : profile.sizes) if (!in.get(size)) return false;
        profile.rotations.resize(static_cast<std::size_t>(frames) * 3);
        for (float& angle : profile.rotations) if (!in.get(angle)) return false;
        profile.colors.resize(static_cast<std::size_t>(frames) * 4);
        for (float& color : profile.colors) if (!in.get(color)) return false;
        unsigned char flag = 0;
        if (!in.get(flag)) return false;
        profile.billboard=flag!=0;
        if (!in.get(profile.sourceBlend) || !in.get(profile.destBlend)) return false;
        // D3DTEXF minification/magnification: preserve the authored pair.
        if (!in.get(profile.minFilter) || !in.get(profile.magFilter)) return false;
        if (!in.get(profile.life) || !in.get(profile.velocity) || !in.get(profile.direction[0]) ||
            !in.get(profile.direction[1]) || !in.get(profile.direction[2]) || !in.get(profile.acceleration[0]) ||
            !in.get(profile.acceleration[1]) || !in.get(profile.acceleration[2]) || !in.get(profile.step)) return false;
        bool modelRange = false;
        if (version > 3 && (!in.get(flag) || !in.name(unusedModelRangeName))) return false;
        if (version > 3) modelRange = flag != 0;
        if (version > 4 && (!in.get(profile.offset[0]) || !in.get(profile.offset[1]) ||
                            !in.get(profile.offset[2]))) return false;
        if (version > 5 && (!in.get(profile.delayTime) || !in.get(profile.playTime))) return false;
        if (version > 8) {
            if (!in.get(flag)) return false;
            if (flag) {
                int points = 0;
                if (!in.get(points) || !in.get(profile.pathVelocity) || points < 0 || points > 200) return false;
                profile.pathPoints.resize(static_cast<std::size_t>(points));
                for (auto& point : profile.pathPoints)
                    for (float& coordinate : point) if (!in.get(coordinate)) return false;
                if (points >= 2) {
                    profile.pathDirections.resize(static_cast<std::size_t>(points - 1));
                    profile.pathDistances.resize(static_cast<std::size_t>(points - 1));
                    for (std::size_t segment = 0; segment + 1 < profile.pathPoints.size(); ++segment) {
                        for (float& coordinate : profile.pathDirections[segment])
                            if (!in.get(coordinate)) return false;
                        // CMPPartSys writes the distance as a 12-byte vector;
                        // only X is consumed by CEffPath::FrameMove. Read all
                        // three floats to keep subsequent v10+ fields aligned.
                        float distanceData[3]{};
                        for (float& value : distanceData) if (!in.get(value)) return false;
                        profile.pathDistances[segment] = distanceData[0];
                    }
                }
            }
        }
        if (version > 9 && !in.get(flag)) return false;
        if (version > 10 && !in.name(profile.hitEffect)) return false;
        if (version > 11 && modelRange) {
            unsigned short points = 0;
            if (!in.get(points) || !in.skip(static_cast<std::size_t>(points) * 12)) return false;
        }
        int serializedDivisor = 0;
        if (version > 12 && !in.get(serializedDivisor)) return false;
        profile.randomDivisor = historical_particle_random::decodedDivisor(
            version, profile.type, serializedDivisor);
        if (version > 13) {
            if (!in.get(flag)) return false;
            profile.modelDirection = flag != 0;
        }
        if (version > 14) {
            if (!in.get(flag)) return false;
            profile.mediayEff = flag != 0;
        }
        // Rejeitar sequencias NaN/infinitas antes de liberar dados para
        // a renderizacao. A leitura deve ser atomica: nunca devolver metade
        // de um controlador truncado como se ele estivesse valido.
        auto valid = [](const std::vector<float>& values) {
            return std::all_of(values.begin(), values.end(), [](float v){ return std::isfinite(v); });
        };
        if (!valid(profile.sizes) || !valid(profile.rotations) || !valid(profile.colors)) return false;
        profile.modelName = modelName;
        auto finite3 = [](const float (&v)[3]) {
            return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
        };
        if (!std::isfinite(profile.life) || !std::isfinite(profile.velocity) ||
            !std::isfinite(profile.step) || !std::isfinite(profile.delayTime) ||
            !std::isfinite(profile.playTime) || !std::isfinite(profile.pathVelocity) || !finite3(profile.range) ||
            !finite3(profile.offset) ||
            !finite3(profile.direction) || !finite3(profile.acceleration)) return false;
        for (const auto& point : profile.pathPoints)
            if (!std::all_of(point.begin(), point.end(), [](float v){ return std::isfinite(v); })) return false;
        for (const auto& direction : profile.pathDirections)
            if (!std::all_of(direction.begin(), direction.end(), [](float v){ return std::isfinite(v); })) return false;
        for (float distance : profile.pathDistances)
            if (!std::isfinite(distance) || distance <= 0.f) return false;
        profile.count = std::max(1, std::min(profile.count, 100));
        if (profile.life <= 0.f) profile.life = controllerLife > 0.f && std::isfinite(controllerLife) ? controllerLife : 1.f;
        profile.life = std::min(profile.life, 60.f);
        std::transform(textureName.begin(), textureName.end(), textureName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (textureName.find('.') == std::string::npos) textureName += ".tga";
        if (MagicEffectCatalog::safeName(textureName))
            profile.texture = resolveTexturePath(root + "/texture/effect/" + textureName);
        decoded.push_back(std::move(profile));
    }
    std::vector<legacy_direct_strip::Definition> decodedStrips;
    if (!legacy_direct_strip::decodeTail(bytes,in.at,version,decodedStrips)) return false;
    for (auto& strip : decodedStrips) {
        if (!MagicEffectCatalog::safeName(strip.texture))return false;
        std::string name=strip.texture;
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
        std::string resolved=resolveTexturePath(root+"/texture/effect/"+name+".tga");
        if(resolved.empty())resolved=resolveTexturePath(root+"/texture/effect/"+name+".dds");
        strip.texture=std::move(resolved);
    }
    if (decoded.empty() && decodedStrips.empty()) return false;
    if(direct)direct->insert(direct->end(),std::make_move_iterator(decodedStrips.begin()),
                             std::make_move_iterator(decodedStrips.end()));
    out.insert(out.end(), std::make_move_iterator(decoded.begin()), std::make_move_iterator(decoded.end()));
    return true;
}

// Preserve a model's own texture when it is available. Never infer a
// filename from an unrelated skill ID or a missing legacy binary.
std::string textureFromName(const std::string& root, const std::string& name) {
    if (!MagicEffectCatalog::safeName(name)) return {};
    std::string base = name;
    const auto dot = base.rfind('.');
    if (dot != std::string::npos) base.resize(dot);
    for (const std::string& candidate : {name, base + ".tga", base + ".dds", base + ".png"}) {
        const std::string path = resolveTexturePath(root + "/texture/effect/" + candidate);
        if (!path.empty()) return path;
    }
    return {};
}

float noise(unsigned seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return static_cast<float>(seed & 0xffffu) / 65535.f;
}

std::array<float, 3> pathPosition(const EffectSystem::ParticleProfile& profile, float age) {
    if (profile.pathPoints.size() < 2 ||
        profile.pathDirections.size() + 1 != profile.pathPoints.size() ||
        profile.pathDistances.size() != profile.pathDirections.size()) return {0.f, 0.f, 0.f};
    float remaining = std::max(0.f, profile.pathVelocity * std::max(0.f, age));
    std::size_t segment = 0;
    // CEffPath loops back to point zero after its final segment.
    while (remaining >= profile.pathDistances[segment]) {
        remaining -= profile.pathDistances[segment];
        segment = (segment + 1) % profile.pathDistances.size();
    }
    std::array<float, 3> result{};
    for (std::size_t axis = 0; axis < result.size(); ++axis)
        result[axis] = profile.pathPoints[segment][axis] +
                       profile.pathDirections[segment][axis] * remaining;
    return result;
}

MeshVertex vertex(float x, float y, float z, float r, float g, float b, float a, float u, float v) {
    MeshVertex out{};
    out.x = x;
    out.y = y;
    out.z = z;
    out.r = r;
    out.g = g;
    out.b = b;
    out.a = a;
    out.u = u;
    out.v = v;
    return out;
}

void quad(EffectBatch& batch, float x, float y, float z, float size,
          const EffectViewBasis& view, float r, float g, float b, float alpha) {
    auto corner=[&](float horizontal,float vertical,float u,float v) {
        return vertex(x+size*(horizontal*view.right[0]+vertical*view.up[0]),
                      y+size*(horizontal*view.right[1]+vertical*view.up[1]),
                      z+size*(horizontal*view.right[2]+vertical*view.up[2]),r,g,b,alpha,u,v);
    };
    const auto a=corner(-1,-1,0,1), e=corner(1,-1,1,1);
    const auto d=corner(-1,1,0,0), c=corner(1,1,1,0);
    batch.verts.insert(batch.verts.end(), {a, e, d, e, c, d});
}

// Rotate within the inverse-view basis, preserving the authored aspect ratio.
void quadRotated(EffectBatch& batch, float x, float y, float z, float size,
                 const EffectViewBasis& view, float rotation,
                 float r, float g, float b, float alpha,
                 const legacy_eff::Subeffect::QuadUV* uv = nullptr,
                 float verticalSize = std::numeric_limits<float>::quiet_NaN()) {
    const float height=std::isfinite(verticalSize)?verticalSize:size;
    if (std::fabs(rotation) < 0.00001f && !uv && height==size) {
        quad(batch, x, y, z, size, view, r, g, b, alpha);
        return;
    }
    const auto fallback = legacy_eff::defaultQuadUV();
    const auto& tex = uv ? *uv : fallback;
    const float c = std::cos(rotation);
    const float s = std::sin(rotation);
    auto corner = [&](float right, float up, float u, float v) {
        const float horizontal = right * size * c - up * height * s;
        const float vertical = right * size * s + up * height * c;
        return vertex(x + view.right[0]*horizontal + view.up[0]*vertical,
                      y + view.right[1]*horizontal + view.up[1]*vertical,
                      z + view.right[2]*horizontal + view.up[2]*vertical, r, g, b, alpha, u, v);
    };
    const MeshVertex a = corner(-1.f, -1.f, tex[0][0], tex[0][1]);
    const MeshVertex e = corner( 1.f, -1.f, tex[3][0], tex[3][1]);
    const MeshVertex d = corner(-1.f,  1.f, tex[1][0], tex[1][1]);
    const MeshVertex c1 = corner(1.f,  1.f, tex[2][0], tex[2][1]);
    batch.verts.insert(batch.verts.end(), {a, e, d, e, c1, d});
}

void groundQuad(EffectBatch& batch, float x, float y, float z, float size, float r, float g, float b, float alpha) {
    const MeshVertex a = vertex(x - size, y, z - size, r, g, b, alpha, 0.f, 1.f);
    const MeshVertex c = vertex(x + size, y, z + size, r, g, b, alpha, 1.f, 0.f);
    const MeshVertex d = vertex(x - size, y, z + size, r, g, b, alpha, 0.f, 0.f);
    const MeshVertex e = vertex(x + size, y, z - size, r, g, b, alpha, 1.f, 1.f);
    batch.verts.insert(batch.verts.end(), {a, e, d, e, c, d});
}

void groundQuadRotated(EffectBatch& batch, float x, float y, float z, float halfSize,
                       float angle, float r, float g, float b, float alpha) {
    const float c=std::cos(angle), s=std::sin(angle);
    auto point=[&](float localX,float localZ,float u,float v) {
        // DX9's row-vector Z rotation maps to GL world X/Z at this boundary.
        return vertex(x+localX*c-localZ*s,y,z+localX*s+localZ*c,
                      r,g,b,alpha,u,v);
    };
    const MeshVertex a=point(-halfSize,-halfSize,0.f,1.f);
    const MeshVertex e=point( halfSize,-halfSize,1.f,1.f);
    const MeshVertex d=point(-halfSize, halfSize,0.f,0.f);
    const MeshVertex c1=point(halfSize,halfSize,1.f,0.f);
    batch.verts.insert(batch.verts.end(),{a,e,d,e,c1,d});
}

void ribbon(EffectBatch& batch, float x0, float y0, float z0, float x1, float y1, float z1, float width,
            float r, float g, float b, float alpha) {
    const MeshVertex a = vertex(x0, y0 - width, z0, r, g, b, alpha, 0.f, 1.f);
    const MeshVertex d = vertex(x0, y0 + width, z0, r, g, b, alpha, 0.f, 0.f);
    const MeshVertex e = vertex(x1, y1 - width, z1, r, g, b, alpha, 1.f, 1.f);
    const MeshVertex c = vertex(x1, y1 + width, z1, r, g, b, alpha, 1.f, 0.f);
    batch.verts.insert(batch.verts.end(), {a, e, d, e, c, d});
}

}  // namespace

bool EffectSystem::open(const std::string& clientRoot) {
    if (const char* trace=std::getenv("TOP_TRACE_SKILL_FX")) {
        if (*trace && *trace!='0')
            std::fprintf(stderr, "[eff-timeline] mode=%s | source=DX9 CMPModelEff::FrameMove\n",
                         legacy_eff::historicalTimeMode() ? "duration-dx9" : "absolute-legacy-gl");
    }
    root_ = clientRoot;
    texture_.clear(); // open() repetido nao deve reutilizar textura do mapa anterior
    resourceCache_.clear();
    namedResourceCache_.clear();
    magicEffects_.load(root_ + "/scripts/table/magicsingleinfo.txt");
    magicGroups_.load(root_ + "/scripts/table/magicgroupinfo.txt");
    sceneEffects_.load(root_ + "/scripts/table/sceneffectinfo.txt");
    live_.clear();
    mapSceneClock_ = 0.f;
    batches_.clear();
    report_ = {};
    castParticles_.clear();
    hitParticles_.clear();
    skillVisuals_.load(root_ + "/scripts/table/skillinfo.txt");
    report_.skillVisualRecords = static_cast<int>(skillVisuals_.size());
    report_.magicEffectRecords = static_cast<int>(magicEffects_.size());
    report_.magicGroupRecords = static_cast<int>(magicGroups_.size());
    report_.sceneEffectRecords = static_cast<int>(sceneEffects_.size());
    const auto* defaultEffect = magicEffects_.find(38);
    const std::string effectName = defaultEffect && !defaultEffect->models.empty() ? defaultEffect->models.front() : std::string{};
    report_.tableLoaded = !effectName.empty();
    report_.legacyFile = effectName;
    if (effectName.empty()) return false;

    std::string path = root_ + "/effect/" + effectName;
    std::vector<std::string> refs;
    report_.legacyLoaded = readLegacy(path, refs);
    report_.references = static_cast<int>(refs.size());

    // O .eff guarda nomes de subefeitos e, em algumas versoes, o nome-base
    // da textura sem extensao. Resolve primeiro esses nomes e conserva um
    // fallback do mesmo conjunto visual do Spiritual Bolt.
    for (const std::string& ref : refs) {
        std::string base = ref;
        const std::size_t dot = base.find('.');
        if (dot != std::string::npos) base.resize(dot);
        const std::string candidates[] = {base + ".tga", base + ".TGA", base + ".dds", base + ".png"};
        for (const std::string& name : candidates) {
            const std::string candidate = resolveTexturePath(root_ + "/texture/effect/" + name);
            if (!candidate.empty()) {
                texture_ = candidate;
                break;
            }
        }
        if (!texture_.empty()) break;
    }
    if (texture_.empty()) {
        const char* fallback[] = {"hpush01.tga", "firelighting02.tga", "eff0212.tga"};
        for (const char* name : fallback) {
            const std::string candidate = resolveTexturePath(root_ + "/texture/effect/" + name);
            if (!candidate.empty()) {
                texture_ = candidate;
                break;
            }
        }
    }
    report_.textureFile = texture_;
    auto particlePath = [&](const char* name) {
        std::string path = root_ + "/effect/" + name;
        return path;
    };
    if (loadParticles(particlePath("hpushf.par"), root_, castParticles_)) ++report_.particleFiles;
    if (loadParticles(particlePath("hpusht.par"), root_, hitParticles_)) ++report_.particleFiles;
    report_.particleSystems = static_cast<int>(castParticles_.size() + hitParticles_.size());
    return report_.legacyLoaded && !texture_.empty();
}

// Lazy resource resolution: common effects load only when first used.
// Missing or partially converted historical files intentionally fall back
// to the generic 05-F billboards instead of hiding the ability animation.
const EffectSystem::VisualResource& EffectSystem::resourceFor(int visualId) {
    const auto existing = resourceCache_.find(visualId);
    if (existing != resourceCache_.end()) return existing->second;
    // Bounded cache for modded/invalid effect IDs, not an unbounded set.
    if (resourceCache_.size() >= 128) resourceCache_.clear();
    VisualResource resolved;
    if (const SceneEffectDefinition* sceneEffect = sceneEffects_.find(visualId)) {
        std::vector<ParticleProfile> particles;
        if (loadParticles(root_ + "/effect/" + sceneEffect->particle, root_, particles, &resolved.directStrips)) {
            for (auto& profile : particles) {
                if (resolved.particles.size() >= 64) break;
                if (!profile.texture.empty() || (effectModelName(profile.modelName) &&
                    MagicEffectCatalog::safeName(profile.modelName)))
                    resolved.particles.push_back(std::move(profile));
            }
        }
    }
    // skillinfo stores magicsingleinfo references as 1000 + table ID;
    // sceneffectinfo references retain their own unprefixed namespace.
    const int magicId = visualId >= 1000 && visualId < 1100 ? visualId - 1000 : visualId;
    const MagicEffectDefinition* definition = magicEffects_.find(magicId);
    if (definition) {
        for (const std::string& model : definition->models) {
            if (!MagicEffectCatalog::safeName(model)) continue;
            LegacyEffectSummary parsed;
            if (!inspectLegacyEff(root_ + "/effect/" + model, parsed,
                                  historicalRawEffModelParams())) continue;
            std::vector<std::string> refs;
            if (parsed.structured) {
                refs = parsed.verifiedResources;
                float cycleLength = 0.f;
                for (const auto& part : parsed.document.parts)
                    cycleLength = std::max(cycleLength, legacy_eff::duration(part));
                for (const auto& part : parsed.document.parts) {
                    // Only native billboard effect tracks can be represented
                    // by a textured OpenGL quad. Other historical meshes
                    // require the separate DX model geometry converter.
                    if (part.keys.empty()) continue;
                    const bool native = !part.billboard && legacy_mesh::recognized(part.model);
                    // External effect models are lwGeomObjInfo .lgo files.
                    // Only load existing, valid meshes from model/effect.
                    const bool external = !part.billboard && !native &&
                        !legacy_external::filename(part.model).empty();
                    if (!part.billboard && !native && !external) continue;
                    if (external) {
                        Model decoded;
                        if (!legacy_external::load(root_, part.model, decoded)) continue;
                        std::vector<std::string> frames;
                        frames.reserve(part.frames.size());
                        for (const auto& frame : part.frames)
                            frames.push_back(textureFromName(root_, frame));
                        resolved.externalMeshes.push_back({part, std::move(decoded),
                            textureFromName(root_, part.texture), std::move(frames), cycleLength});
                        continue;
                    }
                    if (native) {
                        legacy_mesh::Primitive validation;
                        if (!legacy_mesh::make(part.model, legacy_mesh::paramsAt(part, 0.f), validation))
                            continue; // unsafe mesh dimensions or unsupported source
                    }
                    std::string resolvedTexture = textureFromName(root_, part.texture);
                    if (resolvedTexture.empty()) {
                        for (const auto& frame : part.frames) {
                            resolvedTexture = textureFromName(root_, frame);
                            if (!resolvedTexture.empty()) break;
                        }
                    }
                    if (resolvedTexture.empty()) continue;
                    std::vector<std::string> resolvedFrames;
                    resolvedFrames.reserve(part.frames.size());
                    for (const auto& frame : part.frames)
                        resolvedFrames.push_back(textureFromName(root_, frame));
                    if (native) resolved.nativeMeshes.push_back({part, std::move(resolvedTexture), std::move(resolvedFrames), cycleLength});
                    else resolved.billboards.push_back({part, std::move(resolvedTexture), std::move(resolvedFrames), cycleLength});
                }
            } else {
                readLegacy(root_ + "/effect/" + model, refs);
            }
            for (const auto& ref : refs) {
                if (resolved.texture.empty()) resolved.texture = textureFromName(root_, ref);
            }
            if (resolved.texture.empty()) resolved.texture = textureFromName(root_, model);
            // The table can list several *simultaneous* .eff models for one
            // magic ID (for example Frozen Arrow and Black Dragon's Breath).
            // The previous early break discarded every model after the first.
            // Compatibility track caps remain opt-out; the historical path
            // consumes all children accepted by the existing bounded decoder.
        }
        auto tryPart = [&](const std::string& name) {
            if (!MagicEffectCatalog::safeName(name)) return;
            std::string filename = name;
            if (filename.size() < 4 || filename.substr(filename.size()-4) != ".par") filename += ".par";
            std::vector<ParticleProfile> parsed;
            if (!loadParticles(root_ + "/effect/" + filename, root_, parsed, &resolved.directStrips)) return;
            for (auto& profile : parsed) {
                if (resolved.particles.size() >= 64) break;
                if (!profile.texture.empty() || (effectModelName(profile.modelName) &&
                    MagicEffectCatalog::safeName(profile.modelName)))
                    resolved.particles.push_back(std::move(profile));
            }
        };
        for (const std::string& part : definition->particles) tryPart(part);
        // Result is played by CMagicCtrl::Stop(), at the projectile's
        // arrival. It must not be added to the in-flight emitters here.
    }
    resolveNestedModels(resolved);
    return resourceCache_.emplace(visualId, std::move(resolved)).first->second;
}

const EffectSystem::VisualResource& EffectSystem::resourceForName(const std::string& name) {
    static const VisualResource empty;
    if (name.empty() || !MagicEffectCatalog::safeName(name)) return empty;
    std::string filename=name;
    if (filename.size()<4 || filename.substr(filename.size()-4)!=".par") filename += ".par";
    const auto cached=namedResourceCache_.find(filename);
    if (cached!=namedResourceCache_.end()) return cached->second;
    if (namedResourceCache_.size()>=64) namedResourceCache_.clear();
    VisualResource resolved;
    std::vector<ParticleProfile> particles;
    if (loadParticles(root_+"/effect/"+filename,root_,particles,&resolved.directStrips)) {
        for (auto& profile:particles) {
            if (resolved.particles.size()>=64) break;
            if (!profile.texture.empty() || (effectModelName(profile.modelName) &&
                MagicEffectCatalog::safeName(profile.modelName))) {
                if (resolved.texture.empty()) resolved.texture=profile.texture;
                resolved.particles.push_back(std::move(profile));
            }
        }
    }
    resolveNestedModels(resolved);
    return namedResourceCache_.emplace(filename,std::move(resolved)).first->second;
}

void EffectSystem::setMapSceneEffects(const std::vector<MapSceneEffectPlacement>& placements) {
    std::set<std::uint64_t> wanted;
    if (historical_map_scene_effects::enabled())
        for (const auto& placement : placements) wanted.insert(placement.key);
    for (std::size_t i = 0; i < live_.size();) {
        if (live_[i].mapScene && wanted.find(live_[i].mapPlacementKey) == wanted.end())
            live_.erase(live_.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }
    if (!historical_map_scene_effects::enabled()) return;
    for (const auto& placement : placements) {
        const bool alreadyLive = std::any_of(live_.begin(), live_.end(), [&](const Instance& effect) {
            return effect.mapScene && effect.mapPlacementKey == placement.key;
        });
        if (alreadyLive) continue;
        const SceneEffectDefinition* definition = sceneEffects_.find(placement.effectId);
        if (!definition || definition->particle.empty()) continue;
        const VisualResource& resource = resourceForName(definition->particle);
        if (resource.particles.empty()) continue;
        Instance effect;
        effect.kind = EffectEvent::Cast;
        effect.age = mapSceneClock_;
        effect.life = std::numeric_limits<float>::infinity();
        effect.expiresAfter = std::numeric_limits<float>::infinity();
        effect.visualId = -placement.effectId;
        effect.seed = static_cast<unsigned>(placement.key ^ (placement.key >> 32) ^ 0x9e3779b9u);
        effect.namedResource = definition->particle;
        effect.fixedPosition = true;
        effect.absoluteHeight = true;
        effect.fixedWorldPosition = {placement.x, placement.height, placement.y};
        effect.mapScene = true;
        effect.mapPlacementKey = placement.key;
        effect.mapSceneYaw = placement.yaw;
        live_.push_back(std::move(effect));
    }
}

void EffectSystem::resolveNestedModels(VisualResource& resource) {
    // MindPower model-rendering .par systems embed an I_Effect, which can in turn
    // contain primitive geometry, billboards or external .lgo mesh tracks.
    // Reuse the existing .lgo decoder used by standalone .eff tracks.
    std::unordered_map<std::string,std::shared_ptr<const Model>> externalModels;
    for (ParticleProfile& profile : resource.particles) {
        // SHADE uses CMPShadeCtrl::Render, bypassing the model branch.
        if(profile.type==13)continue;
        if (!MagicEffectCatalog::safeName(profile.modelName) ||
            !effectModelName(profile.modelName)) continue;
        const char* traceText = std::getenv("TOP_TRACE_SKILL_FX");
        const bool traceNested = traceText && traceText[0] && traceText[0] != '0';
        LegacyEffectSummary document;
        if (!inspectLegacyEff(root_ + "/effect/" + profile.modelName, document,
                              historicalRawEffModelParams()) ||
            !document.structured) {
            if (traceNested) std::fprintf(stderr,
                "[skill-fx] nested-eff=%s status=missing_or_unreadable\n",
                profile.modelName.c_str());
            continue;
        }
        float cycleLength=0.f;
        for(const auto& part:document.document.parts)
            cycleLength=std::max(cycleLength,legacy_eff::duration(part));
        for (const auto& part : document.document.parts) {
            if (part.keys.empty()) continue;
            const bool primitive = legacy_mesh::recognized(part.model);
            const std::string externalName = !part.billboard && !primitive ?
                legacy_external::filename(part.model) : std::string{};
            if (!part.billboard && !primitive && externalName.empty()) continue;
            std::shared_ptr<const Model> external;
            if (!externalName.empty()) {
                const auto it = externalModels.find(externalName);
                if (it != externalModels.end()) {
                    external = it->second;
                } else {
                    Model decoded;
                    if (!legacy_external::load(root_, externalName, decoded)) {
                        if (traceNested) std::fprintf(stderr,
                            "[skill-fx] nested-eff=%s external=%s status=missing_or_unsupported_lgo\n",
                            profile.modelName.c_str(), externalName.c_str());
                        continue;
                    }
                    external = std::make_shared<const Model>(std::move(decoded));
                    externalModels.emplace(externalName, external);
                }
            }
            const std::string tex = textureFromName(root_, part.texture);
            std::vector<std::string> frames;
            for (const auto& frame : part.frames)
                frames.push_back(textureFromName(root_, frame));
            bool usable = !tex.empty();
            for (const auto& frame : frames) usable |= !frame.empty();
            if (external) {
                for (const auto& geom : external->geoms)
                    for (const auto& subset : geom.subsets)
                        usable |= !textureFromName(root_, subset.texture).empty();
            }
            if (!usable) {
                if (traceNested) std::fprintf(stderr,
                    "[skill-fx] nested-eff=%s model=%s status=texture_unresolved\n",
                    profile.modelName.c_str(), part.model.c_str());
                continue; // no invented texture or white placeholder
            }
            if (traceNested && external) std::fprintf(stderr,
                "[skill-fx] nested-eff=%s external=%s status=loaded geoms=%zu\n",
                profile.modelName.c_str(), externalName.c_str(), external->geoms.size());
            profile.modelTracks.push_back({part, tex, std::move(frames), std::move(external), cycleLength,
                static_cast<std::size_t>(&part-document.document.parts.data())});
        }
        if (profile.type == 6 && !profile.modelTracks.empty()) {
            if (const char* trace = std::getenv("TOP_TRACE_STRIP_SYSTEM")) {
                if (trace[0] && trace[0] != '0')
                    std::fprintf(stderr,
                        "[strip-system] type=6 model=%s tracks=%zu cycle=%.3f replay=%d\n",
                        profile.modelName.c_str(),profile.modelTracks.size(),
                        profile.modelTracks.front().cycleLength,
                        legacy_strip_system::historicalCyclesEnabled() ? 1 : 0);
            }
        }
    }
}

const EffectSystem::VisualResource& EffectSystem::resultForMagic(int magicId) {
    // Negative keys isolate result controllers from scene IDs and magic IDs.
    // For example, result for magic 20 has key -1020, never visual 1020.
    const int cacheKey = -1000 - magicId;
    const auto cached = resourceCache_.find(cacheKey);
    if (cached != resourceCache_.end()) return cached->second;
    if (resourceCache_.size() >= 128) resourceCache_.clear();
    VisualResource resolved;
    const auto* magic = magicEffects_.find(magicId);
    if (magic && !magic->result.empty() && MagicEffectCatalog::safeName(magic->result)) {
        std::string filename = magic->result;
        if (filename.size() < 4 || filename.substr(filename.size()-4) != ".par")
            filename += ".par";
        std::vector<ParticleProfile> profiles;
        bool loaded = loadParticles(root_ + "/effect/" + filename, root_, profiles, &resolved.directStrips);
        // Some source tables omit the leading zero in numeric part names:
        // 1030006 -> 01030006.par. Try the literal name first so mods that
        // ship their own seven-digit file retain priority.
        if (!loaded) {
            const auto stem = filename.substr(0, filename.size()-4);
            const bool numeric = !stem.empty() && stem.size() < 8 &&
                std::all_of(stem.begin(), stem.end(), [](unsigned char c) {
                    return std::isdigit(c) != 0;
                });
            if (numeric) {
                const std::string padded = std::string(8-stem.size(), '0') + stem + ".par";
                loaded = loadParticles(root_ + "/effect/" + padded, root_, profiles, &resolved.directStrips);
            }
        }
        if (loaded) {
            for (auto& profile : profiles) {
                if (resolved.particles.size() >= 64) break;
                if (!profile.texture.empty() || (effectModelName(profile.modelName) &&
                    MagicEffectCatalog::safeName(profile.modelName)))
                    resolved.particles.push_back(std::move(profile));
            }
        }
    }
    resolveNestedModels(resolved);
    if (magicId == 20) {
        const char* trace = std::getenv("TOP_TRACE_FROZEN_RESULT");
        if (trace && trace[0]=='1' && trace[1]=='\0') {
            std::fprintf(stderr,"[frozen-result] magic=20 par=%s systems=%zu\n",
                magic?magic->result.c_str():"<missing>",resolved.particles.size());
            for (const auto& p : resolved.particles) {
                std::fprintf(stderr,
                    "[frozen-result] type=%d model=%s tracks=%zu tex=%s delay=%.3f play=%.3f life=%.3f\n",
                    p.type,p.modelName.c_str(),p.modelTracks.size(),
                    p.texture.c_str(),p.delayTime,p.playTime,p.life);
            }
        }
    }
    return resourceCache_.emplace(cacheKey, std::move(resolved)).first->second;
}

bool EffectSystem::selfTest() {
    EffectBatch batch;
    quad(batch, 1.f, 2.f, 3.f, 0.5f, EffectViewBasis{}, 1.f, 1.f, 1.f, 1.f);
    if (batch.verts.size() != 6) return false;
    return std::fabs(batch.verts[0].x - 0.5f) < 0.001f && std::fabs(batch.verts[2].y - 2.5f) < 0.001f;
}

void EffectSystem::setPlayerDummyMatrices(
        std::unordered_map<int, std::array<float, 16>> matrices) {
    playerDummyMatrices_ = std::move(matrices);
}

void EffectSystem::setTargetDummyMatrices(
        std::unordered_map<int, std::unordered_map<int, std::array<float, 16>>> matrices) {
    targetDummyMatrices_ = std::move(matrices);
}

void EffectSystem::setTargetActorRoots(std::unordered_map<int, std::array<float, 3>> roots) {
    targetActorRoots_ = std::move(roots);
}

void EffectSystem::setPlayerStripBodyMatrices(
        std::unordered_map<int,std::array<float,16>> matrices){
    stripBodyMatrices_=std::move(matrices);
}
void EffectSystem::setPlayerStripItemMatrices(
        std::unordered_map<int,std::array<float,16>> matrices){
    stripItemMatrices_=std::move(matrices);
}

void EffectSystem::setPlayerCastAnchor(float worldX, float worldY, float feetHeight) {
    playerCastAnchorValid_ = std::isfinite(worldX) && std::isfinite(worldY) &&
        std::isfinite(feetHeight);
    if (!playerCastAnchorValid_) return;
    playerCastX_ = worldX;
    playerCastY_ = worldY;
    playerCastFeet_ = feetHeight;
}

const std::unordered_map<int,std::array<float,16>>& EffectSystem::attachmentMatrices(int actor,bool item) const {
    if(actor==-1)return item?stripItemMatrices_:playerDummyMatrices_;
    const auto& owners=item?actorItemDummyMatrices_:targetDummyMatrices_;
    const auto found=owners.find(actor);
    static const std::unordered_map<int,std::array<float,16>> empty;
    return found==owners.end()?empty:found->second;
}

bool EffectSystem::setPlayerAttachedEffect(int slot,int visualId,int dummy) {
    if(slot<0)return false;
    for(const auto& effect:live_)
        if(effect.attachedSlot==slot && effect.visualId==visualId && effect.actorDummy==dummy)
            return true;
    live_.erase(std::remove_if(live_.begin(),live_.end(),[&](const auto& effect){
        return effect.attachedSlot==slot;
    }),live_.end());
    if(visualId<=0)return true;
    const auto& resource=resourceFor(visualId);
    if(resource.particles.empty() && resource.billboards.empty() &&
       resource.nativeMeshes.empty() && resource.externalMeshes.empty() && resource.directStrips.empty())
        return false;
    Instance effect;
    effect.kind=EffectEvent::Cast;effect.visualId=visualId;effect.attachedSlot=slot;
    effect.sourceActorIndex=-1;effect.actorCastFollow=true;effect.actorDummy=dummy;
    effect.seed=serial_++*2654435761u;
    effect.life=effect.expiresAfter=std::numeric_limits<float>::infinity();
    effect.directTrails.resize(resource.directStrips.size());
    live_.push_back(std::move(effect));
    return true;
}

void EffectSystem::setActorItemDummyMatrices(
    std::unordered_map<int,std::unordered_map<int,std::array<float,16>>> matrices) {
    actorItemDummyMatrices_=std::move(matrices);
}

void EffectSystem::emit(const EffectEvent& supplied) {
    EffectEvent event=supplied;
    const int source=event.sourceIsPlayer?-1:event.sourceActorIndex;
    const auto* authored=skillVisuals_.find(event.variant);
    if(event.kind==EffectEvent::Flight && authored) {
        // Character::SkyEffect: body dummy, then attack-item dummy, then root.
        const std::array<float,16>* launch=nullptr;
        for(const auto& channel:authored->channels)if(channel.kind==SkillVisualChannelKind::Flight) {
            const auto& body=attachmentMatrices(source,false);
            const auto b=body.find(channel.dummy);
            if(channel.dummy>=0 && b!=body.end())launch=&b->second;
            else {
                const auto& item=attachmentMatrices(source,true);
                const auto i=item.find(channel.itemDummy);
                if(channel.itemDummy>=0 && i!=item.end())launch=&i->second;
            }
            break;
        }
        if(launch) {
            event.fromX=(*launch)[12];event.fromY=(*launch)[13];event.fromHeight=(*launch)[14];
        } else if(source==-1 && playerCastAnchorValid_) {
            event.fromX=playerCastX_;event.fromY=playerCastY_;event.fromHeight=playerCastFeet_;
        } else if(source>=0) {
            const auto root=targetActorRoots_.find(source);
            if(root!=targetActorRoots_.end()) {
                event.fromX=root->second[0];event.fromY=root->second[2];event.fromHeight=root->second[1];
            }
        }
        // STAttack uses sTargetDummyLink even if no target-effect ID exists.
        const auto& target=attachmentMatrices(event.targetActorIndex,false);
        const auto t=target.find(authored->targetDummy);
        if(event.targetActorIndex!=-2 && authored->targetDummy>=0 && t!=target.end()) {
            event.toX=t->second[12];event.toY=t->second[13];event.toHeight=t->second[14];
        }
    }

    // Skillinfo may declare multiple actor/item effects at cast and distinct
    // target/ground/water effects at impact. The historical table's channels
    // are authoritative, not the old single-ID castId()/hitId() fallback.
    // The default path retains one cast moment. The opt-in callback field lets
    // Play dispatch the channels whose authored callback matches this event.
    struct RequestedId { int id; bool surface; int dummy; bool actor = false; bool item = false; bool target = false; };
    std::vector<RequestedId> ids; // Retain each actor channel's historic dummy.
    const SkillVisual* selectedSkill = skillVisuals_.find(event.variant);
    if (selectedSkill) {
        const SkillVisual* visual = selectedSkill;
        if (event.isCast()) {
            if(event.castCallbackKeyFrame()!=-3) {
                for(const auto& channel:visual->castChannels(event.castCallbackKeyFrame()))
                    ids.push_back({channel.effectId,false,
                        channel.kind==SkillVisualChannelKind::Actor ||
                        channel.kind==SkillVisualChannelKind::Item ||
                        channel.kind==SkillVisualChannelKind::ItemExtra ? channel.dummy : -1,
                        channel.kind==SkillVisualChannelKind::Actor,
                        channel.kind==SkillVisualChannelKind::Item ||
                        channel.kind==SkillVisualChannelKind::ItemExtra});
            } else {
                for (const auto& channel : visual->channels) {
                    if (channel.kind == SkillVisualChannelKind::Actor ||
                        channel.kind == SkillVisualChannelKind::Item ||
                        channel.kind == SkillVisualChannelKind::ItemExtra)
                        if (channel.effectId > 0) ids.push_back({channel.effectId,false,
                            channel.kind==SkillVisualChannelKind::Actor ||
                            channel.kind==SkillVisualChannelKind::Item ||
                            channel.kind==SkillVisualChannelKind::ItemExtra ? channel.dummy : -1,
                            channel.kind==SkillVisualChannelKind::Actor,
                            channel.kind==SkillVisualChannelKind::Item ||
                            channel.kind==SkillVisualChannelKind::ItemExtra});
                }
            }
            if (ids.empty() && visual->channels.empty() && visual->castId() > 0)
                ids.push_back({visual->castId(),false,-1});
        } else if (event.kind == EffectEvent::Flight) {
            if (visual->flightId() > 0) ids.push_back({visual->flightId(),false,-1});
        } else if (event.kind == EffectEvent::SkillHit) {
            for (const auto& channel : visual->hitChannels(event.surface))
                ids.push_back({channel.effectId, channel.kind==SkillVisualChannelKind::Ground ||
                    channel.kind==SkillVisualChannelKind::Water,
                    channel.kind==SkillVisualChannelKind::Target?channel.dummy:-1,
                    false,false,channel.kind==SkillVisualChannelKind::Target});
            if (ids.empty() && visual->channels.empty() && visual->hitId() > 0)
                ids.push_back({visual->hitId(),false,-1});
        } else if (visual->hitId() > 0) {
            // Preserve the previous scalar mapping for legacy non-skill events.
            ids.push_back({visual->hitId(),false,-1});
        }
    }
    if (ids.empty() && selectedSkill && event.isCast() &&
        event.castCallbackKeyFrame()!=-3) return;
    if (ids.empty() && selectedSkill && event.kind == EffectEvent::SkillHit)
        return; // result of a magic projectile is scheduled separately at arrival
    if (ids.empty()) ids.push_back({0,false,-1}); // Historical missing-data fallback.

    // For debugging the historic Illusion Slash independently of animation:
    // TOP_TRACE_ILLUSION_FX=1 logs the per-channel source and availability.
    // TOP_ILLUSION_HIDE_EFFECT_ID=1018 temporarily hides only that ID.
    // The switches never change the actor pose, movement or combat timings.
    const bool illusion = event.variant == 81;
    const char* traceIllusion = illusion ? std::getenv("TOP_TRACE_ILLUSION_FX") : nullptr;
    const bool trace = traceIllusion && traceIllusion[0] && traceIllusion[0] != '0';
    const char* allSkillTrace = std::getenv("TOP_TRACE_SKILL_FX");
    const bool traceSkill = allSkillTrace && allSkillTrace[0] && allSkillTrace[0] != '0';
    const char* hiddenIdText = illusion ? std::getenv("TOP_ILLUSION_HIDE_EFFECT_ID") : nullptr;
    const int hiddenId = hiddenIdText ? std::atoi(hiddenIdText) : 0;
    const char* eventName = event.isCast() ? "cast" :
        event.kind == EffectEvent::Flight ? "flight" :
        event.kind == EffectEvent::SkillHit ? "hit" : "other";

    // Without a usable authored asset, multiple channel IDs would all draw
    // the same procedural placeholder. Keep at most one such fallback per
    // event, while allowing every channel with a real asset to play.
    bool fallbackAdded = false;
    std::vector<RequestedId> seen;
    // First resolve group magic (2000..2999) into their simple-magic members.
    // Part_sequence in the original DX9 client launches successive elements
    // at 0.2s intervals. Leaving ID 2002 unresolved rendered a fake effect.
    struct ScheduledId {int id; float delay; bool groupMember; float fanAngle; bool surface; int dummy; bool actor; bool item; bool target;};
    std::vector<ScheduledId> scheduled;
    for (const auto& requested : ids) {
        // CCharacter::ItemEffect returns false for exactly dummy -1 before
        // looking up the equipped item. Keep the historical rule opt-in.
        if (historicalItemEffectRequireDummyEnabled() && requested.item &&
            requested.dummy == -1) continue;
        const int id=requested.id;
        if (std::any_of(seen.begin(),seen.end(),[&](const RequestedId& other) {
                return other.id==requested.id && other.surface==requested.surface &&
                    other.dummy==requested.dummy && other.actor==requested.actor &&
                    other.item==requested.item && other.target==requested.target;
            })) continue;
        seen.push_back(requested);
        if (seen.size() > 8) break;
        if (id >= 2000 && id < 3000) {
            const auto* group = magicGroups_.find(id - 2000);
            if (group) {
                // DX9 Part_fan spreads member trajectories over 0.75 radians.
                // DX9 Part_sequence leaves the destination unchanged and sets
                // a 0.2s delay between consecutive elements.
                for (std::size_t i=0; i<group->members.size(); ++i) {
                    const float fanAngle = group->motion == 0 && group->members.size()>1 ?
                        -.375f + .75f*static_cast<float>(i)/
                            static_cast<float>(group->members.size()-1) : 0.f;
                    scheduled.push_back({1000 + group->members[i],
                        group->motion == 1 ? .2f*static_cast<float>(i) : 0.f,
                        true,fanAngle,requested.surface,requested.dummy,requested.actor,requested.item,requested.target});
                }
                if (traceSkill) std::fprintf(stderr,
                    "[skill-fx] skill=%d kind=%s group=%d members=%zu motion=%d\n",
                    event.variant,eventName,id,group->members.size(),group->motion);
            }
            // No invented tornado if group metadata is missing or invalid.
            continue;
        }
        scheduled.push_back({id, 0.f, false, 0.f,requested.surface,requested.dummy,
                             requested.actor,requested.item,requested.target});
    }
    for (const auto& scheduledId : scheduled) {
        const int id = scheduledId.id;
        if (illusion && id > 0 && id == hiddenId) {
            if (trace) std::fprintf(stderr, "[illusion-fx] kind=%s id=%d hidden-by-user\n", eventName, id);
            continue;
        }

        const VisualResource* resource = nullptr;
        bool authored = false;
        if (id > 0 && !root_.empty()) {
            resource = &resourceFor(id);
            authored = !resource->texture.empty() || !resource->particles.empty() ||
                       !resource->billboards.empty() || !resource->nativeMeshes.empty() ||
                       !resource->externalMeshes.empty() || !resource->directStrips.empty();
        }
        // Having a texture filename is not sufficient: an unconverted .eff can
        // expose a texture without any mesh/particles that OpenGL can draw.
        const bool drawable = resource && (!resource->particles.empty() ||
            !resource->billboards.empty() || !resource->nativeMeshes.empty() ||
            !resource->externalMeshes.empty() || !resource->directStrips.empty());
        if (trace) {
            const SceneEffectDefinition* scene = sceneEffects_.find(id);
            const MagicEffectDefinition* magic = magicEffects_.find(
                id >= 1000 && id < 1100 ? id - 1000 : id);
            std::fprintf(stderr, "[illusion-fx] kind=%s id=%d drawable=%d texture=%d source=%s%s%s\n",
                eventName, id, int(drawable), int(resource && !resource->texture.empty()),
                scene ? "scene:" : magic ? "magic:" : "unknown",
                scene ? scene->particle.c_str() :
                    magic && !magic->models.empty() ? magic->models.front().c_str() : "none",
                drawable ? "" : " (historical geometry missing)");
        }
        if (traceSkill && event.kind == EffectEvent::Flight &&
            id>=1000 && id<2000) {
            const auto* magic=magicEffects_.find(id-1000);
            if(magic) std::fprintf(stderr,
                "[skill-motion] skill=%d magic=%d mode=%d speed=%.2f name=%s\n",
                event.variant,id-1000,magic->renderMode,magic->velocity,
                historic_magic::modeName(magic->renderMode));
        }
        if (traceSkill)
            std::fprintf(stderr,
                "[skill-fx] skill=%d kind=%s visual=%d drawable=%d particles=%zu "
                "native=%zu billboard=%zu external=%zu delay=%.2f\n",
                event.variant,eventName,id,int(drawable),
                resource ? resource->particles.size() : 0,
                resource ? resource->nativeMeshes.size() : 0,
                resource ? resource->billboards.size() : 0,
                resource ? resource->externalMeshes.size() : 0,
                scheduledId.delay);
        if(traceSkill && resource && !resource->directStrips.empty())
            std::fprintf(stderr,"[direct-strip] skill=%d visual=%d strips=%zu owner=%s\n",
                event.variant,id,resource->directStrips.size(),
                scheduledId.item ? "item" : scheduledId.actor ? "actor" : "unbound");
        if (trace && resource && illusion && id == 195) {
            std::fprintf(stderr, "[illusion-fx] impact particle-systems=%zu\n",
                         resource->particles.size());
            for (const auto& profile : resource->particles)
                std::fprintf(stderr,
                    "[illusion-fx] impact par tex=%s type=%d count=%d delay=%.2f play=%.2f life=%.2f\n",
                    profile.texture.c_str(), profile.type, profile.count,
                    profile.delayTime, profile.playTime, profile.life);
            for (const auto& profile : resource->particles)
                if (profile.type == 5 || profile.type == 6)
                    std::fprintf(stderr, "[illusion-fx] nested-model=%s tracks=%zu type=%d\n",
                        profile.modelName.c_str(), profile.modelTracks.size(), profile.type);
        }
        // Old fallback 'Cast' builds a rising *rotating column* of 12 quads.
        // On Illusion Slash that is a fake tornado, not its historical art.
        // Keep real 186/218/1018/195 where drawable; never invent the spiral
        // just because a historical resource is absent or not yet converted.
        // A historical skill/channel identifies a real asset. If it cannot
        // be converted, silence is preferable to fake spinning rings. Keep
        // placeholders only for events with no authored skill information.
        if (!drawable && id > 0 && (selectedSkill || scheduledId.groupMember)) continue;
        if (!authored) {
            if (fallbackAdded) continue;
            fallbackAdded = true;
        }

        Instance effect;
        effect.kind = event.isCast()?EffectEvent::Cast:event.kind;
        effect.age = -scheduledId.delay;
        effect.fromX = event.fromX;
        effect.fromY = event.fromY;
        effect.toX = event.toX;
        effect.toY = event.toY;
        if (scheduledId.fanAngle != 0.f) {
            const float dx = event.toX-event.fromX, dz = event.toY-event.fromY;
            const float c=std::cos(scheduledId.fanAngle),s=std::sin(scheduledId.fanAngle);
            effect.toX=event.fromX+dx*c-dz*s;
            effect.toY=event.fromY+dx*s+dz*c;
        }
        effect.variant = event.variant;
        effect.visualId = id;
        if (event.kind == EffectEvent::Flight && event.targetActorIndex != -2)
            effect.targetActorIndex = event.targetActorIndex;
        effect.groundAnchored=scheduledId.surface;
        effect.sourceActorIndex=source;
        effect.actorCastFollow = event.isCast() && source!=-2 &&
            scheduledId.actor;
        effect.itemCastFollow = event.isCast() && source!=-2 &&
            scheduledId.item;
        if (effect.itemCastFollow && scheduledId.dummy >= 0 && scheduledId.dummy <= 255)
            effect.itemDummy = scheduledId.dummy;
        if (scheduledId.target && event.targetActorIndex != -2) {
            if (scheduledId.dummy >= 0 && scheduledId.dummy <= 255)
                effect.targetDummy = scheduledId.dummy;
            effect.targetActorIndex = event.targetActorIndex;
            effect.targetActorFollow = true;
        }
        // Bind only player-owned ACTOR cast channels. Dummies on items and
        // target monsters are different coordinate spaces and remain pending.
        if (effect.actorCastFollow &&
            scheduledId.dummy>=0 && scheduledId.dummy<=255)
            effect.actorDummy=scheduledId.dummy;
        effect.seed = serial_++ * 2654435761u;
        effect.life = event.kind == EffectEvent::Flight ?
                          std::max(0.0001f, event.duration > 0.f ? event.duration : 0.28f) :
                      event.isCast() ? 0.75f :
                      event.kind == EffectEvent::SkillHit ? 0.55f :
                      event.kind == EffectEvent::Wake ? 0.8f : 0.45f;
        effect.expiresAfter = effect.life;
        if (resource) {
            effect.directTrails.resize(resource->directStrips.size());
            for (const ParticleProfile& profile : resource->particles) {
                const float emission = profile.delayTime + std::max(0.f, profile.playTime);
                const float profileExpiry=historical_model_lifetime::expiry(
                    emission+profile.life,profile.type,profile.delayTime,profile.playTime);
                effect.expiresAfter = std::max(effect.expiresAfter, profileExpiry);
            }
            auto includeTracks = [&](const std::vector<VisualResource::BillboardTrack>& tracks) {
                for (const auto& track : tracks)
                    effect.expiresAfter = std::max(effect.expiresAfter, legacy_eff::duration(track.subeffect));
            };
            includeTracks(resource->billboards);
            includeTracks(resource->nativeMeshes);
            for (const auto& track : resource->externalMeshes)
                effect.expiresAfter = std::max(effect.expiresAfter, legacy_eff::duration(track.subeffect));
            // CMPStrip::IsPlaying is governed by controller play/pose, not a
            // 100*step artificial tail. Keep the existing action life here.
            // The DX9 CMagicCtrl::Part_fly/Part_dist stops the projectile on
            // reaching its target. The authored .eff track length describes
            // the reusable visual clip, not extra time hovering at impact.
            // Preserve particle-system tails, which are emitted separately.
            if (effect.kind == EffectEvent::Flight && resource->particles.empty() &&
                (!resource->nativeMeshes.empty() || !resource->billboards.empty() ||
                 !resource->externalMeshes.empty()))
                effect.expiresAfter = effect.life;
        }
        if (effect.kind == EffectEvent::Flight && id >= 1000 && id < 2000) {
            const auto* magic = magicEffects_.find(id-1000);
            if (magic && historic_magic::knownMode(magic->renderMode)) {
                effect.absoluteHeight = std::isfinite(event.fromHeight) && std::isfinite(event.toHeight);
                effect.path.mode = magic->renderMode;
                effect.path.from = {effect.fromX, effect.absoluteHeight?event.fromHeight:0.f, effect.fromY};
                effect.path.to = {effect.toX, effect.absoluteHeight?event.toHeight:0.f, effect.toY};
                // Character::SkyEffect -> SetVel(sSkySpd/100) overrides nVel.
                // event.duration already represents that skill speed in Play.
                const float planar = std::hypot(effect.toX-effect.fromX,effect.toY-effect.fromY);
                effect.path.speed = event.duration>0.f && planar>1e-7f
                    ? planar/event.duration : magic->velocity;
                effect.partTraceRequested = historicalPartTraceEnabled() &&
                    effect.path.mode == 2 && effect.absoluteHeight &&
                    effect.targetActorIndex != -2;
                if (effect.partTraceRequested)
                    effect.partTraceTimeline.reset(effect.path);
                if(resource) {
                    for(const auto& t:resource->billboards) effect.path.clipLength=std::max(effect.path.clipLength,t.cycleLength);
                    for(const auto& t:resource->nativeMeshes) effect.path.clipLength=std::max(effect.path.clipLength,t.cycleLength);
                    for(const auto& t:resource->externalMeshes) effect.path.clipLength=std::max(effect.path.clipLength,t.cycleLength);
                }
                if(effect.path.speed>0.f || effect.path.mode==5)
                    effect.life=std::max(.0001f,effect.path.duration());
                else {
                    effect.path.mode=-1; // invalid stationary movement retains legacy fallback
                    effect.partTraceRequested=false;
                }
                // Stop() terminates every flight component before playing Result.
                effect.expiresAfter=effect.life;
            }
        }
        live_.push_back(effect);
    }
}

void EffectSystem::step(float dt, const MapFile* map) {
    if (!std::isfinite(dt) || dt < 0.f) return;
    mapSceneClock_ += dt;
    std::vector<Instance> arrivals;
    for (std::size_t i = 0; i < live_.size();) {
        Instance& flight = live_[i];
        const float previousAge = flight.age;
        flight.age += dt;
        if (flight.kind == EffectEvent::Flight && flight.partTraceRequested &&
            !flight.partTraceFollowing) {
            const auto target=targetActorRoots_.find(flight.targetActorIndex);
            if (target!=targetActorRoots_.end()) {
                flight.partTraceFollowing=true;
                flight.expiresAfter=std::numeric_limits<float>::infinity();
            }
        }
        if (flight.partTraceFollowing && !flight.partTraceFinished) {
            auto target=flight.partTraceTimeline.oldTarget;
            const auto actor=targetActorRoots_.find(flight.targetActorIndex);
            if (actor!=targetActorRoots_.end())
                target={actor->second[0],actor->second[1]+1.f,actor->second[2]};
            flight.partTraceFinished=flight.partTraceTimeline.advance(
                dt,target,map,flight.path.speed);
            if (flight.partTraceFinished) {
                flight.life=flight.age;
                flight.expiresAfter=flight.age;
                flight.partTraceRetirePending=true;
            }
        }
        if (historicalFshadeFrameCountEnabled() &&
            flight.kind == EffectEvent::Flight && flight.path.mode == historic_magic::kFshade)
            flight.fshadeTimeline.advance(dt);
        if (flight.mapScene || (historical_particle_pool::enabled() &&
             historical_particle_pool::sharedEmitterTimerEnabled()) ||
            historical_eff_timeline::enabled() ||
            historical_eff_timeline::controllerClocksEnabled() ||
            historical_eff_timeline::snowFireEnabled() ||
            historical_model_timeline::enabled() ||
            historical_shrink::timelineEnabled() ||
            historical_blast_group::enabled() ||
            historical_ripple::enabled() ||
            (historical_particle_extended::enabled() &&
             historical_blast3_timeline::enabled()) ||
            (historical_particle_pool::enabled() && historical_particle_motion::enabled() &&
             historical_blast_motion::actualDtEnabled()) ||
            (historical_particle_extended::enabled() &&
             historical_range2_collision::enabled() &&
             historical_range2_collision::actualDtEnabled()) ||
            (historical_arraw_timeline::enabled() &&
             historical_particle_extended::enabled()) ||
            (historical_particle_extended::enabled() &&
             historical_round_timeline::enabled())) {
            const std::vector<ParticleProfile>* profiles = nullptr;
            if (!flight.namedResource.empty()) {
                const auto found = namedResourceCache_.find(flight.namedResource);
                if (found != namedResourceCache_.end()) profiles = &found->second.particles;
            } else {
                const auto found = resourceCache_.find(flight.visualId);
                if (found != resourceCache_.end()) profiles = &found->second.particles;
                else profiles = (flight.kind == EffectEvent::Cast ||
                                 flight.kind == EffectEvent::Flight) ?
                    &castParticles_ : &hitParticles_;
            }
            if (profiles && historical_particle_extended::enabled() &&
                historical_range2_collision::enabled() &&
                historical_range2_collision::actualDtEnabled() &&
                std::any_of(profiles->begin(),profiles->end(),
                    [](const ParticleProfile& profile){return profile.type==15;})) {
                flight.range2FrameSteps.push_back({previousAge,flight.age,dt});
                if (flight.range2Timelines.size()<profiles->size())
                    flight.range2Timelines.resize(profiles->size());
            }
            if (profiles) {
                if (flight.snowFireTimelines.size() < profiles->size())
                    flight.snowFireTimelines.resize(profiles->size());
                if (flight.snowFireEffTimelines.size() < profiles->size())
                    flight.snowFireEffTimelines.resize(profiles->size());
                if (flight.modelTimelines.size() < profiles->size())
                    flight.modelTimelines.resize(profiles->size());
                if (flight.modelEffTimelines.size() < profiles->size())
                    flight.modelEffTimelines.resize(profiles->size());
                if (flight.rippleTimelines.size() < profiles->size())
                    flight.rippleTimelines.resize(profiles->size());
                if (flight.rippleEffTimelines.size() < profiles->size())
                    flight.rippleEffTimelines.resize(profiles->size());
                if (flight.stripEffTimelines.size() < profiles->size())
                    flight.stripEffTimelines.resize(profiles->size());
                if (flight.arrawEffTimelines.size() < profiles->size())
                    flight.arrawEffTimelines.resize(profiles->size());
                if (flight.blast3Timelines.size() < profiles->size())
                    flight.blast3Timelines.resize(profiles->size());
                if (flight.blast3EffTimelines.size() < profiles->size())
                    flight.blast3EffTimelines.resize(profiles->size());
                if (flight.blastMotionTimelines.size() < profiles->size())
                    flight.blastMotionTimelines.resize(profiles->size());
                if (flight.blastGroupTimelines.size() < profiles->size())
                    flight.blastGroupTimelines.resize(profiles->size());
                if (flight.roundTimelines.size() < profiles->size())
                    flight.roundTimelines.resize(profiles->size());
                if (flight.roundEffTimelines.size() < profiles->size())
                    flight.roundEffTimelines.resize(profiles->size());
                if (flight.shrinkTimelines.size() < profiles->size())
                    flight.shrinkTimelines.resize(profiles->size());
                for (std::size_t profileIndex = 0; profileIndex < profiles->size(); ++profileIndex) {
                    const ParticleProfile& profile = (*profiles)[profileIndex];
                    if (profile.type != 1 && profile.type != 2 && profile.type != 3 && profile.type != 4 &&
                        profile.type != 8 && profile.type != 9 && profile.type != 11 &&
                        profile.type != 12 &&
                        profile.type != 5 && profile.type != 6) continue;
                    const float oldParticleAge = previousAge - profile.delayTime;
                    const float newParticleAge = flight.age - profile.delayTime;
                    const float activeDelta = std::max(0.f, newParticleAge) -
                                              std::max(0.f, oldParticleAge);
                    const bool useModelTimeline=profile.type==5 &&
                        (flight.mapScene || historical_model_timeline::enabled());
                    if (activeDelta <= 0.f && !useModelTimeline) continue;
                    if (profile.type == 12) {
                        if (historical_shrink::timelineEnabled()) {
                            const int count=historical_particle_count::resolve(
                                profile.type,profile.count,false,
                                historical_particle_count::enabled());
                            const float stopAt=profile.playTime>0.f?
                                std::max(0.f,profile.playTime-profile.delayTime):-1.f;
                            flight.shrinkTimelines[profileIndex].advance(
                                count,activeDelta,
                                {{profile.range[0],profile.range[1],profile.range[2]}},
                                profile.velocity,profile.life,profile.step,
                                profile.randomDivisor,profile.frameCount,flight.seed,
                                stopAt<0.f||newParticleAge<stopAt);
                        }
                        continue;
                    }
                    if (profile.type == 3) {
                        if (historical_blast_group::enabled()) {
                            const int count=historical_particle_count::resolve(
                                profile.type,profile.count,false,
                                historical_particle_count::enabled());
                            const bool mayRestart=profile.playTime<=0.f ||
                                previousAge<profile.playTime;
                            flight.blastGroupTimelines[profileIndex].advance(
                                count,activeDelta,profile.life,profile.frameCount,
                                profile.randomDivisor,flight.seed,true,mayRestart);
                        }
                        if (historical_particle_pool::enabled() &&
                            historical_particle_motion::enabled() &&
                            historical_blast_motion::actualDtEnabled()) {
                            const int count=historical_particle_count::resolve(
                                profile.type,profile.count,false,
                                historical_particle_count::enabled());
                            const float stopAt=profile.playTime>0.f?
                                std::max(0.f,profile.playTime-profile.delayTime):-1.f;
                            std::vector<historical_blast_motion::SlotSample> samples(
                                static_cast<std::size_t>(count));
                            const auto group=historical_blast_group::enabled()?
                                flight.blastGroupTimelines[profileIndex].snapshot():
                                historical_blast_group::SystemSample{};
                            for(int particle=0;particle<count;++particle) {
                                const auto sample=historical_blast_group::enabled()&&
                                    static_cast<std::size_t>(particle)<group.slots.size()?
                                    group.slots[static_cast<std::size_t>(particle)]:
                                    historical_particle_pool::at(3,particle,count,newParticleAge,
                                        profile.life,profile.step,stopAt,flight.seed,
                                        profile.randomDivisor);
                                samples[static_cast<std::size_t>(particle)]=
                                    {sample.alive,sample.generation};
                            }
                            flight.blastMotionTimelines[profileIndex].advance(
                                activeDelta,samples);
                        }
                        continue;
                    }
                    if (profile.type == 6) {
                        if ((flight.mapScene || historical_eff_timeline::enabled() ||
                             historical_eff_timeline::controllerClocksEnabled()) &&
                            !profile.modelTracks.empty())
                            flight.stripEffTimelines[profileIndex].advance(
                                activeDelta, profile.modelTracks,
                                legacy_strip_system::historicalCyclesEnabled() || flight.mapScene);
                        continue;
                    }
                    if (profile.type == 5) {
                        if (useModelTimeline) {
                            const float frameDelta=historical_model_timeline::controllerDelta(
                                previousAge,dt,profile.delayTime);
                            flight.modelTimelines[profileIndex].advance(
                                frameDelta,profile.life,
                                static_cast<unsigned>(profile.frameCount));
                            if ((flight.mapScene || historical_model_eff_timeline::enabled()) &&
                                !profile.modelTracks.empty() &&
                                !flight.modelTimelines[profileIndex].holdingLastFrame)
                                flight.modelEffTimelines[profileIndex].advance(
                                    frameDelta,profile.modelTracks,true);
                        }
                        continue;
                    }
                    if (profile.type == 8) {
                        if (historical_arraw_timeline::enabled() &&
                            historical_particle_extended::enabled()) {
                            flight.modelTimelines[profileIndex].advance(
                                activeDelta, profile.life,
                                static_cast<unsigned>(profile.frameCount));
                            if (!profile.modelTracks.empty())
                                flight.arrawEffTimelines[profileIndex].advance(
                                    activeDelta, profile.modelTracks, true);
                        }
                        continue;
                    }
                    if (profile.type == 9) {
                        if (historical_particle_extended::enabled() &&
                            historical_round_timeline::enabled()) {
                            const int count=historical_particle_count::resolve(
                                profile.type,profile.count,false,false);
                            flight.roundTimelines[profileIndex].advance(
                                count,activeDelta,profile.life,
                                static_cast<unsigned>(profile.frameCount),profile.velocity);
                            if (!profile.modelTracks.empty())
                                flight.roundEffTimelines[profileIndex].advance(
                                    activeDelta,profile.modelTracks,true);
                        }
                        continue;
                    }
                    if (profile.type == 11) {
                        if (historical_particle_extended::enabled() &&
                            historical_blast3_timeline::enabled()) {
                            const int count=historical_particle_count::resolve(
                                profile.type,profile.count,false,false);
                            flight.blast3Timelines[profileIndex].advance(count,activeDelta,
                                profile.life,profile.frameCount,profile.velocity,
                                {{profile.direction[0],profile.direction[1],profile.direction[2]}},
                                {{profile.acceleration[0],profile.acceleration[1],profile.acceleration[2]}});
                            if(!profile.modelTracks.empty())
                                flight.blast3EffTimelines[profileIndex].advance(
                                    activeDelta,profile.modelTracks,true);
                        }
                        continue;
                    }
                    if (profile.type == 4) {
                        if (flight.mapScene || historical_ripple::enabled()) {
                            const int rippleCount=historical_particle_count::resolve(
                                profile.type,profile.count,false,false);
                            const float stopAt=!flight.mapScene&&profile.playTime>0.f?
                                std::max(0.f,profile.playTime-profile.delayTime):-1.f;
                            flight.rippleTimelines[profileIndex].advance(
                                rippleCount,activeDelta,profile.life,profile.step,stopAt,
                                flight.seed,profile.randomDivisor,profile.frameCount,
                                stopAt<0.f||newParticleAge<stopAt);
                            if (!profile.modelTracks.empty())
                                flight.rippleEffTimelines[profileIndex].advance(
                                    activeDelta,profile.modelTracks,true);
                        }
                        continue;
                    }
                    if ((profile.type == 1 || profile.type == 2) &&
                        historical_eff_timeline::snowFireEnabled() &&
                        !profile.mediayEff && !profile.modelTracks.empty())
                        flight.snowFireEffTimelines[profileIndex].advance(
                            activeDelta, profile.modelTracks, true);
                    if (!historical_particle_pool::enabled() ||
                        !historical_particle_pool::sharedEmitterTimerEnabled()) continue;
                    float stopAt = profile.playTime > 0.f ?
                        std::max(0.f, profile.playTime - profile.delayTime) : -1.f;
                    if (profile.type == 2 && profileIndex < flight.fireHitStopTimes.size() &&
                        flight.fireHitStopTimes[profileIndex] >= 0.f)
                        stopAt = flight.fireHitStopTimes[profileIndex];
                    const int count = historical_particle_count::resolve(
                        profile.type, profile.count, false,
                        historical_particle_count::enabled());
                    const bool snowCanSpawn = profile.type != 1 ||
                        (profile.direction[2] != 0.f && profile.velocity != 0.f);
                    flight.snowFireTimelines[profileIndex].advance(
                        profile.type, count, activeDelta, profile.life, profile.step,
                        stopAt, flight.seed, profile.randomDivisor, profile.frameCount,
                        snowCanSpawn, stopAt < 0.f || newParticleAge < stopAt,
                        {{profile.acceleration[0], profile.acceleration[1],
                          profile.acceleration[2]}},
                        historical_particle_jitter::frameAccelerationEnabled());
                }
            }
        }
        // DirectX CMagicCtrl::Stop() fires its .par Result when the missile
        // reaches the target. A damage HIT/MISS event is a separate channel.
        const bool arrived = flight.partTraceFollowing ? flight.partTraceFinished :
            flight.age >= flight.life;
        if (flight.kind == EffectEvent::Flight && !flight.resultStarted && arrived) {
            flight.resultStarted = true;
            // IDs 1000..1999 address magicsingleinfo; grouped magic already
            // expands to individual simple-magic IDs in EffectSystem::emit.
            if (flight.visualId >= 1000 && flight.visualId < 2000) {
                const int magicId = flight.visualId - 1000;
                const auto* magic = magicEffects_.find(magicId);
                if (magic && !magic->result.empty()) {
                    const VisualResource& resultResource = resultForMagic(magicId);
                    if (!resultResource.particles.empty()) {
                        Instance result;
                        result.kind = EffectEvent::SkillHit; // stationary at target
                        result.variant = flight.variant;
                        result.visualId = -1000 - magicId;
                        result.fromX = flight.fromX;
                        result.fromY = flight.fromY;
                        result.toX = flight.toX;
                        result.toY = flight.toY;
                        if (flight.partTraceFollowing ||
                            historic_magic::knownMode(flight.path.mode)) {
                            const auto arrival=flight.partTraceFollowing
                                ? flight.partTraceTimeline.position
                                : flight.path.position(flight.life);
                            result.toX=arrival.x;
                            result.toY=arrival.z;
                            result.absoluteHeight=flight.absoluteHeight;
                            result.path.to=arrival;
                        }
                        result.life = 0.55f;
                        result.age = historicalMagicResultStartsOnArrival() ? 0.f :
                            std::max(0.f, flight.age-flight.life);
                        result.seed = serial_++ * 2654435761u;
                        result.expiresAfter = result.life;
                        // SK2.16-C: opt-in diagnostic for the ice .lgo impact.
                        // The projectile's arrival height is not necessarily the
                        // terrain height. This is intentionally NOT the default:
                        // it lets users distinguish a height/occlusion problem
                        // from missing mesh, UV, texture or fragment output.
                        if (magicId == 20) {
                            if (const char* env = std::getenv("TOP_FROZEN_RESULT_GROUND")) {
                                if (env[0] == '1' && env[1] == '\0') {
                                    result.absoluteHeight = false;
                                    result.groundAnchored = true;
                                }
                            }
                        }
                        for (const auto& profile : resultResource.particles)
                            result.expiresAfter = std::max(result.expiresAfter,
                                profile.delayTime + std::max(0.f, profile.playTime) + profile.life);
                        // Duration of the embedded I_Effect models matters too.
                        for (const auto& profile : resultResource.particles)
                            for (const auto& track : profile.modelTracks)
                                result.expiresAfter = std::max(result.expiresAfter,
                                    profile.delayTime + legacy_eff::duration(track.subeffect));
                        if (result.age < result.expiresAfter) arrivals.push_back(result);
                        const char* trace = std::getenv("TOP_TRACE_SKILL_FX");
                        if (trace && trace[0] && trace[0] != '0') {
                            std::size_t nestedTracks = 0;
                            for (const auto& profile : resultResource.particles)
                                nestedTracks += profile.modelTracks.size();
                            std::fprintf(stderr,
                                "[skill-fx] skill=%d kind=result visual=%d magic=%d par=%s systems=%zu nested_tracks=%zu at=(%.2f,%.2f)\n",
                                flight.variant, result.visualId, magicId,
                                magic->result.c_str(), resultResource.particles.size(),
                                nestedTracks, result.toX, result.toY);
                        }
                    }
                }
            }
        }
        if (flight.partTraceRetirePending) {
            flight.partTraceRetirePending=false;
            ++i;
        } else if (!flight.mapScene && flight.attachedSlot<0 && flight.age >= flight.expiresAfter)
            live_.erase(live_.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }
    // Append AFTER iteration: no invalidated references or recursive emission.
    live_.insert(live_.end(), arrivals.begin(), arrivals.end());
}

void EffectSystem::build(MapFile& map, float cameraRightX, float cameraRightY) {
    EffectViewBasis view;
    view.right={{cameraRightX,0.f,cameraRightY}};
    build(map,view);
}

void EffectSystem::build(MapFile& map, const EffectViewBasis& view) {
    batches_.clear();
    if (live_.empty()) return;
    const char* frozenTraceEnv = std::getenv("TOP_TRACE_FROZEN_GEOMETRY");
    const bool traceFrozen = frozenTraceEnv && frozenTraceEnv[0] == '1' &&
                             frozenTraceEnv[1] == '\0';
    // Per result, layer and quarter-second. Cap growth for long sessions.
    static std::set<std::tuple<unsigned,int,int>> frozenLogged;
    const auto movementFor = [&](const Instance& effect) -> int {
        if(effect.kind!=EffectEvent::Flight || effect.visualId<1000 ||
           effect.visualId>=2000) return -1;
        const auto* magic=magicEffects_.find(effect.visualId-1000);
        return magic ? magic->renderMode : -1;
    };
    const auto positionForAge = [&](const Instance& effect,float sampleAge) -> historic_magic::Vec3 {
        if (effect.fixedPosition) return effect.fixedWorldPosition;
        // Explicit actor cast channels track their own dummy at EVERY frame.
        // The matrix is DX row-vector XYZ (Z-up); GL world is X/height/Y.
        if (effect.actorCastFollow) {
            if (effect.actorDummy>=0) {
                const auto& matrices=attachmentMatrices(effect.sourceActorIndex,false);
                const auto found=matrices.find(effect.actorDummy);
                if (found!=matrices.end()) {
                    const auto& m=found->second;
                    return {m[12],m[14],m[13]};
                }
            }
            // SK2.13-A only followed dummies when an opt-in switch was set.
            // No dummy is not a reason to leave an actor aura at the old spot.
            if (effect.sourceActorIndex==-1 && playerCastAnchorValid_)
                return {playerCastX_,playerCastFeet_,playerCastY_};
            const auto root=targetActorRoots_.find(effect.sourceActorIndex);
            if(root!=targetActorRoots_.end())return {root->second[0],root->second[1],root->second[2]};
        }
        // DX9 CCharacter::ItemEffect follows GetAttackItem() at the skill's
        // authored item dummy. The GL bridge already samples those matrices
        // from the same rendered .lab frame for direct strips and ordinary
        // .eff/.par channels.
        if (effect.itemCastFollow && historicalItemDummyBindingEnabled() &&
            effect.itemDummy >= 0) {
            const auto& matrices=attachmentMatrices(effect.sourceActorIndex,true);
            const auto found=matrices.find(effect.itemDummy);
            if (found!=matrices.end()) {
                const auto& m=found->second;
                return {m[12],m[14],m[13]};
            }
            // ItemEffect emits at GetPos() when the item dummy is absent.
            // Do not substitute the unrelated generic body-height offset.
            if (effect.sourceActorIndex==-1 && playerCastAnchorValid_)
                return {playerCastX_,playerCastFeet_,playerCastY_};
            const auto root=targetActorRoots_.find(effect.sourceActorIndex);
            if(root!=targetActorRoots_.end())return {root->second[0],root->second[1],root->second[2]};
        }
        if (effect.kind == EffectEvent::SkillHit && historicalTargetDummyBindingEnabled() &&
            effect.targetActorFollow) {
            if (effect.targetDummy >= 0) {
                const auto actor=targetDummyMatrices_.find(effect.targetActorIndex);
                if (actor!=targetDummyMatrices_.end()) {
                    const auto found=actor->second.find(effect.targetDummy);
                    if (found!=actor->second.end()) {
                        const auto& m=found->second;
                        return {m[12],m[14],m[13]};
                    }
                }
            }
            const auto root=targetActorRoots_.find(effect.targetActorIndex);
            if (root!=targetActorRoots_.end())
                return {root->second[0],root->second[1],root->second[2]};
        }
        if (effect.kind==EffectEvent::Flight && effect.partTraceFollowing) {
            if (sampleAge>=effect.partTraceTimeline.renderAge)
                return effect.partTraceTimeline.renderPosition;
            return effect.partTraceTimeline.positionAt(sampleAge);
        }
        historic_magic::Vec3 p;
        if(effect.kind==EffectEvent::Flight && historic_magic::knownMode(effect.path.mode))
            p=effect.path.position(sampleAge);
        else {
            const float travel=effect.kind==EffectEvent::Flight?
                std::clamp(sampleAge/std::max(.0001f,effect.life),0.f,1.f):
                (effect.kind==EffectEvent::SkillHit?1.f:0.f);
            p={effect.fromX+(effect.toX-effect.fromX)*travel,
               effect.absoluteHeight?effect.path.to.y:0.f,
               effect.fromY+(effect.toY-effect.fromY)*travel};
        }
        if(!effect.absoluteHeight) {
            const auto tile=map.at(static_cast<int>(p.x),static_cast<int>(p.z));
            p.y+=(tile.valid?map.heightAt(p.x,p.z):0.f)+(effect.groundAnchored?0.f:.9f);
        }
        return p;
    };
    const auto positionFor = [&](const Instance& effect) -> historic_magic::Vec3 {
        return positionForAge(effect,effect.age);
    };
    const auto orientationFor = [&](const Instance& effect) {
        historic_magic::Path orientation=effect.path;
        if (effect.partTraceFollowing) {
            orientation.from=effect.partTraceTimeline.renderPosition;
            orientation.to=orientation.from+effect.partTraceTimeline.renderDirection;
        }
        return std::array<float,2>{{
            historic_magic::knownMode(orientation.mode)?orientation.yaw():0.f,
            historic_magic::knownMode(orientation.mode)?orientation.pitch():0.f}};
    };
    const auto dummyBindingFor = [&](const Instance& effect) -> const std::array<float,16>* {
        if (effect.actorCastFollow && effect.actorDummy >= 0) {
            const auto& matrices=attachmentMatrices(effect.sourceActorIndex,false);
            const auto found=matrices.find(effect.actorDummy);
            return found!=matrices.end()?&found->second:nullptr;
        }
        if (effect.itemCastFollow && historicalItemDummyBindingEnabled() && effect.itemDummy >= 0) {
            const auto& matrices=attachmentMatrices(effect.sourceActorIndex,true);
            const auto found=matrices.find(effect.itemDummy);
            return found!=matrices.end()?&found->second:nullptr;
        }
        if (effect.targetActorFollow && historicalTargetDummyBindingEnabled() && effect.targetDummy >= 0) {
            const auto actor = targetDummyMatrices_.find(effect.targetActorIndex);
            if(actor!=targetDummyMatrices_.end()) {
                const auto found=actor->second.find(effect.targetDummy);
                if(found!=actor->second.end())return &found->second;
            }
        }
        return nullptr;
    };
    const auto range2At = [&](const ParticleProfile& profile,int count,
                              float age,unsigned seed,const historic_magic::Vec3& origin,
                              const std::array<float,3>& path,
                              const std::vector<unsigned>& reportedGeneration) {
        const historical_range2_collision::V3 base{{
            origin.x+profile.offset[0]+path[0],
            origin.z+profile.offset[1]+path[1],
            origin.y-.9f+profile.offset[2]+path[2]}};
        const historical_range2_collision::V3 range{{profile.range[0],profile.range[1],profile.range[2]}};
        const historical_range2_collision::V3 direction{{profile.direction[0],profile.direction[1],profile.direction[2]}};
        const auto ground=[&](int x,int y) {
            return historical_range2_ground::sample(map,x,y);
        };
        return historical_range2_collision::simulate(age,count,profile.life,
            profile.step,profile.velocity,base,range,direction,seed,ground,map.ok(),
            reportedGeneration);
    };
    if (historical_particle_extended::enabled() && historical_range2_collision::enabled()) {
        struct PendingRange2Hit {
            std::string effect;
            historic_magic::Vec3 position;
            float age=0.f;
            int variant=0;
            unsigned seed=0;
        };
        std::vector<PendingRange2Hit> pending;
        for (Instance& effect:live_) {
            if (effect.age<0.f || !effect.namedResource.empty()) continue;
            const auto asset=resourceCache_.find(effect.visualId);
            if (asset==resourceCache_.end()) {
                effect.range2FrameSteps.clear();
                continue;
            }
            const auto origin=positionFor(effect);
            for (std::size_t profileIndex=0;profileIndex<asset->second.particles.size();++profileIndex) {
                const ParticleProfile& profile=asset->second.particles[profileIndex];
                if (profile.type!=15 || effect.age<profile.delayTime) continue;
                const float age=effect.age-profile.delayTime;
                const float pathAge=profile.playTime>0.f?std::min(age,profile.playTime):age;
                const auto path=pathPosition(profile,pathAge);
                const int count=std::min(profile.count,24);
                const std::size_t baseIndex=profileIndex*100u;
                const bool actualDt=historical_range2_collision::actualDtEnabled() &&
                                    !effect.actorCastFollow;
                if(actualDt&&effect.range2Timelines.size()<asset->second.particles.size())
                    effect.range2Timelines.resize(asset->second.particles.size());
                if(actualDt&&profileIndex<effect.range2Timelines.size()) {
                    if(!effect.range2FrameSteps.empty()) {
                    auto& timeline=effect.range2Timelines[profileIndex];
                    const historical_range2_collision::V3 range{{
                        profile.range[0],profile.range[1],profile.range[2]}};
                    const historical_range2_collision::V3 direction{{
                        profile.direction[0],profile.direction[1],profile.direction[2]}};
                    const auto ground=[&](int x,int y) {
                        return historical_range2_ground::sample(map,x,y);
                    };
                    for(const auto& frame:effect.range2FrameSteps) {
                        if(frame.oldAge<profile.delayTime)continue;
                        const float frameAge=frame.newAge-profile.delayTime;
                        const float framePathAge=profile.playTime>0.f?
                            std::min(frameAge,profile.playTime):frameAge;
                        const auto frameOrigin=positionForAge(effect,frame.newAge);
                        const auto framePath=pathPosition(profile,framePathAge);
                        const historical_range2_collision::V3 base{{
                            frameOrigin.x+profile.offset[0]+framePath[0],
                            frameOrigin.z+profile.offset[1]+framePath[1],
                            frameOrigin.y-.9f+profile.offset[2]+framePath[2]}};
                        const auto frameResult=timeline.advance(frame.dt,count,profile.life,
                            profile.step,profile.velocity,base,range,direction,
                            effect.seed,ground,map.ok());
                        for(const auto& hit:frameResult.hits) {
                            if(!profile.hitEffect.empty())
                                pending.push_back({profile.hitEffect,
                                    {hit.position[0],hit.position[2],hit.position[1]},
                                    std::max(0.f,effect.age-frame.newAge),
                                    effect.variant,serial_++*2654435761u});
                        }
                    }
                    }
                    continue;
                }
                if(effect.range2HitGenerations.size()<baseIndex+static_cast<std::size_t>(count))
                    effect.range2HitGenerations.resize(baseIndex+static_cast<std::size_t>(count));
                std::vector<unsigned> reported(static_cast<std::size_t>(count));
                for(int particle=0;particle<count;++particle)
                    reported[static_cast<std::size_t>(particle)]=
                        effect.range2HitGenerations[baseIndex+static_cast<std::size_t>(particle)];
                const auto system=range2At(profile,count,age,effect.seed,origin,path,reported);
                for(const auto& hit:system.hits) {
                    const std::size_t index=baseIndex+static_cast<std::size_t>(hit.slot);
                    effect.range2HitGenerations[index]=std::max(
                        effect.range2HitGenerations[index],hit.generation);
                    if(!profile.hitEffect.empty())
                        pending.push_back({profile.hitEffect,
                            {hit.position[0],hit.position[2],hit.position[1]},
                            hit.age,effect.variant,serial_++*2654435761u});
                }
            }
            effect.range2FrameSteps.clear();
        }
        for (const auto& hit:pending) {
            const VisualResource& visual=resourceForName(hit.effect);
            if (visual.particles.empty()) continue;
            Instance child;
            child.kind=EffectEvent::SkillHit;
            child.age=hit.age;
            child.life=.55f;
            child.expiresAfter=child.life;
            for (const auto& profile:visual.particles)
                child.expiresAfter=std::max(child.expiresAfter,
                    profile.delayTime+std::max(0.f,profile.playTime)+profile.life);
            child.variant=hit.variant;
            child.seed=hit.seed;
            child.namedResource=hit.effect;
            child.absoluteHeight=true;
            child.groundAnchored=true;
            child.fixedPosition=true;
            child.fixedWorldPosition=hit.position;
            live_.push_back(std::move(child));
        }
    }
    if (historical_fire_hit::enabled() && historical_particle_pool::enabled()) {
        struct PendingFireHit {
            std::string effect;
            historic_magic::Vec3 position;
            float age = 0.f;
            int variant = 0;
            unsigned seed = 0;
        };
        std::vector<PendingFireHit> pending;
        for (Instance& effect : live_) {
            if (effect.age < 0.f || !effect.namedResource.empty()) continue;
            const auto asset = resourceCache_.find(effect.visualId);
            if (asset == resourceCache_.end()) continue;
            if (effect.fireHitReported.size() < asset->second.particles.size())
                effect.fireHitReported.resize(asset->second.particles.size(), 0);
            if (effect.fireHitStopTimes.size() < asset->second.particles.size())
                effect.fireHitStopTimes.resize(asset->second.particles.size(), -1.f);
            const auto origin = positionFor(effect);
            for (std::size_t index = 0; index < asset->second.particles.size(); ++index) {
                const ParticleProfile& profile = asset->second.particles[index];
                if (profile.type != 2 || profile.hitEffect.empty() ||
                    profile.pathPoints.size() < 2 || !map.ok()) continue;
                const auto event = historical_fire_hit::at(effect.age, profile.delayTime,
                    profile.playTime, profile.pathVelocity, profile.pathDistances,
                    !profile.hitEffect.empty(), map.ok(), effect.fireHitReported[index] != 0);
                if (!event.trigger) continue;
                effect.fireHitReported[index] = 1;
                effect.fireHitStopTimes[index] = event.stopAt;
                const float pathAge = std::max(0.f, effect.age - profile.delayTime);
                const auto path = pathPosition(profile, pathAge);
                historic_magic::Vec3 hitPosition{
                    origin.x + path[0], origin.y + path[2], origin.z + path[1]};
                const float ground = map.range2GroundHeightAt(
                    static_cast<int>(hitPosition.x) * 2,
                    static_cast<int>(hitPosition.z) * 2);
                hitPosition.y = ground < 0.f ? 0.1f : ground;
                pending.push_back({profile.hitEffect, hitPosition, 0.f,
                                   effect.variant, serial_++ * 2654435761u});
            }
        }
        for (const auto& hit : pending) {
            const VisualResource& visual = resourceForName(hit.effect);
            if (visual.particles.empty()) continue;
            Instance child;
            child.kind = EffectEvent::SkillHit;
            child.age = hit.age;
            child.life = .55f;
            child.expiresAfter = child.life;
            for (const auto& profile : visual.particles)
                child.expiresAfter = std::max(child.expiresAfter,
                    profile.delayTime + std::max(0.f, profile.playTime) + profile.life);
            child.variant = hit.variant;
            child.seed = hit.seed;
            child.namedResource = hit.effect;
            child.absoluteHeight = true;
            child.groundAnchored = true;
            child.fixedPosition = true;
            child.fixedWorldPosition = hit.position;
            live_.push_back(std::move(child));
        }
    }
    const auto hasAuthoredVisual = [&](const Instance& effect) {
        const auto asset = resourceCache_.find(effect.visualId);
        if (asset == resourceCache_.end()) return false;
        const VisualResource& visual = asset->second;
        if (effect.kind == EffectEvent::Flight)
            return !visual.particles.empty() || !visual.billboards.empty() ||
                   !visual.nativeMeshes.empty() || !visual.externalMeshes.empty() ||
                   !visual.directStrips.empty();
        return !visual.particles.empty() || !visual.billboards.empty() ||
               !visual.nativeMeshes.empty() || !visual.externalMeshes.empty() ||
               !visual.directStrips.empty();
    };
    // SK2.15-A: CMPStrip trails are independent of normal type=6 CMPPartSys.
    // Source DX9 CMPStrip::GetTrack samples TWO real dummy matrices and keeps
    // previous pairs in world coordinates. No texture-only fake ribbon when
    // equipment dummy endpoints are unavailable.
    for (Instance& effect : live_) {
        if(effect.age<0.f || effect.kind!=EffectEvent::Cast ||
           (!effect.actorCastFollow&&!effect.itemCastFollow))continue;
        const auto asset=resourceCache_.find(effect.visualId);
        if(asset==resourceCache_.end())continue;
        const auto& definitions=asset->second.directStrips;
        const auto& matrices=effect.sourceActorIndex==-1 ?
            (effect.itemCastFollow?stripItemMatrices_:stripBodyMatrices_) :
            attachmentMatrices(effect.sourceActorIndex,effect.itemCastFollow);
        for(std::size_t i=0;i<definitions.size()&&i<effect.directTrails.size();++i){
            const auto& def=definitions[i];
            if(def.texture.empty())continue;
            const auto a=matrices.find(def.dummyA),b=matrices.find(def.dummyB);
            const char* trace=std::getenv("TOP_TRACE_DIRECT_STRIP");
            const bool traceOn=trace&&trace[0]=='1'&&trace[1]=='\0';
            if(a==matrices.end()||b==matrices.end()){
                if(traceOn&&!effect.directStripMissingLogged){
                    std::fprintf(stderr,"[direct-strip] skill=%d visual=%d owner=%s missing_dummies=%d,%d\n",
                        effect.variant,effect.visualId,effect.itemCastFollow?"item":"actor",
                        def.dummyA,def.dummyB);
                    effect.directStripMissingLogged=true;
                }
                continue;
            }
            auto& history=effect.directTrails[i];
            // The covered player actor/item routes bind the DX9 defaults:
            // SetStripCharacter leaves m_bLoop=false and SetStripItem uses false.
            legacy_direct_strip::append(history,def,effect.age,a->second,b->second,false);
            EffectBatch next;next.mode=4;next.texture=def.texture;
            setEffectBlend(next,def.srcBlend,def.dstBlend);
            legacy_direct_strip::emit(history,def,effect.age,next.verts);
            if(!next.verts.empty()){
                if(traceOn&&!effect.directStripLogged){
                    std::fprintf(stderr,"[direct-strip] skill=%d visual=%d owner=%s texture=%s pairs=%zu triangles=%zu dx9=(%d,%d)\n",
                        effect.variant,effect.visualId,effect.itemCastFollow?"item":"actor",
                        def.texture.c_str(),history.pairs.size(),next.verts.size()/3,
                        def.srcBlend,def.dstBlend);
                    effect.directStripLogged=true;
                }
                batches_.push_back(std::move(next));
            }
        }
    }
    for (const Instance& effect : live_) {
        if (effect.age < 0.f) continue;
        // Keep the procedural appearance only when the original effect is
        // unavailable. Drawing both made valid skill .par/.eff resources look
        // like a fast generic ring/orb layered over the historical animation.
        if ((effect.kind == EffectEvent::Cast || effect.kind == EffectEvent::Flight ||
             effect.kind == EffectEvent::SkillHit) && hasAuthoredVisual(effect)) continue;
        EffectBatch batch;
        batch.mode = 4;
        const auto it = resourceCache_.find(effect.visualId);
        batch.texture = it != resourceCache_.end() && !it->second.texture.empty() ? it->second.texture : texture_;
        const float t = std::min(1.f, effect.age / effect.life);
        const float fade = std::min(1.f, (1.f - t) * 2.5f);
        if (effect.kind == EffectEvent::Cast) {
            const Tile tile = map.at(static_cast<int>(effect.fromX), static_cast<int>(effect.fromY));
            const float floor = tile.valid ? map.heightAt(effect.fromX, effect.fromY) : 0.f;
            for (int i = 0; i < 12; ++i) {
                const float angle = static_cast<float>(i) * (2.f * kPi / 12.f) + effect.age * 5.f;
                const float radius = 0.45f + t * 0.75f;
                const float x = effect.fromX + std::sin(angle) * radius;
                const float z = effect.fromY + std::cos(angle) * radius;
                const float y = floor + 0.25f + t * 1.45f + noise(effect.seed + static_cast<unsigned>(i)) * 0.35f;
                quad(batch, x, y, z, 0.16f + 0.1f * (1.f - t), view, 0.45f, 0.72f, 1.f, fade);
            }
        } else if (effect.kind == EffectEvent::Flight ||
                   (effect.kind == EffectEvent::SkillHit && effect.variant >= 900)) {
            // Cannonballs retain the previous projectile behavior. Skill
            // projectiles are emitted BEFORE the impact event, not after it.
            const unsigned visual = static_cast<unsigned>(effect.visualId > 0 ? effect.visualId : effect.variant);
            const float hue = static_cast<float>((visual * 37u) % 100u) / 100.f;
            const bool cannon = effect.variant >= 900;

            const float red = cannon ? 1.f : 0.35f + hue * 0.55f;
            const float green = cannon ? 0.3f : 0.45f + (1.f - hue) * 0.35f;
            const float blue = cannon ? 0.06f : 0.92f - hue * 0.35f;
            const float fly = std::min(1.f, effect.age / (cannon ? 0.28f : effect.life));
            const float x = effect.fromX + (effect.toX - effect.fromX) * fly;
            const float z = effect.fromY + (effect.toY - effect.fromY) * fly;
            const Tile from = map.at(static_cast<int>(effect.fromX), static_cast<int>(effect.fromY));
            const Tile to = map.at(static_cast<int>(effect.toX), static_cast<int>(effect.toY));
            const float y0 = (from.valid ? map.heightAt(effect.fromX, effect.fromY) : 0.f) + 1.25f;
            const float y1 = (to.valid ? map.heightAt(effect.toX, effect.toY) : 0.f) + 0.9f;
            const float y = y0 + (y1 - y0) * fly + std::sin(fly * kPi) * 0.45f;
            quad(batch, x, y, z, (cannon ? 0.55f : 0.42f) - 0.12f * fly, view, red, green, blue, fade);
            for (int i = 0; i < 5; ++i) {
                const float trail = std::max(0.f, fly - static_cast<float>(i + 1) * 0.055f);
                const float tx = effect.fromX + (effect.toX - effect.fromX) * trail;
                const float tz = effect.fromY + (effect.toY - effect.fromY) * trail;
                const float ty = y0 + (y1 - y0) * trail + std::sin(trail * kPi) * 0.45f;
                quad(batch, tx, ty, tz, 0.2f, view, red * .75f, green * .75f, blue, fade * 0.55f);
            }
            if (cannon && fly >= 0.95f) {
                for (int i = 0; i < 10; ++i) {
                    const float angle = noise(effect.seed + static_cast<unsigned>(i * 17)) * 2.f * kPi;
                    const float radius = (effect.age - 0.27f) * (1.2f + noise(effect.seed + static_cast<unsigned>(i)));
                    quad(batch, effect.toX + std::sin(angle) * radius, y1 + std::cos(angle) * radius,
                         effect.toY, cannon ? 0.2f : 0.13f, view,
                         cannon ? 1.f : 0.65f, cannon ? 0.35f : 0.85f, cannon ? 0.05f : 1.f, fade);
                }
            }
        } else if (effect.kind == EffectEvent::SkillHit) {
            // Burst at the target, synchronized with Play::landPlayerAttack.
            const Tile tile = map.at(static_cast<int>(effect.toX), static_cast<int>(effect.toY));
            const float floor = tile.valid ? map.heightAt(effect.toX, effect.toY) : 0.f;
            const unsigned visual = static_cast<unsigned>(effect.visualId > 0 ? effect.visualId : effect.variant);
            const float hue = static_cast<float>((visual * 37u) % 100u) / 100.f;
            for (int i = 0; i < 14; ++i) {
                const float a = static_cast<float>(i) * (2.f * kPi / 14.f);
                const float spread = t * (0.3f + noise(effect.seed + static_cast<unsigned>(i)) * 0.8f);
                quad(batch, effect.toX + std::sin(a) * spread,
                     floor + 0.9f + std::cos(a) * spread, effect.toY,
                     0.12f + 0.08f * (1.f - t), view,
                     0.4f + hue * 0.55f, 0.5f + (1.f - hue) * 0.35f, 0.9f - hue * 0.3f, fade);
            }
        } else if (effect.kind == EffectEvent::Wake) {
            const float dx = effect.toX - effect.fromX;
            const float dz = effect.toY - effect.fromY;
            const float length = std::sqrt(dx * dx + dz * dz);
            const float nx = length > 0.01f ? -dz / length : 1.f;
            const float nz = length > 0.01f ? dx / length : 0.f;
            const float x = effect.fromX;
            const float z = effect.fromY;
            const Tile tile = map.at(static_cast<int>(x), static_cast<int>(z));
            const float floor = tile.valid ? map.heightAt(x, z) : 0.f;
            for (int i = 0; i < 4; ++i) {
                const float spread = (static_cast<float>(i) - 1.5f) * 0.32f;
                const float age = std::min(1.f, effect.age / effect.life);
                const float length = 0.45f + age * 1.15f + static_cast<float>(i) * 0.12f;
                groundQuad(batch, x + nx * spread - dx * age * 0.16f, floor + 0.035f,
                           z + nz * spread - dz * age * 0.16f, length,
                           0.72f, 0.86f, 0.98f, (1.f - age) * 0.38f);
            }
        } else {
            const float x = effect.toX;
            const float z = effect.toY;
            const Tile tile = map.at(static_cast<int>(x), static_cast<int>(z));
            const float floor = tile.valid ? map.heightAt(x, z) : 0.f;
            const float red = effect.kind == EffectEvent::Hurt ? 1.f : 1.f;
            const float green = effect.kind == EffectEvent::Hurt ? 0.18f : 0.62f;
            for (int i = 0; i < 7; ++i) {
                const float angle = noise(effect.seed + static_cast<unsigned>(i * 31)) * 2.f * kPi;
                const float radius = t * (0.35f + noise(effect.seed + static_cast<unsigned>(i)) * 0.7f);
                quad(batch, x + std::sin(angle) * radius, floor + 0.7f + std::cos(angle) * radius, z, 0.12f,
                     view, red, green, 0.18f, fade);
            }
        }
        if (!batch.verts.empty()) batches_.push_back(std::move(batch));
    }

    // 05-I: native I_Effect billboard tracks with authored size, position,
    // alpha, color and Z-rotation keyframes. These are real values read
    // from the historic .eff; no attempt is made to guess missing DX meshes.
    for (const Instance& effect : live_) {
        if (effect.age < 0.f) continue;
        auto asset = resourceCache_.find(effect.visualId);
        if (asset == resourceCache_.end() || asset->second.billboards.empty()) continue;
        const auto origin=positionFor(effect);
        const float baseX=origin.x, baseZ=origin.z;
        const float floor=origin.y-.9f;
        for (const auto& track : asset->second.billboards) {
            const auto& part = track.subeffect;
            const float authoredTime = historic_magic::modelTime(effect.age, track.cycleLength, movementFor(effect));
            if (legacy_eff::framePhase(part, authoredTime).finished) continue;
            const auto key = legacy_eff::sample(part, authoredTime);
            // Billboard X/Y are independent authored scale channels. Do not
            // average them into a square or keep a visible minimum at zero.
            const float width=key.size[0], height=key.size[1];
            // CTexFrame switches texture files with its own authored period.
            // Missing individual frames do not suppress the effect.
            std::string selectedTexture = track.resolvedTexture;
            if (!track.resolvedFrames.empty()) {
                const std::size_t index = legacy_eff::textureFrameIndex(
                    authoredTime, part.textureFrameTime, track.resolvedFrames.size());
                if (!track.resolvedFrames[index].empty())
                    selectedTexture = track.resolvedFrames[index];
            }
            const int blendMode = historicBlendMode(part);
            EffectBatch* dest = nullptr;
            for (auto& existing : batches_) {
                if (existing.texture == selectedTexture && existing.mode == blendMode &&
                    sameEffectBlend(existing, part.sourceBlend, part.destBlend)) {
                    dest = &existing; break;
                }
            }
            if (!dest) {
                batches_.push_back({});
                dest = &batches_.back();
                dest->mode = blendMode;
                setEffectBlend(*dest, part.sourceBlend, part.destBlend);
                dest->texture = selectedTexture;
            }
            const auto uv = legacy_eff::sampleUV(part, authoredTime);
            // MindPower uses (x, y=ground, z=up); OpenGL world uses
            // (x, height, z=ground) as in main.cpp/terrain conversion.
            auto emitBillboardCopy=[&](float x,float z,float size,float alpha) {
                    quadRotated(*dest, x+key.position[0], floor+.9f+key.position[2],
                        z+key.position[1], width*historical_effect_units::planeHalfExtent*size*
                        frozenArrowMultiplier(effect.variant,effect.visualId),
                        view,key.angle[2],
                        std::clamp(key.color[0],0.f,1.f),std::clamp(key.color[1],0.f,1.f),
                        std::clamp(key.color[2],0.f,1.f),std::clamp(key.color[3]*alpha,0.f,1.f),&uv,
                        height*historical_effect_units::planeHalfExtent*size*frozenArrowMultiplier(effect.variant,effect.visualId));
                };
            if (historicalFshadeFrameCountEnabled() && effect.kind == EffectEvent::Flight &&
                movementFor(effect) == historic_magic::kFshade) {
                historic_magic::forEachModelCopy(movementFor(effect),effect.age,
                    effect.fshadeTimeline.state(effect.age),baseX,baseZ,
                    effect.fromX,effect.fromY,effect.toX,effect.toY,emitBillboardCopy);
            } else {
                historic_magic::forEachModelCopy(movementFor(effect),effect.age,
                    baseX,baseZ,effect.fromX,effect.fromY,effect.toX,effect.toY,emitBillboardCopy);
            }
        }
    }

    // 05-K: predefined CEffectModel shapes. The client registers these seven
    // meshes in CMPResManger::LoadTotalMesh(); they are NOT external .lgo
    // geometry. Transform positions in MindPower local space, then map Z-up
    // to Y-up, preserving the authored texture sequence and per-vertex UVs.
    for (const Instance& effect : live_) {
        if (effect.age < 0.f) continue;
        const auto asset = resourceCache_.find(effect.visualId);
        if (asset == resourceCache_.end()) continue;
        const auto origin=positionFor(effect);
        const float baseX=origin.x, baseZ=origin.z;
        const float floor=origin.y-.9f;
        for (const auto& track : asset->second.nativeMeshes) {
            const auto& part = track.subeffect;
            const float at = historic_magic::modelTime(effect.age, track.cycleLength, movementFor(effect));
            if (legacy_eff::framePhase(part, at).finished) continue;
            legacy_mesh::Primitive primitive;
            if (!legacy_mesh::make(part.model, legacy_mesh::paramsAt(part, at), primitive)) continue;
            std::string selected = track.resolvedTexture;
            if (!track.resolvedFrames.empty()) {
                const auto index = legacy_eff::textureFrameIndex(at, part.textureFrameTime,
                                                                 track.resolvedFrames.size());
                if (!track.resolvedFrames[index].empty()) selected = track.resolvedFrames[index];
            }
            if (selected.empty()) continue;
            // DX9 I_Effect::Render respects SRCBLEND/DESTBLEND from the .eff.
            // The mode below is only the old compatibility fallback; the
            // authored D3D factors are forwarded separately. Factor 7 is
            // DESTALPHA, while 6 is INVSRCALPHA.
            const int nativeBlendMode = historicBlendMode(part);
            EffectBatch* dest = nullptr;
            for (auto& batch : batches_) {
                if (batch.mode == nativeBlendMode && batch.texture == selected &&
                    sameEffectBlend(batch, part.sourceBlend, part.destBlend)) {dest = &batch;break;}
            }
            if (!dest) {
                batches_.push_back({});
                dest = &batches_.back();
                dest->mode = nativeBlendMode;
                setEffectBlend(*dest, part.sourceBlend, part.destBlend);
                dest->texture = selected;
            }
            std::vector<legacy_mesh::UV> defaultUV;
            defaultUV.reserve(primitive.points.size());
            for (const auto& point : primitive.points) defaultUV.push_back(point.uv);
            auto authoredUV = legacy_eff::sampleMeshUV(part, at, defaultUV);
            const int movement=movementFor(effect);
            const auto orientation=orientationFor(effect);
            const float heading=orientation[0];
            const float pitch=orientation[1];
            // CEffectModel and LAB dummies share world units. Preserve the
            // authored size, with the existing provisional skill tuning below.
            float worldScale = historical_effect_units::mesh;
            if (effect.kind == EffectEvent::Flight && effect.variant == 81 &&
                effect.visualId == 1018) {
                // DX9 Part_fshade itself performs Scaling(3) after the first
                // draw. Do not stack the earlier guessed 2.75x multiplier.
                float multiplier = movement == historic_magic::kFshade ? 1.f : 2.75f;
                if (const char* raw = std::getenv("TOP_ILLUSION_FX_SCALE")) {
                    char* end = nullptr;
                    const float requested = std::strtof(raw, &end);
                    if (end != raw && *end == '\0' && std::isfinite(requested) &&
                        requested >= 1.f && requested <= 6.f)
                        multiplier = requested;
                }
                worldScale *= multiplier;
            }
            worldScale *= frozenArrowMultiplier(effect.variant,effect.visualId);
            const auto key=legacy_eff::sample(part,at);
            auto emitMagicCopy=[&](float x,float z,float size,float alpha) {
                    auto shaded=key;
                    shaded.color[3]*=alpha;
                    legacy_mesh::emit(primitive,shaded,authoredUV,x,floor+.9f,z,
                                      dest->verts,worldScale*size,heading,pitch);
                };
            if (historicalFshadeFrameCountEnabled() && effect.kind == EffectEvent::Flight &&
                movement == historic_magic::kFshade) {
                historic_magic::forEachModelCopy(movement,effect.age,
                    effect.fshadeTimeline.state(effect.age),baseX,baseZ,
                    effect.fromX,effect.fromY,effect.toX,effect.toY,emitMagicCopy);
            } else {
                historic_magic::forEachModelCopy(movement,effect.age,
                    baseX,baseZ,effect.fromX,effect.fromY,effect.toX,effect.toY,emitMagicCopy);
            }
        }
    }

    // 05-L: geometry authored in model/effect/*.lgo. ModelFile is the same
    // decoder already used for scene/character .lgo objects, not a new parser.
    // Effect UV/keyframe animation is shared with 05-K. One batch per material
    // texture, retaining the generic 05-G fallback if the asset is unsupported.
    for (const Instance& effect : live_) {
        if (effect.age < 0.f) continue;
        const auto asset = resourceCache_.find(effect.visualId);
        if (asset == resourceCache_.end()) continue;
        const auto origin=positionFor(effect);
        const float baseX=origin.x, baseZ=origin.z;
        const float floor=origin.y-.9f;
        for (const auto& track : asset->second.externalMeshes) {
            if (track.model.geoms.empty()) continue;
            const auto& geom = track.model.geoms.front();
            const auto& part = track.subeffect;
            const float at = historic_magic::modelTime(effect.age, track.cycleLength, movementFor(effect));
            if (legacy_eff::framePhase(part, at).finished) continue;
            for (const auto& subset : geom.subsets) {
                legacy_mesh::Primitive primitive;
                if (!legacy_external::subsetPrimitive(geom, subset, at, primitive)) continue;
                const std::string materialTexture = textureFromName(root_, subset.texture);
                std::string frameTexture;
                if (!track.resolvedFrames.empty()) {
                    const auto frame = legacy_eff::textureFrameIndex(
                        at, part.textureFrameTime, track.resolvedFrames.size());
                    frameTexture = track.resolvedFrames[frame];
                }
                const std::string selected = legacy_effect_texture::choose(
                    track.fallbackTexture, materialTexture, frameTexture,
                    legacy_effect_texture::historicalEnabled()).path;
                if (selected.empty()) continue; // no guessed texture
                const int blendMode = historicBlendMode(part);
                EffectBatch* dest = nullptr;
                for (auto& batch : batches_) {
                    if (batch.mode == blendMode && batch.texture == selected &&
                        sameEffectBlend(batch, part.sourceBlend, part.destBlend)) {
                        dest = &batch; break;
                    }
                }
                if (!dest) {
                    batches_.push_back({});
                    dest = &batches_.back();
                    dest->mode = blendMode;
                    setEffectBlend(*dest, part.sourceBlend, part.destBlend);
                    dest->texture = selected;
                }
                std::vector<legacy_mesh::UV> defaultUV;
                defaultUV.reserve(primitive.points.size());
                for (const auto& point : primitive.points) defaultUV.push_back(point.uv);
                const auto animatedUV = legacy_eff::sampleMeshUV(part, at, defaultUV);
                const auto oldCount = dest->verts.size();
                const auto key=legacy_eff::sample(part,at);
                auto emitNativeCopy=[&](float x,float z,float size,float alpha) {
                        auto shaded=key;
                        shaded.color[3]*=alpha;
                        legacy_mesh::emit(primitive,shaded,animatedUV,
                            x,floor+.9f,z,dest->verts,historical_effect_units::mesh*size*
                            frozenArrowMultiplier(effect.variant,effect.visualId),
                            orientationFor(effect)[0], orientationFor(effect)[1]);
                    };
                if (historicalFshadeFrameCountEnabled() && effect.kind == EffectEvent::Flight &&
                    movementFor(effect) == historic_magic::kFshade) {
                    historic_magic::forEachModelCopy(movementFor(effect),effect.age,
                        effect.fshadeTimeline.state(effect.age),baseX,baseZ,
                        effect.fromX,effect.fromY,effect.toX,effect.toY,emitNativeCopy);
                } else {
                    historic_magic::forEachModelCopy(movementFor(effect),effect.age,
                        baseX,baseZ,effect.fromX,effect.fromY,effect.toX,effect.toY,emitNativeCopy);
                }
                // Blend subset material diffuse with the authored effect color.
                const float red = std::isfinite(subset.r) ? std::clamp(subset.r, 0.f, 1.f) : 1.f;
                const float green = std::isfinite(subset.g) ? std::clamp(subset.g, 0.f, 1.f) : 1.f;
                const float blue = std::isfinite(subset.b) ? std::clamp(subset.b, 0.f, 1.f) : 1.f;
                for (std::size_t j=oldCount; j<dest->verts.size(); ++j) {
                    dest->verts[j].r *= red;
                    dest->verts[j].g *= green;
                    dest->verts[j].b *= blue;
                }
            }
        }
    }

    // Sistemas binarios CMPPartCtrl do cliente antigo. Os parametros de
    // quantidade, faixa, vida, velocidade, tamanho e cor agora alimentam os
    // billboards OpenGL; cada textura historica conserva seu proprio lote.
    for (const Instance& effect : live_) {
        if (effect.age < 0.f) continue;
        const VisualResource* asset=nullptr;
        if (!effect.namedResource.empty()) {
            const auto named=namedResourceCache_.find(effect.namedResource);
            if (named!=namedResourceCache_.end()) asset=&named->second;
        } else {
            const auto found=resourceCache_.find(effect.visualId);
            if (found!=resourceCache_.end()) asset=&found->second;
        }
        // An authored .eff mesh without declared .par must remain mesh-only.
        // Previously the empty particle vector fell through to hpushf.par
        // (or hpusht.par), superimposing generic rings on the real flight
        // geometry. In particular, magicsingleinfo ID 18 (visual 1018)
        // declares 04000009.eff and ZERO particle controllers.
        if (asset && asset->particles.empty() &&
            (!asset->billboards.empty() || !asset->nativeMeshes.empty() ||
             !asset->externalMeshes.empty() || !asset->directStrips.empty())) continue;
        const std::vector<ParticleProfile>& profiles =
            (asset && !asset->particles.empty()) ? asset->particles :
            ((effect.kind == EffectEvent::Cast || effect.kind == EffectEvent::Flight) ? castParticles_ : hitParticles_);
        if (profiles.empty() || (effect.kind != EffectEvent::Cast && effect.kind != EffectEvent::Flight &&
                                 effect.kind != EffectEvent::SkillHit)) continue;
        for (std::size_t profileIndex=0;profileIndex<profiles.size();++profileIndex) {
            const ParticleProfile& profile=profiles[profileIndex];
            const bool useHistoricalDummy=profile.type==16 && effect.itemCastFollow;
            // CMPPartSys::Create uses an .eff model in every particle type,
            // not just MODEL/STRIP. A controller mask (notably black 1.tga)
            // is not that model's drawable texture. Never substitute it when
            // the declared geometry is missing or unsupported.
            if (effectModelName(profile.modelName) && profile.modelTracks.empty()) continue;
            if (profile.texture.empty() && profile.modelTracks.empty()) continue;
            if (effect.age < profile.delayTime) continue;
            const float controllerPlayTime = (effect.mapScene || effect.attachedSlot>=0) ? 0.f : profile.playTime;
            // IsDelay() compares the controller clock with _fPlayTime;
            // MODEL/STRIP Stop() immediately sets _bPlay=false. In particular,
            // an end of 2.3 with delay .3 means 2 seconds, not 2.3 more seconds.
            if((profile.type==5 || profile.type==6) && controllerPlayTime>0.f &&
               effect.age>=controllerPlayTime)continue;
            if(profile.type==3 && historical_blast_group::enabled() &&
               controllerPlayTime>0.f && effect.age>=controllerPlayTime)continue;
            const float particleAge = effect.age - profile.delayTime;
            if (controllerPlayTime > 0.f && particleAge > controllerPlayTime + profile.life) continue;
            const float pathAge = controllerPlayTime > 0.f ? std::min(particleAge, controllerPlayTime) : particleAge;
            const std::array<float, 3> authoredPath = pathPosition(profile, pathAge);
            EffectBatch* historical = nullptr;
            for (EffectBatch& candidate : batches_) {
                if (candidate.texture == profile.texture &&
                    sameEffectBlend(candidate, profile.sourceBlend, profile.destBlend) &&
                    sameEffectFilter(candidate, profile.minFilter, profile.magFilter)) {
                    historical = &candidate; break;
                }
            }
            if (!historical) {
                EffectBatch next;
                next.mode = 4;
                setEffectBlend(next, profile.sourceBlend, profile.destBlend);
                setEffectFilter(next, profile.minFilter, profile.magFilter);
                next.texture = profile.texture;
                batches_.push_back(std::move(next));
                historical = &batches_.back();
            }
            const bool singleModel=profile.type==5 || profile.type==6;
            const bool useHistoricalModelTimeline=((profile.type==5 &&
                (historical_model_timeline::enabled() || effect.mapScene)) || (profile.type==8 &&
                historical_arraw_timeline::enabled() &&
                historical_particle_extended::enabled())) &&
                profileIndex<effect.modelTimelines.size() &&
                effect.modelTimelines[profileIndex].initialized;
            const float modelPhase=useHistoricalModelTimeline?
                effect.modelTimelines[profileIndex].phase():0.f;
            const float curveAge=profile.type==6?0.f:
                (profile.type==5?(useHistoricalModelTimeline?
                    modelPhase*profile.life:std::fmod(particleAge,profile.life)):particleAge);
            const float local = useHistoricalModelTimeline?modelPhase:
                std::min(1.f, curveAge / std::max(0.0001f, profile.life));
            const bool useHistoricalRipple=profile.type==4&&
                (historical_ripple::enabled() || effect.mapScene);
            const auto ripplePool=useHistoricalRipple?
                (profileIndex<effect.rippleTimelines.size()&&
                 effect.rippleTimelines[profileIndex].initialized?
                    effect.rippleTimelines[profileIndex].snapshot(profile.frameCount):
                    historical_particle_pool::SystemSample{}):
                historical_particle_pool::SystemSample{};
            const legacyfx::CurveSample curve = legacyfx::sample(
                profile.sizes, profile.colors, profile.rotations, local);
            // 05-BH: the Illusion Slash's historical target impact (195)
            // was shrunk by the generic 0.12 coefficient as well. Keep the
            // authored curve; change only its final world-space scale.
            // Tune only the linked Illusion Slash impact; the same .eff
            // renderer is shared by many other historical skills. Preserve
            // authored curves and allow visual tuning without recompilation.
            float impactScale = 1.f;
            if (effect.kind == EffectEvent::SkillHit && effect.variant == 81 &&
                effect.visualId == 195) {
                impactScale = 2.5f; // SK2.8: user-confirmed provisional impact size
                if (const char* raw = std::getenv("TOP_ILLUSION_IMPACT_SCALE")) {
                    char* end = nullptr;
                    const float requested = std::strtof(raw, &end);
                    if (end != raw && *end == '\0' && std::isfinite(requested) &&
                        requested >= .5f && requested <= 12.f)
                        impactScale = requested;
                }
            }
            impactScale *= frozenArrowMultiplier(effect.variant,effect.visualId);
            const int count = historical_particle_count::resolve(profile.type,profile.count,
                singleModel || (historical_particle_extended::enabled() && profile.type==8),
                historical_particle_count::enabled());
            // SK2.16-A: evaluate births/deaths for SNOW/FIRE/BLAST separately.
            // All other particle types, including the STRIP and MODEL fixes,
            // keep their existing timeline and geometry paths.
            const bool useExtended = (historical_particle_extended::enabled() || effect.mapScene) &&
                (profile.type==7 || profile.type==8 || profile.type==9 ||
                 profile.type==10 || profile.type==11 || profile.type==14 || profile.type==15);
            const bool useRange2Collision = useExtended && profile.type==15 &&
                historical_range2_collision::enabled();
            const bool useBlast3Timeline=useExtended && profile.type==11 &&
                historical_blast3_timeline::enabled() &&
                profileIndex<effect.blast3Timelines.size() &&
                effect.blast3Timelines[profileIndex].initialized;
            const bool useBlast3EffTimeline=useBlast3Timeline &&
                profileIndex<effect.blast3EffTimelines.size() &&
                effect.blast3EffTimelines[profileIndex].initialized;
            const bool usePool = (historical_particle_pool::enabled() || effect.mapScene) &&
                                 (profile.type == 1 || profile.type == 2 || profile.type == 3);
            // Historical motion requires per-slot ages from SK2.16-A; neither
            // switch alone changes the legacy motion path.
            const bool useHistoricalMotion = usePool &&
                (historical_particle_motion::enabled() || effect.mapScene);
            const bool useHistoricalShrink = profile.type == 12 &&
                (historical_shrink::enabled() || historical_shrink::timelineEnabled());
            const bool useHistoricalShrinkTimeline = profile.type == 12 &&
                historical_shrink::timelineEnabled() &&
                profileIndex<effect.shrinkTimelines.size() &&
                effect.shrinkTimelines[profileIndex].initialized;
            const bool useRoundTimeline = profile.type == 9 &&
                historical_particle_extended::enabled() &&
                historical_round_timeline::enabled() &&
                profileIndex < effect.roundTimelines.size() &&
                effect.roundTimelines[profileIndex].initialized;
            const float stopAt = controllerPlayTime > 0.f ?
                std::max(0.f,controllerPlayTime-profile.delayTime) : -1.f;
            const float fireStopAt = profile.type == 2 &&
                profileIndex < effect.fireHitStopTimes.size() ?
                effect.fireHitStopTimes[profileIndex] : -1.f;
            const float effectiveStopAt = fireStopAt >= 0.f ? fireStopAt : stopAt;
            const auto origin=positionFor(effect);
            const float baseX=origin.x+profile.offset[0]+authoredPath[0];
            const float baseZ=origin.z+profile.offset[1]+authoredPath[1];
            const float floor=origin.y-(effect.mapScene?0.f:.9f)+
                              profile.offset[2]+authoredPath[2];
            const historical_dummy::Timeline* dummyTimeline=nullptr;
            if(useHistoricalDummy && profileIndex<profiles.size()) {
                const auto& itemMatrices=attachmentMatrices(effect.sourceActorIndex,true);
                const auto first=itemMatrices.find(1);
                const auto second=itemMatrices.find(2);
                if(first!=itemMatrices.end() && second!=itemMatrices.end()) {
                    if(effect.dummyTimelines.size()<profiles.size())
                        effect.dummyTimelines.resize(profiles.size());
                    const auto xyz=[](const std::array<float,16>& m) {
                        return historical_dummy::V3{m[12],m[13],m[14]};
                    };
                    historical_dummy::advance(effect.dummyTimelines[profileIndex],
                        particleAge,xyz(first->second),xyz(second->second),count,
                        profile.life,profile.step,profile.randomDivisor,profile.velocity,
                        {profile.acceleration[0],profile.acceleration[1],profile.acceleration[2]},
                        effect.seed);
                    dummyTimeline=&effect.dummyTimelines[profileIndex];
                }
            }
            // In DX9 _CreateBlast2 initializes every slot with the same
            // _vFontDir and lateral animation. Avoid re-simulating its
            // 60Hz state machine for each quad in the same frame.
            const auto blast2Shared = (useExtended && profile.type==10) ?
                historical_particle_extended::at(10,0,count,particleAge,
                    profile.life,profile.step,effectiveStopAt,
                    {{profile.range[0],profile.range[1],profile.range[2]}},
                    {{profile.direction[0],profile.direction[1],profile.direction[2]}},
                    {{profile.acceleration[0],profile.acceleration[1],profile.acceleration[2]}},
                    profile.velocity,effect.seed,profile.frameCount,
                    profile.rotations.empty()?0.f:profile.rotations[0]) :
                historical_particle_extended::Sample{};
            std::vector<unsigned> range2Reported;
            if(useRange2Collision) {
                range2Reported.resize(static_cast<std::size_t>(count));
                const std::size_t baseIndex=profileIndex*100u;
                for(int particle=0;particle<count;++particle) {
                    const std::size_t index=baseIndex+static_cast<std::size_t>(particle);
                    if(index<effect.range2HitGenerations.size())
                        range2Reported[static_cast<std::size_t>(particle)]=
                            effect.range2HitGenerations[index];
                }
            }
            const auto range2System=useRange2Collision ?
                (historical_range2_collision::actualDtEnabled()&&!effect.actorCastFollow&&
                 profileIndex<effect.range2Timelines.size() ?
                    effect.range2Timelines[profileIndex].snapshot() :
                    range2At(profile,count,particleAge,effect.seed,origin,
                             authoredPath,range2Reported)) :
                historical_range2_collision::SystemSample{};
            const bool useSharedEmitterTimer = usePool &&
                historical_particle_pool::sharedEmitterTimerEnabled() &&
                (profile.type==1 || profile.type==2);
            const bool useHistoricalFrameAcceleration = useSharedEmitterTimer &&
                useHistoricalMotion &&
                historical_particle_jitter::frameAccelerationEnabled();
            const auto snowFirePool = useSharedEmitterTimer ?
                (profileIndex < effect.snowFireTimelines.size() &&
                 effect.snowFireTimelines[profileIndex].initialized ?
                    effect.snowFireTimelines[profileIndex].snapshot(profile.frameCount) :
                    historical_particle_pool::snowFireSystemAt(profile.type,count,particleAge,
                        profile.life,profile.step,effectiveStopAt,effect.seed,
                        profile.randomDivisor,profile.frameCount,
                        profile.type!=1 || (profile.direction[2]!=0.f && profile.velocity!=0.f))) :
                historical_particle_pool::SystemSample{};
            const auto blast3Pool=useBlast3Timeline?
                effect.blast3Timelines[profileIndex].snapshot():
                historical_blast3_timeline::SystemSample{};
            const bool useBlastGroup=usePool&&profile.type==3&&
                historical_blast_group::enabled()&&
                profileIndex<effect.blastGroupTimelines.size()&&
                effect.blastGroupTimelines[profileIndex].initialized;
            const auto blastGroupPool=useBlastGroup?
                effect.blastGroupTimelines[profileIndex].snapshot():
                historical_blast_group::SystemSample{};
            for (int particle = 0; particle < count; ++particle) {
                const unsigned key = effect.seed + static_cast<unsigned>(particle * 101 + profile.type * 977);
                const float nx = noise(key) * 2.f - 1.f;
                const float ny = noise(key + 1u) * 2.f - 1.f;
                const float nz = noise(key + 2u) * 2.f - 1.f;
                const historical_dummy::Slot* dummySlot=nullptr;
                if(useHistoricalDummy) {
                    if(!dummyTimeline || static_cast<std::size_t>(particle)>=dummyTimeline->slots.size())
                        continue;
                    dummySlot=&dummyTimeline->slots[static_cast<std::size_t>(particle)];
                    if(!dummySlot->alive) continue;
                }
                const auto individual = useHistoricalRipple &&
                    static_cast<std::size_t>(particle)<ripplePool.slots.size() ?
                    ripplePool.slots[static_cast<std::size_t>(particle)] : useSharedEmitterTimer ?
                    snowFirePool.slots[static_cast<std::size_t>(particle)] :
                    useBlastGroup && static_cast<std::size_t>(particle)<blastGroupPool.slots.size() ?
                    blastGroupPool.slots[static_cast<std::size_t>(particle)] :
                    usePool ? historical_particle_pool::at(profile.type,particle,count,
                        particleAge,profile.life,profile.step,effectiveStopAt,
                        effect.seed,profile.randomDivisor) : historical_particle_pool::Sample{};
                if ((usePool && !individual.alive) ||
                    (useHistoricalRipple && !individual.alive)) continue;
                const historical_range2_collision::SlotSample range2Sample =
                    useRange2Collision && static_cast<std::size_t>(particle)<range2System.slots.size() ?
                    range2System.slots[static_cast<std::size_t>(particle)] :
                    historical_range2_collision::SlotSample{};
                historical_particle_extended::Sample extra =
                    useExtended && profile.type==10 ? blast2Shared : useExtended ?
                    historical_particle_extended::at(profile.type,particle,count,particleAge,
                        profile.life,profile.step,stopAt,
                        {{profile.range[0],profile.range[1],profile.range[2]}},
                        {{profile.direction[0],profile.direction[1],profile.direction[2]}},
                        {{profile.acceleration[0],profile.acceleration[1],profile.acceleration[2]}},
                        profile.velocity,effect.seed,profile.frameCount,
                        profile.rotations.empty()?0.f:profile.rotations[0]) : historical_particle_extended::Sample{};
                if (useRoundTimeline) {
                    const auto round=effect.roundTimelines[profileIndex].sample(
                        particle,{{profile.range[0],profile.range[1],profile.range[2]}});
                    extra.alive=round.alive;
                    extra.normalized=round.normalized;
                    extra.position=round.position;
                }
                const auto blast3Sample=useBlast3Timeline &&
                    static_cast<std::size_t>(particle)<blast3Pool.slots.size()?
                    blast3Pool.slots[static_cast<std::size_t>(particle)]:
                    historical_blast3_timeline::Sample{};
                const float shrinkAge = particleAge - static_cast<float>(particle)*
                                        std::max(0.f,profile.step);
                const auto shrinkSample=useHistoricalShrinkTimeline?
                    effect.shrinkTimelines[profileIndex].sample(
                        static_cast<std::size_t>(particle),profile.frameCount):
                    (useHistoricalShrink && !historical_shrink::timelineEnabled())?
                    historical_shrink::at(
                        {{profile.range[0],profile.range[1],profile.range[2]}},
                        profile.velocity,shrinkAge,effect.seed,particle):
                    historical_shrink::Sample{};
                if ((useExtended && !useRange2Collision && !useBlast3Timeline && !extra.alive) ||
                    (useRange2Collision && !range2Sample.alive) ||
                    (useBlast3Timeline && !blast3Sample.alive) ||
                    (profile.type==12 && historical_shrink::timelineEnabled() &&
                     !useHistoricalShrinkTimeline) ||
                    (useHistoricalShrink && !shrinkSample.alive)) continue;
                const float age = useBlast3Timeline ? blast3Sample.age : useRange2Collision ? range2Sample.age :
                    (usePool || useHistoricalRipple) ? individual.age :
                    useHistoricalShrink ? shrinkAge :
                    particleAge + static_cast<float>(particle) * std::max(0.f, profile.step);
                float particleLocal = useBlast3Timeline?blast3Sample.normalized:
                    useHistoricalRipple?individual.normalized:
                    useRange2Collision ?
                    std::clamp(range2Sample.age/std::max(.0001f,profile.life),0.f,1.f) :
                    useHistoricalShrinkTimeline ? shrinkSample.normalized :
                    useHistoricalShrink ? std::clamp(shrinkAge/std::max(.0001f,profile.life),0.f,1.f) :
                    useExtended ? extra.normalized : (usePool ? individual.normalized : local);
                const legacyfx::CurveSample particleCurve = (usePool || useExtended || useHistoricalShrink || useHistoricalRipple) ?
                    legacyfx::sample(profile.sizes,profile.colors,profile.rotations,
                                     particleLocal) : curve;
                const float size = particleCurve.size * 0.12f * impactScale;
                const float* color = particleCurve.color;
                float x = baseX + nx * profile.range[0] * 0.22f + profile.direction[0] * profile.velocity * age * 0.12f;
                float y = floor + (effect.mapScene?0.f:0.65f) + nz * profile.range[2] * 0.18f +
                                profile.direction[2] * profile.velocity * age * 0.12f +
                                0.5f * profile.acceleration[2] * age * age * 0.08f;
                float z = baseZ + ny * profile.range[1] * 0.22f + profile.direction[1] * profile.velocity * age * 0.12f;
                if (useHistoricalMotion) {
                    // CMPPartSys authored positions use DX9 X/Y horizontal,
                    // Z vertical. No ad-hoc .12/.18/.22 speed/range factors.
                    const historical_particle_motion::V3 range{{profile.range[0],
                        profile.range[1],profile.range[2]}};
                    const historical_particle_motion::V3 direction{{profile.direction[0],
                        profile.direction[1],profile.direction[2]}};
                    const historical_particle_motion::V3 acceleration{{profile.acceleration[0],
                        profile.acceleration[1],profile.acceleration[2]}};
                    // Use the pool's generation when enabled, otherwise one
                    // stable seeded particle motion for the legacy lifetime.
                    auto pose=historical_particle_motion::at(profile.type,
                        range,direction,acceleration,profile.velocity,age,
                        effect.seed,particle,usePool?individual.generation:0u,
                        profile.randomDivisor);
                    if (useHistoricalFrameAcceleration)
                        for (std::size_t axis = 0; axis < pose.position.size(); ++axis)
                            pose.position[axis] += individual.randomAccelerationOffset[axis];
                    if (profile.type == 3 && historical_blast_motion::actualDtEnabled() &&
                        profileIndex < effect.blastMotionTimelines.size()) {
                        const auto correction = effect.blastMotionTimelines[profileIndex].correction(
                            acceleration,static_cast<std::size_t>(particle),individual.generation);
                        for (std::size_t axis = 0; axis < correction.size(); ++axis)
                            pose.position[axis] += correction[axis];
                    } else if (profile.type == 3 && historical_blast_motion::enabled()) {
                        const auto correction = historical_blast_motion::discreteAccelerationCorrection(
                            acceleration, age);
                        for (std::size_t axis = 0; axis < correction.size(); ++axis)
                            pose.position[axis] += correction[axis];
                    }
                    if (profile.type==1 && std::fabs(pose.velocity[2])<1e-8f)
                        continue; // _CreateSnow returns false for zero vertical motion
                    x=baseX+pose.position[0];
                    z=baseZ+pose.position[1];
                    y=floor+(effect.mapScene?0.f:.9f)+pose.position[2];
                    if (historical_particle_extended::enabled() &&
                        !useHistoricalFrameAcceleration &&
                        (profile.type==1 || profile.type==2)) {
                        // In DX9 FIRE/SNOW choose +/- acceleration per frame;
                        // reproduce the rule on a repeatable nominal 60Hz
                        // clock, without a fake persistent force.
                        const auto jitter=historical_particle_jitter::offset(
                            acceleration,age,effect.seed,particle,individual.generation);
                        x+=jitter[0]; z+=jitter[1]; y+=jitter[2];
                    }
                }
                if (useExtended) {
                    // Historical extended modes are in DX9 coordinates;
                    // X/Y remain horizontal and Z becomes OpenGL up.
                    x=baseX+extra.position[0];
                    z=baseZ+extra.position[1];
                    y=floor+(effect.mapScene?0.f:.9f)+extra.position[2];
                }
                if (useBlast3Timeline) {
                    // Keep the DX9 particle-space vector in XYZ; map its Z-up
                    // axis only at the existing OpenGL coordinate boundary.
                    x=baseX+blast3Sample.position[0];
                    z=baseZ+blast3Sample.position[1];
                    y=floor+(effect.mapScene?0.f:.9f)+blast3Sample.position[2];
                }
                if (useRange2Collision) {
                    // DX9 coordinates are X/Y horizontal and Z-up. MapFile
                    // supplies terrain height in world units; reorder only at
                    // this OpenGL boundary.
                    x=range2Sample.position[0];
                    z=range2Sample.position[1];
                    y=range2Sample.position[2];
                }
                if(dummySlot) {
                    // The item matrices are in GL world storage. Reorder back
                    // to DX9 XYZ inside the controller, then map Z-up to GL
                    // height here, at the renderer boundary.
                    x=dummySlot->position.x;
                    z=dummySlot->position.y;
                    y=dummySlot->position.z;
                }
                if(singleModel) {
                    // MoveTo subtract range/2. MODEL adds it back in
                    // _FrameMoveModel; STRIP deliberately retains that corner.
                    const float corner=profile.type==6?.5f:0.f;
                    if(profile.type==5 && historical_model_position::enabled()) {
                        const auto midpoint=historical_model_position::midpointOffset(
                            {{profile.range[0],profile.range[1],profile.range[2]}});
                        x=baseX+midpoint[0];
                        y=floor+(effect.mapScene?0.f:.9f)+midpoint[2];
                        z=baseZ+midpoint[1];
                    } else {
                        x=baseX-profile.range[0]*corner;
                        y=floor+(effect.mapScene?0.f:.9f)-profile.range[2]*corner;
                        z=baseZ-profile.range[1]*corner;
                    }
                }
                if(profile.type==4) {
                    // _CreateRipple samples the emitter range; _FrameMoveRipple
                    // animates size/color but does not translate the particle.
                    if(useHistoricalRipple) {
                        const unsigned rippleKey=key^(individual.generation*0x9e3779b9u);
                        x=baseX+noise(rippleKey)*profile.range[0];
                        y=floor+(effect.mapScene?0.f:.9f)+noise(rippleKey+2u)*profile.range[2];
                        z=baseZ+noise(rippleKey+1u)*profile.range[1];
                    } else {
                        x=baseX+nx*profile.range[0]*.5f;
                        y=floor+(effect.mapScene?0.f:.9f)+nz*profile.range[2]*.5f;
                        z=baseZ+ny*profile.range[1]*.5f;
                    }
                }
                if (effect.mapScene && std::fabs(effect.mapSceneYaw) > 1e-6f) {
                    const float dx = x - origin.x;
                    const float dz = z - origin.z;
                    historical_map_scene_effects::rotateHorizontal(
                        dx, dz, effect.mapSceneYaw, x, z);
                    x += origin.x;
                    z += origin.z;
                }
                const float alpha = color[3]; // track RGBA historico inclui o fade
                if (profile.type == 13 && historical_shade::enabled()) {
                    // DX9 SHADE draws one CMPShadeMap projected over the
                    // terrain; it is not a free-standing particle billboard.
                    // Convert its grid to GL's Y-up coordinates and let the
                    // existing per-system blend/filter and texture path apply.
                    if (particle != 0) continue;
                    const float radius = profile.sizes.empty() ? particleCurve.size :
                                         profile.sizes.front();
                    const auto shade = historical_shade::build(baseX, baseZ, radius, floor,
                        [&map](float gridX, float gridZ, bool edgeX, bool edgeZ) {
                            return historical_shade::terrainGridEnabled() ?
                                map.shadeGroundHeightAt(gridX, gridZ, edgeX, edgeZ) :
                                map.heightAt(gridX, gridZ);
                        });
                    for (const auto& point : shade)
                        historical->verts.push_back(vertex(point.x, point.y, point.z,
                            color[0], color[1], color[2], alpha, point.u, point.v));
                    continue;
                }
                if((singleModel || (profile.type>=1 && profile.type<=4)) &&
                   legacy_mesh::recognized(profile.modelName)) {
                    if(profile.billboard) {
                        // RectPlane spans -0.5..+0.5; quadRotated takes a half-size.
                        quadRotated(*historical,x,y,z,particleCurve.size*historical_effect_units::planeHalfExtent*impactScale,
                            view,0.f,
                            color[0],color[1],color[2],alpha);
                    } else {
                        legacy_mesh::Primitive primitive;
                        if(legacy_mesh::make(profile.modelName,{},primitive)) {
                            legacy_eff::Key pose;
                            pose.size={particleCurve.size,particleCurve.size,particleCurve.size};
                            if(profile.type!=4)for(int c=0;c<3;++c) pose.angle[c]=legacyfx::sampleTrack(
                                profile.rotations,3,c,particleLocal,0.f);
                            for(int c=0;c<4;++c)pose.color[c]=color[c];
                            legacy_mesh::emit(primitive,pose,{},x,y,z,historical->verts,historical_effect_units::mesh*impactScale);
                        }
                    }
                    continue;
                }
                // A particle referencing a nested .eff must render its
                // authored geometry+texture, rather than a quad using the
                // (potentially alpha-only) controller texture such as 1.tga.
                if (!profile.modelTracks.empty()) {
                    const bool useStripEffFrameTimeline = profile.type == 6 &&
                        (historical_eff_timeline::enabled() || effect.mapScene) &&
                        profileIndex < effect.stripEffTimelines.size() &&
                        effect.stripEffTimelines[profileIndex].initialized;
                    const bool useStripEffControllerClocks = profile.type == 6 &&
                        (historical_eff_timeline::controllerClocksEnabled() || effect.mapScene) &&
                        profileIndex < effect.stripEffTimelines.size() &&
                        effect.stripEffTimelines[profileIndex].initialized;
                    const bool useRippleEffTimeline = profile.type == 4 &&
                        (historical_ripple::enabled() || effect.mapScene) &&
                        profileIndex < effect.rippleEffTimelines.size() &&
                        effect.rippleEffTimelines[profileIndex].initialized;
                    const bool useArrawEffTimeline = profile.type == 8 &&
                        historical_arraw_timeline::enabled() &&
                        historical_particle_extended::enabled() &&
                        profileIndex < effect.arrawEffTimelines.size() &&
                        effect.arrawEffTimelines[profileIndex].initialized;
                    const bool useModelEffTimeline=profile.type==5 &&
                        (historical_model_timeline::enabled() || effect.mapScene) &&
                        (historical_model_eff_timeline::enabled() || effect.mapScene) &&
                        profileIndex<effect.modelEffTimelines.size() &&
                        effect.modelEffTimelines[profileIndex].initialized;
                    const bool useRoundEffTimeline=profile.type==9 &&
                        historical_particle_extended::enabled() &&
                        historical_round_timeline::enabled() &&
                        profileIndex<effect.roundEffTimelines.size() &&
                        effect.roundEffTimelines[profileIndex].initialized;
                    const bool useSnowFireEffTimeline=(profile.type==1 || profile.type==2) &&
                        historical_eff_timeline::snowFireEnabled() && !profile.mediayEff &&
                        profileIndex<effect.snowFireEffTimelines.size() &&
                        effect.snowFireEffTimelines[profileIndex].initialized;
                    const auto* effTimeline = useBlast3EffTimeline ?
                        &effect.blast3EffTimelines[profileIndex] : useArrawEffTimeline ?
                        &effect.arrawEffTimelines[profileIndex] :
                        (useRoundEffTimeline ? &effect.roundEffTimelines[profileIndex] :
                        (useModelEffTimeline ? &effect.modelEffTimelines[profileIndex] :
                        (useSnowFireEffTimeline ? &effect.snowFireEffTimelines[profileIndex] :
                        (useRippleEffTimeline ?
                        &effect.rippleEffTimelines[profileIndex] :
                        (useStripEffFrameTimeline || useStripEffControllerClocks ?
                            &effect.stripEffTimelines[profileIndex] : nullptr)))));
                    const bool useHistoricalEffFrames = useBlast3EffTimeline ||
                                                       useStripEffFrameTimeline ||
                                                       useRippleEffTimeline ||
                                                       useArrawEffTimeline ||
                                                       useModelEffTimeline ||
                                                       useRoundEffTimeline ||
                                                       useSnowFireEffTimeline;
                    const bool useHistoricalEffClocks = useBlast3EffTimeline ||
                                                        useStripEffControllerClocks ||
                                                        useRippleEffTimeline ||
                                                        useArrawEffTimeline ||
                                                        useModelEffTimeline ||
                                                        useRoundEffTimeline ||
                                                        useSnowFireEffTimeline;
                    for (std::size_t modelTrackIndex = 0;
                         modelTrackIndex < profile.modelTracks.size(); ++modelTrackIndex) {
                        const auto& track = profile.modelTracks[modelTrackIndex];
                        const auto& part = track.subeffect;
                        const int iceLayer = static_cast<int>(&track - profile.modelTracks.data());
                        const bool traceIce = traceFrozen && effect.visualId == -1020 &&
                                              profile.type == 6 && iceLayer < 6;
                        const int iceTimeBin = std::clamp(
                            static_cast<int>(effect.age * 4.f), 0, 11);
                        bool traceIceNow = false;
                        if (traceIce && frozenLogged.size() < 250) {
                            traceIceNow = frozenLogged.emplace(
                                effect.seed, iceLayer, iceTimeBin).second;
                        }
                        // CMPModelEff restarts the entire .eff when its last
                        // subeffect stops. Short layers stay off until then.
                        // DX9 PARTTICLE_STRIP is a one-particle controller,
                        // but its nested CMPModelEff still animates and loops.
                        // The old `!singleModel` rule incorrectly finished the
                        // child after one cycle for cast/hit type-6 systems.
                        const float modelAge = legacy_strip_system::nestedModelAge(
                            profile.type, effect.kind==EffectEvent::Flight || effect.attachedSlot>=0,
                            (usePool ? age : (useExtended ? extra.age : particleAge)), track.cycleLength,
                            legacy_strip_system::historicalCyclesEnabled() || effect.mapScene);
                        const auto effFrame = useHistoricalEffFrames ?
                            effTimeline->phase(
                                modelTrackIndex, profile.modelTracks) :
                            legacy_eff::framePhase(part, modelAge);
                        if (effFrame.finished) {
                            if (traceIceNow) std::fprintf(stderr,
                                "[frozen-geometry] layer=%d age=%.2f status=finished\n",
                                iceLayer, effect.age);
                            continue;
                        }
                        auto keyframe = useHistoricalEffFrames ?
                            legacy_eff::samplePhase(part, effFrame) :
                            legacy_eff::sample(part, modelAge);
                        legacy_mesh::ParentTransform parent;
                        parent.scale=particleCurve.size*impactScale;
                        parent.billboard=profile.billboard;
                        parent.view=view;
                        if(historicalBoundModelMatrixEnabled() && singleModel &&
                           !profile.billboard && !profile.modelDirection && !part.billboard)
                            parent.bone=dummyBindingFor(effect);
                        // BindingBone supplies SBoneMat directly for MODEL/STRIP
                        // .eff children (MPParticleSys.cpp:3157-3159,3218-3220).
                        // Range/path/parent angles from MoveTo are not that matrix.
                        const float modelX=parent.bone?origin.x+profile.offset[0]:x;
                        const float modelY=parent.bone?origin.y+profile.offset[2]:y;
                        const float modelZ=parent.bone?origin.z+profile.offset[1]:z;
                        const auto traceBinding = [&](std::size_t vertices,bool boundCenter) {
                            if(!effect_binding_trace::enabled())return;
                            effect_binding_trace::Row row;
                            row.skill=effect.variant;row.visual=effect.visualId;
                            row.kind=effect.kind;row.seed=effect.seed;row.age=effect.age;
                            row.system=profile.sourceSystem;row.part=track.sourcePart;row.type=profile.type;
                            row.parentBillboard=profile.billboard;row.modelDirection=profile.modelDirection;
                            row.childBillboard=part.billboard;row.resource=profile.modelName;row.model=part.model;
                            row.particle=profile.sourceParticle;
                            row.matrix=dummyBindingFor(effect);row.vertices=vertices;
                            row.boundMatrix=parent.bone!=nullptr;row.boundCenter=boundCenter;
                            if(effect.actorCastFollow){row.owner=effect.sourceActorIndex==-1?"player_actor":"source_actor";row.dummy=effect.actorDummy;row.actor=effect.sourceActorIndex;}
                            else if(effect.itemCastFollow){row.owner=effect.sourceActorIndex==-1?"player_item":"source_item";row.dummy=effect.itemDummy;row.actor=effect.sourceActorIndex;}
                            else if(effect.targetActorFollow){row.owner="target_actor";row.dummy=effect.targetDummy;row.actor=effect.targetActorIndex;}
                            effect_binding_trace::record(row);
                        };
                        if(profile.type!=4 && !(profile.type==8 &&
                           historical_arraw_timeline::enabled() &&
                           historical_particle_extended::enabled()))
                            for(int c=0;c<3;++c)
                            parent.angle[c]=legacyfx::sampleTrack(profile.rotations,3,c,local,0.f);
                        if (effect.mapScene) parent.angle[1] += effect.mapSceneYaw;
                        if (historical_strip_alpha::enabled() && profile.type == 6) {
                            keyframe.color[3] = historical_strip_alpha::effectiveAlpha(
                                profile.type, color[3], keyframe.color[3]);
                        } else {
                            for (int channel = 0; channel < 4; ++channel)
                                keyframe.color[static_cast<std::size_t>(channel)] *= color[channel];
                        }
                        std::string selected = track.texture;
                        if (!track.frames.empty()) {
                            const auto index = useHistoricalEffClocks && part.type == 1 ?
                                effTimeline->textureFrame(modelTrackIndex) :
                                legacy_eff::textureFrameIndex(modelAge, part.textureFrameTime,
                                                              track.frames.size());
                            if (!track.frames[index].empty()) selected = track.frames[index];
                        }
                        // External model tracks can use the material texture of
                        // each subset instead of the parent .eff texture.
                        if (track.externalModel) {
                            const auto& model = *track.externalModel;
                            for (const auto& geom : model.geoms) {
                                for (const auto& subset : geom.subsets) {
                                    legacy_mesh::Primitive primitive;
                                    if (!legacy_external::subsetPrimitive(geom,subset,modelAge,primitive)) {
                                        if (traceIceNow) std::fprintf(stderr,
                                            "[frozen-geometry] layer=%d age=%.2f status=invalid_subset\n",
                                            iceLayer, effect.age);
                                        continue;
                                    }
                                    const std::string materialTexture = textureFromName(root_, subset.texture);
                                    std::string frameTexture;
                                    if (!track.frames.empty()) {
                                        const auto frame=useHistoricalEffClocks && part.type == 1 ?
                                            effTimeline->textureFrame(modelTrackIndex) :
                                            legacy_eff::textureFrameIndex(modelAge,part.textureFrameTime,
                                                                          track.frames.size());
                                        frameTexture=track.frames[frame];
                                    }
                                    const auto chosenTexture = legacy_effect_texture::choose(
                                        track.texture, materialTexture, frameTexture,
                                        legacy_effect_texture::historicalEnabled());
                                    const std::string& subsetTex = chosenTexture.path;
                                    if (subsetTex.empty()) {
                                        if (traceIceNow) std::fprintf(stderr,
                                            "[frozen-geometry] layer=%d age=%.2f status=missing_texture "
                                            "part_tex=%s subset_tex=%s\n",
                                            iceLayer, effect.age, part.texture.c_str(),
                                            subset.texture.c_str());
                                        continue;
                                    }
                                    const int mode=historicBlendMode(part);
                                    EffectBatch* target=nullptr;
                                    for (auto& candidate:batches_) {
                                        if (candidate.mode==mode && candidate.texture==subsetTex &&
                                            sameEffectBlend(candidate, part.sourceBlend, part.destBlend) &&
                                            sameEffectFilter(candidate, profile.minFilter, profile.magFilter)) {
                                            target=&candidate;break;
                                        }
                                    }
                                    if (!target) {
                                        batches_.push_back({});
                                        target=&batches_.back();
                                        target->mode=mode;
                                        setEffectBlend(*target, part.sourceBlend, part.destBlend);
                                        setEffectFilter(*target, profile.minFilter, profile.magFilter);
                                        target->texture=subsetTex;
                                    }
                                    std::vector<legacy_mesh::UV> defaultUV;
                                    defaultUV.reserve(primitive.points.size());
                                    for (const auto& point:primitive.points)
                                        defaultUV.push_back(point.uv);
                                    const auto uv=useHistoricalEffClocks ?
                                        effTimeline->sampleMesh(
                                            modelTrackIndex,part,defaultUV) :
                                        legacy_eff::sampleMeshUV(part,modelAge,defaultUV);
                                    const auto before=target->verts.size();
                                    // Keep historic scale/curves; no special
                                    // enlargement for Frozen Arrow.
                                    legacy_mesh::emit(primitive,keyframe,uv,modelX,modelY,modelZ,
                                                      target->verts,historical_effect_units::mesh,0.f,0.f,&parent);
                                    traceBinding(target->verts.size()-before,false);
                                    const float red=std::isfinite(subset.r)?std::clamp(subset.r,0.f,1.f):1.f;
                                    const float green=std::isfinite(subset.g)?std::clamp(subset.g,0.f,1.f):1.f;
                                    const float blue=std::isfinite(subset.b)?std::clamp(subset.b,0.f,1.f):1.f;
                                    for (std::size_t j=before;j<target->verts.size();++j) {
                                        target->verts[j].r *= red;
                                        target->verts[j].g *= green;
                                        target->verts[j].b *= blue;
                                    }
                                    if (traceIceNow) {
                                        float minY = std::numeric_limits<float>::infinity();
                                        float maxY = -std::numeric_limits<float>::infinity();
                                        float minAlpha = 1.f, maxAlpha = 0.f;
                                        for (std::size_t j=before;j<target->verts.size();++j) {
                                            const auto& v=target->verts[j];
                                            minY=std::min(minY,v.y); maxY=std::max(maxY,v.y);
                                            minAlpha=std::min(minAlpha,v.a);
                                            maxAlpha=std::max(maxAlpha,v.a);
                                        }
                                        const auto tile=map.at(static_cast<int>(x),static_cast<int>(z));
                                        const float ground=tile.valid ? map.heightAt(x,z) : 0.f;
                                        std::fprintf(stderr,
                                            "[frozen-geometry] layer=%d age=%.2f model_age=%.2f "
                                            "triangles=%zu rgba_alpha=%.3f..%.3f "
                                            "height=%.3f..%.3f terrain=%.3f "
                                            "srcdst=%d,%d texture=%s ground_anchor=%d "
                                            "texture_source=%s eff_tex=%s material_tex=%s\n",
                                            iceLayer,effect.age,modelAge,
                                            (target->verts.size()-before)/3,
                                            minAlpha,maxAlpha,minY,maxY,ground,
                                            part.sourceBlend,part.destBlend,subsetTex.c_str(),
                                            effect.groundAnchored?1:0,
                                            chosenTexture.origin == legacy_effect_texture::Origin::Effect ? "eff" :
                                            chosenTexture.origin == legacy_effect_texture::Origin::Animated ? "frame" :
                                            chosenTexture.origin == legacy_effect_texture::Origin::Material ? "lgo" : "none",
                                            track.texture.c_str(), materialTexture.c_str());
                                    }
                                }
                            }
                            continue;
                        }
                        if (selected.empty()) continue;
                        const int mode = historicBlendMode(part);
                        EffectBatch* target = nullptr;
                        for (EffectBatch& candidate : batches_)
                            if (candidate.mode == mode && candidate.texture == selected &&
                                sameEffectBlend(candidate, part.sourceBlend, part.destBlend) &&
                                sameEffectFilter(candidate, profile.minFilter, profile.magFilter)) {
                                target = &candidate; break;
                            }
                        if (!target) {
                            batches_.push_back({});
                            target = &batches_.back();
                            target->mode = mode;
                            setEffectBlend(*target, part.sourceBlend, part.destBlend);
                            setEffectFilter(*target, profile.minFilter, profile.magFilter);
                            target->texture = selected;
                        }
                        const auto beforePart=target->verts.size();
                        bool usedBindingCenter=false;
                        if (part.billboard) {
                            const auto uv=useHistoricalEffClocks ?
                                effTimeline->sampleQuad(
                                    modelTrackIndex,part) : legacy_eff::sampleUV(part,modelAge);
                            // DX9 also replaces a billboard child's center when
                            // its non-billboard MODEL/STRIP parent uses BindingBone.
                            const bool bindingCenter = parent.billboard ||
                                (historicalBoundBillboardCenterEnabled() && singleModel &&
                                 !profile.modelDirection && dummyBindingFor(effect));
                            usedBindingCenter=bindingCenter;
                            const auto pos=parent.apply(keyframe.position);
                            quadRotated(*target, x+(bindingCenter?0.f:pos[0]),
                                y+(bindingCenter?0.f:pos[2]),
                                z+(bindingCenter?0.f:pos[1]),
                                keyframe.size[0] * historical_effect_units::planeHalfExtent * parent.scale,
                                view, keyframe.angle[2],
                                keyframe.color[0], keyframe.color[1],
                                keyframe.color[2], keyframe.color[3],&uv,keyframe.size[1]*historical_effect_units::planeHalfExtent*parent.scale);
                        } else {
                            legacy_mesh::Primitive primitive;
                            if (!legacy_mesh::make(part.model,
                                legacy_mesh::paramsAt(part, modelAge), primitive)) continue;
                            std::vector<legacy_mesh::UV> defaultUV;
                            for (const auto& point : primitive.points) defaultUV.push_back(point.uv);
                            const auto uv = useHistoricalEffClocks ?
                                effTimeline->sampleMesh(
                                    modelTrackIndex,part,defaultUV) :
                                legacy_eff::sampleMeshUV(part, modelAge, defaultUV);
                            legacy_mesh::emit(primitive, keyframe, uv,
                                modelX, modelY, modelZ, target->verts,historical_effect_units::mesh,0.f,0.f,&parent);
                        }
                        traceBinding(target->verts.size()-beforePart,usedBindingCenter);
                    }
                    continue;
                }
                // Tipos do CMPPartCtrl: neve cai, fogo sobe, blast se afasta;
                // ripple/round/range ficam no plano do terreno e strip/arrow/
                // line conservam uma cauda orientada em vez de um billboard.
                if (!useHistoricalMotion) {
                    if (profile.type == 1) y -= profile.velocity * age * 0.18f;
                    if (profile.type == 2) y += std::fabs(profile.velocity) * age * 0.2f;
                }
                if (profile.type == 12) {
                    // CMPPartCtrl::SHRINK creates each particle in the authored
                    // range and moves it toward the range midpoint. It is the
                    // inverse of BLAST/BLAST3, not another radial burst.
                    // The historical pool emits particles at _fStep intervals;
                    // age them from deterministic staggered birth times.
                    if (useHistoricalShrink) {
                        x = baseX + shrinkSample.position[0];
                        z = baseZ + shrinkSample.position[1];
                        y = floor + (effect.mapScene?0.f:0.9f) + shrinkSample.position[2];
                    } else {
                        const float dx = nx * profile.range[0] * 0.22f;
                        const float dy = nz * profile.range[2] * 0.18f;
                        const float dz = ny * profile.range[1] * 0.22f;
                        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                        const float travel = std::min(distance,
                            std::fabs(profile.velocity) * shrinkAge * 0.12f);
                        const float fraction = distance > 0.0001f ? travel / distance : 1.f;
                        x = baseX + dx * (1.f - fraction);
                        y = floor + (effect.mapScene?0.f:0.65f) + dy * (1.f - fraction);
                        z = baseZ + dz * (1.f - fraction);
                        particleLocal = std::min(1.f, shrinkAge / std::max(0.05f, profile.life));
                        if (fraction >= 1.f || distance <= 0.06f) continue;
                    }
                }
                if ((profile.type == 3 && !useHistoricalMotion) || profile.type == 11) {
                    const float radial = std::fabs(profile.velocity) * age * 0.1f;
                    x += nx * radial;
                    y += nz * radial;
                    z += ny * radial;
                }
                if (profile.type == 4 || profile.type == 10 ||
                    (profile.type == 16 && !useHistoricalDummy) || profile.type == 17) {
                    const float spread = size * (1.f + particleLocal * 2.5f);
                    groundQuad(*historical, x, floor + 0.025f + static_cast<float>(particle) * 0.0005f, z,
                               spread, color[0], color[1], color[2], alpha);
                } else if (profile.type == 7 && profile.billboard &&
                           profile.modelName == "RectPlane" && useExtended) {
                    // DX9 billboards WIND by its camera basis, then applies
                    // its accumulated Z rotation. RectPlane is authored at
                    // +/-0.5, while quadRotated takes a half extent.
                    quadRotated(*historical,x,y,z,particleCurve.size*impactScale*.5f,view,
                        profile.velocity*extra.age,color[0],color[1],color[2],alpha);
                } else if (profile.type == 7 && !profile.billboard &&
                           profile.modelName == "RectPlane" && useExtended) {
                    groundQuadRotated(*historical,x,y,z,particleCurve.size*impactScale*.5f,
                        profile.velocity*extra.age,color[0],color[1],color[2],alpha);
                } else if (profile.type == 7 || profile.type == 9 || profile.type == 18) {
                    float dx = profile.direction[0];
                    float dz = profile.direction[1];
                    float length = std::sqrt(dx * dx + dz * dz);
                    if (length < 0.01f) { dx = effect.toX - effect.fromX; dz = effect.toY - effect.fromY; length = std::sqrt(dx * dx + dz * dz); }
                    if (length < 0.01f) { dx = 0.f; dz = 1.f; length = 1.f; }
                    dx /= length; dz /= length;
                    ribbon(*historical, x - dx * size * 4.f, y, z - dz * size * 4.f, x, y, z, size * .45f,
                           color[0], color[1], color[2], alpha);
                } else {
                    // O numero de keyframes do .par NAO descreve quantas
                    // colunas existem na textura. Nao recortar o atlas UV.
                    quadRotated(*historical, x, y, z, size,
                                view, particleCurve.rotation,
                                color[0], color[1], color[2], alpha);
                }
            }
        }
    }
}

}  // namespace top
