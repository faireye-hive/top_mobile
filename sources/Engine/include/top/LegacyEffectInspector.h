#pragma once
// Inspecao conservadora de .eff MindPower (05-H).
// Header e strings possiveis de recursos: NAO e um decodificador de malha,
// transformacoes ou hierarquia CMPModelEff. Mantem offsets para comparar
// arquivos reais ao evoluir o formato; entradas invalidas nao vazam memoria.

#include "top/MagicEffectCatalog.h"
#include "top/LegacyEffFormat.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace top {

struct LegacyEffectReference {
    std::size_t offset = 0;
    std::string candidate;
};
struct LegacyEffectSummary {
    std::uint32_t version = 0;
    std::size_t bytes = 0;
    std::vector<LegacyEffectReference> candidates;
    legacy_eff::Document document;
    bool structured = false; // true only if all versioned fields consumed
    bool headerKnown = false;
    int declaredSubeffects = 0;
    std::vector<std::string> verifiedResources;
};

inline bool inspectLegacyEff(const std::string& filename, LegacyEffectSummary& out,
                             bool allowRawModelParams = false) {
    std::ifstream input(filename, std::ios::binary | std::ios::ate);
    if (!input) return false;
    const std::streamoff length = input.tellg();
    if (length < 8 || length > 8 * 1024 * 1024) return false;
    input.seekg(0);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    if (!input.read(reinterpret_cast<char*>(bytes.data()), length)) return false;
    const std::uint32_t version = static_cast<std::uint32_t>(bytes[0]) |
                                  (static_cast<std::uint32_t>(bytes[1]) << 8) |
                                  (static_cast<std::uint32_t>(bytes[2]) << 16) |
                                  (static_cast<std::uint32_t>(bytes[3]) << 24);
    if (version == 0 || version > 32) return false;
    LegacyEffectSummary decoded;
    decoded.version = version;
    decoded.bytes = bytes.size();
    std::string run;
    std::size_t begin = 4;
    auto flush = [&]() {
        if (run.size() >= 3 && run.size() <= 100 &&
            MagicEffectCatalog::safeName(run) && decoded.candidates.size() < 64)
            decoded.candidates.push_back({begin, run});
        run.clear();
    };
    for (std::size_t at = 4; at < bytes.size(); ++at) {
        const unsigned char c = bytes[at];
        if (c >= 32 && c < 127) {
            if (run.empty()) begin = at;
            if (run.size() <= 100) run.push_back(static_cast<char>(c));
        } else {
            flush();
        }
    }
    flush();
    // The original CMPResManger and I_Effect loaders define this binary
    // layout. A structurally valid document is distinct from incidental
    // ASCII runs. Only its explicit filename fields are verified references.
    legacy_eff::Document partial;
    if (legacy_eff::header(bytes, partial, decoded.declaredSubeffects)) {
        decoded.headerKnown = true;
        decoded.document = partial;
    }
    legacy_eff::Document full;
    if (legacy_eff::decode(bytes, full, allowRawModelParams)) {
        decoded.structured = true;
        decoded.document = std::move(full);
        auto add = [&](const std::string& value) {
            if (MagicEffectCatalog::safeName(value) &&
                std::find(decoded.verifiedResources.begin(), decoded.verifiedResources.end(), value) ==
                    decoded.verifiedResources.end())
                decoded.verifiedResources.push_back(value);
        };
        if (decoded.document.usePath) add(decoded.document.path);
        if (decoded.document.useSound) add(decoded.document.sound);
        for (const auto& part : decoded.document.parts) {
            add(part.texture);
            add(part.model);
            for (const auto& name : part.frames) add(name);
        }
    }
    out = std::move(decoded); // nunca retornar dados parciais em arquivo invalido
    return true;
}

} // namespace top
