
namespace SE {

float StoreTexture2D::max_anisotropic_filter = 0; //-1;

StoreTexture2D::StoreTexture2D(const Settings & oNewSettings) : oSettings(oNewSettings) {
/*FIXME check extension support in GL ctx
        if (max_anisotropic_filter != -1) {
                return;
        }

        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropic_filter);
        log_d("max anisotropic filtering = {}", max_anisotropic_filter);
        if (max_anisotropic_filter == -1) {
                max_anisotropic_filter = 0;
        }
*/
}



StoreTexture2D::~StoreTexture2D() throw() {

        //gl free tex id
}



ret_code_t StoreTexture2D::Store(const TextureStock & oTextureStock, uint32_t & id, uint32_t & gl_type) {

        ret_code_t ret_code = uSUCCESS;

        if (!oTextureStock.raw_image || !oTextureStock.raw_image_size) {

                log_e("empty image data");
                return uWRONG_INPUT_DATA;
        }

        if (!oTextureStock.width || !oTextureStock.height) {

                log_e("wrong image dimensions, width = {}, height = {}",
                                oTextureStock.width,
                                oTextureStock.height);
                return uWRONG_INPUT_DATA;
        }

        uint32_t _id;

        glGenTextures(1, &_id);

        glBindTexture(GL_TEXTURE_2D, _id);

        id = _id;

        log_d("id = {}, width = {}, height = {}, format = {}",
                        id,
                        oTextureStock.width,
                        oTextureStock.height,
                        oTextureStock.format);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, oSettings.wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, oSettings.wrap);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, oSettings.min_filter);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, oSettings.mag_filter);

        //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filter);

        glTexImage2D(GL_TEXTURE_2D,
                        0,
                        oTextureStock.internal_format,
                        oTextureStock.width,
                        oTextureStock.height,
                        0,
                        oTextureStock.format,
                        GL_UNSIGNED_BYTE,
                        oTextureStock.raw_image);
        if (oSettings.mipmap_enabled) {
                glGenerateMipmap(GL_TEXTURE_2D);
        }

        log_d("is tex = {}", glIsTexture(_id));

        gl_type = GL_TEXTURE_2D;

        return ret_code;
}


} // namespace SE
