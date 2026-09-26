#ifndef __ALL_VISIBLE_H__
#define __ALL_VISIBLE_H__ 1

#include <algorithm>
#include <glm/glm.hpp>
#include <RenderCommand.h>

namespace SE {

template <class ... TRenderableComponents > class AllVisible {

        public:

        using TVariant = std::variant<TRenderableComponents * ...>;

        // Result of a visibility query — references to internal lists, valid until next GetVisible call.
        struct VisibilityResult {
                const std::vector<RenderCommand const *> & opaque;       // unsorted
                const std::vector<RenderCommand const *> & transparent;  // unsorted
                bool changed{false};
        };

        private:

        std::unordered_map <std::uintptr_t, TVariant>   mActiveRenderables;
        // Pre-built command lists, populated on GetVisible
        std::vector<RenderCommand const *>              vOpaqueCommands;
        std::vector<RenderCommand const *>              vTransparentCommands;
        bool                                            changed{true};

        // camera view-projection tracking (see UpdateCameraView)
        glm::mat4                                       mLastViewProjection{1.0f};
        bool                                            view_valid{false};
        float                                           view_epsilon{1e-3f};

        public:

        AllVisible();
        template <class TRenderable > void AddRenderable(TRenderable * pComponent);
        template <class TRenderable > void RemoveRenderable(TRenderable * pComponent);
        /** Track the active camera view-projection — the renderers call this every
         *  frame. Any change beyond epsilon — position, orientation, fov, viewport —
         *  invalidates the cached command lists so that view-dependent renderables
         *  (terrain LOD selection, frustum culling) rebuild. A standing camera
         *  reproduces the same matrix bit-for-bit, so static scenes never rebuild. */
        void UpdateCameraView(const glm::mat4 & mViewProjection);
        /** Force a rebuild on the next GetVisible (rare: world-level content swaps). */
        void MarkDirty() { changed = true; }
        /** Rebuilds the command lists when the renderable set changed — or the camera
         *  moved beyond epsilon. */
        VisibilityResult GetVisible();
};


} //namespace SE

#include <AllVisible.tcc>

#endif
