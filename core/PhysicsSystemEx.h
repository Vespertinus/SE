
#ifndef __PHYSICS_SYSTEM_EX_H__
#define __PHYSICS_SYSTEM_EX_H__

// Included after TSceneTree is defined in GlobalTypes.h.
// Provides a typed wrapper around PhysicsSystem::GetNode().

namespace SE {

inline TSceneTree::TSceneNodeExact* PhysicsGetNode(BodyHandle h) {
        return static_cast<TSceneTree::TSceneNodeExact*>(GetSystem<PhysicsSystem>().GetNode(h));
}

} // namespace SE

#endif
