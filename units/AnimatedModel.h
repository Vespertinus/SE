#ifndef __ANIMATED_MODEL_H__
#define __ANIMATED_MODEL_H__ 1

#include <Component_generated.h>
#include <StrID.h>

namespace SE {

//FIXME remove after switching on uniform buffers
static const std::array<StrID, 8> vWeightsNames = {
        "BS_1W",
        "BS_2W",
        "BS_3W",
        "BS_4W",
        "BS_5W",
        "BS_6W",
        "BS_7W",
        "BS_8W"
};

class AnimatedModel : public StaticModel {


        //TTexture         * pTexBuffer{};
        std::vector<float> vWeights;

        public:
        using TSerialized = FlatBuffers::AnimatedModel;

        AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                      bool enabled,
                      TMesh * pNewMesh,
                      Material * pNewMaterial,
                      const uint8_t blendshapes_cnt);
        AnimatedModel(TSceneTree::TSceneNodeExact  * pNewNode,
                      const SE::FlatBuffers::AnimatedModel * pModel);

        //Update... ?
        ret_code_t      SetMaterial(Material * pNewMaterial, const uint8_t blendshapes_cnt);
        //SetBuffer..
        ret_code_t      SetWeight(const uint8_t index, const float weight);
        float           GetWeight(const uint8_t index);
        uint8_t         BlendShapesCnt() const;
        void            Print(const size_t indent);
        std::string     Str() const;
};

}

#endif

