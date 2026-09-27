
#ifndef SE_SKELETON_INSTANCE_H
#define SE_SKELETON_INSTANCE_H 1

#include <ResourceHandle.h>
#include <StrID.h>

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace SE {

class Skeleton;

// SkeletonInstance — one resolved skeleton instance (one character). Owns the
// joint scene nodes the bind resolved and the per-frame skin result; the
// AnimatedModels drawn over it (body, hood, head grafts) are its mesh
// sections: the joint matrices are computed once per frame on the instance
// and each section copies them into its own ANIMATION uniform block.
//
// Sections find their instance through the skeleton's root joint node — every
// bind resolution of one character lands on the same node. The acquire index
// holds weak pointers: an instance lives exactly as long as the last model
// that owns it.
class SkeletonInstance {

public:
        // per-frame skin result, laid out per the owning model's joint indexes
        uint64_t               frame = 0;
        std::vector<uint16_t>  vJointIndexes;
        std::vector<glm::mat4> vMatrices;

        // canonical resolved joint set + the skeleton it came from
        std::vector<TSceneTree::TSceneNode> vJointNodes;
        H<Skeleton>            hSkeleton {};

        static std::shared_ptr<SkeletonInstance> Acquire(const void * pRootKey);
};

} // namespace SE

#endif
