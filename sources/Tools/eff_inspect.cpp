#include "top/LegacyEffectInspector.h"
#include "top/LegacyEffectGeometry.h"
#include <cctype>
#include <iostream>

namespace {
bool plausibleFile(const std::string& name) {
    if (!top::MagicEffectCatalog::safeName(name)) return false;
    const auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 == name.size()) return false;
    std::string ext = name.substr(dot);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".eff" || ext == ".par" || ext == ".tga" || ext == ".dds" ||
           ext == ".png" || ext == ".wav" || ext == ".ogg" || ext == ".bmp";
}
}
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Uso: top_eff_inspect Client/effect/nome.eff\n";
        return 2;
    }
    top::LegacyEffectSummary effect;
    if (!top::inspectLegacyEff(argv[1], effect)) {
        std::cerr << "Arquivo .eff ausente, grande demais ou cabecalho invalido.\n";
        return 1;
    }
    std::cout << "EFF: versao " << effect.version << ", " << effect.bytes << " bytes\n";
    if (effect.structured) {
        const auto& doc = effect.document;
        std::cout << "Estrutura CMPResManger/I_Effect: validada ate o fim\n"
                  << "  tecnica " << doc.technique << ", subefeitos " << doc.parts.size()
                  << ", caminho " << (doc.usePath ? doc.path : "(nao)")
                  << ", som " << (doc.useSound ? doc.sound : "(nao)") << "\n";
        for (std::size_t i = 0; i < doc.parts.size(); ++i) {
            const auto& part = doc.parts[i];
            std::cout << "  #" << i << " nome=" << part.name
                      << " tipo=" << part.type << " comprimento=" << part.length
                      << " quadros=" << part.keys.size() << " textura=" << part.texture
                      << " modelo=" << part.model << " billboard=" << part.billboard
                      << " uv=" << part.coordFrames.size() << " regiao=" << part.regions.size()
                      << " texturas-anim=" << part.frames.size()
                      << " mesh-nativo=" << (!part.billboard && top::legacy_mesh::recognized(part.model))
                      << " quadros-param=" << part.animatedParams.size() << "\n";
            if (!part.keys.empty()) {
                auto low = part.keys.front().position, high = low;
                for (const auto& key : part.keys) for (int axis = 0; axis < 3; ++axis) {
                    low[axis] = std::min(low[axis], key.position[axis]);
                    high[axis] = std::max(high[axis], key.position[axis]);
                }
                std::cout << "    posicao-min=" << low[0] << ',' << low[1] << ',' << low[2]
                          << " posicao-max=" << high[0] << ',' << high[1] << ',' << high[2] << '\n';
            }
        }
        std::cout << "Recursos em campos identificados: " << effect.verifiedResources.size() << "\n";
        for (const auto& ref : effect.verifiedResources) std::cout << "  " << ref << '\n';
    } else {
        if (effect.headerKnown) {
            std::cout << "Cabecalho original: tecnica " << effect.document.technique
                      << ", subefeitos declarados " << effect.declaredSubeffects
                      << ", caminho " << (effect.document.usePath ? effect.document.path : "(nao)")
                      << ", som " << (effect.document.useSound ? effect.document.sound : "(nao)") << "\n";
        }
        std::cout << "Estrutura incompleta/desconhecida: NAO foram validados subefeitos; "
                     "possiveis referencias ASCII:\n";
        int found = 0;
        for (const auto& ref : effect.candidates) {
            if (!plausibleFile(ref.candidate)) continue;
            std::cout << "  +" << ref.offset << " " << ref.candidate << " (nao confirmado)\n";
            ++found;
        }
        if (!found) std::cout << "  nenhuma referencia de arquivo reconhecivel\n";
    }
}
