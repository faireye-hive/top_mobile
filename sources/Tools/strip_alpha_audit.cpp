// Read-only inventory of type-6 parent alpha versus nested .eff key alphas.
#include "top/LegacyEffectInspector.h"
#include "top/LegacyParAudit.h"
#include "top/SkillVisual.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
std::vector<std::string> tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    for (;;) {
        const auto end = line.find('\t', begin);
        fields.push_back(line.substr(begin, end == std::string::npos ? end : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return fields;
}
const char* channelName(top::SkillVisualChannelKind kind) {
    using K = top::SkillVisualChannelKind;
    switch (kind) {
        case K::Actor: return "actor";
        case K::Item: return "item";
        case K::ItemExtra: return "item_extra";
        case K::Flight: return "flight";
        case K::Target: return "target";
        case K::Ground: return "ground";
        case K::Water: return "water";
    }
    return "unknown";
}
int toInt(const std::string& value) {
    try { return std::stoi(value); } catch (...) { return 0; }
}
std::string csv(const std::string& value) {
    if (value.find_first_of(",\r\n\"") == std::string::npos) return value;
    std::string quoted = "\"";
    for (char c : value) { if (c == '"') quoted += '"'; quoted += c; }
    return quoted + '"';
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: top_strip_alpha_audit Client/effect\n";
        return 2;
    }
    const fs::path root(argv[1]);
    const fs::path clientRoot = root.parent_path();
    const fs::path scenePath = clientRoot / "scripts/table/sceneffectinfo.txt";
    const fs::path skillPath = clientRoot / "scripts/table/skillinfo.txt";
    std::map<int, std::string> sceneFiles;
    std::ifstream sceneInput(scenePath);
    std::string line;
    while (std::getline(sceneInput, line)) {
        const auto fields = tabs(line);
        if (fields.size() > 1) {
            const int id = toInt(fields[0]);
            if (id > 0) sceneFiles[id] = lower(fs::path(fields[1]).filename().string());
        }
    }
    top::SkillVisualCatalog skillCatalog;
    const bool skillsAvailable = skillCatalog.load(skillPath.string());
    std::map<int, std::string> skillNames;
    std::ifstream skillInput(skillPath);
    while (std::getline(skillInput, line)) {
        const auto fields = tabs(line);
        if (fields.size() > 1) {
            const int id = toInt(fields[0]);
            if (id > 0) skillNames[id] = fields[1];
        }
    }
    std::size_t parents = 0, decoded = 0, differing = 0, unreadable = 0, badPar = 0;
    std::size_t sceneLinked = 0, skillLinked = 0;
    std::unordered_set<int> linkedSkills, differingSkills;
    std::cout << "par,index,model,parent_alpha,child_parts,nonunit_alpha_parts,scene_effect_ids,direct_skill_channels,status\n";
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".par") continue;
        const auto par = top::par_audit::inspectFile(entry.path().string());
        if (!par.ok) { ++badPar; continue; }
        for (std::size_t index = 0; index < par.document.systems.size(); ++index) {
            const auto& system = par.document.systems[index];
            std::string modelExt = system.model.size() >= 4 ? system.model.substr(system.model.size() - 4) : "";
            std::transform(modelExt.begin(), modelExt.end(), modelExt.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (system.type != 6 || modelExt != ".eff" || system.colors.size() < 4 ||
                !(system.colors[3] < 1.f) || system.model.find_first_of("/\\:") != std::string::npos)
                continue;
            ++parents;
            const std::string parName = lower(entry.path().filename().string());
            std::set<int> sceneIds;
            for (const auto& [id, file] : sceneFiles) if (file == parName) sceneIds.insert(id);
            std::set<std::string> skillChannels;
            std::set<int> rowSkillIds;
            if (skillsAvailable && !sceneIds.empty()) {
                for (const auto& [skillId, skillName] : skillNames) {
                    const auto* visual = skillCatalog.find(skillId);
                    if (!visual) continue;
                    for (const auto& channel : visual->channels) {
                        if (sceneIds.count(channel.effectId)) {
                            skillChannels.insert(std::to_string(skillId) + ":" + skillName + ":" + channelName(channel.kind));
                            rowSkillIds.insert(skillId);
                        }
                    }
                }
            }
            if (!sceneIds.empty()) ++sceneLinked;
            if (!skillChannels.empty()) {
                ++skillLinked;
                linkedSkills.insert(rowSkillIds.begin(), rowSkillIds.end());
            }
            std::string ids, channels;
            for (int id : sceneIds) { if (!ids.empty()) ids += ';'; ids += std::to_string(id); }
            for (const auto& channel : skillChannels) { if (!channels.empty()) channels += ';'; channels += channel; }
            const auto prefix = csv(entry.path().filename().string()) + ',' + std::to_string(index) + ',' +
                csv(system.model) + ',' + std::to_string(system.colors[3]) + ',';
            top::LegacyEffectSummary effect;
            const fs::path effectPath = root / system.model;
            if (!top::inspectLegacyEff(effectPath.string(), effect) || !effect.structured) {
                ++unreadable;
                std::cout << prefix << "0,0," << ids << ',' << csv(channels) << ",eff_not_structured\n";
                continue;
            }
            ++decoded;
            std::size_t differingParts = 0;
            for (const auto& part : effect.document.parts) {
                bool nonunit = false;
                for (const auto& key : part.keys)
                    if (std::abs(key.color[3] - 1.f) > 1e-6f) { nonunit = true; break; }
                if (nonunit) ++differingParts;
            }
            if (differingParts) {
                ++differing;
                differingSkills.insert(rowSkillIds.begin(), rowSkillIds.end());
            }
            std::cout << prefix << effect.document.parts.size() << ',' << differingParts << ','
                      << ids << ',' << csv(channels) << ','
                      << (differingParts ? "parent_override_changes_alpha" : "alpha_one_equivalent") << '\n';
        }
    }
    std::cerr << "type6_parent_alpha_lt1=" << parents << "\n"
              << "nested_eff_structurally_decoded=" << decoded << "\n"
              << "with_nonunit_child_alpha=" << differing << "\n"
              << "linked_by_sceneffectinfo=" << sceneLinked << "\n"
              << "systems_linked_by_skillinfo_channel=" << skillLinked << "\n"
              << "skills_linked_to_type6_alpha_parents=" << linkedSkills.size() << "\n"
              << "skills_linked_to_alpha_divergences=" << differingSkills.size() << "\n"
              << "skill_linkage_data=" << (skillsAvailable && sceneInput.is_open() ? "available" : "partial_or_missing") << "\n"
              << "eff_not_structured=" << unreadable << "\n"
              << "par_decode_failures=" << badPar << "\n";
    return 0;
}
