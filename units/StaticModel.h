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
        /** Culling sphere radius around the node position in MESH units —
         *  farthest bbox corner from the origin (pivot-safe); scaled by the
         *  node's world scale at query time. 0 when unknown. */
        float                           cull_radius = 0.f;

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
        /** Culling sphere radius in mesh units (farthest bbox corner from the
         *  origin; scaled by node world scale at query time). */
        float           GetCullRadius() const { return cull_radius; }
        /** VisibilityManager opt-in: sphere around the node position covering
         *  the whole mesh regardless of pivot offset or node scale. */
        bool            GetCullSphere(glm::vec3 & vCenter, float & fRadius) const {
                                if (cull_radius <= 0.f || !pNode) { return false; }
                                const glm::mat4 & mWorld = pNode->GetTransform().GetWorld();
                                const float world_scale = glm::max(
                                        glm::length(glm::vec3(mWorld[0])),
                                        glm::max(glm::length(glm::vec3(mWorld[1])),
                                                 glm::length(glm::vec3(mWorld[2]))));
                                vCenter = pNode->GetTransform().GetWorldPos();
                                fRadius = cull_radius * world_scale;
                                return true;
                        }
        void            DrawDebug() const;
};

}

#endif
