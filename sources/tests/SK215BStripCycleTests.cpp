// SK2.15-B: tests DX9's type=6 CMPPartSys child-I_Effect replay semantics.
// It is *not* the direct CMPStrip between item dummies (SK2.15-A).
#include "top/LegacyStripSystem.h"
#include "top/LegacyEffFormat.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#define REQUIRE(x) do {if(!(x)) throw std::runtime_error(#x);} while(0)
int main(){
    using namespace top;
    using legacy_strip_system::nestedModelAge;
    // One CMPModelEff contains subeffects of different length. DX9 loops
    // their common controller when ALL have stopped, not each one alone.
    legacy_eff::Subeffect shortPart, longPart;
    shortPart.keys.resize(1); shortPart.keys[0].time=.1f;
    longPart.keys.resize(2); longPart.keys[0].time=.1f;
    longPart.keys[1].time=.2f;
    const float cycle=legacy_eff::duration(longPart);
    REQUIRE(std::fabs(cycle-.3f)<1e-5f);
    REQUIRE(!legacy_eff::framePhase(shortPart,nestedModelAge(6,false,.04f,cycle)).finished);
    // Short layer has already finished; long layer stays visible.
    REQUIRE(legacy_eff::framePhase(shortPart,nestedModelAge(6,false,.21f,cycle)).finished);
    REQUIRE(!legacy_eff::framePhase(longPart,nestedModelAge(6,false,.21f,cycle)).finished);
    // On the next group cycle short layer becomes drawable again.
    REQUIRE(!legacy_eff::framePhase(shortPart,nestedModelAge(6,false,.35f,cycle)).finished);
    REQUIRE(!legacy_eff::framePhase(longPart,nestedModelAge(6,false,.35f,cycle)).finished);
    // A MODEL (type 5) cast keeps its old one-shot policy.
    REQUIRE(legacy_eff::framePhase(shortPart,nestedModelAge(5,false,.35f,cycle)).finished);
    // Flight behavior is unchanged, including MODEL type 5.
    REQUIRE(!legacy_eff::framePhase(shortPart,nestedModelAge(5,true,.35f,cycle)).finished);
    // Opt-out returns type 6 to its old one-shot behavior.
    REQUIRE(legacy_eff::framePhase(shortPart,nestedModelAge(6,false,.35f,cycle,false)).finished);
    // Invalid/no cycle length does not introduce modulo-by-zero or NaNs.
    REQUIRE(nestedModelAge(6,false,.35f,0.f)==.35f);
    REQUIRE(nestedModelAge(6,false,.35f,NAN)==.35f);
    REQUIRE(nestedModelAge(6,false,-1.f,cycle)==0.f);
    std::cout << "SK2.15-B: group cycles, non-loop MODEL and legacy opt-out OK\n";
}
