#include "top/FrustumCulling.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
    using top::frustum_culling::Frustum;
    Frustum f;
    // No matrix: fail open, never accidentally hide characters.
    assert(f.visibleSphere(1e5f, 0.f, 0.f, 0.f));
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    assert(f.setFromClipMatrix(identity));
    assert(f.visibleSphere(0.f, 0.f, 0.f, .1f));
    assert(f.visibleSphere(0.9f, 0.f, 0.f, .2f)); // edge sphere not cut
    assert(f.visibleSphere(1.15f, 0.f, 0.f, .2f));
    assert(!f.visibleSphere(1.25f, 0.f, 0.f, .2f));
    assert(!f.visibleSphere(-2.f, 0.f, 0.f, .1f));
    assert(!f.visibleSphere(0.f, 2.f, 0.f, .1f));
    assert(!f.visibleSphere(0.f, 0.f, -3.f, .1f));
    assert(!f.visibleSphere(0.f, 0.f, 3.f, .1f));
    // Translation in column-major clip space: center shifts in +X.
    float move[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, -5,0,0,1};
    assert(f.setFromClipMatrix(move));
    assert(f.visibleSphere(5.f, 0.f, 0.f, 0.f));
    assert(!f.visibleSphere(0.f, 0.f, 0.f, 0.f));
    // Invalid matrix must not cull everything.
    float bad[16] = {};
    assert(!f.setFromClipMatrix(bad));
    assert(f.visibleSphere(-1e5f, 0.f, 0.f, 0.f));
    // A perspective matrix with camera pointed down -Z: beyond far and behind
    // camera must be rejected; a generously sized monster near the edge kept.
    const float nearZ = .3f, farZ = 100.f;
    const float cot = 1.f / std::tan(.9f / 2.f);
    float projection[16] = {cot,0,0,0, 0,cot,0,0, 0,0,(farZ+nearZ)/(nearZ-farZ),-1,
                            0,0,2*farZ*nearZ/(nearZ-farZ),0};
    assert(f.setFromClipMatrix(projection));
    assert(f.visibleSphere(0.f, 0.f, -10.f, 1.f));
    assert(!f.visibleSphere(0.f, 0.f, 10.f, .1f));
    assert(!f.visibleSphere(0.f, 0.f, -200.f, 1.f));
    assert(!f.visibleSphere(30.f, 0.f, -10.f, 1.f));
    assert(f.visibleSphere(7.f, 0.f, -10.f, 8.f));
    std::puts("05-X6 frustum: planos OpenGL, tolerancia de esfera e fail-open OK");
    return 0;
}
