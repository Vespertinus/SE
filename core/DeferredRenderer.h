
#ifndef DEFERRED_RENDERER_H
#define DEFERRED_RENDERER_H 1

#include <RenderCommand.h>
#include <GBuffer.h>
#include <SSAOBuffer.h>
#include <ShadowBuffer.h>
#include <BloomBuffer.h>
#include <Light.h>
#include <ClusterConfig.h>
#include <ClusterSSBO.h>
#include <CommonEvents.h>
#include <EventManager.h>
#include <ImageUtil.h>

namespace SE {

class Camera;

template <class TVisibilityManager> class DeferredRenderer {

        std::unique_ptr<TVisibilityManager>  pManager;
        Camera                             * pCamera       { nullptr };
        glm::uvec2                           screen_size   { 1024, 1024 };
        std::vector<RenderCommand const *>   vRenderCommands;
        std::vector<RenderCommand const *>   vRenderInstantCommands;
        std::vector<RenderCommand const *>   vTransparentCommands;          // transparent/alpha commands (pre-sorted)

        // G-buffer
        GBuffer  oGBuffer;

        // HDR accumulation buffer (shares depth-stencil with GBuffer)
        FrameBuffer      oHdrFBO;
        H<TTexture>      hHdrTex;

        // OIT accumulation buffer (shares depth-stencil with GBuffer)
        FrameBuffer      oOitFBO;
        H<TTexture>      hOitAccumTex;
        H<TTexture>      hOitRevealTex;

        // LDR buffer for FXAA input
        FrameBuffer      oLdrFBO;
        H<TTexture>      hLdrTex;

        // SSAO
        SSAOBuffer oSSAOBuffer;
        bool       ssao_enabled { true };

        // Shadow map
        ShadowBuffer     oShadowBuffer;
        H<ShaderProgram> hShadowShader;
        glm::mat4        oLightVP;

        // Fullscreen quad
        uint32_t quad_vao { 0 };
        uint32_t quad_vbo { 0 };

        // Shader programs
        H<ShaderProgram> hClusterBuildShader;       // compute: build light index
        H<ShaderProgram> hClusterLightingShader;    // fullscreen frag: all lighting
        H<ShaderProgram> hSSAOShader;
        H<ShaderProgram> hSSAOBlurShader;
        H<ShaderProgram> hToneMapShader;
        H<ShaderProgram> hOitAccumShader;           // forward PBR → OIT MRT targets
        H<ShaderProgram> hOitComposeShader;         // fullscreen compose: OIT onto HDR

        // Clustered shading
        ClusterConfig  oClusterConfig;
        ClusterSSBO    oClusterSSBO;
        uint32_t       maxLightCapacity { 1024 };   // max point lights supported

        // IBL — diffuse irradiance + specular prefiltered env cubemaps
        H<TTexture>   hIrradianceTex;
        H<TTexture>   hPrefilteredEnvTex;
        float         ibl_scale    { 1.0f };
        glm::mat3     ibl_rotation { 1.0f };  // identity — no rotation

        // Bloom
        BloomBuffer      oBloomBuffer;
        H<ShaderProgram> hBloomBrightShader;
        H<ShaderProgram> hBloomDownShader;
        H<ShaderProgram> hBloomUpShader;
        bool  bloom_enabled   { true  };
        float bloom_threshold { 1.0f  };
        float bloom_strength  { 0.5f };

        // FXAA
        H<ShaderProgram> hFxaaShader;
        bool             fxaa_enabled { true };

        // Light state
        std::vector<PointLight> vPointLights;
        DirLight                oDirLight;

        // Debug: per-pass image capture
        bool                    debug_next_frame   { false };
        uint32_t                debug_frame_counter { 0 };
        void                    DumpPassImages(const std::string &dir);

        void CreateQuad();
        void CreateHdrBuffer();
        void DestroyHdrBuffer() noexcept;
        void CreateLdrBuffer();
        void DestroyLdrBuffer() noexcept;
        void CreateOitBuffer();
        void DestroyOitBuffer() noexcept;

        void PrepareVisible();
        void GeometryPass();
        void SSAOPass();
        void SSAOBlurPass();
        void ShadowPass();
        void ClusterBuildPass();
        void ClusteredLightingPass();
        void OitAccumPass();
        void OitComposePass();
        void ToneMapPass();
        void BloomPass();
        void FxaaPass();

        /** Recompute cluster depth slices from camera near/far and re-upload
         *  view-space AABBs. Called at init, on camera set, and on projection change. */
        void RebuildClusterConfig();

        /** ECameraProjChanged handler: rebuilds cluster config when FOV/clip changes. */
        void OnCameraProjChanged(const Event &);

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

        // --- debug ---
        void     WriteDebug    ();

        // --- light management ---
        size_t AddPointLight   (const PointLight &);  // returns index
        void   RemovePointLight(size_t idx);
        void   SetDirLight     (const DirLight &);
        void   SetIBL          (H<TTexture> hIrradiance, H<TTexture> hPrefiltered = H<TTexture>::Null());
        void   SetIBLScale     (float scale);
        void   SetIBLRotation  (float yaw_deg);   ///< rotation around world Y axis, degrees
        void   SetBloomEnabled  (bool  enabled);
        void   SetBloomThreshold(float threshold);
        void   SetBloomStrength (float strength);
        void   SetFxaaEnabled   (bool  enabled);
};

} // namespace SE

#endif
