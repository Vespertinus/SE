
#ifndef __STORE_TEXTURE_2D_H__
#define __STORE_TEXTURE_2D_H__ 1

namespace SE {

class StoreTexture2D {

        public:
        struct Settings {
                int32_t wrap;
                int32_t min_filter,
                        mag_filter;
                int32_t apply_method;
                bool    mipmap_enabled;

                Settings(bool mipmap_on = true) :
                        wrap(GL_CLAMP_TO_EDGE),
                        min_filter((mipmap_on) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR),
                        mag_filter(GL_LINEAR),
                        apply_method(GL_MODULATE),
                        mipmap_enabled(mipmap_on)
                { ;; }
        };

        typedef Settings  TChild;

        private:

        const Settings  & oSettings;
        static float max_anisotropic_filter;

        public:

        StoreTexture2D(const Settings & oNewSettings);
        ~StoreTexture2D() throw();
        ret_code_t Store(TextureStock & oTextureStock, uint32_t & id);

};

} //namespace SE

#ifdef SE_IMPL
#include <StoreTexture2D.tcc>
#endif

#endif
