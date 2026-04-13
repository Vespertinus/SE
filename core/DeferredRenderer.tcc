
#include <glm/gtc/matrix_transform.hpp>

namespace SE {

// ---------------------------------------------------------------------------
// DeferredRenderer implementation
// ---------------------------------------------------------------------------

template <class TVisibilityManager>
DeferredRenderer<TVisibilityManager>::DeferredRenderer() :
        pManager(std::make_unique<TVisibilityManager>()) {

        vRenderCommands.reserve(1000);
        CreateQuad();

        auto & cfg = GetSystem<SE::Config>();

        hClusterBuildShader    = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/cluster_build.sesp");
        hClusterLightingShader = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/cluster_lighting.sesp");
        hSSAOShader            = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/ssao.sesp");
        hSSAOBlurShader        = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/ssao_blur.sesp");
        hToneMapShader         = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/tonemapping.sesp");
        hShadowShader          = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/shadow_depth.sesp");
        hBloomBrightShader     = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/bloom_bright.sesp");
        hBloomDownShader       = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/bloom_downsample.sesp");
        hBloomUpShader         = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/bloom_upsample.sesp");
        hFxaaShader            = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/fxaa.sesp");
        hOitAccumShader        = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/oit_accum.sesp");
        hOitComposeShader      = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/oit_compose.sesp");
        oShadowBuffer.Init();

        // Default neutral IBL: 0.03 * PI so (irradiance * albedo / PI * ao) == old (0.03 * albedo * ao)
        const float kDefaultIrr = 0.03f * glm::pi<float>();
        hIrradianceTex = CreateResource<TTexture>("ibl/default", TextureStock{},
                StoreTextureCubeMap::Settings(glm::vec3(kDefaultIrr)));
        // Prefiltered env default: constant env radiance L = kDefaultIrr / PI = 0.03
        hPrefilteredEnvTex = CreateResource<TTexture>("ibl/default_prefiltered", TextureStock{},
                StoreTextureCubeMap::Settings(glm::vec3(0.03f)));

        // Init cluster config and SSBOs
        RebuildClusterConfig();

        // Subscribe to camera projection changes so clusters stay in sync with camera near/far/FOV
        GetSystem<EventManager>().AddListener<ECameraProjChanged,
                &DeferredRenderer::OnCameraProjChanged>(this);
}

template <class TVisibilityManager>
DeferredRenderer<TVisibilityManager>::~DeferredRenderer() noexcept {

        GetSystem<EventManager>().RemoveListener<ECameraProjChanged,
                &DeferredRenderer::OnCameraProjChanged>(this);

        DestroyOitBuffer();
        DestroyHdrBuffer();
        DestroyLdrBuffer();

        if (hIrradianceTex.IsValid()) {
                TResourceManager::Instance().Destroy(hIrradianceTex);
                hIrradianceTex = H<TTexture>::Null();
        }
        if (hPrefilteredEnvTex.IsValid()) {
                TResourceManager::Instance().Destroy(hPrefilteredEnvTex);
                hPrefilteredEnvTex = H<TTexture>::Null();
        }

        DestroyResource(hBloomBrightShader);
        DestroyResource(hBloomDownShader);
        DestroyResource(hBloomUpShader);
        DestroyResource(hFxaaShader);
        DestroyResource(hClusterBuildShader);
        DestroyResource(hClusterLightingShader);
        DestroyResource(hSSAOShader);
        DestroyResource(hSSAOBlurShader);
        DestroyResource(hToneMapShader);
        DestroyResource(hShadowShader);
        DestroyResource(hOitAccumShader);
        DestroyResource(hOitComposeShader);

        if (quad_vao)    { glDeleteVertexArrays(1, &quad_vao);    quad_vao    = 0; }
        if (quad_vbo)    { glDeleteBuffers(1, &quad_vbo);         quad_vbo    = 0; }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::CreateHdrBuffer() {

        TextureStock ts { nullptr, 0, GL_RGBA, GL_RGBA16F, screen_size.x, screen_size.y };
        hHdrTex = CreateResource<TTexture>("hdr/accum", ts,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT));

        oHdrFBO.Create();
        oHdrFBO.Bind();
        oHdrFBO.AttachColor(0, GetResource(hHdrTex));
        // Share depth-stencil from GBuffer (enables depth test during light volumes pass)
        oHdrFBO.AttachDepthStencil(oGBuffer.GetDepthTex());
        oHdrFBO.CheckComplete();
        oHdrFBO.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::DestroyHdrBuffer() noexcept {

        oHdrFBO.Destroy();
        if (hHdrTex.IsValid()) {
                TResourceManager::Instance().Destroy(hHdrTex);
                hHdrTex = H<TTexture>::Null();
        }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::CreateLdrBuffer() {

        TextureStock ts { nullptr, 0, GL_RGB, GL_RGB8, screen_size.x, screen_size.y };
        hLdrTex = CreateResource<TTexture>("ldr/fxaa_input", ts,
                StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_BYTE, GL_LINEAR, GL_LINEAR));
        oLdrFBO.Create();
        oLdrFBO.Bind();
        oLdrFBO.AttachColor(0, GetResource(hLdrTex));
        oLdrFBO.CheckComplete();
        oLdrFBO.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::DestroyLdrBuffer() noexcept {

        oLdrFBO.Destroy();
        if (hLdrTex.IsValid()) {
                TResourceManager::Instance().Destroy(hLdrTex);
                hLdrTex = H<TTexture>::Null();
        }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::CreateOitBuffer() {

        TextureStock ts_a { nullptr, 0, GL_RGBA, GL_RGBA16F, screen_size.x, screen_size.y };
        hOitAccumTex = CreateResource<TTexture>("oit/accum", ts_a,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT));

        TextureStock ts_r { nullptr, 0, GL_RED, GL_R16F, screen_size.x, screen_size.y };
        hOitRevealTex = CreateResource<TTexture>("oit/revealage", ts_r,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT));

        oOitFBO.Create();
        oOitFBO.Bind();
        oOitFBO.AttachColor(0, GetResource(hOitAccumTex));
        oOitFBO.AttachColor(1, GetResource(hOitRevealTex));
        oOitFBO.AttachDepthStencil(oGBuffer.GetDepthTex());
        oOitFBO.SetDrawBuffers({0, 1});
        oOitFBO.CheckComplete();
        oOitFBO.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::DestroyOitBuffer() noexcept {

        oOitFBO.Destroy();
        if (hOitAccumTex.IsValid()) {
                TResourceManager::Instance().Destroy(hOitAccumTex);
                hOitAccumTex = H<TTexture>::Null();
        }
        if (hOitRevealTex.IsValid()) {
                TResourceManager::Instance().Destroy(hOitRevealTex);
                hOitRevealTex = H<TTexture>::Null();
        }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::CreateQuad() {

        // Full-screen quad: 6 vertices (2 triangles), interleaved Position(vec2) + TexCoord0(vec2)
        const float vertices[] = {
                // Position    TexCoord
                -1.f, -1.f,   0.f, 0.f,
                1.f, -1.f,   1.f, 0.f,
                1.f,  1.f,   1.f, 1.f,
                -1.f, -1.f,   0.f, 0.f,
                1.f,  1.f,   1.f, 1.f,
                -1.f,  1.f,   0.f, 1.f,
        };

        glGenVertexArrays(1, &quad_vao);
        glGenBuffers(1, &quad_vbo);

        glBindVertexArray(quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Position at attribute location 0
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void *>(0));
        glEnableVertexAttribArray(0);

        // TexCoord0 at attribute location 2
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void *>(2 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::PrepareVisible() {

        auto result = pManager->GetVisible(pCamera->GetWorldPos());

        if (!result.changed) { return; }

        vRenderCommands.clear();
        vRenderCommands.reserve(result.opaque.size());
        for (const auto * pCmd : result.opaque) {
                vRenderCommands.emplace_back(pCmd);
        }

        vTransparentCommands.clear();
        vTransparentCommands.reserve(result.transparent.size());
        for (const auto * pCmd : result.transparent) {
                vTransparentCommands.emplace_back(pCmd);
        }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::GeometryPass() {

        oGBuffer.Bind();

        auto & gs = GetSystem<GraphicsState>();
        gs.Clear(ClearBuffer::COLOR | ClearBuffer::DEPTH);
        gs.SetDepthTest(true);
        gs.SetDepthMask(true);
        gs.SetDepthFunc(DepthFunc::LESS);

        gs.SetViewProjection(pCamera->GetWorldMVP());

        for (auto * pCmd : vRenderCommands) {
                pCmd->Draw();
        }
        for (auto * pCmd : vRenderInstantCommands) {
                pCmd->Draw();
        }
        vRenderInstantCommands.clear();

        oGBuffer.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SSAOPass() {

        oSSAOBuffer.Bind();

        auto & gs = GetSystem<GraphicsState>();
        gs.Clear(ClearBuffer::COLOR);
        gs.SetDepthTest(false);
        gs.SetDepthMask(false);

        gs.SetShaderProgram(GetResource(hSSAOShader));

        gs.SetTexture(TextureUnit::NORMAL, oGBuffer.GetNormalMetallicTex());
        gs.SetTexture(TextureUnit::BUFFER, oGBuffer.GetDepthTex());

        gs.SetTexture(TextureUnit::NOISE, oSSAOBuffer.GetNoiseTex());

        static StrID inv_vp_id("InvVPMatrix");
        static StrID vp_id("VPMatrix");
        static StrID noise_scale_id("NoiseScale");

        const glm::mat4 & vp    = pCamera->GetWorldMVP();
        glm::mat4         inv_vp = glm::inverse(vp);
        glm::vec2         noise_scale(
                        static_cast<float>(screen_size.x) / 4.0f,
                        static_cast<float>(screen_size.y) / 4.0f);

        gs.SetVariable(inv_vp_id, inv_vp);
        gs.SetVariable(vp_id, vp);
        gs.SetVariable(noise_scale_id, noise_scale);

        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        oSSAOBuffer.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SSAOBlurPass() {

        oSSAOBuffer.BindBlur();

        auto & gs = GetSystem<GraphicsState>();
        gs.Clear(ClearBuffer::COLOR);
        gs.SetDepthTest(false);
        gs.SetDepthMask(false);

        gs.SetShaderProgram(GetResource(hSSAOBlurShader));

        gs.SetTexture(TextureUnit::SSAO_TEX, oSSAOBuffer.GetSSAOTex());

        static StrID texel_size_id("TexelSize");
        glm::vec2 texel_size(
                        1.0f / static_cast<float>(screen_size.x),
                        1.0f / static_cast<float>(screen_size.y));
        gs.SetVariable(texel_size_id, texel_size);

        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        oSSAOBuffer.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::ShadowPass() {

        const glm::vec3 lightDir  = glm::normalize(oDirLight.direction);
        const glm::vec3 up        = (glm::abs(lightDir.y) > 0.99f)
                                  ? glm::vec3(1.f, 0.f, 0.f)
                                  : glm::vec3(0.f, 1.f, 0.f);
        const float     halfExt   = oDirLight.shadow_ortho;
        const glm::vec3 lightPos  = -lightDir * oDirLight.shadow_far * 0.5f;
        const glm::mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, up);
        const glm::mat4 lightProj = glm::ortho(-halfExt, halfExt, -halfExt, halfExt,
                                                oDirLight.shadow_near, oDirLight.shadow_far);
        oLightVP = lightProj * lightView;

        oShadowBuffer.Bind();

        auto & gs = GetSystem<GraphicsState>();
        gs.Clear(ClearBuffer::DEPTH);
        gs.SetDepthTest(true);
        gs.SetDepthMask(true);
        gs.SetDepthFunc(DepthFunc::LESS);
        gs.SetCullFace(true, CullFace::FRONT);

        gs.SetShaderProgram(GetResource(hShadowShader));
        gs.SetViewProjection(oLightVP);

        for (auto * pCmd : vRenderCommands) {
                pCmd->DrawDepth();
        }

        gs.SetCullFace(false);
        oShadowBuffer.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::ClusterBuildPass() {

        if (vPointLights.empty()) { return; }

        auto & gs = GetSystem<GraphicsState>();

        // Set compute shader program first
        gs.SetShaderProgram(GetResource(hClusterBuildShader));

        // Upload light data
        oClusterSSBO.UploadLightData(vPointLights);

        // Re-bind ALL SSBOs explicitly after data upload (ensures clean state on NVIDIA)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, oClusterSSBO.GetClusterHeadersBuffer());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, oClusterSSBO.GetLightIndexBuffer());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, oClusterSSBO.GetLightDataBuffer());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, oClusterSSBO.GetDebugCounterBuffer());

        // Full barrier before dispatch
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        // Set uniforms
        static StrID view_id("ViewMatrix");
        static StrID light_cnt_id("TotalLightCount");

        // ViewMatrix = V = inverse(P) * VP  (since GetWorldMVP returns VP for the camera)
        glm::mat4 viewMat = glm::inverse(pCamera->GetProjectionMatrix()) * pCamera->GetWorldMVP();

        gs.SetVariable(view_id, viewMat);
        gs.SetVariable(light_cnt_id, static_cast<uint32_t>(vPointLights.size()));

        // Dispatch: one invocation per cluster (tileX * tileY * depthSlices)
        uint32_t totalClusters = oClusterConfig.totalClusters;
        glDispatchCompute((totalClusters + 15) / 16, 1, 1);

        // Check for GL errors after dispatch
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
                log_e("ClusterBuildPass: GL error after dispatch: {:x}", err);
        }

        // Ensure compute writes are visible to fragment shader
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        CheckOpenGLError();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::ClusteredLightingPass() {

        oHdrFBO.Bind();

        auto & gs = GetSystem<GraphicsState>();
        gs.Clear(ClearBuffer::COLOR);
        gs.SetBlend(false);
        gs.SetDepthMask(false);
        gs.SetDepthTest(false);

        gs.SetShaderProgram(GetResource(hClusterLightingShader));

        // G-buffer textures
        gs.SetTexture(TextureUnit::DIFFUSE,  oGBuffer.GetAlbedoRoughnessTex());
        gs.SetTexture(TextureUnit::NORMAL,   oGBuffer.GetNormalMetallicTex());
        gs.SetTexture(TextureUnit::RENDER_BUFFER, oGBuffer.GetEmissiveTex());
        gs.SetTexture(TextureUnit::BUFFER,   oGBuffer.GetDepthTex());
        gs.SetTexture(TextureUnit::SHADOW,   oShadowBuffer.GetDepthTex());
        gs.SetTexture(TextureUnit::ENV,             GetResource(hIrradianceTex));
        gs.SetTexture(TextureUnit::PREFILTERED_ENV, GetResource(hPrefilteredEnvTex));

        if (ssao_enabled) {
                gs.SetTexture(TextureUnit::SSAO_TEX, oSSAOBuffer.GetBlurTex());
        }

        // Bind SSBOs for fragment shader
        oClusterSSBO.BindForFragment();

        static StrID inv_vp_id("InvVPMatrix");
        static StrID cam_pos_id("CamPos");
        static StrID dir_dir_id("DirLightDir");
        static StrID dir_col_id("DirLightColor");
        static StrID dir_int_id("DirLightIntensity");
        static StrID light_vp_id("DirLightVPMatrix");
        static StrID near_id("NearZ");
        static StrID far_id("FarZ");
        static StrID ibl_scale_id("IBLScale");
        static StrID ibl_rot_id("IBLRotation");

        glm::mat4         inv_vp = glm::inverse(pCamera->GetWorldMVP());
        glm::vec3         cam_pos = pCamera->GetWorldPos();

        gs.SetVariable(inv_vp_id, inv_vp);
        gs.SetVariable(cam_pos_id, cam_pos);
        gs.SetVariable(light_vp_id, oLightVP);
        gs.SetVariable(dir_dir_id, oDirLight.direction);
        gs.SetVariable(dir_col_id, oDirLight.color);
        gs.SetVariable(dir_int_id, oDirLight.intensity);
        gs.SetVariable(near_id, oClusterConfig.nearZ);
        gs.SetVariable(far_id, oClusterConfig.farZ);
        gs.SetVariable(ibl_scale_id, ibl_scale);
        gs.SetVariable(ibl_rot_id,   ibl_rotation);

        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        gs.SetDepthMask(true);

        // Unbind SSBOs
        ClusterSSBO::Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::OitAccumPass() {

        if (vTransparentCommands.empty()) { return; }

        oOitFBO.Bind();

        // Clear: accum → 0, revealage → 1
        static const float zeros[4] = { 0.f, 0.f, 0.f, 0.f };
        static const float one      = 1.f;
        glClearBufferfv(GL_COLOR, 0, zeros);
        glClearBufferfv(GL_COLOR, 1, &one);

        auto & gs = GetSystem<GraphicsState>();
        gs.SetDepthTest(true);
        gs.SetDepthMask(false);
        gs.SetDepthFunc(DepthFunc::LEQUAL);
        gs.SetCullFace(false);
        gs.SetBlend(true);

        // Per-buffer blend functions.
        // glBlendFunci must be used here — glBlendFunc would override both attachments uniformly.
        glBlendFunci(0, GL_ONE, GL_ONE);                   // accum: additive
        glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);  // revealage: multiplicative

        gs.SetShaderProgram(GetResource(hOitAccumShader));
        gs.SetViewProjection(pCamera->GetWorldMVP());

        gs.SetTexture(TextureUnit::SSAO_TEX, oSSAOBuffer.GetBlurTex());
        gs.SetTexture(TextureUnit::SHADOW,   oShadowBuffer.GetDepthTex());
        gs.SetTexture(TextureUnit::ENV,             GetResource(hIrradianceTex));
        gs.SetTexture(TextureUnit::PREFILTERED_ENV, GetResource(hPrefilteredEnvTex));

        static StrID cam_pos_id("CamPos");
        static StrID dir_vp_id("DirLightVPMatrix");
        static StrID dir_dir_id("DirLightDir");
        static StrID dir_col_id("DirLightColor");
        static StrID dir_int_id("DirLightIntensity");

        gs.SetVariable(cam_pos_id,  pCamera->GetWorldPos());
        gs.SetVariable(dir_vp_id,   oLightVP);
        gs.SetVariable(dir_dir_id,  oDirLight.direction);
        gs.SetVariable(dir_col_id,  oDirLight.color);
        gs.SetVariable(dir_int_id,  oDirLight.intensity);

        for (const auto * pCmd : vTransparentCommands) {
                pCmd->DrawTransparent();
        }

        gs.SetBlend(false);
        gs.SetDepthMask(true);
        gs.SetDepthFunc(DepthFunc::LESS);

        oOitFBO.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::OitComposePass() {

        if (vTransparentCommands.empty()) { return; }

        oHdrFBO.Bind();

        auto & gs = GetSystem<GraphicsState>();
        gs.SetDepthTest(false);
        gs.SetDepthMask(false);
        gs.SetBlendMode(BlendMode::Translucent);

        gs.SetShaderProgram(GetResource(hOitComposeShader));
        gs.SetTexture(TextureUnit::OIT_ACCUM,     GetResource(hOitAccumTex));
        gs.SetTexture(TextureUnit::OIT_REVEALAGE, GetResource(hOitRevealTex));

        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        gs.SetBlend(false);
        gs.SetDepthMask(true);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::ToneMapPass() {

        if (fxaa_enabled) {
                oLdrFBO.Bind();
        } else {
                oHdrFBO.Unbind();
        }

        auto & gs = GetSystem<GraphicsState>();
        gs.SetViewport(0, 0,
                static_cast<int32_t>(screen_size.x), static_cast<int32_t>(screen_size.y));
        gs.Clear(ClearBuffer::COLOR);
        gs.SetDepthTest(false);
        gs.SetDepthMask(false);
        gs.SetBlend(false);

        gs.SetShaderProgram(GetResource(hToneMapShader));

        gs.SetTexture(TextureUnit::HDR, GetResource(hHdrTex));

        static StrID bloom_str_id("BloomStrength");
        if (bloom_enabled && oBloomBuffer.IsInitialized()) {
                gs.SetTexture(TextureUnit::CUSTOM, oBloomBuffer.GetBloomTex());
                gs.SetVariable(bloom_str_id, bloom_strength);
        } else {
                gs.SetVariable(bloom_str_id, 0.0f);
        }

        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        gs.SetDepthMask(true);
        gs.SetDepthTest(true);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::BloomPass() {

        if (!bloom_enabled || !oBloomBuffer.IsInitialized()) { return; }

        auto & gs = GetSystem<GraphicsState>();
        gs.SetDepthTest(false);
        gs.SetDepthMask(false);
        gs.SetBlend(false);

        static StrID texel_size_id("TexelSize");
        static StrID threshold_id("BloomThreshold");

        // Bright pass: HDR → down[0] at half-res
        oBloomBuffer.BindDown(0);
        gs.Clear(ClearBuffer::COLOR);
        gs.SetShaderProgram(GetResource(hBloomBrightShader));
        gs.SetTexture(TextureUnit::HDR, GetResource(hHdrTex));
        gs.SetVariable(texel_size_id, glm::vec2(1.0f / screen_size.x, 1.0f / screen_size.y));
        gs.SetVariable(threshold_id, bloom_threshold);
        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Downsample chain: down[k-1] → down[k]
        gs.SetShaderProgram(GetResource(hBloomDownShader));
        for (int k = 1; k < BloomBuffer::MipCount; ++k) {
                glm::uvec2 src_size = oBloomBuffer.MipSize(k - 1);
                oBloomBuffer.BindDown(k);
                gs.Clear(ClearBuffer::COLOR);
                gs.SetTexture(TextureUnit::DIFFUSE, oBloomBuffer.GetDownTex(k - 1));
                gs.SetVariable(texel_size_id, glm::vec2(1.0f / src_size.x, 1.0f / src_size.y));
                glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // Upsample chain: up[k] = tent(src) + down[k]
        //   k descends from MipCount-2 to 0
        //   src = down[MipCount-1] for first iteration, up[k+1] thereafter
        gs.SetShaderProgram(GetResource(hBloomUpShader));
        for (int k = BloomBuffer::MipCount - 2; k >= 0; --k) {
                const int  src_level = k + 1;
                glm::uvec2 src_size  = oBloomBuffer.MipSize(src_level);
                TTexture * pSrc      = (k == BloomBuffer::MipCount - 2)
                                       ? oBloomBuffer.GetDownTex(BloomBuffer::MipCount - 1)
                                       : oBloomBuffer.GetUpTex(k + 1);
                oBloomBuffer.BindUp(k);
                gs.Clear(ClearBuffer::COLOR);
                gs.SetTexture(TextureUnit::DIFFUSE, pSrc);
                gs.SetTexture(TextureUnit::NORMAL,  oBloomBuffer.GetDownTex(k));
                gs.SetVariable(texel_size_id, glm::vec2(1.0f / src_size.x, 1.0f / src_size.y));
                glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);
        oBloomBuffer.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::FxaaPass() {

        if (!fxaa_enabled) { return; }

        oLdrFBO.Unbind();

        auto & gs = GetSystem<GraphicsState>();
        gs.SetViewport(0, 0,
                static_cast<int32_t>(screen_size.x), static_cast<int32_t>(screen_size.y));
        gs.Clear(ClearBuffer::COLOR);
        gs.SetDepthTest(false);
        gs.SetDepthMask(false);
        gs.SetBlend(false);

        gs.SetShaderProgram(GetResource(hFxaaShader));
        gs.SetTexture(TextureUnit::BUFFER, GetResource(hLdrTex));

        static StrID texel_size_id("TexelSize");
        gs.SetVariable(texel_size_id,
                glm::vec2(1.0f / screen_size.x, 1.0f / screen_size.y));

        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        gs.SetDepthMask(true);
        gs.SetDepthTest(true);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::Render() {

        if (!pCamera) {
                log_e("deferred renderer: main camera was not set");
                return;
        }

        PrepareVisible();
        GeometryPass();
        ShadowPass();

        if (ssao_enabled) {
                SSAOPass();
                SSAOBlurPass();
        }

        ClusterBuildPass();
        ClusteredLightingPass();
        OitAccumPass();
        OitComposePass();
        BloomPass();
        ToneMapPass();
        FxaaPass();

        if (debug_next_frame) {
                // Read back cluster build debug counters (4 uint32_t)
                struct DebugCounters {
                        uint32_t assigned;
                        uint32_t depthPass;
                        uint32_t frustumPass;
                        uint32_t loopIter;
                } counters = {};
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, oClusterSSBO.GetDebugCounterBuffer());
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(counters), &counters);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
                log_i("ClusterBuildPass debug: {} assigned, {} depth-pass, {} frustum-pass, {} loop-iter across {} clusters ({} point lights)",
                                counters.assigned, counters.depthPass, counters.frustumPass, counters.loopIter,
                                oClusterConfig.totalClusters, vPointLights.size());

                auto &oConfig = GetSystem<Config>();
                std::string dir = oConfig.sDebugDir + "/frame_" + std::to_string(debug_frame_counter++);
                DumpPassImages(dir);
                debug_next_frame = false;
        }
}

template <class TVisibilityManager>
template <class TRenderable>
void DeferredRenderer<TVisibilityManager>::AddRenderable(TRenderable * pComponent) {

        pManager->AddRenderable(pComponent);
}

template <class TVisibilityManager>
template <class TRenderable>
void DeferredRenderer<TVisibilityManager>::RemoveRenderable(TRenderable * pComponent) {

        pManager->RemoveRenderable(pComponent);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::AddRenderCmd(RenderCommand const * pCmd) {

        vRenderInstantCommands.emplace_back(pCmd);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetScreenSize(glm::uvec2 new_size) {

        screen_size = new_size;
        oGBuffer.Resize(new_size);
        DestroyHdrBuffer();
        CreateHdrBuffer();
        DestroyOitBuffer();
        CreateOitBuffer();
        oSSAOBuffer.Resize(new_size);
        oBloomBuffer.Resize(new_size);
        DestroyLdrBuffer();
        CreateLdrBuffer();

        // Recompute cluster config and reinitialize SSBOs
        oClusterSSBO.Destroy();
        oClusterSSBO.Init(oClusterConfig, maxLightCapacity);
        RebuildClusterConfig();

        if (pCamera) {
                pCamera->UpdateDimension(new_size);
        }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetCamera(Camera * pCurCamera) {

        pCamera = pCurCamera;
        if (pCamera) {
                pCamera->UpdateDimension(screen_size);
                RebuildClusterConfig();
        }
}

template <class TVisibilityManager>
Camera * DeferredRenderer<TVisibilityManager>::GetCamera() const {

        return pCamera;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::RebuildClusterConfig() {

        const float near = pCamera ? pCamera->GetVolume().near_clip : 0.1f;
        const float far  = pCamera ? pCamera->GetVolume().far_clip  : 1000.f;

        const uint32_t oldTotal = oClusterConfig.totalClusters;
        oClusterConfig.Recompute(screen_size.x, screen_size.y, near, far);

        // Re-allocate SSBOs when cluster grid dimensions change (first call, screen resize)
        if (oClusterConfig.totalClusters != oldTotal) {
                oClusterSSBO.Init(oClusterConfig, maxLightCapacity);
        }

        const glm::mat4 projMat = pCamera
                ? pCamera->GetProjectionMatrix()
                : glm::perspective(glm::radians(60.f),
                                   static_cast<float>(screen_size.x) / static_cast<float>(screen_size.y),
                                   near, far);
        oClusterSSBO.UploadClusterHeaders(oClusterConfig, projMat);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::OnCameraProjChanged(const Event &) {

        RebuildClusterConfig();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::Print(size_t indent) {

        log_d_clean("{:>{}} DeferredRenderer: resolution: ({}, {}), commands cnt: {}\n",
                        ">", indent, screen_size[0], screen_size[1], vRenderCommands.size());
        for (const auto * pItem : vRenderCommands) {
                log_d_clean("{}", pItem->StrDump(indent));
        }
}

template <class TVisibilityManager>
size_t DeferredRenderer<TVisibilityManager>::AddPointLight(const PointLight & light) {

        vPointLights.push_back(light);
        return vPointLights.size() - 1;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::RemovePointLight(size_t idx) {

        if (idx < vPointLights.size()) {
                vPointLights.erase(vPointLights.begin() + static_cast<ptrdiff_t>(idx));
        }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetDirLight(const DirLight & light) {

        oDirLight = light;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetBloomEnabled(bool enabled) {
        bloom_enabled = enabled;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetBloomThreshold(float t) {
        bloom_threshold = t;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetBloomStrength(float s) {
        bloom_strength = s;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetFxaaEnabled(bool enabled) {
        fxaa_enabled = enabled;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetIBL(H<TTexture> hIrradiance,
                                                   H<TTexture> hPrefiltered) {
        hIrradianceTex = hIrradiance;
        if (hPrefiltered.IsValid()) hPrefilteredEnvTex = hPrefiltered;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetIBLScale(float scale) {
        ibl_scale = scale;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetIBLRotation(float yaw_deg) {
        const float rad = glm::radians(yaw_deg);
        ibl_rotation = glm::mat3(glm::rotate(glm::mat4(1.f), rad, glm::vec3(0.f, 1.f, 0.f)));
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::WriteDebug() {
        debug_next_frame = true;
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::DumpPassImages(const std::string &dir) {

        namespace fs = boost::filesystem;
        fs::create_directories(dir);

        const std::string s = "/";

        // --- G-buffer (GeometryPass output) ---
        ImageUtil::WriteTexture2D(oGBuffer.GetAlbedoRoughnessTex(), dir + s + "00_gbuffer_rt0.png");
        ImageUtil::WriteTexture2D(oGBuffer.GetNormalMetallicTex(),  dir + s + "01_gbuffer_rt1.png");
        ImageUtil::WriteTexture2D(oGBuffer.GetEmissiveTex(),        dir + s + "02_gbuffer_rt2.png");
        ImageUtil::WriteDepthTexture(oGBuffer.GetDepthTex(),
                        oClusterConfig.nearZ, oClusterConfig.farZ, dir + s + "03_gbuffer_depth.png");

        // G-buffer FBO reads (verifies texture dumps match FBO attachments)
        oGBuffer.Bind();
        ImageUtil::WriteReadBufferToFile(oGBuffer.GetFBO(), GL_COLOR_ATTACHMENT0,
                        screen_size.x, screen_size.y, dir + s + "00_gbuffer_rt0_fbo.png");
        ImageUtil::WriteReadBufferToFile(oGBuffer.GetFBO(), GL_COLOR_ATTACHMENT1,
                        screen_size.x, screen_size.y, dir + s + "01_gbuffer_rt1_fbo.png");
        ImageUtil::WriteReadBufferToFile(oGBuffer.GetFBO(), GL_COLOR_ATTACHMENT2,
                        screen_size.x, screen_size.y, dir + s + "02_gbuffer_rt2_fbo.png");
        oGBuffer.Unbind();

        // --- SSAO (if enabled) ---
        if (ssao_enabled) {
                ImageUtil::WriteTexture2D(oSSAOBuffer.GetBlurTex(), dir + s + "04_ssao.png");
        }

        // --- HDR (ClusteredLightingPass output) ---
        if (hHdrTex.IsValid()) {
                ImageUtil::WriteTexture2D(GetResource(hHdrTex), dir + s + "05_hdr.png");
        }
        // HDR FBO read
        oHdrFBO.Bind();
        ImageUtil::WriteReadBufferToFile(oHdrFBO.ID(), GL_COLOR_ATTACHMENT0,
                        screen_size.x, screen_size.y, dir + s + "05_hdr_fbo.png");
        oHdrFBO.Unbind();

        // --- Bloom (BloomPass output) ---
        if (bloom_enabled && oBloomBuffer.IsInitialized()) {
                ImageUtil::WriteTexture2D(oBloomBuffer.GetDownTex(0), dir + s + "06_bloom_bright.png");
                ImageUtil::WriteTexture2D(oBloomBuffer.GetBloomTex(), dir + s + "07_bloom_final.png");
        }

        // --- LDR (ToneMapPass output) ---
        if (hLdrTex.IsValid()) {
                ImageUtil::WriteTexture2D(GetResource(hLdrTex), dir + s + "08_ldr.png");
        }
        // LDR FBO read
        oLdrFBO.Bind();
        ImageUtil::WriteReadBufferToFile(oLdrFBO.ID(), GL_COLOR_ATTACHMENT0,
                        screen_size.x, screen_size.y, dir + s + "08_ldr_fbo.png");
        oLdrFBO.Unbind();

        // --- Final screen output (after FXAA) ---
        ImageUtil::WriteReadBufferToFile(0, GL_FRONT_LEFT,
                        screen_size.x, screen_size.y, dir + s + "09_final.png");

        log_i("Debug pass images written to '{}'", dir);
}

} // namespace SE
