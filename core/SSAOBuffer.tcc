
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
        oFboSSAO.Create();
        oFboSSAO.Bind();
        oFboSSAO.AttachColor(0, pSSAOTex);
        oFboSSAO.CheckComplete();

        // Blur FBO
        oFboBlur.Create();
        oFboBlur.Bind();
        oFboBlur.AttachColor(0, pBlurTex);
        oFboBlur.CheckComplete();

        oFboBlur.Unbind();
}

void SSAOBuffer::Destroy() noexcept {

        oFboSSAO.Destroy();
        oFboBlur.Destroy();

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

        oFboSSAO.Bind();
        GetSystem<GraphicsState>().SetViewport(0, 0,
                static_cast<int32_t>(size.x), static_cast<int32_t>(size.y));
}

void SSAOBuffer::BindBlur() {

        oFboBlur.Bind();
        GetSystem<GraphicsState>().SetViewport(0, 0,
                static_cast<int32_t>(size.x), static_cast<int32_t>(size.y));
}

void SSAOBuffer::Unbind() {

        oFboSSAO.Unbind();
}


} // namespace SE
