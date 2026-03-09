
#ifndef STORE_TEXTURE_2D_RENDER_TARGET_H
#define STORE_TEXTURE_2D_RENDER_TARGET_H 1

namespace SE {

// Store strategy that allocates an empty 2D texture for use as an FBO render
// target.  Unlike StoreTexture2D it does not require pixel data in TextureStock;
// internal_format, format, width, and height come from TextureStock while the
// data type and sampler parameters are supplied via Settings.
class StoreTexture2DRenderTarget {

        public:
        struct Settings {
                int32_t type;        // e.g. GL_UNSIGNED_BYTE, GL_FLOAT
                int32_t min_filter;  // default GL_NEAREST
                int32_t mag_filter;  // default GL_NEAREST
                int32_t wrap;        // default GL_CLAMP_TO_EDGE

                Settings(int32_t data_type,
                         int32_t min_f    = GL_NEAREST,
                         int32_t mag_f    = GL_NEAREST,
                         int32_t wrap_opt = GL_CLAMP_TO_EDGE)
                        : type(data_type),
                          min_filter(min_f),
                          mag_filter(mag_f),
                          wrap(wrap_opt) {}
        };

        typedef Settings TChild;

        private:

        const Settings & oSettings;

        public:

        StoreTexture2DRenderTarget(const Settings & oNewSettings);
        ret_code_t Store(const TextureStock & oTextureStock, uint32_t & id, uint32_t & gl_type);
};

} // namespace SE
#endif
