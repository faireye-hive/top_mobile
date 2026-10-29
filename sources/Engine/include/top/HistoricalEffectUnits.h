#pragma once
namespace top::historical_effect_units {
// CEffectModel primitives, LGO vertices, LAB dummies and floating world
// coordinates use the same unit. Only integer gameplay positions use cm.
constexpr float mesh = 1.f;
// RectPlane is authored from -0.5 to +0.5. quadRotated accepts half extents.
constexpr float planeHalfExtent = .5f;
constexpr float fromCentimetres(float value) { return value/100.f; }
}
