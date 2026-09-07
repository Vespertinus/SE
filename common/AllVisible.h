#ifndef __ALL_VISIBLE_H__
#define __ALL_VISIBLE_H__ 1

#include <algorithm>
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

        public:

        AllVisible();
        template <class TRenderable > void AddRenderable(TRenderable * pComponent);
        template <class TRenderable > void RemoveRenderable(TRenderable * pComponent);
        /** Rebuilds the command lists only when the renderable set changed — camera
         *  movement does not affect their contents (no camera-dependent data today). */
        VisibilityResult GetVisible();
};


} //namespace SE

#include <AllVisible.tcc>

#endif
