
#ifndef __SKELETON_H__
#define __SKELETON_H__ 1

#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <ResourceHolder.h>
#include <StrID.h>

namespace SE::FlatBuffers { struct Skeleton; }

namespace SE {

/**
 * Skeleton — runtime skeleton definition.
 *
 * Stores per-bone bind pose, inverse bind matrices, and optional bone masks
 * for layered animation blending. Decoded from a baked .sesk FlatBuffer file.
 *
 * Bones are topologically sorted: parent_index < bone_index for all non-root bones.
 */
class Skeleton : public ResourceHolder {

public:
        static constexpr uint16_t kNoParent = 0xFFFF;

        struct BoneData {
                char       name[48]     = {};
                uint16_t   parentIndex  = kNoParent;
                glm::vec3  bindPos      = glm::vec3(0.0f);
                glm::quat  bindRot      = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3  bindScale    = glm::vec3(1.0f);
                glm::mat4  invBindMatrix = glm::mat4(1.0f);
        };

        struct BoneMask {
                StrID               name;
                std::vector<float>  weights;  // one entry per bone; 0=fully base, 1=fully layer
        };

private:
        std::vector<BoneData>  vBones;
        std::vector<BoneMask>  vMasks;

        void LoadFromFB(const SE::FlatBuffers::Skeleton* pFB);

public:
        Skeleton(const std::string& sName, rid_t rid);
        Skeleton(const std::string& sName, rid_t rid,
                 const SE::FlatBuffers::Skeleton* pFB);

        uint32_t                       BoneCount() const { return static_cast<uint32_t>(vBones.size()); }
        const std::vector<BoneData>&   Bones()     const { return vBones; }
        const BoneMask*                FindMask(StrID name) const;

        std::string Str() const;
};

} // namespace SE

#endif
