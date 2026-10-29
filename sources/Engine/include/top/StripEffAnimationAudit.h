#pragma once

#include "top/FxCoverageAudit.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace top { namespace strip_eff_animation_audit {
namespace fs = std::filesystem;

struct Options {
    fs::path client = "Client";
    fs::path output = "build/strip-eff-animation-audit";
    fs::path deps = "sources/auditoria-historica/dependencies.csv";
    fs::path dx9Header = "arquivo/fontes/Engine/include/I_Effect.h";
    fs::path dx9Controller = "arquivo/fontes/Engine/src/MPModelEff.cpp";
};

struct Reference {
    std::string parFile;
    std::size_t system = 0;
};

inline bool readText(const fs::path& path, std::string& text) {
    std::ifstream in(path);
    if (!in) return false;
    text.assign(std::istreambuf_iterator<char>(in), {});
    return !in.bad();
}

inline std::size_t sourceLine(const fs::path& path, const std::string& token) {
    std::ifstream in(path);
    std::string line;
    std::size_t number = 0;
    while (std::getline(in,line)) {
        ++number;
        if (line.find(token) != std::string::npos) return number;
    }
    return 0;
}

inline std::size_t sourceLineAfter(const fs::path& path, const std::string& anchor,
                                   const std::string& token) {
    std::ifstream in(path);
    std::string line;
    std::size_t number = 0;
    bool started = false;
    while (std::getline(in,line)) {
        ++number;
        if (!started && line.find(anchor) != std::string::npos) started = true;
        if (started && line.find(token) != std::string::npos) return number;
    }
    return 0;
}

inline std::size_t sourceLastLine(const fs::path& path, const std::string& token) {
    std::ifstream in(path);
    std::string line;
    std::size_t number = 0, found = 0;
    while (std::getline(in,line)) {
        ++number;
        if (line.find(token) != std::string::npos) found = number;
    }
    return found;
}

inline bool verifyDx9Evidence(const Options& options, std::string& detail) {
    std::string header, controller;
    if (!readText(options.dx9Header, header) || !readText(options.dx9Controller, controller)) {
        detail = "original DX9 header/controller source unavailable";
        return false;
    }
    const std::vector<std::pair<std::string, int>> expected = {
        {"EFFECT_FRAMETEX", 1}, {"EFFECT_MODELUV", 2},
        {"EFFECT_MODELTEXTURE", 3}
    };
    std::ostringstream evidence;
    for (const auto& item : expected) {
        const std::regex declaration("\\b" + item.first + "\\s*=\\s*([0-9]+)");
        std::smatch match;
        if (!std::regex_search(header, match, declaration) ||
            std::stoi(match[1].str()) != item.second) {
            detail = "enum value mismatch/missing: " + item.first;
            return false;
        }
        if (controller.find("getType() == " + item.first) == std::string::npos) {
            detail = "controller dispatch missing: " + item.first;
            return false;
        }
        const auto enumLine = sourceLine(options.dx9Header,item.first);
        const auto dispatchLine = sourceLineAfter(options.dx9Controller,
            "void CMPModelEff::FrameMove(DWORD", "getType() == " + item.first);
        if (enumLine == 0 || dispatchLine == 0) {
            detail = "source line lookup failed for " + item.first;
            return false;
        }
        if (evidence.tellp() > 0) evidence << "; ";
        evidence << options.dx9Header.filename().string() << ':' << enumLine << " -> "
                 << options.dx9Controller.filename().string() << ':' << dispatchLine;
    }
    if (controller.find("if(!m_pCEffect->IsItem())") == std::string::npos ||
        controller.find("else if(m_pCEffect->getType() == EFFECT_FRAMETEX)") == std::string::npos) {
        detail = "IsItem dispatch condition changed; inspect audit before interpreting counts";
        return false;
    }
    detail = evidence.str() + "; IsItem dispatch in " + options.dx9Controller.filename().string() + ':' +
        std::to_string(sourceLine(options.dx9Controller,"if(!m_pCEffect->IsItem())")) +
        ", type-1 item path line " +
        std::to_string(sourceLastLine(options.dx9Controller,
            "else if(m_pCEffect->getType() == EFFECT_FRAMETEX)"));
    return true;
}

inline std::string modelClass(const std::string& model) {
    if (legacy_mesh::recognized(model)) return "native_primitive";
    const auto lower = blend_audit::lower(model);
    if (lower.size() > 4 && lower.substr(lower.size()-4) == ".lgo") return "external_lgo";
    if (model.empty() || model == "0") return "empty";
    return "other_model_loader_dependent";
}

inline std::string csvSafeBytes(const std::string& value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char byte : value) {
        if (byte >= 0x20 && byte < 0x7f && byte != '\\') result.push_back(char(byte));
        else if (byte == '\\') result += "\\\\";
        else {
            result += "\\x";
            result.push_back(hex[byte >> 4]);
            result.push_back(hex[byte & 0x0f]);
        }
    }
    return result;
}

