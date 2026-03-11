
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

        oFBO.Create();
        oFBO.Bind();
        oFBO.AttachColor(0, pRT0);
        oFBO.AttachColor(1, pRT1);
        oFBO.AttachColor(2, pRT2);
        oFBO.AttachDepthStencil(pDepth);
        oFBO.SetDrawBuffers({0, 1, 2});
        oFBO.CheckComplete();
        oFBO.Unbind();
}

void GBuffer::Destroy() noexcept {

        oFBO.Destroy();

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

        oFBO.Bind();
        GetSystem<GraphicsState>().SetViewport(0, 0,
                static_cast<int32_t>(size.x), static_cast<int32_t>(size.y));
}

void GBuffer::Unbind() {

        oFBO.Unbind();
}

} // namespace SE
