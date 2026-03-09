
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
        glm::vec3 direction { 0.f, -1.f, 0.f };   // normalised
        float     intensity { 1.f };
        glm::vec3 color     { 1.f, 1.f, 1.f };
        float     _pad      { 0.f };
};

} // namespace SE

#endif
