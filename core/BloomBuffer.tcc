
namespace SE {

glm::uvec2 BloomBuffer::MipSize(int level) const {

        return glm::max(size >> static_cast<uint32_t>(level), glm::uvec2(1u));
}

void BloomBuffer::Create() {

        for (int i = 0; i < kMipLevels; ++i) {
                glm::uvec2 s = MipSize(i);
                TextureStock ts { nullptr, 0, GL_RGB, GL_RGB16F, s.x, s.y };
                hDownTex[i] = CreateResource<TTexture>(
                        fmt::format("bloom/down_{}", i), ts,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT, GL_LINEAR, GL_LINEAR));
                oFboDown[i].Create();
                oFboDown[i].Bind();
                oFboDown[i].AttachColor(0, GetResource(hDownTex[i]));
                oFboDown[i].CheckComplete();
        }

        for (int i = 0; i < kMipLevels - 1; ++i) {
                glm::uvec2 s = MipSize(i);
                TextureStock ts { nullptr, 0, GL_RGB, GL_RGB16F, s.x, s.y };
                hUpTex[i] = CreateResource<TTexture>(
                        fmt::format("bloom/up_{}", i), ts,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT, GL_LINEAR, GL_LINEAR));
                oFboUp[i].Create();
                oFboUp[i].Bind();
                oFboUp[i].AttachColor(0, GetResource(hUpTex[i]));
                oFboUp[i].CheckComplete();
        }

        oFboUp[kMipLevels - 2].Unbind();
        initialized = true;
}

void BloomBuffer::Destroy() noexcept {

        auto & rm = TResourceManager::Instance();

        for (int i = 0; i < kMipLevels; ++i) {
                oFboDown[i].Destroy();
                if (hDownTex[i].IsValid()) { rm.Destroy(hDownTex[i]); hDownTex[i] = H<TTexture>::Null(); }
        }
        for (int i = 0; i < kMipLevels - 1; ++i) {
                oFboUp[i].Destroy();
                if (hUpTex[i].IsValid()) { rm.Destroy(hUpTex[i]); hUpTex[i] = H<TTexture>::Null(); }
        }

        initialized = false;
}

BloomBuffer::~BloomBuffer() noexcept { Destroy(); }

void BloomBuffer::Resize(glm::uvec2 new_size) {

        size = new_size >> 1u;   // level 0 is half the screen
        Destroy();
        Create();
}

void BloomBuffer::BindDown(int level) {

        oFboDown[level].Bind();
        glm::uvec2 s = MipSize(level);
        GetSystem<GraphicsState>().SetViewport(0, 0,
                static_cast<int32_t>(s.x), static_cast<int32_t>(s.y));
}

void BloomBuffer::BindUp(int level) {

        oFboUp[level].Bind();
        glm::uvec2 s = MipSize(level);
        GetSystem<GraphicsState>().SetViewport(0, 0,
                static_cast<int32_t>(s.x), static_cast<int32_t>(s.y));
}

void BloomBuffer::Unbind() { oFboDown[0].Unbind(); }

TTexture * BloomBuffer::GetDownTex(int level) const { return GetResource(hDownTex[level]); }
TTexture * BloomBuffer::GetUpTex  (int level) const { return GetResource(hUpTex[level]);   }

} // namespace SE
