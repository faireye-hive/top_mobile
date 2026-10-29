#include "top/PopulationIdentity.h"
#include <cassert>
#include <vector>

int main() {
    using namespace top::population_identity;
    const Spawn first{45, 10.f, 20.f, false, 1234};
    const Spawn sameKey{45, 10.5f, 22.f, false, 1234}; // mob moved in battle
    const Spawn otherSpawn{45, 10.f, 20.f, false, 5678};
    const Spawn differentKind{46, 10.f, 20.f, false, 1234};
    const Spawn npc{45, 10.f, 20.f, true, 1234};
    assert(same(first, sameKey));
    assert(!same(first, otherSpawn));
    assert(!same(first, differentKind));
    assert(!same(first, npc));
    const Spawn noKey{45, 10.f, 20.f, false, 0};
    assert(same(noKey, {45, 10.0005f, 20.f, false, 0}));
    assert(!same(noKey, {45, 10.2f, 20.f, false, 0}));
    assert(!same(noKey, first));
    // A reordering cannot borrow the old index of a different spawn.
    const std::vector<Spawn> previous{first, otherSpawn};
    const std::vector<Spawn> reordered{otherSpawn, sameKey};
    for (std::size_t i = 0; i < reordered.size(); ++i) {
        int found = -1;
        for (std::size_t j = 0; j < previous.size(); ++j)
            if (same(reordered[i], previous[j])) { found = static_cast<int>(j); break; }
        assert(found == static_cast<int>(1 - i));
    }
}
