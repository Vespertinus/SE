
#ifndef DEFERRED_RENDERER_H
#define DEFERRED_RENDERER_H 1

#include <RenderCommand.h>
#include <GBuffer.h>
#include <Light.h>

namespace SE {

class Camera;

template <class TVisibilityManager> class DeferredRenderer {

        std::unique_ptr<TVisibilityManager>  pManager;
        Camera                             * pCamera       { nullptr };
        glm::uvec2                           screen_size   { 1024, 1024 };
        std::vector<RenderCommand const *>   vRenderCommands;
        std::vector<RenderCommand const *>   vRenderInstantCommands;

        // G-buffer & internal rendering resources
        GBuffer  oGBuffer;
        uint32_t quad_vao      { 0 };
        uint32_t quad_vbo      { 0 };

        // Light state
        ShaderProgram         * pLightingShader;
        std::unique_ptr<UniformBlock> pBlock;

        std::vector<PointLight> vPointLights;
        DirLight                oDirLight;

        void PrepareVisible();
        void GeometryPass();
        void LightingPass();
        void CreateQuad();

        public:

        DeferredRenderer();
        ~DeferredRenderer() noexcept;

        // API Renderer<T> ---
        void Render();
        template <class TRenderable> void AddRenderable   (TRenderable *);
        template <class TRenderable> void RemoveRenderable(TRenderable *);
        void     AddRenderCmd  (RenderCommand const *);
        void     SetScreenSize (glm::uvec2 new_size);
        void     SetCamera     (Camera *);
        Camera * GetCamera     () const;
        void     Print         (size_t indent = 0);

        // --- light management ---
        size_t AddPointLight   (const PointLight &);  // returns index
        void   RemovePointLight(size_t idx);
        void   SetDirLight     (const DirLight &);
};

} // namespace SE

#endif
