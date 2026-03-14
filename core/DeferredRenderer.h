
#ifndef DEFERRED_RENDERER_H
#define DEFERRED_RENDERER_H 1

#include <RenderCommand.h>
#include <GBuffer.h>
#include <SSAOBuffer.h>
#include <Light.h>

namespace SE {

class Camera;

template <class TVisibilityManager> class DeferredRenderer {

        std::unique_ptr<TVisibilityManager>  pManager;
        Camera                             * pCamera       { nullptr };
        glm::uvec2                           screen_size   { 1024, 1024 };
        std::vector<RenderCommand const *>   vRenderCommands;
        std::vector<RenderCommand const *>   vRenderInstantCommands;

        // G-buffer
        GBuffer  oGBuffer;

        // HDR accumulation buffer (shares depth-stencil with GBuffer)
        FrameBuffer      oHdrFBO;
        H<TTexture> hHdrTex;

        // SSAO
        SSAOBuffer oSSAOBuffer;
        bool       ssao_enabled { true };

        // Fullscreen quad
        uint32_t quad_vao { 0 };
        uint32_t quad_vbo { 0 };

        // Sphere mesh for point light volumes
        uint32_t sphere_vao         { 0 };
        uint32_t sphere_vbo         { 0 };
        uint32_t sphere_ibo         { 0 };
        uint32_t sphere_index_count { 0 };

        // Shader programs
        H<ShaderProgram> hAmbientDirShader;
        H<ShaderProgram> hPointLightShader;
        H<ShaderProgram> hSSAOShader;
        H<ShaderProgram> hSSAOBlurShader;
        H<ShaderProgram> hToneMapShader;

        // Lighting UBO for ambient+directional pass
        std::unique_ptr<UniformBlock> pLightingBlock;

        // Light state
        std::vector<PointLight> vPointLights;
        DirLight                oDirLight;

        void CreateQuad();
        void CreateSphere(int rings = 16, int sectors = 32);
        void CreateHdrBuffer();
        void DestroyHdrBuffer() noexcept;

        void PrepareVisible();
        void GeometryPass();
        void SSAOPass();
        void SSAOBlurPass();
        void AmbientDirPass();
        void PointLightPass();
        void ToneMapPass();

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
