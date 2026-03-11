
#ifndef GBUFFER_H
#define GBUFFER_H 1

#include <GLUtil.h>
#include <glm/vec2.hpp>
#include <FrameBuffer.h>

namespace SE {

class GBuffer {

        FrameBuffer oFBO;
        TTexture * pRT0   { nullptr };   // albedo (RGB) + roughness (A) — GL_RGBA8
        TTexture * pRT1   { nullptr };   // oct-normal (RG) + metallic (B) + AO (A) — GL_RGBA16F
        TTexture * pRT2   { nullptr };   // emissive (RGB) — GL_R11F_G11F_B10F
        TTexture * pDepth { nullptr };   // depth + stencil — GL_DEPTH24_STENCIL8
        glm::uvec2 size   { 0 };

        void Create();
        void Destroy() noexcept;

public:

        ~GBuffer() noexcept;

        void Init  (glm::uvec2 screen_size);
        void Resize(glm::uvec2 new_size);

        void Bind();    // bind as render target (geometry pass)
        void Unbind();  // bind FBO 0 back

        // Bind G-buffer textures (lighting pass).
        TTexture * GetAlbedoRoughnessTex() const { return pRT0; }
        TTexture * GetNormalMetallicTex()  const { return pRT1; }
        TTexture * GetEmissiveTex()        const { return pRT2; }
        TTexture * GetDepthTex()           const { return pDepth; }
};

} // namespace SE

#endif
