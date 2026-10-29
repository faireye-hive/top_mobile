#include "top/EffectSystem.h"
#include "top/SkillVisual.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

int main() {
    // An isolated fixture reproduces the actual 74-column historical layout.
    const std::string filename = "/tmp/top_05f_skill_fixture_" + std::to_string(static_cast<long>(::getpid())) + ".txt";
    {
        std::ofstream out(filename);
        std::vector<std::string> cols(74, "0");
        cols[0] = "5"; cols[1] = "Fiery Arrow";
        cols[51] = "108"; cols[54] = "0,0"; cols[59] = "1012";
        cols[63] = "109"; cols[65] = "108";
        for (std::size_t i = 0; i < cols.size(); ++i) out << (i ? "\t" : "") << cols[i];
        out << '\n';
        cols[0] = "6"; cols[51] = "-1"; cols[54] = "102,0";
        cols[59] = "not-a-number"; cols[63] = "";
        for (std::size_t i = 0; i < cols.size(); ++i) out << (i ? "\t" : "") << cols[i];
        out << '\n';
    }
    top::SkillVisualCatalog catalog;
    assert(catalog.load(filename));
    assert(catalog.size() == 2);
    const auto* fiery = catalog.find(5);
    assert(fiery && fiery->castId() == 108 && fiery->flightId() == 1012 && fiery->hitId() == 109);
    const auto* other = catalog.find(6);
    assert(other && other->castId() == 102 && other->flightId() == 0);
    assert(catalog.find(9999) == nullptr);
    std::remove(filename.c_str());

    top::EffectSystem effects;
    // No textures or binary assets necessary for lifecycle and fallback tests.
    effects.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 5, .5f});
    assert(effects.activeCount() == 1);
    top::MapFile map;
    effects.build(map, 1.f, 0.f);
    assert(!effects.batches().empty() && effects.batches()[0].verts.size() >= 6);
    // Flight moves before damage, and expires at the impact keyframe.
    const float firstX = effects.batches()[0].verts[0].x;
    effects.step(.24f);
    assert(effects.activeCount() == 1);
    effects.build(map, 1.f, 0.f);
    assert(effects.batches()[0].verts[0].x > firstX + 1.f);
    effects.step(.27f);
    assert(effects.activeCount() == 0);

    effects.emit({top::EffectEvent::SkillHit, 1.f, 2.f, 6.f, 7.f, 5});
    effects.build(map, 1.f, 0.f);
    assert(effects.activeCount() == 1);
    assert(effects.batches()[0].verts.size() >= 14 * 6);
    effects.step(.56f);
    assert(effects.activeCount() == 0);

    // Zero/invalid duration must never introduce divide-by-zero.
    effects.emit({top::EffectEvent::Flight, 1.f, 2.f, 6.f, 7.f, 5, -1.f});
    effects.build(map, 1.f, 0.f);
    assert(!effects.batches().empty());
    effects.step(4.f);
    assert(effects.activeCount() == 0);
    std::cout << "05-F: skill visual ids, projectile timing and hit bursts OK\n";
    return 0;
}
