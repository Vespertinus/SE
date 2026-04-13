
namespace SE {

void ShadowBuffer::Create() {

        TextureStock ts { nullptr, 0, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT24,
                          SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
        hDepthTex = CreateResource<TTexture>("shadow/depth", ts,
                StoreTexture2DRenderTarget::Settings(GL_FLOAT, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_BORDER));

        // White border → outside shadow frustum = fully lit
        auto * pTex = GetResource(hDepthTex);
        glBindTexture(GL_TEXTURE_2D, pTex->GetID());
        const float border[] = { 1.f, 1.f, 1.f, 1.f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glBindTexture(GL_TEXTURE_2D, 0);

        oFbo.Create();
        oFbo.Bind();
        oFbo.AttachDepth(GetResource(hDepthTex));
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        oFbo.CheckComplete();
        oFbo.Unbind();
}

void ShadowBuffer::Destroy() noexcept {

        oFbo.Destroy();
        if (hDepthTex.IsValid()) {
                TResourceManager::Instance().Destroy(hDepthTex);
                hDepthTex = H<TTexture>::Null();
        }
}

ShadowBuffer::~ShadowBuffer() noexcept { Destroy(); }

void ShadowBuffer::Init() { Create(); }

void ShadowBuffer::Bind() {

        oFbo.Bind();
        GetSystem<GraphicsState>().SetViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
}

void ShadowBuffer::Unbind() { oFbo.Unbind(); }

TTexture * ShadowBuffer::GetDepthTex() const { return GetResource(hDepthTex); }

} // namespace SE
