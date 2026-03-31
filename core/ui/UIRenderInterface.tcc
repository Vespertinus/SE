#ifdef SE_UI_ENABLED

#include <glm/gtc/matrix_transform.hpp>
#include <RmlUi/Core/RenderInterface.h>

namespace SE {

// Per-geometry GPU data stored as a heap object; cast to/from CompiledGeometryHandle
struct UICompiledGeometry {
        uint32_t vao_id;
        uint32_t vbo_id;
        uint32_t ebo_id;
        uint32_t index_count;
};

UIRenderInterface::UIRenderInterface() {

        InitGLData();
        BuildNullTexture();
}

UIRenderInterface::~UIRenderInterface() {

        for (auto & h : vTextures) {
                if (h.IsValid()) UnlockResource(h);
        }
        if (hNullTexture.IsValid()) UnlockResource(hNullTexture);
}

void UIRenderInterface::InitGLData() {

        hShader = CreateResource<SE::ShaderProgram>(
                GetSystem<Config>().sResourceDir + "shader_program/rmlui.sesp");
}

void UIRenderInterface::BuildNullTexture() {

        // 1×1 opaque white pixel (RGBA, premultiplied — all channels 255)
        static const uint8_t pWhite[4] = { 255, 255, 255, 255 };
        hNullTexture = CreateResource<SE::TTexture>(
                "UIRmlNullTexture",
                TextureStock {
                        pWhite,
                        4,
                        GL_RGBA,
                        GL_RGBA8,
                        1,
                        1
                },
                StoreTexture2D::Settings(false));
        LockResource(hNullTexture);
}

void UIRenderInterface::SetOrtho(int width, int height) {

        // Standard 2D ortho: (0,0) = top-left, (w,h) = bottom-right
        mOrtho = glm::ortho(0.0f, static_cast<float>(width),
                            static_cast<float>(height), 0.0f,
                            -1.0f, 1.0f);
}

// --- Compiled geometry ---

Rml::CompiledGeometryHandle UIRenderInterface::CompileGeometry(
        Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int>         indices) {

        auto * pGeom = new UICompiledGeometry();
        pGeom->index_count = static_cast<uint32_t>(indices.size());

        glGenVertexArrays(1, &pGeom->vao_id);
        glGenBuffers(1, &pGeom->vbo_id);
        glGenBuffers(1, &pGeom->ebo_id);

        glBindVertexArray(pGeom->vao_id);

        glBindBuffer(GL_ARRAY_BUFFER, pGeom->vbo_id);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(Rml::Vertex)),
                     vertices.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pGeom->ebo_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(int)),
                     indices.data(),
                     GL_STATIC_DRAW);

        // Attribute layout matches mAttributeLocation in GLUtil.h:
        //   Position  → location 0,  vec2 float,          offset 0
        //   Color     → location 6,  vec4 ubyte (normalized), offset 8
        //   TexCoord0 → location 2,  vec2 float,          offset 12
        constexpr GLsizei stride = sizeof(Rml::Vertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT,         GL_FALSE, stride,
                              reinterpret_cast<const GLvoid *>(offsetof(Rml::Vertex, position)));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE,  stride,
                              reinterpret_cast<const GLvoid *>(offsetof(Rml::Vertex, colour)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT,         GL_FALSE, stride,
                              reinterpret_cast<const GLvoid *>(offsetof(Rml::Vertex, tex_coord)));

        glBindVertexArray(0);

        return reinterpret_cast<Rml::CompiledGeometryHandle>(pGeom);
}

void UIRenderInterface::RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f               translation,
        Rml::TextureHandle          texture) {

        auto * pGeom = reinterpret_cast<UICompiledGeometry *>(geometry);

        // Bake translation into MVP — use element transform if active
        const glm::mat4 & base = has_transform ? mTransform : mOrtho;
        const glm::mat4 mMVP = glm::translate(
                base, glm::vec3(translation.x, translation.y, 0.0f));

        auto & oGS = GetSystem<GraphicsState>();
        oGS.SetShaderProgram(GetResource(hShader));

        static StrID mat_id("MVPMatrix");
        oGS.SetVariable(mat_id, mMVP);

        // Resolve texture: 0 means untextured → use null (white) texture
        TTexture * pTex = nullptr;
        if (texture != 0) {
                const size_t idx = static_cast<size_t>(texture) - 1;
                if (idx < vTextures.size() && vTextures[idx].IsValid())
                        pTex = GetResource(vTextures[idx]);
        }
        if (!pTex)
                pTex = GetResource(hNullTexture);

        oGS.SetTexture(SE::TextureUnit::DIFFUSE, pTex);
        oGS.Draw(pGeom->vao_id, GL_TRIANGLES, VertexIndexType::INT, 0, pGeom->index_count);
}

void UIRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {

        if (!geometry) return;
        auto * pGeom = reinterpret_cast<UICompiledGeometry *>(geometry);
        glDeleteBuffers(1, &pGeom->vbo_id);
        glDeleteBuffers(1, &pGeom->ebo_id);
        glDeleteVertexArrays(1, &pGeom->vao_id);
        delete pGeom;
}

// --- Textures ---

