
#ifndef SSAO_BUFFER_H
#define SSAO_BUFFER_H 1

#include <GLUtil.h>
#include <glm/vec2.hpp>

namespace SE {

class SSAOBuffer {

        uint32_t   fbo_ssao  { 0 };   // raw SSAO output FBO
        uint32_t   fbo_blur  { 0 };   // SSAO blurred output FBO
        TTexture * pSSAOTex  { nullptr };  // GL_R8
        TTexture * pBlurTex  { nullptr };  // GL_R8
        TTexture * pNoiseTex { nullptr };  // 4×4 GL_RG16F (random rotations, GL_REPEAT)
        glm::uvec2 size      { 0 };

        void Create();
        void Destroy() noexcept;
        void CreateNoiseTex();

public:

        ~SSAOBuffer() noexcept;

        void Resize(glm::uvec2 new_size);

        void Bind();     // bind fbo_ssao for SSAO pass output
        void BindBlur(); // bind fbo_blur for blur pass output
        void Unbind();

        TTexture * GetSSAOTex()  const { return pSSAOTex; }
        TTexture * GetBlurTex()  const { return pBlurTex; }
        TTexture * GetNoiseTex() const { return pNoiseTex; }
};

} // namespace SE

#endif
