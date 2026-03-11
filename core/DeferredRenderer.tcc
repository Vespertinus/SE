
namespace SE {

// ---------------------------------------------------------------------------
// DeferredRenderer implementation
// ---------------------------------------------------------------------------

template <class TVisibilityManager>
DeferredRenderer<TVisibilityManager>::DeferredRenderer() :
        pManager(std::make_unique<TVisibilityManager>()) {

        vRenderCommands.reserve(1000);
        CreateQuad();
        CreateSphere();

        auto & cfg = GetSystem<SE::Config>();

        pAmbientDirShader = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/ambient_dir.sesp");
        pPointLightShader = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/point_light.sesp");
        pSSAOShader       = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/ssao.sesp");
        pSSAOBlurShader   = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/ssao_blur.sesp");
        pToneMapShader    = CreateResource<SE::ShaderProgram>(cfg.sResourceDir + "shader_program/tonemapping.sesp");

        //THINK only one block.. reuse cam variables accross shaders
        pLightingBlock = std::make_unique<UniformBlock>(pAmbientDirShader, UniformUnitInfo::Type::LIGHTING);
}

template <class TVisibilityManager>
DeferredRenderer<TVisibilityManager>::~DeferredRenderer() noexcept {

        DestroyHdrBuffer();

        DestroyResource<ShaderProgram>(pAmbientDirShader);
        DestroyResource<ShaderProgram>(pPointLightShader);
        DestroyResource<ShaderProgram>(pSSAOShader);
        DestroyResource<ShaderProgram>(pSSAOBlurShader);
        DestroyResource<ShaderProgram>(pToneMapShader);

        if (quad_vao)    { glDeleteVertexArrays(1, &quad_vao);    quad_vao    = 0; }
        if (quad_vbo)    { glDeleteBuffers(1, &quad_vbo);         quad_vbo    = 0; }
        if (sphere_vao)  { glDeleteVertexArrays(1, &sphere_vao);  sphere_vao  = 0; }
        if (sphere_vbo)  { glDeleteBuffers(1, &sphere_vbo);       sphere_vbo  = 0; }
        if (sphere_ibo)  { glDeleteBuffers(1, &sphere_ibo);       sphere_ibo  = 0; }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::CreateHdrBuffer() {

        TextureStock ts { nullptr, 0, GL_RGBA, GL_RGBA16F, screen_size.x, screen_size.y };
        pHdrTex = CreateResource<TTexture>("hdr/accum", ts,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT));

        oHdrFBO.Create();
        oHdrFBO.Bind();
        oHdrFBO.AttachColor(0, pHdrTex);
        // Share depth-stencil from GBuffer (enables depth test during light volumes pass)
        oHdrFBO.AttachDepthStencil(oGBuffer.GetDepthTex());
        oHdrFBO.CheckComplete();
        oHdrFBO.Unbind();
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::DestroyHdrBuffer() noexcept {

        oHdrFBO.Destroy();
        if (pHdrTex) {
                TResourceManager::Instance().Destroy<TTexture>(pHdrTex->RID());
                pHdrTex = nullptr;
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
void DeferredRenderer<TVisibilityManager>::CreateSphere(int rings, int sectors) {

        std::vector<float>    vertices;
        std::vector<uint16_t> indices;

        vertices.reserve(static_cast<size_t>((rings + 1) * (sectors + 1) * 3));
        indices.reserve(static_cast<size_t>(rings * sectors * 6));

        const float pi    = 3.14159265358979f;
        const float two_pi = 2.0f * pi;

        for (int r = 0; r <= rings; ++r) {
                const float phi = pi * static_cast<float>(r) / static_cast<float>(rings);
                for (int s = 0; s <= sectors; ++s) {
                        const float theta = two_pi * static_cast<float>(s) / static_cast<float>(sectors);
                        vertices.push_back( sinf(phi) * cosf(theta));
                        vertices.push_back( cosf(phi));
                        vertices.push_back( sinf(phi) * sinf(theta));
                }
        }

        for (int r = 0; r < rings; ++r) {
                for (int s = 0; s < sectors; ++s) {
                        const int row_len = sectors + 1;
                        const int v0 = r       * row_len + s;
                        const int v1 = (r + 1) * row_len + s;
                        const int v2 = r       * row_len + (s + 1);
                        const int v3 = (r + 1) * row_len + (s + 1);
                        indices.push_back(static_cast<uint16_t>(v0));
                        indices.push_back(static_cast<uint16_t>(v1));
                        indices.push_back(static_cast<uint16_t>(v2));
                        indices.push_back(static_cast<uint16_t>(v1));
                        indices.push_back(static_cast<uint16_t>(v3));
                        indices.push_back(static_cast<uint16_t>(v2));
                }
        }

        sphere_index_count = static_cast<uint32_t>(indices.size());

        glGenVertexArrays(1, &sphere_vao);
        glGenBuffers(1, &sphere_vbo);
        glGenBuffers(1, &sphere_ibo);

        glBindVertexArray(sphere_vao);

        glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                        vertices.data(), GL_STATIC_DRAW);
        // Position at attribute location 0
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                        reinterpret_cast<void *>(0));
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                        static_cast<GLsizeiptr>(indices.size() * sizeof(uint16_t)),
                        indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::PrepareVisible() {

        bool changed = false;
        auto & vVisibleRenderables = pManager->GetVisible(changed);

        if (!changed) { return; }

        vRenderCommands.clear();

        for (auto * pRenderable : vVisibleRenderables) {
                std::visit([this](auto * pRenderableComponent) {
                                auto & vComponentRenderCommands = pRenderableComponent->GetRenderCommands();
                                        for (auto & oRenderCommand : vComponentRenderCommands) {
                                                vRenderCommands.emplace_back(&oRenderCommand);
                                        }
                                },
                                *pRenderable);
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

        gs.SetShaderProgram(pSSAOShader);

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

        gs.SetShaderProgram(pSSAOBlurShader);

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
void DeferredRenderer<TVisibilityManager>::AmbientDirPass() {

        oHdrFBO.Bind();

        auto & gs = GetSystem<GraphicsState>();
        gs.Clear(ClearBuffer::COLOR);
        gs.SetBlend(false);
        gs.SetDepthMask(false);
        gs.SetDepthTest(false);

        gs.SetShaderProgram(pAmbientDirShader);

        gs.SetTexture(TextureUnit::DIFFUSE,  oGBuffer.GetAlbedoRoughnessTex());
        gs.SetTexture(TextureUnit::NORMAL,   oGBuffer.GetNormalMetallicTex());
        gs.SetTexture(TextureUnit::EMISSIVE, oGBuffer.GetEmissiveTex());
        gs.SetTexture(TextureUnit::BUFFER,   oGBuffer.GetDepthTex());

        if (ssao_enabled) {
                gs.SetTexture(TextureUnit::SSAO_TEX, oSSAOBuffer.GetBlurTex());
        }

        static StrID inv_vp_id("InvVPMatrix");
        static StrID cam_pos_id("CamPos");
        static StrID dir_dir_id("DirLightDir");
        static StrID dir_col_id("DirLightColor");
        static StrID dir_int_id("DirLightIntensity");

        const glm::mat4 & vp     = pCamera->GetWorldMVP();
        glm::mat4         inv_vp = glm::inverse(vp);
        glm::vec3         cam_pos = pCamera->GetWorldPos();

        gs.SetVariable(inv_vp_id, inv_vp);
        gs.SetVariable(cam_pos_id, cam_pos);

        pLightingBlock->SetVariable(dir_dir_id, oDirLight.direction);
        pLightingBlock->SetVariable(dir_col_id, oDirLight.color);
        pLightingBlock->SetVariable(dir_int_id, oDirLight.intensity);
        pLightingBlock->Apply();

        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        gs.SetDepthMask(true);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::PointLightPass() {

        if (vPointLights.empty()) { return; }

        // HDR FBO still bound from AmbientDirPass.
        auto & gs = GetSystem<GraphicsState>();
        gs.SetBlend(true);
        gs.SetBlendFunc(BlendFactor::ONE, BlendFactor::ONE);
        gs.SetDepthMask(false);
        gs.SetDepthTest(true);
        gs.SetDepthFunc(DepthFunc::GEQUAL);
        gs.SetCullFace(true, CullFace::FRONT);

        gs.SetShaderProgram(pPointLightShader);

        gs.SetTexture(TextureUnit::DIFFUSE, oGBuffer.GetAlbedoRoughnessTex());
        gs.SetTexture(TextureUnit::NORMAL,  oGBuffer.GetNormalMetallicTex());
        gs.SetTexture(TextureUnit::BUFFER,  oGBuffer.GetDepthTex());

        static StrID inv_vp_id("InvVPMatrix");
        static StrID vp_id("VPMatrix");
        static StrID cam_pos_id("CamPos");
        static StrID light_pos_id("LightPos");
        static StrID light_radius_id("LightRadius");
        static StrID light_color_id("LightColor");
        static StrID light_intensity_id("LightIntensity");

        const glm::mat4 & vp     = pCamera->GetWorldMVP();
        glm::mat4         inv_vp = glm::inverse(vp);
        glm::vec3         cam_pos = pCamera->GetWorldPos();

        gs.SetVariable(inv_vp_id, inv_vp);
        gs.SetVariable(vp_id, vp);
        gs.SetVariable(cam_pos_id, cam_pos);

        glBindVertexArray(sphere_vao);

        //THINK setup shader variables for each light.. too slow
        for (const auto & light : vPointLights) {
                gs.SetVariable(light_pos_id,       light.position);
                gs.SetVariable(light_radius_id,    light.radius);
                gs.SetVariable(light_color_id,     light.color);
                gs.SetVariable(light_intensity_id, light.intensity);

                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sphere_index_count),
                                GL_UNSIGNED_SHORT, nullptr);
        }

        glBindVertexArray(0);

        // Restore state
        gs.SetBlend(false);
        gs.SetDepthFunc(DepthFunc::LESS);
        gs.SetCullFace(false);
        gs.SetDepthMask(true);
        gs.SetDepthTest(true);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::ToneMapPass() {

        oHdrFBO.Unbind();

        auto & gs = GetSystem<GraphicsState>();
        gs.SetViewport(0, 0,
                static_cast<int32_t>(screen_size.x), static_cast<int32_t>(screen_size.y));
        gs.Clear(ClearBuffer::COLOR);
        gs.SetDepthTest(false);
        gs.SetDepthMask(false);
        gs.SetBlend(false);

        gs.SetShaderProgram(pToneMapShader);

        gs.SetTexture(TextureUnit::HDR, pHdrTex);

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

        if (ssao_enabled) {
                SSAOPass();
                SSAOBlurPass();
        }

        AmbientDirPass();
        PointLightPass();
        ToneMapPass();
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
        oSSAOBuffer.Resize(new_size);

        if (pCamera) {
                pCamera->UpdateDimension(new_size);
        }
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::SetCamera(Camera * pCurCamera) {

        pCamera = pCurCamera;
        if (pCamera) {
                pCamera->UpdateDimension(screen_size);
        }
}

template <class TVisibilityManager>
Camera * DeferredRenderer<TVisibilityManager>::GetCamera() const {

        return pCamera;
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

} // namespace SE