Rml::TextureHandle UIRenderInterface::LoadTexture(
        Rml::Vector2i &    texture_dimensions,
        const Rml::String & source) {

        const std::string path = GetSystem<Config>().sResourceDir + std::string(source);
        H<TTexture> h = CreateResource<TTexture>(path, StoreTexture2D::Settings(false));
        if (!h.IsValid()) {
                log_w("UIRenderInterface: failed to load texture '{}'", path);
                return 0;
        }
        TTexture * pTex = GetResource(h);
        if (pTex) {
                const auto & dim = pTex->GetDimensions();
                texture_dimensions = { static_cast<int>(dim.first), static_cast<int>(dim.second) };
        }
        LockResource(h);
        vTextures.push_back(h);
        return static_cast<Rml::TextureHandle>(vTextures.size()); // 1-based
}

Rml::TextureHandle UIRenderInterface::GenerateTexture(
        Rml::Span<const Rml::byte> source,
        Rml::Vector2i              source_dimensions) {

        const auto name = "UIRmlGenTex_" + std::to_string(vTextures.size());
        H<TTexture> h = CreateResource<TTexture>(
                name,
                TextureStock {
                        source.data(),
                        static_cast<uint32_t>(source.size()),
                        GL_RGBA,
                        GL_RGBA8,
                        static_cast<uint16_t>(source_dimensions.x),
                        static_cast<uint16_t>(source_dimensions.y)
                },
                StoreTexture2D::Settings(false));
        if (!h.IsValid()) {
                log_w("UIRenderInterface: GenerateTexture failed ({}×{})",
                      source_dimensions.x, source_dimensions.y);
                return 0;
        }
        LockResource(h);
        vTextures.push_back(h);
        return static_cast<Rml::TextureHandle>(vTextures.size()); // 1-based
}

void UIRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {

        if (!texture) return;
        const size_t idx = static_cast<size_t>(texture) - 1;
        if (idx >= vTextures.size()) return;
        if (vTextures[idx].IsValid()) {
                UnlockResource(vTextures[idx]);
                DestroyResource(vTextures[idx]);
                vTextures[idx] = H<TTexture>{};
        }
}

// --- Scissor ---

void UIRenderInterface::EnableScissorRegion(bool enable) {

        if (enable)
                glEnable(GL_SCISSOR_TEST);
        else
                glDisable(GL_SCISSOR_TEST);
}

void UIRenderInterface::SetScissorRegion(Rml::Rectanglei region) {

        const auto & screen_size = GetSystem<GraphicsState>().GetScreenSize();
        const int fb_height = static_cast<int>(screen_size.y);
        // OpenGL scissor origin is bottom-left; RmlUi uses top-left
        glScissor(region.Left(),
                  fb_height - region.Bottom(),
                  region.Width(),
                  region.Height());
}

// --- Stencil clip mask ---

void UIRenderInterface::EnableClipMask(bool enable) {

        if (enable)
                glEnable(GL_STENCIL_TEST);
        else
                glDisable(GL_STENCIL_TEST);
}

void UIRenderInterface::RenderToClipMask(Rml::ClipMaskOperation       operation,
                                          Rml::CompiledGeometryHandle   geometry,
                                          Rml::Vector2f                 translation) {

        using Rml::ClipMaskOperation;

        GLint stencil_write_value = 1;
        GLint stencil_test_value  = 1;

        switch (operation) {
                case ClipMaskOperation::Set: {
                        glClear(GL_STENCIL_BUFFER_BIT);
                        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                        break;
                }
                case ClipMaskOperation::SetInverse: {
                        // Fill stencil with 1, then overwrite with 0 where geometry is
                        glClearStencil(1);
                        glClear(GL_STENCIL_BUFFER_BIT);
                        glClearStencil(0);
                        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                        stencil_write_value = 0;
                        break;
                }
                case ClipMaskOperation::Intersect: {
                        // Intersect with existing mask by incrementing; test at new level
                        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
                        glGetIntegerv(GL_STENCIL_REF, &stencil_test_value);
                        stencil_test_value += 1;
                        break;
                }
        }

        // Write to stencil only — suppress color output
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glStencilFunc(GL_ALWAYS, stencil_write_value, GLuint(-1));
        RenderGeometry(geometry, translation, {});

        // Restore color writes; subsequent geometry tests against the new mask
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_EQUAL, stencil_test_value, GLuint(-1));
}

// --- CSS transform ---

void UIRenderInterface::SetTransform(const Rml::Matrix4f * transform) {

        if (transform) {
                // Convert Rml::ColumnMajorMatrix4f to glm::mat4 column by column
                const auto c0 = transform->GetColumn(0);
                const auto c1 = transform->GetColumn(1);
                const auto c2 = transform->GetColumn(2);
                const auto c3 = transform->GetColumn(3);
                const glm::mat4 rml(
                        c0.x, c0.y, c0.z, c0.w,
                        c1.x, c1.y, c1.z, c1.w,
                        c2.x, c2.y, c2.z, c2.w,
                        c3.x, c3.y, c3.z, c3.w);
                mTransform    = mOrtho * rml;
                has_transform = true;
        } else {
                has_transform = false;
        }
}

} // namespace SE

#endif // SE_UI_ENABLED
