#include "top/StaticBatchCache.h"
#include "top/WorldStreamSchedule.h"
#include "top/GfxDevice.h"
#include <cassert>
#include <vector>
#include <set>
#include <iostream>

struct Batch { std::vector<top::MeshVertex> verts; };
static Batch vertices(int n) { Batch b; b.verts.resize(static_cast<std::size_t>(n)); return b; }
int main() {
    top::StaticBatchCache cache;
    std::set<unsigned> alive;
    unsigned next = 10;
    int uploaded = 0, disposed = 0;
    auto upload = [&](unsigned reuse, const top::MeshVertex* v, int count) {
        assert(v && count > 0);
        ++uploaded;
        if (reuse) { assert(alive.count(reuse)); return reuse; }
        const auto id = next++;
        alive.insert(id);
        return id;
    };
    auto release = [&](unsigned id) { assert(alive.erase(id) == 1); ++disposed; };
    std::vector<Batch> batches{vertices(3), vertices(6)};
    cache.update(batches, upload);
    assert(cache.size() == 2 && uploaded == 2 && alive.size() == 2);
    const auto a=cache.at(0).buffer,b=cache.at(1).buffer;
    assert(a && b && a != b);
    // Quadro normal: nao ha upload, apenas desenho dos handles antigos.
    assert(cache.at(0).buffer == a && cache.at(0).vertices == 3 && uploaded == 2);
    // Cenario animado substitui dados usando os mesmos VBOs.
    batches[1] = vertices(12);
    cache.update(batches, upload);
    assert(uploaded == 4 && cache.at(0).buffer == a && cache.at(1).buffer == b && cache.at(1).vertices == 12);
    batches = {vertices(9)};
    cache.update(batches, upload);
    assert(cache.size() == 1 && cache.retained() == 2 && uploaded == 5);
    cache.clear(release);
    assert(cache.size() == 0 && alive.empty() && disposed == 2);
    // Viagem/teleporte exige nova malha; nao pode desenhar geometria velha.
    assert(cache.at(0).buffer == 0);
    top::WorldStreamSchedule schedule;
    auto first = schedule.advance(0,0,0,true);
    assert(first.terrain && first.scene);
    assert(!schedule.advance(1,1,2,true).terrain);
    assert(schedule.advance(12,12,4,true).terrain);
    schedule.invalidate();
    assert(schedule.advance(12,12,4,true).terrain);
    std::cout << "05-X2 GPU cache lifecycle and terrain streaming ok\n";
}
