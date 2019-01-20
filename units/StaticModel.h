#ifndef __STATIC_MODEL_H__
#define __STATIC_MODEL_H__ 1

#include <Component_generated.h>

namespace SE {

/**
TODO submesh meta for each used shape inside Mesh + material

*/

class StaticModel {

        protected:

        TMesh                         * pMesh;
        Material                      * pMaterial;
        TSceneTree::TSceneNodeExact   * pNode;
        std::vector<RenderCommand>      vRenderCommands;

        StaticModel();
        void FillRenderCommands();

        public:
        using TSerialized = FlatBuffers::StaticModel;

        StaticModel(TSceneTree::TSceneNodeExact * pNewNode, bool enabled);
        StaticModel(TSceneTree::TSceneNodeExact * pNewNode,
                    bool enabled,
                    TMesh * pNewMesh,
                    Material * pNewMaterial);
        StaticModel(TSceneTree::TSceneNodeExact  * pNewNode,
                    const SE::FlatBuffers::StaticModel * pModel);
        ~StaticModel() noexcept;

        //Update... ?
        void            SetMesh(TMesh * pNewMesh);
        void            SetMaterial(Material * pNewMaterial);
        Material *      GetMaterial() const;
        void            Enable();
        void            Disable();
        void            Print(const size_t indent);
        std::string     Str() const;

        const std::vector<RenderCommand> & GetRenderCommands() const;
};

}

#endif
