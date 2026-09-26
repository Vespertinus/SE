
#ifndef __RENDER_COMMAND_H__
#define __RENDER_COMMAND_H__ 1

#include <type_traits>
#include <vector>

#include <Transform.h>

class GeometryEntity;
class Material;
class ShaderProgramState;

namespace SE {

class RenderCommand {

        const GeometryEntity          * pGeom;
        const Material                * pMaterial;
        ShaderProgramState              oState;
        const Transform               & oTransform;
        /** State-group sort key: folds shader / material / geometry pointers
         *  into 16-bit slices — sorting by it groups draws with identical GPU
         *  state so the apply-cache hits. Stable within the process only. */
        uint64_t                        sort_key;
        /** Conservative world-space culling sphere radius around the transform
         *  position (0 = unknown — never culled; e.g. terrain tiles set their
         *  tile half-diagonal, models default to none). */
        float                           cull_radius = 0.f;

        void    UpdateKey();

        public:
        RenderCommand(const GeometryEntity * pNewGeom,
                      const Material * pNewMaterial,
                      const Transform & oNewTransform);

        void            Draw() const;
        void            DrawDepth() const;
        void            DrawTransparent() const;
        /** Swap the material in place: rebuilds the program state (blend, block,
         *  textures) and the sort key. Runtime-attached blocks (ANIMATION on
         *  skinned commands) would be lost — intended for static commands
         *  (terrain debug material swap). */
        void            SetMaterial(const Material * pNewMaterial);
        /** True when the command carries the ANIMATION block (skinned model) —
         *  passes that swap depth programs per variant key off this. */
        bool            IsSkinned() const {
                                return oState.HasBlock(UniformUnitInfo::Type::ANIMATION);
                        }
        uint64_t        GetSortKey() const { return sort_key; }
        float           GetCullRadius() const { return cull_radius; }
        /** Index count of the command's geometry — the debug overlay's
         *  scene-wide triangle budget sums this over the submitted commands. */
        uint32_t        GetGeomIndexCount() const;
        /** Set once at command creation (terrain tiles); not a hot-path setter. */
        void            SetCullRadius(const float radius) { cull_radius = radius; }
        const Transform & GetTransform() const;
        /** modify blocks via State().SetBlock() */
        ShaderProgramState & State();
        const ShaderProgramState & State() const;
        std::string     StrDump(const size_t indent) const;

        static void* operator new(size_t sz);
        static void  operator delete(void* ptr) noexcept;
};

/** Renderables expose their commands either as an owned vector<RenderCommand>
 *  (models) or a stable pointer view into per-tile storage (terrain). This
 *  normalizes both to pointer visits (kept out of generic lambdas — gcc-12
 *  instantiates the discarded branch of if-constexpr there). */
template <class TCommands, class F>
inline void ForEachRenderCommand(const TCommands & cmds, F && fn) {
        if constexpr (std::is_same_v<TCommands, std::vector<RenderCommand>>) {
                for (const RenderCommand & cmd : cmds) { fn(&cmd); }
        }
        else {
                for (const RenderCommand * pCmd : cmds) { fn(pCmd); }
        }
}


} //namespace SE
#endif
