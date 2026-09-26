#ifndef __VISIBILITY_MANAGER_H__
#define __VISIBILITY_MANAGER_H__ 1

#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <glm/glm.hpp>
#include <Frustum.h>
#include <RenderCommand.h>

namespace SE {

// ---------------------------------------------------------------------------
// VisibilityManager — the production TRenderer visibility manager.
//
// AllVisible (kept for testing/ground-truth comparisons) gathers EVERY command
// from EVERY renderable on each camera move. This manager adds the two things
// a real renderer needs:
//
//   1. Frustum culling per renderable: a renderable opts in by exposing
//        bool GetCullSphere(glm::vec3 & vCenter, float & fRadius) const
//      (false / absent = cannot cull — always gathered). StaticModel/Animated-
//      Model use the mesh bbox radius around the node position; Terrain uses
//      its manifest extent.
//   2. Culled-gather bookkeeping: records which renderables produced visible
//      commands last build (WasRenderableVisible) so CPU-side hot paths —
//      e.g. skinned-matrix computation — can skip off-screen work.
//
// Ordering (sorting) stays renderer-side: the manager decides WHAT is visible,
// the renderer decides the ORDER (state-group key + depth).
// ---------------------------------------------------------------------------

template <class ... TRenderableComponents > class VisibilityManager {

        public:

        using TVariant = std::variant<TRenderableComponents * ...>;

        // Result of a visibility query — references to internal lists, valid until next GetVisible call.
        struct VisibilityResult {
                const std::vector<RenderCommand const *> & opaque;
                const std::vector<RenderCommand const *> & transparent;
                bool changed{false};
        };

        private:

        std::unordered_map <std::uintptr_t, TVariant>   mActiveRenderables;
        // Pre-built command lists, populated on GetVisible
        std::vector<RenderCommand const *>              vOpaqueCommands;
        std::vector<RenderCommand const *>              vTransparentCommands;
        bool                                            changed{true};

        /** Renderables whose commands are in the current lists (keyed the same
         *  way as mActiveRenderables — the component pointer). */
        std::unordered_set<std::uintptr_t>              mVisibleRenderables;

        // camera view-projection tracking (see UpdateCameraView)
        glm::mat4                                       mLastViewProjection{1.0f};
        bool                                            view_valid{false};
        float                                           view_epsilon{1e-3f};

        // SFINAE: does this renderable expose a cull sphere?
        template <class T, class = void>
        struct HasCullSphere : std::false_type {};
        template <class T>
        struct HasCullSphere<T, std::void_t<
                decltype(std::declval<const T &>().GetCullSphere(
                                std::declval<glm::vec3 &>(), std::declval<float &>()))>>
                : std::true_type {};

        template <class TRenderable>
        bool CullTest(const TRenderable * pRenderable, const SFrustum & oFrustum) const;

        public:

        VisibilityManager() { vOpaqueCommands.reserve(1000); vTransparentCommands.reserve(200); }

        template <class TRenderable > void AddRenderable(TRenderable * pComponent);
        template <class TRenderable > void RemoveRenderable(TRenderable * pComponent);
        /** Same contract as AllVisible::UpdateCameraView. */
        void UpdateCameraView(const glm::mat4 & mViewProjection);
        void MarkDirty() { changed = true; }
        VisibilityResult GetVisible();
        /** True when the renderable contributed at least one visible command in
         *  the last build — the animation/skinning visibility gate. */
        bool WasRenderableVisible(const void * pComponent) const {
                return mVisibleRenderables.count(reinterpret_cast<std::uintptr_t>(pComponent)) != 0;
        }
        uint32_t GetLastRenderableCount() const { return uint32_t(mVisibleRenderables.size()); }
        uint32_t GetLastTotalCount()     const { return uint32_t(mActiveRenderables.size()); }
};

} //namespace SE

#include <VisibilityManager.tcc>

#endif
