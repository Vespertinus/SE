#ifndef __ANIMATED_MODEL_H__
#define __ANIMATED_MODEL_H__ 1

#include <Component_generated.h>
#include <StrID.h>

namespace SE {

class Skeleton;
class CharacterShell;

class AnimatedModel : public StaticModel {

        static const StrID BS_WEIGHT;
        static const StrID BS_WEIGHTS_CNT;
        static const StrID JOINTS_CNT;
        static const StrID JOINTS_PER_VERTEX;
        static const StrID JOINTS_MATRICES;

        struct SkeletonPart {

                std::vector<glm::mat4>  vJointBaseMat;
                std::vector<uint8_t>    vJointIndexes;
                CharacterShell        * pShell{};

                /*ret_code_t FillData(
                                const SE::FlatBuffers::CharacterShellHolder * pHolder,
                                const flatbuffers::Vector<uint8_t> * pJointIndices,
                                TSceneTree::TSceneNodeExact * pTargetNode);*/
                ret_code_t FillData(
                                const SE::FlatBuffers::AnimatedModel * pModel,
                                TSceneTree::TSceneNodeExact * pTargetNode);
        };

        TTexture                      * pTexBuffer{};
        std::unique_ptr<UniformBlock>   pBlock;
        //CharacterShell                    oSkeleton;
        SkeletonPart                    oSkeletonMeta;
        uint8_t                         blendshapes_cnt{};
        /** supported max 4 */ //THINK inside Mesh
        uint8_t                         joints_per_vertex{};
        bool                            skinning_dirty{false};

        void FillRenderCommands();

        public:
        using TSerialized = FlatBuffers::AnimatedModel;

        //TODO with skeleton
        AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                      TMesh * pNewMesh,
                      Material * pNewMaterial,
                      TTexture * pNewTexBuf,
                      const uint8_t new_blendshapes_cnt);
        AnimatedModel(TSceneTree::TSceneNodeExact  * pNewNode,
                      const SE::FlatBuffers::AnimatedModel * pModel);
        ~AnimatedModel() noexcept;
        ret_code_t      PostLoad(const SE::FlatBuffers::AnimatedModel * pModel);

        //Update... ?
        ret_code_t      SetMaterial(Material * pNewMaterial);
        //SetBuffer.. SetBlendShapes
        ret_code_t      SetWeight(const uint8_t index, const float weight);
        float           GetWeight(const uint8_t index);
        uint8_t         BlendShapesCnt() const;
        void            Print(const size_t indent);
        std::string     Str() const;
        void            TargetTransformChanged(TSceneTree::TSceneNodeExact * pTargetNode);
        //void            TargetDeleted(TSceneNodeExact * pTargetNode);
        void            Enable();
        void            Disable();
        void            PostUpdate(const Event & oEvent);
        void            DrawDebug() const;
};

}

#endif