inline int run(const Options& options, std::ostream& log) {
    std::error_code ec;
    if (!blend_audit::outsideClient(options.client, options.output)) {
        log << "Output must be outside Client\n";
        return 2;
    }
    std::string dx9Evidence;
    if (!verifyDx9Evidence(options, dx9Evidence)) {
        log << "DX9 source evidence check failed: " << dx9Evidence << '\n';
        return 2;
    }
    fs::create_directories(options.output, ec);
    if (ec) { log << "Cannot create output: " << ec.message() << '\n'; return 2; }

    std::vector<blend_audit::SkillDep> deps;
    std::size_t skippedDeps = 0;
    const bool haveDeps = !options.deps.empty() &&
        fx_coverage::loadEffectDeps(options.deps, deps, skippedDeps);
    std::map<std::string, std::set<std::string>> skillsByPar;
    if (haveDeps) {
        for (const auto& dep : deps)
            for (const auto& name : fx_coverage::possibleNames(dep.file))
                skillsByPar[name].insert(dep.id);
    }

    std::vector<fs::path> paths;
    for (fs::recursive_directory_iterator it(options.client/"effect",
            fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        const auto ext = blend_audit::lower(it->path().extension().string());
        if (ext == ".par" || ext == ".eff") paths.push_back(it->path());
    }
    std::sort(paths.begin(), paths.end());

    std::map<std::string, std::vector<fs::path>> effByBase;
    std::map<std::string, std::vector<fs::path>> filesByBase;
    for (const auto& path : paths)
        filesByBase[fx_coverage::normalizedBase(path.filename().string())].push_back(path);
    for (const auto& path : paths)
        if (blend_audit::lower(path.extension().string()) == ".eff")
            effByBase[fx_coverage::normalizedBase(path.filename().string())].push_back(path);

    std::map<std::string, std::vector<Reference>> refsByEff;
    std::map<std::string, std::set<std::string>> effLinksByPar;
    std::size_t parFiles = 0, parSystems = 0, stripSystems = 0;
    for (const auto& path : paths) {
        if (blend_audit::lower(path.extension().string()) != ".par") continue;
        ++parFiles;
        const auto rel = fs::relative(path, options.client, ec).generic_string();
        const auto parsed = par_audit::inspectFile(path.string());
        if (!parsed.ok) continue;
        for (std::size_t i = 0; i < parsed.document.systems.size(); ++i) {
            ++parSystems;
            const auto& system = parsed.document.systems[i];
            if (system.type != 6) continue;
            const auto model = blend_audit::lower(fs::path(system.model).filename().string());
            if (model.size() <= 4 || model.substr(model.size()-4) != ".eff") continue;
            const auto base = fx_coverage::normalizedBase(model);
            refsByEff[base].push_back({rel, i});
            effLinksByPar[rel].insert(base);
            ++stripSystems;
        }
    }

    // Follow the same read-only dependency graph as the FX audit so skills
    // reached through scene PARs and nested model references are included.
    std::map<std::string, std::set<std::string>> skillsByParPath;
    std::map<std::string, std::set<std::string>> skillsByEffPath;
    if (haveDeps) {
        for (const auto& dep : deps) {
            std::vector<std::string> frontier = fx_coverage::possibleNames(dep.file);
            std::set<std::string> visited;
            for (std::size_t pos=0; pos<frontier.size() && pos<2000; ++pos) {
                const auto candidate = filesByBase.find(frontier[pos]);
                if (candidate == filesByBase.end()) continue;
                for (const auto& path : candidate->second) {
                    const auto rel = fs::relative(path, options.client, ec).generic_string();
                    if (!visited.insert(rel).second) continue;
                    if (blend_audit::lower(path.extension().string()) == ".par") {
                        skillsByParPath[rel].insert(dep.id);
                        const auto children = effLinksByPar.find(rel);
                        if (children != effLinksByPar.end())
                            frontier.insert(frontier.end(), children->second.begin(), children->second.end());
                    } else if (blend_audit::lower(path.extension().string()) == ".eff") {
                        skillsByEffPath[rel].insert(dep.id);
                    }
                }
            }
        }
    }

    std::ofstream parts(options.output/"fx_strip_eff_parts.csv");
    std::ofstream unresolved(options.output/"fx_strip_eff_unresolved.csv");
    std::ofstream summary(options.output/"fx_strip_eff_animation_summary.csv");
    if (!parts || !unresolved || !summary) { log << "Cannot open audit outputs\n"; return 2; }
    blend_audit::row(parts,{"eff_file","referencing_par_files","strip_system_references",
        "skills_reached","part_index","part_name","eff_type","model","model_class",
        "coord_frames","regions","texture_frames","animated_track","dx9_item_status"});
    blend_audit::row(unresolved,{"eff_basename","referencing_par_files","strip_system_references",
        "candidate_files","status"});
    blend_audit::row(summary,{"eff_type","dx9_name","referenced_parts","animated_parts",
        "files","strip_system_references","strip_par_files","skills_reached",
        "item_branch_candidates","DX9_source_check","IsItem_scope"});

    struct Totals {
        std::size_t parts = 0, animated = 0, references = 0;
        std::size_t itemUnknown = 0;
        std::set<std::string> files, pars, skills;
    } totals[5];
    std::set<std::string> processed;
    std::size_t unresolvedCount = 0, failedReferenced = 0, effParsed = 0;
    for (const auto& entry : refsByEff) {
        const auto found = effByBase.find(entry.first);
        if (found == effByBase.end()) {
            std::set<std::string> pars;
            for (const auto& ref : entry.second) pars.insert(ref.parFile);
            blend_audit::row(unresolved,{entry.first,std::to_string(pars.size()),
                std::to_string(entry.second.size()),"0","referenced_eff_missing"});
            ++unresolvedCount;
            continue;
        }
        for (const auto& path : found->second) {
            const auto rel = fs::relative(path, options.client, ec).generic_string();
            if (!processed.insert(rel).second) continue;
            LegacyEffectSummary effect;
            if (!inspectLegacyEff(path.string(), effect, true) || !effect.structured) {
                std::set<std::string> pars;
                for (const auto& ref : entry.second) pars.insert(ref.parFile);
                blend_audit::row(unresolved,{entry.first,std::to_string(pars.size()),
                    std::to_string(entry.second.size()),std::to_string(found->second.size()),
                    "referenced_eff_decode_failed"});
                ++failedReferenced;
                continue;
            }
            ++effParsed;
            std::set<std::string> pars, skills;
            for (const auto& ref : entry.second) {
                pars.insert(ref.parFile);
                if (const auto skillSet = skillsByParPath.find(ref.parFile); skillSet != skillsByParPath.end())
                    skills.insert(skillSet->second.begin(), skillSet->second.end());
            }
            if (const auto skillSet = skillsByEffPath.find(rel); skillSet != skillsByEffPath.end())
                skills.insert(skillSet->second.begin(), skillSet->second.end());
            for (std::size_t i = 0; i < effect.document.parts.size(); ++i) {
                const auto& part = effect.document.parts[i];
                const int type = part.type;
                if (type < 1 || type > 3) continue;
                auto& total = totals[type];
                ++total.parts;
                total.files.insert(rel);
                total.pars.insert(pars.begin(), pars.end());
                total.skills.insert(skills.begin(), skills.end());
                total.references += entry.second.size();
                const std::size_t coordCount = part.meshCoordFrames.empty() ?
                    part.coordFrames.size() : part.meshCoordFrames.size();
                const std::size_t regionCount = part.meshRegions.empty() ?
                    part.regions.size() : part.meshRegions.size();
                const bool animated = type == 1 ? part.frames.size() > 1 :
                    (type == 2 ? coordCount > 1 : regionCount > 1);
                if (animated) ++total.animated;
                const std::string classification = modelClass(part.model);
                if (type != 1 && classification != "native_primitive") ++total.itemUnknown;
                const std::string itemStatus = type == 1 ?
                    "DX9_FrameTex_clock_runs_for_item_and_nonitem" :
                    (classification == "native_primitive" ?
                     "DX9_native_mesh_IsItem_false_by_buffer" :
                     (classification == "external_lgo" ?
                     "possible_lgo_item_branch_not_serialized" :
                     "IsItem_depends_on_runtime_model_loader_not_encoded_in_eff"));
                blend_audit::row(parts,{rel,std::to_string(pars.size()),
                    std::to_string(entry.second.size()),haveDeps?std::to_string(skills.size()):"n/a",
                    std::to_string(i),csvSafeBytes(part.name),std::to_string(type),
                    csvSafeBytes(part.model),classification,
                    std::to_string(coordCount),std::to_string(regionCount),
                    std::to_string(part.frames.size()),animated?"yes":"no",itemStatus});
            }
        }
    }
    const char* names[] = {"none","EFFECT_FRAMETEX","EFFECT_MODELUV","EFFECT_MODELTEXTURE","EFFECT_MODEL"};
    for (int type = 1; type <= 3; ++type) {
        const auto& t = totals[type];
        blend_audit::row(summary,{std::to_string(type),names[type],std::to_string(t.parts),
            std::to_string(t.animated),std::to_string(t.files.size()),std::to_string(t.references),
            std::to_string(t.pars.size()),haveDeps?std::to_string(t.skills.size()):"n/a",
            std::to_string(t.itemUnknown),dx9Evidence,
            t.itemUnknown == 0 ? "all referenced model names map to native DX9 meshes; item gate false" :
                                 "some models require runtime loader classification"});
    }
    log << "STRIP nested EFF audit: par=" << parFiles << " systems=" << parSystems
        << " strip_eff_systems=" << stripSystems << " referenced_eff_parsed=" << effParsed
        << " unresolved=" << unresolvedCount << " referenced_decode_failures=" << failedReferenced
        << " DX9=" << dx9Evidence << " | " << options.output << '\n';
    return 0;
}

}} // namespace top::strip_eff_animation_audit
