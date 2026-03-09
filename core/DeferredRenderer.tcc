
namespace SE {

// ---------------------------------------------------------------------------
// DeferredRenderer implementation
// ---------------------------------------------------------------------------

template <class TVisibilityManager>
DeferredRenderer<TVisibilityManager>::DeferredRenderer() :
        pManager(std::make_unique<TVisibilityManager>()) {

    vRenderCommands.reserve(1000);
    CreateQuad();

    //TODO constructor param for delete shader after program link
    pLightingShader = CreateResource<SE::ShaderProgram>(GetSystem<SE::Config>().sResourceDir + "shader_program/lighting_pass.sesp");
    pBlock = std::make_unique<UniformBlock>(pLightingShader, UniformUnitInfo::Type::LIGHTING);
}

template <class TVisibilityManager>
DeferredRenderer<TVisibilityManager>::~DeferredRenderer() noexcept {

    DestroyResource<ShaderProgram>(pLightingShader);

    if (quad_vao)      { glDeleteVertexArrays(1, &quad_vao); quad_vao      = 0; }
    if (quad_vbo)      { glDeleteBuffers(1, &quad_vbo);      quad_vbo      = 0; }
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

    // Position at attribute location 0 (matches GLUtil.h mAttributeLocation)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(0);

    // TexCoord0 at attribute location 2 (matches GLUtil.h mAttributeLocation)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(2);

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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GetSystem<GraphicsState>().SetViewProjection(pCamera->GetWorldMVP());

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
void DeferredRenderer<TVisibilityManager>::LightingPass() {

        auto & oGraphicsState = GetSystem<GraphicsState>();

        glClear(GL_COLOR_BUFFER_BIT);

        oGraphicsState.SetShaderProgram(pLightingShader);

        static StrID inv_vp_id("InvVPMatrix");
        static StrID cam_pos_id("CamPos");

        static StrID dir_light_dir_id("DirLightDir");
        static StrID dir_light_color_id("DirLightColor");
        static StrID dir_light_intensity_id("DirLightIntensity");

        static StrID point_lights_cnt_id("NumPointLights");
        static StrID pl_pos_id("PL_Pos");
        static StrID pl_radius_id("PL_Radius");
        static StrID pl_color_id("PL_Color");
        static StrID pl_intensity_id("PL_Intensity");

        // Inverse view-projection for world-position reconstruction from depth
        const glm::mat4 & vp     = pCamera->GetWorldMVP();
        glm::mat4         inv_vp = glm::inverse(vp);
        glm::vec3         cam_pos = pCamera->GetWorldPos();

        ret_code_t res;

        // Bind G-buffer textures
        oGraphicsState.SetTexture(TextureUnit::DIFFUSE, oGBuffer.GetAlbedoRoughnessTex());
        oGraphicsState.SetTexture(TextureUnit::NORMAL,  oGBuffer.GetNormalMetallicTex());
        oGraphicsState.SetTexture(TextureUnit::BUFFER,  oGBuffer.GetDepthTex());


        // Camera uniforms
        res = oGraphicsState.SetVariable(inv_vp_id, inv_vp);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set variable: '{}', shader: '{}'",
                                                "InvVPMatrix",
                                                pLightingShader->Name()
                                                )));
        }
        res = oGraphicsState.SetVariable(cam_pos_id, cam_pos);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set variable: '{}', shader: '{}'",
                                                "CamPos",
                                                pLightingShader->Name()
                                                )));
        }

        // Directional light
        res = pBlock->SetVariable(dir_light_dir_id, oDirLight.direction);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set variable: '{}', shader: '{}'",
                                                "DirLightDir",
                                                pLightingShader->Name()
                                                )));
        }
        res = pBlock->SetVariable(dir_light_color_id, oDirLight.color);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set variable: '{}', shader: '{}'",
                                                "DirLightColor",
                                                pLightingShader->Name()
                                                )));
        }
        res = pBlock->SetVariable(dir_light_intensity_id, oDirLight.intensity);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set variable: '{}', shader: '{}'",
                                                "DirLightIntensity",
                                                pLightingShader->Name()
                                                )));
        }

        // Point lights
        int num_pl = static_cast<int>(vPointLights.size());
        if (num_pl > 16) { num_pl = 16; }
        res = pBlock->SetVariable(point_lights_cnt_id, num_pl);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set variable: '{}', shader: '{}'",
                                                "NumPointLights",
                                                pLightingShader->Name()
                                                )));
        }


        for (int i = 0; i < num_pl; ++i) {

                res = pBlock->SetArrayElement(pl_pos_id, i, vPointLights[i].position);
                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format("failed to set array variable: '{}', shader: '{}'",
                                                        pl_pos_id,
                                                        pLightingShader->Name()
                                                        )));
                }
                res = pBlock->SetArrayElement(pl_radius_id, i, vPointLights[i].radius);
                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format("failed to set array variable: '{}', shader: '{}'",
                                                        pl_radius_id,
                                                        pLightingShader->Name()
                                                        )));
                }
                res = pBlock->SetArrayElement(pl_color_id, i, vPointLights[i].color);
                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format("failed to set array variable: '{}', shader: '{}'",
                                                        pl_color_id,
                                                        pLightingShader->Name()
                                                        )));
                }
                res = pBlock->SetArrayElement(pl_intensity_id, i, vPointLights[i].intensity);
                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format("failed to set array variable: '{}', shader: '{}'",
                                                        pl_intensity_id,
                                                        pLightingShader->Name()
                                                        )));
                }
        }

        pBlock->Apply();

        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glUseProgram(0);
}

template <class TVisibilityManager>
void DeferredRenderer<TVisibilityManager>::Render() {

    if (!pCamera) {
        log_e("deferred renderer: main camera was not set");
        return;
    }

    PrepareVisible();
    GeometryPass();
    LightingPass();
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
