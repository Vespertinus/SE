
#ifndef STATIC_MODEL_H
#define STATIC_MODEL_H

#include <Component_generated.h>

namespace SE {

class TMesh {};
class Material {};

class StaticModelMock {

        public:

        using TSerialized = FlatBuffers::StaticModel;

        StaticModelMock(TSceneTree::TSceneNodeExact * pNewNode) {};
        /*StaticModel(TSceneTree::TSceneNodeExact * pNewNode,
                    TMesh * pNewMesh,
                    Material * pNewMaterial);*/
        StaticModelMock(TSceneTree::TSceneNodeExact  * pNewNode,
                    const SE::FlatBuffers::StaticModel * pModel) {};

        MOCK_METHOD1(SetMesh, void(TMesh * pNewMesh));
        MOCK_METHOD1(SetMaterial, void(Material * pNewMaterial));
        MOCK_CONST_METHOD0(GetMaterial, Material *());
        MOCK_METHOD0(Enable, void());
        MOCK_METHOD0(Disable, void());
        MOCK_METHOD1(Print, void(const size_t indent));
        MOCK_CONST_METHOD0(Str, std::string());

        //const std::vector<RenderCommand> & GetRenderCommands() const;
        MOCK_METHOD0(DrawDebug, void());
        MOCK_METHOD1(PostLoad, ret_code_t(const SE::FlatBuffers::StaticModel * pModel)); //for TSceneTree PostLoad check
};

}

#endif
