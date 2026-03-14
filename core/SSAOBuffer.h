
#ifndef SSAO_BUFFER_H
#define SSAO_BUFFER_H 1

#include <GLUtil.h>
#include <glm/vec2.hpp>
#include <FrameBuffer.h>

namespace SE {

class SSAOBuffer {

        FrameBuffer      oFboSSAO;
        FrameBuffer      oFboBlur;
        H<TTexture> hSSAOTex;   // GL_R8
        H<TTexture> hBlurTex;   // GL_R8
        H<TTexture> hNoiseTex;  // 4×4 GL_RG16F (random rotations, GL_REPEAT)
        glm::uvec2       size      { 0 };

        void Create();
        void Destroy() noexcept;
        void CreateNoiseTex();

public:

        ~SSAOBuffer() noexcept;

        void Resize(glm::uvec2 new_size);

        void Bind();     // bind fbo_ssao for SSAO pass output
        void BindBlur(); // bind fbo_blur for blur pass output
        void Unbind();

        TTexture * GetSSAOTex()  const;
        TTexture * GetBlurTex()  const;
        TTexture * GetNoiseTex() const;
};

} // namespace SE

#endif
