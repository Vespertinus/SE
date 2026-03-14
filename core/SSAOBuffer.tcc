
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
        hNoiseTex = CreateResource<TTexture>("ssao/noise", ts,
                StoreTexture2DRenderTarget::Settings(GL_FLOAT, GL_NEAREST, GL_NEAREST, GL_REPEAT));

        // Upload noise data (StoreTexture2DRenderTarget allocates empty texture, so sub-upload)
        auto * pNoiseTex = GetResource(hNoiseTex);
        glBindTexture(GL_TEXTURE_2D, pNoiseTex->GetID());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RG, GL_FLOAT, noise);
        glBindTexture(GL_TEXTURE_2D, 0);
}

void SSAOBuffer::Create() {

        TextureStock ts { nullptr, 0, GL_RED, GL_R8, size.x, size.y };

        hSSAOTex = CreateResource<TTexture>("ssao/raw", ts,
                StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_BYTE));
        hBlurTex = CreateResource<TTexture>("ssao/blur", ts,
                StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_BYTE));

        // SSAO FBO
        oFboSSAO.Create();
        oFboSSAO.Bind();
        oFboSSAO.AttachColor(0, GetResource(hSSAOTex));
        oFboSSAO.CheckComplete();

        // Blur FBO
        oFboBlur.Create();
        oFboBlur.Bind();
        oFboBlur.AttachColor(0, GetResource(hBlurTex));
        oFboBlur.CheckComplete();

        oFboBlur.Unbind();
}

void SSAOBuffer::Destroy() noexcept {

        oFboSSAO.Destroy();
        oFboBlur.Destroy();

        auto & rm = TResourceManager::Instance();
        if (hSSAOTex.IsValid()) { rm.Destroy(hSSAOTex); hSSAOTex = H<TTexture>::Null(); }
        if (hBlurTex.IsValid()) { rm.Destroy(hBlurTex); hBlurTex = H<TTexture>::Null(); }
}

SSAOBuffer::~SSAOBuffer() noexcept {

        Destroy();
        if (hNoiseTex.IsValid()) {
                TResourceManager::Instance().Destroy(hNoiseTex);
                hNoiseTex = H<TTexture>::Null();
        }
}

void SSAOBuffer::Resize(glm::uvec2 new_size) {

        size = new_size;
        Destroy();
        if (!hNoiseTex.IsValid()) { CreateNoiseTex(); }
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

TTexture * SSAOBuffer::GetSSAOTex()  const { return GetResource(hSSAOTex); }
TTexture * SSAOBuffer::GetBlurTex()  const { return GetResource(hBlurTex); }
TTexture * SSAOBuffer::GetNoiseTex() const { return GetResource(hNoiseTex); }

} // namespace SE
