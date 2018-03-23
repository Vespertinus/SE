
namespace SE {

float StoreTexture2D::max_anisotropic_filter = -1;

StoreTexture2D::StoreTexture2D(const Settings & oNewSettings) : oSettings(oNewSettings) {

        if (max_anisotropic_filter != -1) {
                return;
        }

        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropic_filter);
        log_d("max anisotropic filtering = {}", max_anisotropic_filter);
        if (max_anisotropic_filter == -1) {
                max_anisotropic_filter = 0;
        }
}



StoreTexture2D::~StoreTexture2D() throw() {

        //gl free tex id
}



ret_code_t StoreTexture2D::Store(TextureStock & oTextureStock, uint32_t & id) {

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

        log_d("id = {}, width = {}, height = {}, bpp = {}",
                        id,
                        oTextureStock.width,
                        oTextureStock.height,
                        oTextureStock.bpp);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, oSettings.wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, oSettings.wrap);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, oSettings.min_filter);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, oSettings.mag_filter);

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oSettings.apply_method);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filter);

        glTexImage2D(GL_TEXTURE_2D,
                        0,
                        GL_RGBA8,//oTextureStock.color_order,
                        oTextureStock.width,
                        oTextureStock.height,
                        0,
                        oTextureStock.color_order,
                        GL_UNSIGNED_BYTE,
                        oTextureStock.raw_image);
        if (oSettings.mipmap_enabled) {
                glGenerateMipmap(GL_TEXTURE_2D);
        }

        log_d("is tex = {}", glIsTexture(_id));

        oTextureStock.gl_type = GL_TEXTURE_2D;

        //Calc stat
        /*
        uint8_t min_alpha = std::numeric_limits<uint8_t>::max();
        uint8_t max_alpha = std::numeric_limits<uint8_t>::min();
        if (oTextureStock.bpp == 4) {
                for (uint32_t i = 3; i < oTextureStock.raw_image_size; i += 4) {
                        min_alpha = std::min(min_alpha, oTextureStock.raw_image[i]);
                        max_alpha = std::max(max_alpha, oTextureStock.raw_image[i]);
                }
                log_d("tex id = {}, min_alpha = {}, max_alpha = {}",
                                _id,
                                min_alpha,
                                max_alpha);
        }
        */

        return ret_code;
}


} // namespace SE
