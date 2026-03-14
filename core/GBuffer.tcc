
namespace SE {

void GBuffer::Create() {

        TextureStock oTexStock { nullptr, 0, 0, 0, size.x, size.y };

        // RT0: albedo (RGB) + roughness (A) — RGBA8
        oTexStock.internal_format = GL_RGBA8;
        oTexStock.format          = GL_RGBA;
        hRT0 = CreateResource<TTexture>("gbuffer/rt0", oTexStock,
                        StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_BYTE));

        // RT1: oct-normal (RG) + metallic (B) + AO (A) — RGBA16F
        oTexStock.internal_format = GL_RGBA16F;
        oTexStock.format          = GL_RGBA;
        hRT1 = CreateResource<TTexture>("gbuffer/rt1", oTexStock,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT));

        // RT2: emissive (RGB) — R11F_G11F_B10F
        oTexStock.internal_format = GL_R11F_G11F_B10F;
        oTexStock.format          = GL_RGB;
        hRT2 = CreateResource<TTexture>("gbuffer/rt2", oTexStock,
                        StoreTexture2DRenderTarget::Settings(GL_FLOAT));

        // Depth + stencil — DEPTH24_STENCIL8
        oTexStock.internal_format = GL_DEPTH24_STENCIL8;
        oTexStock.format          = GL_DEPTH_STENCIL;
        hDepth = CreateResource<TTexture>("gbuffer/depth", oTexStock,
                        StoreTexture2DRenderTarget::Settings(GL_UNSIGNED_INT_24_8));

        oFBO.Create();
        oFBO.Bind();
        oFBO.AttachColor(0, GetResource(hRT0));
        oFBO.AttachColor(1, GetResource(hRT1));
        oFBO.AttachColor(2, GetResource(hRT2));
        oFBO.AttachDepthStencil(GetResource(hDepth));
        oFBO.SetDrawBuffers({0, 1, 2});
        oFBO.CheckComplete();
        oFBO.Unbind();
}

void GBuffer::Destroy() noexcept {

        oFBO.Destroy();

        auto & rm = TResourceManager::Instance();
        if (hRT0.IsValid())   { rm.Destroy(hRT0);   hRT0   = H<TTexture>::Null(); }
        if (hRT1.IsValid())   { rm.Destroy(hRT1);   hRT1   = H<TTexture>::Null(); }
        if (hRT2.IsValid())   { rm.Destroy(hRT2);   hRT2   = H<TTexture>::Null(); }
        if (hDepth.IsValid()) { rm.Destroy(hDepth); hDepth = H<TTexture>::Null(); }
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

        oFBO.Bind();
        GetSystem<GraphicsState>().SetViewport(0, 0,
                static_cast<int32_t>(size.x), static_cast<int32_t>(size.y));
}

void GBuffer::Unbind() {

        oFBO.Unbind();
}

TTexture * GBuffer::GetAlbedoRoughnessTex() const { return GetResource(hRT0); }
TTexture * GBuffer::GetNormalMetallicTex()  const { return GetResource(hRT1); }
TTexture * GBuffer::GetEmissiveTex()        const { return GetResource(hRT2); }
TTexture * GBuffer::GetDepthTex()           const { return GetResource(hDepth); }

} // namespace SE
