#ifndef __ANIMATED_MODEL_H__
#define __ANIMATED_MODEL_H__ 1

#include <Component_generated.h>
#include <ResourceHandle.h>
#include <StrID.h>
#include <SkeletonInstance.h>

#include <memory>

namespace SE {

class Skeleton;
class SkeletonInstance;

class AnimatedModel : public StaticModel {

        static const StrID BS_WEIGHT;
        static const StrID BS_WEIGHTS_CNT;
        static const StrID JOINTS_CNT;
        static const StrID JOINTS_PER_VERTEX;
        static const StrID JOINTS_MATRICES;

        struct SkeletonPart {

                std::vector<uint16_t>   vJointsIndexes;
                std::vector<glm::mat4>  vJointsInvBindPose;
                glm::mat4               mBindPose;
                std::string             sSkeletonRootNode;
                H<Skeleton>             hSkeleton;

                ret_code_t FillData(
                                const SE::FlatBuffers::AnimatedModel * pModel,
                                TSceneTree::TSceneNodeExact * pTargetNode);
        };

        H<TTexture>                     hTexBuffer;
        std::unique_ptr<UniformBlock>   pBlock;
        SkeletonPart                    oSkeletonMeta;
        // Joint nodes owning shared refs, resolved once at bind time. Per-frame
        // access is vJointNodes[i].get() — a raw pointer load, no refcount
        // traffic. Ownership guarantees the joints cannot be destroyed while this
        // model lives, wherever in the scene the bind resolved them (skin meshes
        // are siblings of the bone nodes under the armature, not their ancestors).
        std::vector<TSceneTree::TSceneNode> vJointNodes;
        uint8_t                         blendshapes_cnt{};
        /** supported max 4 */ //THINK inside Mesh
        uint8_t                         joints_per_vertex{};
        bool                            skinning_dirty{false};
        // The resolved skeleton instance: owns the joint node set and the
        // per-frame skin result shared by every mesh section of the character
        // (body, hood, head grafts compute once, the rest bulk-copy). Null
        // when the section's node transform is non-identity (its skin space
        // would differ) or the bind is incomplete — such sections skin into
        // their own block on their own.
        std::shared_ptr<SkeletonInstance> pSkeletonInstance;

        void FillRenderCommands();

        public:
        using TSerialized = FlatBuffers::AnimatedModel;

        //TODO with skeleton
        AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                      H<TMesh> hNewMesh,
                      H<Material> hNewMaterial,
                      H<TTexture> hNewTexBuf,
                      const uint8_t new_blendshapes_cnt);
        AnimatedModel(TSceneTree::TSceneNodeExact  * pNewNode,
                      const SE::FlatBuffers::AnimatedModel * pModel);
        ~AnimatedModel() noexcept;
        ret_code_t      PostLoad(const SE::FlatBuffers::AnimatedModel * pModel);

        //Update... ?
        ret_code_t      SetMaterial(H<Material> hNewMaterial);
        //SetBuffer.. SetBlendShapes
        ret_code_t      SetWeight(const uint8_t index, const float weight);
        float           GetWeight(const uint8_t index);
        uint8_t         BlendShapesCnt() const;
        void            Print(const size_t indent);
        std::string     Str() const;
        void            BindSkeleton(H<Skeleton> hSkel);
        void            MarkSkinningDirty();
        void            TargetTransformChanged(TSceneTree::TSceneNodeExact* pTargetNode);
        void            Enable();
        void            Disable();
        void            PostUpdate(const Event & oEvent);
        void            DrawDebug() const;
        const std::string & GetSkeletonRootNode() const;
        const std::vector<uint16_t> & GetJointIndexes() const;
        std::vector<uint16_t> & GetJointIndexes();
        // the instance's canonical joint set when shared, the local bind otherwise
        const std::vector<TSceneTree::TSceneNode>& JointNodes() const {
                return pSkeletonInstance ? pSkeletonInstance->vJointNodes : vJointNodes;
        }
        H<Skeleton> GetSkeletonHandle() const { return oSkeletonMeta.hSkeleton; }
};

}

#endif

