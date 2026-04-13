
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H 1

#include <initializer_list>

namespace SE {

class FrameBuffer {

        uint32_t fbo_id { 0 };

public:

        ~FrameBuffer() noexcept;

        void Create();
        void Destroy() noexcept;

        void AttachColor(uint32_t slot, TTexture * pTex);
        void AttachDepthStencil(TTexture * pTex);
        void AttachDepth(TTexture * pTex);
        void SetDrawBuffers(std::initializer_list<uint32_t> slots);
        bool CheckComplete() const;

        void Bind()   const;   // glBindFramebuffer(GL_FRAMEBUFFER, fbo_id)
        void Unbind() const;   // glBindFramebuffer(GL_FRAMEBUFFER, 0)

        uint32_t ID() const { return fbo_id; }
};

} // namespace SE

#endif
