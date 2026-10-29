#include "top/ActorGpuCache.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct Batch {
    int mode = 3;
    std::string texture;
    std::vector<top::MeshVertex> verts;
};
top::MeshVertex vertex(float x) {
    top::MeshVertex result{};
    result.x = x;
    result.ny = 1.f;
    return result;
}
}

int main() {
    top::ActorGpuCache cache;
    std::vector<Batch> batches = {{3, "skin", {vertex(1.f), vertex(2.f), vertex(3.f)}},
                                  {3, "sword", {vertex(4.f), vertex(5.f), vertex(6.f)}}};
    unsigned next = 10;
    int uploads = 0, releases = 0;
    auto upload = [&](unsigned old, const top::MeshVertex* values, int n) {
        assert(values && n > 0);
        ++uploads;
        return old ? old : next++;
    };
    auto release = [&](unsigned handle) {
        assert(handle >= 10 && handle < next);
        ++releases;
    };
    cache.update(batches, upload, release);
    assert(uploads == 2 && cache.stats().uploadedBatches == 2 && cache.stats().reusedBatches == 0);
    assert(cache.at(0).vertices == 3 && cache.at(1).buffer != cache.at(0).buffer);
    const auto first = cache.at(0).buffer;
    cache.update(batches, upload, release);
    assert(uploads == 2 && cache.stats().reusedBatches == 2);
    assert(cache.stats().reusedBytes == 6 * sizeof(top::MeshVertex));
    // Position/pose changes; no stale geometry even when count is unchanged.
    batches[0].verts[0].x += 0.01f;
    cache.update(batches, upload, release);
    assert(uploads == 3 && cache.stats().uploadedBatches == 1 && cache.stats().reusedBatches == 1);
    assert(cache.at(0).buffer == first);
    // A weapon texture changes with the same number of vertices.
    batches[1].texture = "other_weapon";
    cache.update(batches, upload, release);
    assert(uploads == 4 && cache.stats().uploadedBatches == 1);
    // A change in render mode invalidates the snapshot, too.
    batches[0].mode = 4;
    cache.update(batches, upload, release);
    assert(uploads == 5);
    // Reordered materials must not reuse the wrong contents.
    std::swap(batches[0], batches[1]);
    cache.update(batches, upload, release);
    assert(uploads == 7 && cache.at(0).vertices == 3);
    // A population shrink must free the unused VBO immediately.
    batches.pop_back();
    cache.update(batches, upload, release);
    assert(cache.retained() == 1 && releases == 1);
    // Invalid (empty) batch does not draw stale vertices.
    batches[0].verts.clear();
    cache.update(batches, upload, release);
    assert(cache.at(0).vertices == 0);
    batches[0].verts.push_back(vertex(8.f));
    cache.update(batches, upload, release);
    assert(cache.at(0).vertices == 1);
    // Repeated map transitions never leave handles behind.
    cache.clear(release);
    assert(cache.retained() == 0 && releases == 2);
    // Failure to upload is a normal fallback to the uncached draw path.
    batches[0].verts.push_back(vertex(9.f));
    cache.update(batches, [](unsigned, const top::MeshVertex*, int) { return 0u; }, release);
    assert(cache.at(0).buffer == 0 && cache.at(0).vertices == 0);
    cache.clear(release);
    assert(releases == 2);
    std::cout << "05-X5: exact actor GPU cache, material/pose invalidation, release and fallback OK\n";
}
