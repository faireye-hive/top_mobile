#pragma once
namespace top::inventory_confirm {
enum class Click { Cancel, Confirm };
// Exact geometry of the 05-W2 two-button modal, shared by input and tests.
inline Click hit(int x,int y,int sw,int sh) {
    const float lx=float(x)-(float(sw)-332.f)*.5f;
    const float ly=float(y)-(float(sh)-126.f)*.5f;
    return lx>=18.f && lx<156.f && ly>=84.f && ly<114.f ? Click::Confirm : Click::Cancel;
}
} // namespace top::inventory_confirm
