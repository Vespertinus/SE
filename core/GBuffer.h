
#ifndef GBUFFER_H
#define GBUFFER_H 1

#include <GLUtil.h>
#include <glm/vec2.hpp>

namespace SE {

class GBuffer {

        uint32_t   fbo_id { 0 };
        TTexture * pRT0   { nullptr };   // albedo (RGB) + roughness (A) — GL_RGBA8
        TTexture * pRT1   { nullptr };   // world-normal (RGB) + metallic (A) — GL_RGBA16F
        TTexture * pDepth { nullptr };   // GL_DEPTH_COMPONENT24
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
        TTexture * GetDepthTex()           const { return pDepth; }
};

} // namespace SE

#endif
