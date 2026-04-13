
namespace SE {

FrameBuffer::~FrameBuffer() noexcept {

        Destroy();
}

void FrameBuffer::Create() {

        glGenFramebuffers(1, &fbo_id);
}

void FrameBuffer::Destroy() noexcept {

        if (fbo_id) {
                glDeleteFramebuffers(1, &fbo_id);
                fbo_id = 0;
        }
}

void FrameBuffer::AttachColor(uint32_t slot, TTexture * pTex) {

        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0 + slot,
                               GL_TEXTURE_2D,
                               pTex->GetID(),
                               0);
}

void FrameBuffer::AttachDepthStencil(TTexture * pTex) {

        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D,
                               pTex->GetID(),
                               0);
}

void FrameBuffer::AttachDepth(TTexture * pTex) {

        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D,
                               pTex->GetID(),
                               0);
}

void FrameBuffer::SetDrawBuffers(std::initializer_list<uint32_t> slots) {

        std::vector<GLenum> attachments;
        attachments.reserve(slots.size());
        for (uint32_t slot : slots) {
                attachments.push_back(GL_COLOR_ATTACHMENT0 + slot);
        }
        glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data());
}

bool FrameBuffer::CheckComplete() const {

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
                log_e("FBO incomplete, status: {:#x}", static_cast<uint32_t>(status));
                return false;
        }
        return true;
}

void FrameBuffer::Bind() const {

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
}

void FrameBuffer::Unbind() const {

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace SE
