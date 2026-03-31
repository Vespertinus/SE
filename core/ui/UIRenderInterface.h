#ifndef __UI_RENDER_INTERFACE_H__
#define __UI_RENDER_INTERFACE_H__ 1

#ifdef SE_UI_ENABLED

#include <vector>
#include <glm/mat4x4.hpp>
#include <RmlUi/Core/RenderInterface.h>
#include <ResourceHandle.h>

namespace SE {

class UIRenderInterface : public Rml::RenderInterface {

        H<ShaderProgram>           hShader;
        H<TTexture>                hNullTexture;      // 1×1 white fallback for untextured geometry
        std::vector<H<TTexture>>   vTextures;         // lifetime owners; handle = index+1
        glm::mat4                  mOrtho{1.0f};      // base ortho matrix, updated on resize
        glm::mat4                  mTransform{1.0f};  // ortho × current CSS transform
        bool                       has_transform{false};

        void InitGLData();
        void BuildNullTexture();

public:
        UIRenderInterface();
        ~UIRenderInterface();

        // Called by UISystem on window resize to update ortho matrix and context dimensions
        void SetOrtho(int width, int height);

        // --- Rml::RenderInterface (required) ---
        Rml::CompiledGeometryHandle CompileGeometry (Rml::Span<const Rml::Vertex> vertices,
                                                     Rml::Span<const int>         indices) override;
        void RenderGeometry  (Rml::CompiledGeometryHandle geometry,
                              Rml::Vector2f               translation,
                              Rml::TextureHandle          texture)  override;
        void ReleaseGeometry (Rml::CompiledGeometryHandle geometry) override;

        Rml::TextureHandle LoadTexture    (Rml::Vector2i &       texture_dimensions,
                                           const Rml::String &   source)           override;
        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                           Rml::Vector2i              source_dimensions) override;
        void               ReleaseTexture (Rml::TextureHandle texture)              override;

        void EnableScissorRegion(bool enable)           override;
        void SetScissorRegion   (Rml::Rectanglei region) override;

        // --- Rml::RenderInterface (optional) ---
        void EnableClipMask  (bool enable)                                override;
        void RenderToClipMask(Rml::ClipMaskOperation       operation,
                              Rml::CompiledGeometryHandle   geometry,
                              Rml::Vector2f                 translation)  override;
        void SetTransform    (const Rml::Matrix4f * transform)            override;
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_RENDER_INTERFACE_H__
