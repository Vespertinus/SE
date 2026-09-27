#ifndef __STATIC_MODEL_H__
#define __STATIC_MODEL_H__ 1

// ---------------------------------------------------------------------------
// HEADLESS TEST DOUBLE — shadows units/StaticModel.h for dedicated targets.
//
// Declaration mirrors the production class exactly (same members, access and
// default ctor) because the REAL AnimatedModel still derives from it in these
// compositions; only the implementations differ — no GL material/shader loads
// and no TRenderer registration, which cannot exist in the no-GL tier.
// Same house rules as shadow/AnimatedModel.h:
//   * shadow_static/ is placed FIRST on the include path of dedicated targets
//     only — never add it to global include dirs;
//   * one TU per binary;
//   * the shadow .tcc is compiled via CoreComponents' impl section
//     (GlobalTypes.h → AllImpl.tcc) — do not include it manually.
// ---------------------------------------------------------------------------

#include <string_view>
#include <Component_generated.h>

namespace SE {

class StaticModel {

        protected:

        H<TMesh>                        hMesh;
        std::vector<H<Material>>        hMaterials;
        TSceneTree::TSceneNodeExact   * pNode = nullptr;
        std::vector<RenderCommand>      vRenderCommands;
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
        float           GetCullRadius() const { return cull_radius; }
        bool            GetCullSphere(glm::vec3 & , float & ) const { return false; }  // headless: no bounds
        void            DrawDebug() const;
};

}

#endif
