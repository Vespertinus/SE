#ifndef __ANIMATED_MODEL_H__
#define __ANIMATED_MODEL_H__ 1

#include <Component_generated.h>
#include <StrID.h>

namespace SE {

class AnimatedModel : public StaticModel {

        static const StrID BS_WEIGHT;
        static const StrID BS_WEIGHTS_CNT;

        TTexture                      * pTexBuffer{};
        std::unique_ptr<UniformBlock>   pBlock;
        uint8_t                         blendshapes_cnt;

        void FillRenderCommands();

        public:
        using TSerialized = FlatBuffers::AnimatedModel;

        AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                      TMesh * pNewMesh,
                      Material * pNewMaterial,
                      TTexture * pNewTexBuf,
                      const uint8_t new_blendshapes_cnt);
        AnimatedModel(TSceneTree::TSceneNodeExact  * pNewNode,
                      const SE::FlatBuffers::AnimatedModel * pModel);

        //Update... ?
        ret_code_t      SetMaterial(Material * pNewMaterial);
        //SetBuffer.. SetBlendShapes
        ret_code_t      SetWeight(const uint8_t index, const float weight);
        float           GetWeight(const uint8_t index);
        uint8_t         BlendShapesCnt() const;
        void            Print(const size_t indent);
        std::string     Str() const;
};

}

#endif

