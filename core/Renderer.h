
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

        void PrepareVisible();

        public:
        Renderer(std::unique_ptr<TVisibilityManager>  pNewManager);
        Renderer();
        void Render();
        template <class TRenderable > void AddRenderable(TRenderable * pComponent);
        template <class TRenderable > void RemoveRenderable(TRenderable * pComponent);
        void                               AddRenderCmd(RenderCommand const * pCmd);
        void                               SetScreenSize(const glm::uvec2 new_screen_size);
        void                               SetCamera(Camera * pCurCamera);
        Camera *                           GetCamera() const;
        void                               Print(const size_t indent = 0);

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


