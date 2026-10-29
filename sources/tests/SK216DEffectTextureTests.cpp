// SK2.16-D: historical external .eff texture takes precedence over .lgo material.
#include "top/LegacyEffectTextureSelection.h"
#include "top/TexturePaths.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
namespace fs=std::filesystem;
#define REQUIRE(expr) do { if (!(expr)) throw std::runtime_error(#expr); } while (0)
int main() {
    using top::legacy_effect_texture::Origin;
    using top::legacy_effect_texture::choose;
    // Reference case: icearrowh.eff declares "ice1"; ice.lgo's material
    // references "1.tga" (a black RGB mask). Neither should be guessed.
    const fs::path root=fs::temp_directory_path()/("sk216d-"+std::to_string(getpid()));
    fs::create_directories(root/"texture/effect");
    {std::ofstream f(root/"texture/effect/Ice1.TGA.png"); f<<"blue ice";}
    {std::ofstream f(root/"texture/effect/1.tga.png"); f<<"black mask";}
    const std::string ice=top::resolveTexturePath((root/"texture/effect/ice1.tga").string());
    const std::string mask=top::resolveTexturePath((root/"texture/effect/1.tga").string());
    REQUIRE(ice.find("Ice1.TGA.png")!=std::string::npos);
    REQUIRE(mask.find("1.tga.png")!=std::string::npos);
    REQUIRE(choose(ice,mask,"",false).path==mask); // previous GL behaviour
    const auto historic=choose(ice,mask,"",true);
    REQUIRE(historic.path==ice && historic.origin==Origin::Effect);
    REQUIRE(choose(ice,mask,"animated.png",true).origin==Origin::Animated);
    REQUIRE(choose("",mask,"",true).origin==Origin::Material);
    REQUIRE(choose("","","",true).origin==Origin::None);
    // Pre-existing / missing case ambiguity does not silently select one.
    {std::ofstream f(root/"texture/effect/ICE1.tga.png"); f<<"collision";}
    REQUIRE(top::resolveTexturePath((root/"texture/effect/ICE1.tga").string())==
            (root/"texture/effect/ICE1.tga.png").string());
    REQUIRE(top::resolveTexturePath((root/"texture/effect/ice1.tga").string()).empty());
    fs::remove_all(root);
    std::cout<<"SK2.16-D: case-insensitive historical name and authored .eff texture priority OK\n";
}
