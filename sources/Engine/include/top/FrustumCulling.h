#pragma once
// 05-X6: conservative, normalized world-space clip planes from column-major
// OpenGL projection * view. Culling never touches simulation/AI or save data.
#include <array>
#include <cmath>

namespace top::frustum_culling {

struct Plane {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
};

class Frustum {
public:
    bool setFromClipMatrix(const float* m) {
        if (!m) { valid_ = false; return false; }
        // Rows in OpenGL column-major memory.
        const std::array<Plane, 4> rows{{
            {m[0], m[4], m[8], m[12]},
            {m[1], m[5], m[9], m[13]},
            {m[2], m[6], m[10], m[14]},
            {m[3], m[7], m[11], m[15]}
        }};
        auto add = [](const Plane& a, const Plane& b, float sign) -> Plane {
            return {a.x + sign * b.x, a.y + sign * b.y,
                    a.z + sign * b.z, a.w + sign * b.w};
        };
        planes_ = {{add(rows[3], rows[0], 1.f), add(rows[3], rows[0], -1.f),
                    add(rows[3], rows[1], 1.f), add(rows[3], rows[1], -1.f),
                    add(rows[3], rows[2], 1.f), add(rows[3], rows[2], -1.f)}};
        valid_ = true;
        for (Plane& plane : planes_) {
            const float len = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
            if (!std::isfinite(len) || len < 1e-8f || !std::isfinite(plane.w)) {
                valid_ = false;
                return false; // Fail open: never hide an actor on invalid projection.
            }
            plane.x /= len; plane.y /= len; plane.z /= len; plane.w /= len;
        }
        return true;
    }

    bool visibleSphere(float x, float y, float z, float radius) const {
        if (!valid_ || !std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(z) || !std::isfinite(radius) || radius < 0.f) return true;
        for (const Plane& p : planes_) {
            if (p.x * x + p.y * y + p.z * z + p.w < -radius) return false;
        }
        return true;
    }
    bool valid() const { return valid_; }
private:
    std::array<Plane, 6> planes_{};
    bool valid_ = false;
};

} // namespace top::frustum_culling
