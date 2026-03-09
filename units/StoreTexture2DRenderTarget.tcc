
namespace SE {

StoreTexture2DRenderTarget::StoreTexture2DRenderTarget(const Settings & oNewSettings)
        : oSettings(oNewSettings) {}

ret_code_t StoreTexture2DRenderTarget::Store(const TextureStock & oTextureStock, uint32_t & id, uint32_t & gl_type) {

        if (!oTextureStock.width || !oTextureStock.height) {
                log_e("wrong render target dimensions: width={}, height={}", oTextureStock.width, oTextureStock.height);
                return uWRONG_INPUT_DATA;
        }

        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, oSettings.wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, oSettings.wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, oSettings.min_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, oSettings.mag_filter);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     oTextureStock.internal_format,
                     oTextureStock.width,
                     oTextureStock.height,
                     0,
                     oTextureStock.format,
                     oSettings.type,
                     nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);
        gl_type = GL_TEXTURE_2D;
        return uSUCCESS;
}

} // namespace SE
