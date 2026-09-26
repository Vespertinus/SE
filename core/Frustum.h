
#ifndef __FRUSTUM_H__
#define __FRUSTUM_H__ 1

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace SE {

// Frustum extracted from a view-projection matrix (Gribb-Hartmann plane
// extraction). Planes are (normal, d) with the inside being where
// dot(normal, p) + d >= 0.
//
// Not to be confused with Camera::Volume: Volume is the projection PARAMETER
// block (near/far/fov/aspect/ortho extents) that produces the matrix; SFrustum
// is the derived world-space CULLING VOLUME built from the finished VP. glm
// ships matrix constructors only — plane extraction and intersection tests are
// deliberately hand-rolled here.
//
// Lives in core so the renderer-side visibility manager and any subsystem
// (terrain quadtree, shadow passes) share one implementation.
struct SFrustum {

        glm::vec4 vPlanes[6];   // L R B T N F

        static SFrustum FromVP(const glm::mat4 & mVP) {
                // glm matrices are column-major; rows of the row-major VP give the planes
                const glm::vec4 r0 = glm::row(mVP, 0);
                const glm::vec4 r1 = glm::row(mVP, 1);
                const glm::vec4 r2 = glm::row(mVP, 2);
                const glm::vec4 r3 = glm::row(mVP, 3);

                SFrustum f;
                f.vPlanes[0] = r3 + r0;   // left
                f.vPlanes[1] = r3 - r0;   // right
                f.vPlanes[2] = r3 + r1;   // bottom
                f.vPlanes[3] = r3 - r1;   // top
                f.vPlanes[4] = r3 + r2;   // near
                f.vPlanes[5] = r3 - r2;   // far
                for (auto & p : f.vPlanes) {
                        const float len = glm::length(glm::vec3(p));
                        if (len > 1e-8f) { p /= len; }
                }
                return f;
        }

        // Positive-vertex (conservative) AABB test.
        bool IntersectsAABB(const glm::vec3 & vMin, const glm::vec3 & vMax) const {
                for (const auto & p : vPlanes) {
                        const glm::vec3 n(p.x, p.y, p.z);
                        // the AABB corner furthest along the plane normal
                        const glm::vec3 vP(
                                p.x >= 0.0f ? vMax.x : vMin.x,
                                p.y >= 0.0f ? vMax.y : vMin.y,
                                p.z >= 0.0f ? vMax.z : vMin.z);
                        if (glm::dot(n, vP) + p.w < 0.0f) { return false; }
                }
                return true;
        }

        // Conservative sphere-as-AABB test (center ± radius on every axis).
        bool IntersectsSphere(const glm::vec3 & vCenter, const float radius) const {
                return IntersectsAABB(vCenter - glm::vec3(radius),
                                      vCenter + glm::vec3(radius));
        }
};

} //namespace SE
#endif
