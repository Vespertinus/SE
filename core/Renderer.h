
#ifndef __RENDERER_H__
#define __RENDERER_H__ 1

#include <RenderCommand.h>

/**
- basic renderer api
- visibility (Culling) manager as param
-
*/

namespace SE {

class Camera;

template <class TVisibilityManager> class Renderer {

        std::unique_ptr<TVisibilityManager>     pManager;
        /** stat */
        //uint32_t                                rendered_cnt;
        //uint32_t                                total_elements_cnt;
        //??? shader values?
        glm::uvec2                              screen_size{1024, 1024};
        Camera                                * pCamera{nullptr};
        std::vector<RenderCommand const *>      vRenderCommands;
        std::vector<RenderCommand const *>      vRenderInstantCommands;
        std::vector<RenderCommand const *>      vTransparentCommands;

        /** sort scratch: {command, squared depth} — reused across gathers */
        struct SortItem {
                RenderCommand const * pCmd;
                float                 depth;
        };
        std::vector<SortItem>                   vSortScratch;

        void PrepareVisible();

        public:
        Renderer(std::unique_ptr<TVisibilityManager>  pNewManager);
        Renderer();
        void Render();
        template <class TRenderable > void AddRenderable(TRenderable * pComponent);
        template <class TRenderable > void RemoveRenderable(TRenderable * pComponent);
        /** Invalidate the cached command lists for the next frame — for renderable
         *  state that changes outside the camera (e.g. streamed-in terrain tiles
         *  gaining GPU buffers). */
        void                               MarkVisibleDirty();
        void                               AddRenderCmd(RenderCommand const * pCmd);
        void                               SetScreenSize(const glm::uvec2 new_screen_size);
        void                               SetCamera(Camera * pCurCamera);
        Camera *                           GetCamera() const;
        void                               Print(const size_t indent = 0);
        /** Animation gate (visible-set only — no shadow pass here). */
        bool                               IsRenderableHot(const void * pComponent,
                                                           const glm::vec3 &, float) const {
                                                   return !pManager || pManager->WasRenderableVisible(pComponent);
                                           }

        /**
                - set of all TRenderable
                - array of visible TRenderable
                -- array of RenderCommand from visible drawables
                - sort render commands keys..?
                - draw all render commands
                - keep dirty flag, for rendering same objects set
                -- same numer of elements but other position --> need to call GetVisible again
                --- TRenderable->Update invalidate state..
                -- camera movement --> invalidate again

                TODO separate list for dirty TRenderable
        */
};


} //namespace SE

#endif


