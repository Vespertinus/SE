
#ifndef LIGHT_H
#define LIGHT_H 1

#include <glm/vec3.hpp>

namespace SE {

struct PointLight {
        glm::vec3 position;
        float     radius    { 10.f };
        glm::vec3 color     { 1.f, 1.f, 1.f };
        float     intensity { 1.f };
};

struct DirLight {
        glm::vec3 direction    { 0.f, -1.f, 0.f };   // normalised
        float     intensity    { 1.f };
        glm::vec3 color        { 1.f, 1.f, 1.f };
        float     _pad         { 0.f };
        // Shadow frustum (orthographic). shadow_center anchors the frustum —
        // the renderer follows it (snapped to the shadow-map texel grid) so a
        // tight frustum can track the player instead of pinning to the origin.
        // CPU-side only: individual fields reach the shader as uniforms.
        float     shadow_ortho   { 20.f };  // half-extent (world units)
        float     shadow_near    { 0.1f };
        float     shadow_far     { 50.f };
        glm::vec3 shadow_center  { 0.f, 0.f, 0.f };
};

} // namespace SE

#endif
