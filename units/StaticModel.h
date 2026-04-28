#ifndef __STATIC_MODEL_H__
#define __STATIC_MODEL_H__ 1

#include <string_view>
#include <Component_generated.h>

namespace SE {

/**
TODO submesh meta for each used shape inside Mesh + material

*/

class StaticModel {

        protected:

        H<TMesh>                        hMesh;
        std::vector<H<Material>>        hMaterials;
        TSceneTree::TSceneNodeExact   * pNode;
        std::vector<RenderCommand>      vRenderCommands;

        StaticModel();
        void FillRenderCommands();

        public:
        using TSerialized = FlatBuffers::StaticModel;

        StaticModel(TSceneTree::TSceneNodeExact * pNewNode);
        StaticModel(TSceneTree::TSceneNodeExact * pNewNode,
                    H<TMesh> hNewMesh,
                    H<Material> hNewMaterial);
        StaticModel(TSceneTree::TSceneNodeExact  * pNewNode,
                    const SE::FlatBuffers::StaticModel * pModel);
        ~StaticModel() noexcept;

        //Update... ?
        void            SetMesh(H<TMesh> hNewMesh);
        void            SetMaterial(H<Material> hNewMaterial);  ///< replaces first (or only) material
        void            AddMaterial(H<Material> hNewMaterial);  ///< appends a material slot
        Material *      GetMaterial(size_t idx = 0) const;
        void            Enable();
        void            Disable();
        void            Print(const size_t indent);
        std::string     Str() const;

        const std::vector<RenderCommand> & GetRenderCommands() const;
        void            DrawDebug() const;
};

}

#endif
