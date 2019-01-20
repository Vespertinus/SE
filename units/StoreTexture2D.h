
#ifndef __STORE_TEXTURE_2D_H__
#define __STORE_TEXTURE_2D_H__ 1

namespace SE {

class StoreTexture2D {

        public:
        struct Settings {
                int32_t wrap;
                int32_t min_filter,
                        mag_filter;
                bool    mipmap_enabled;

                Settings(bool    mipmap          = true,
                         int32_t wrap_option     = GL_CLAMP_TO_EDGE) :
                        wrap(wrap_option),
                        min_filter((mipmap) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR),
                        mag_filter(GL_LINEAR),
                        mipmap_enabled(mipmap) { ;; }
                Settings(bool    mipmap,
                         int32_t wrap_option,
                         int32_t min,
                         int32_t mag) :
                        wrap(wrap_option),
                        min_filter(min),
                        mag_filter(mag),
                        mipmap_enabled(mipmap) { ;; }
        };

        typedef Settings  TChild;

        private:

        const Settings  & oSettings;
        static float max_anisotropic_filter;

        public:

        StoreTexture2D(const Settings & oNewSettings);
        ~StoreTexture2D() throw();
        ret_code_t Store(const TextureStock & oTextureStock, uint32_t & id, uint32_t & gl_type);

};

} //namespace SE
#endif
