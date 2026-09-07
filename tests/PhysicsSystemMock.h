
#ifndef PHYSICS_SYSTEM_MOCK_H
#define PHYSICS_SYSTEM_MOCK_H 1

#include <PhysicsTypes.h>
#include <glm/vec3.hpp>

namespace SE {

struct PhysicsSystemMock {
        bool      grounded_result    = false;
        glm::vec3 floor_normal       = {0.f, 1.f, 0.f};
        glm::vec3 ground_velocity    = {0.f, 0.f, 0.f};
        glm::vec3 last_set_velocity  = {0.f, 0.f, 0.f};
        int       set_velocity_calls = 0;

        CharHandle CreateCharacter(const CharacterDesc&)      { return CharHandle{0}; }
        void       DestroyCharacter(CharHandle)                {}
        void       RegisterCharacterNode(CharHandle, void*)    {}
        void       UnregisterCharacterNode(CharHandle)         {}

        void SetCharacterVelocity(CharHandle, glm::vec3 v) {
                last_set_velocity = v;
                ++set_velocity_calls;
        }

        bool       IsCharacterGrounded(CharHandle) const      { return grounded_result; }
        glm::vec3  GetCharacterFloorNormal(CharHandle) const   { return floor_normal; }
        glm::vec3  GetCharacterGroundVelocity(CharHandle) const{ return ground_velocity; }
        glm::vec3  GetGravity() const                          { return {0.f, -9.81f, 0.f}; }
};

} // namespace SE

#endif
