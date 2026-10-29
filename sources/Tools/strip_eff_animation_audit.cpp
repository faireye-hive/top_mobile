#include "top/StripEffAnimationAudit.h"

#include <iostream>

int main(int argc, char** argv) {
    top::strip_eff_animation_audit::Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto take = [&](std::filesystem::path& target) {
            if (i + 1 >= argc) return false;
            target = argv[++i];
            return true;
        };
        if (arg == "--client") { if (!take(options.client)) return 2; }
        else if (arg == "--out") { if (!take(options.output)) return 2; }
        else if (arg == "--deps") { if (!take(options.deps)) return 2; }
        else if (arg == "--dx9-header") { if (!take(options.dx9Header)) return 2; }
        else if (arg == "--dx9-controller") { if (!take(options.dx9Controller)) return 2; }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "top_strip_eff_animation_audit [--client Client] "
                         "[--out build/strip-eff-animation-audit] "
                         "[--deps sources/auditoria-historica/dependencies.csv] "
                         "[--dx9-header arquivo/fontes/Engine/include/I_Effect.h] "
                         "[--dx9-controller arquivo/fontes/Engine/src/MPModelEff.cpp]\n"
                         "Read-only PAR->EFF inventory; writes CSV outside Client. No GL required.\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << '\n';
            return 2;
        }
    }
    return top::strip_eff_animation_audit::run(options, std::cout);
}
