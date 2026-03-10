
#include <random>

namespace SE {

void SSAOBuffer::CreateNoiseTex() {

        // 4×4 = 16 random tangent-space rotation vectors, packed as RG floats.
        // Fixed seed for deterministic results.
        std::mt19937 rng(12345u);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        float noise[32]; // 16 vec2 entries
        for (auto & v : noise) { v = dist(rng); }

        TextureStock ts { nullptr, 0, GL_RG, GL_RG16F, 4, 4 };
        pNoiseTex = CreateResource<TTexture>("ssao/noise", ts,
                StoreTexture2DRenderTarget::Settings(GL_FLOAT, GL_NEAREST, GL_NEAREST, GL_REPEAT));

        // Upload noise data (StoreTexture2DRenderTarget allocates empty texture, so sub-upload)
        glBindTexture(GL_TEXTURE_2D, pNoiseTex->GetID());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RG, GL_FLOAT, noise);
        glBindTexture(GL_TEXTURE_2D, 0);
}

void SSAOBuffer::Create() {

        TextureStock ts { nullptr, 0, GL_RED, GL_R8, size.x, size.y };

        pSSAOTex = CreateResource<TTexture>("ssao/raw", ts,
                StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_BYTE));
        pBlurTex = CreateResource<TTexture>("ssao/blur", ts,
                StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_BYTE));

        // SSAO FBO
        glGenFramebuffers(1, &fbo_ssao);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_ssao);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pSSAOTex->GetID(), 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                log_e("SSAO FBO incomplete");
        }

        // Blur FBO
        glGenFramebuffers(1, &fbo_blur);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_blur);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pBlurTex->GetID(), 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                log_e("SSAO blur FBO incomplete");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAOBuffer::Destroy() noexcept {

        if (fbo_ssao) { glDeleteFramebuffers(1, &fbo_ssao); fbo_ssao = 0; }
        if (fbo_blur)  { glDeleteFramebuffers(1, &fbo_blur);  fbo_blur  = 0; }

        auto & rm = TResourceManager::Instance();
        if (pSSAOTex) { rm.Destroy<TTexture>(pSSAOTex->RID()); pSSAOTex = nullptr; }
        if (pBlurTex) { rm.Destroy<TTexture>(pBlurTex->RID()); pBlurTex = nullptr; }
}

SSAOBuffer::~SSAOBuffer() noexcept {

        Destroy();
        if (pNoiseTex) {
                TResourceManager::Instance().Destroy<TTexture>(pNoiseTex->RID());
                pNoiseTex = nullptr;
        }
}

void SSAOBuffer::Resize(glm::uvec2 new_size) {

        size = new_size;
        Destroy();
        if (!pNoiseTex) { CreateNoiseTex(); }
        Create();
}

void SSAOBuffer::Bind() {

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_ssao);
        glViewport(0, 0, size.x, size.y);
}

void SSAOBuffer::BindBlur() {

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_blur);
        glViewport(0, 0, size.x, size.y);
}

void SSAOBuffer::Unbind() {

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


} // namespace SE
