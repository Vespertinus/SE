
namespace SE {

void GBuffer::Create() {

        TextureStock oTexStock { nullptr, 0, 0, 0, size.x, size.y };

        // RT0: albedo (RGB) + roughness (A) — RGBA8
        oTexStock.internal_format = GL_RGBA8;
        oTexStock.format          = GL_RGBA;
        pRT0 = CreateResource<TTexture>("gbuffer/rt0", oTexStock,
                        StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_BYTE));

        // RT1: oct-normal (RG) + metallic (B) + AO (A) — RGBA16F
        oTexStock.internal_format = GL_RGBA16F;
        oTexStock.format          = GL_RGBA;
        pRT1 = CreateResource<TTexture>("gbuffer/rt1", oTexStock,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT));

        // RT2: emissive (RGB) — R11F_G11F_B10F
        oTexStock.internal_format = GL_R11F_G11F_B10F;
        oTexStock.format          = GL_RGB;
        pRT2 = CreateResource<TTexture>("gbuffer/rt2", oTexStock,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT));

        // Depth + stencil — DEPTH24_STENCIL8
        oTexStock.internal_format = GL_DEPTH24_STENCIL8;
        oTexStock.format          = GL_DEPTH_STENCIL;
        pDepth = CreateResource<TTexture>("gbuffer/depth", oTexStock,
                        StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_INT_24_8));

        glGenFramebuffers(1, &fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,      GL_TEXTURE_2D, pRT0->GetID(),   0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,      GL_TEXTURE_2D, pRT1->GetID(),   0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,      GL_TEXTURE_2D, pRT2->GetID(),   0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, pDepth->GetID(), 0);

        GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
        glDrawBuffers(3, attachments);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
                log_e("GBuffer FBO incomplete, status: {:#x}", static_cast<uint32_t>(status));
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::Destroy() noexcept {

        if (fbo_id) { glDeleteFramebuffers(1, &fbo_id); fbo_id = 0; }

        auto & rm = TResourceManager::Instance();
        if (pRT0)   { rm.Destroy<TTexture>(pRT0->RID());   pRT0   = nullptr; }
        if (pRT1)   { rm.Destroy<TTexture>(pRT1->RID());   pRT1   = nullptr; }
        if (pRT2)   { rm.Destroy<TTexture>(pRT2->RID());   pRT2   = nullptr; }
        if (pDepth) { rm.Destroy<TTexture>(pDepth->RID()); pDepth = nullptr; }
}

GBuffer::~GBuffer() noexcept {

        Destroy();
}

void GBuffer::Init(glm::uvec2 screen_size) {

        size = screen_size;
        Create();
}

void GBuffer::Resize(glm::uvec2 new_size) {

        size = new_size;
        Destroy();
        Create();
}

void GBuffer::Bind() {

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
        glViewport(0, 0, size.x, size.y);
}

void GBuffer::Unbind() {

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace SE
