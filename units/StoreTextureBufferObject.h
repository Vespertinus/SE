
#ifndef __STORE_TEXTURE_BUFFER_OBJECT_H__
#define __STORE_TEXTURE_BUFFER_OBJECT_H__ 1

namespace SE {

class StoreTextureBufferObject {

        public:
        struct Settings { };

        typedef Settings  TChild;

        private:

        const Settings  & oSettings;

        public:

        StoreTextureBufferObject(const Settings & oNewSettings);
        ret_code_t Store(const TextureStock & oTextureStock, uint32_t & id, uint32_t & gl_type);

};

} //namespace SE
#endif

