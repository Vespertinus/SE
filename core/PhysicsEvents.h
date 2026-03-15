
#ifndef __PHYSICS_EVENTS_H__
#define __PHYSICS_EVENTS_H__

#include <PhysicsTypes.h>

namespace SE {

struct EPhysicsContactEnter {
        BodyHandle  hBodyA, hBodyB;
        glm::vec3   vNormal, vPoint;
};

struct EPhysicsContactExit {
        BodyHandle  hBodyA, hBodyB;
};

struct EPhysicsTriggerEnter {
        BodyHandle  hTrigger, hOther;
};

struct EPhysicsTriggerExit {
        BodyHandle  hTrigger, hOther;
};

} // namespace SE

#endif
