
#include <GLUtil.h>

namespace SE {

StoreTextureBufferObject::StoreTextureBufferObject(const Settings & oNewSettings) : oSettings(oNewSettings) { }

ret_code_t StoreTextureBufferObject::Store(const TextureStock & oTextureStock, uint32_t & id, uint32_t & gl_type) {

        if (!oTextureStock.raw_image || !oTextureStock.raw_image_size) {

                log_e("empty image data");
                return uWRONG_INPUT_DATA;
        }

        if (!oTextureStock.width || oTextureStock.height) {

                log_e("wrong image dimensions, width = {}, height = {}, height must mbe equal to 0",
                                oTextureStock.width,
                                oTextureStock.height);
                return uWRONG_INPUT_DATA;
        }

        CheckOpenGLError();

        uint32_t tbo_id;

        glGenBuffers(1, &tbo_id);

        if (!tbo_id) {
                log_e("failed to genereate texture buffer");
                return uEXT_LIBRARY_ERROR;
        }

        CheckOpenGLError();
        glBindBuffer(GL_TEXTURE_BUFFER, tbo_id);
        CheckOpenGLError();
        glBufferData(GL_TEXTURE_BUFFER, oTextureStock.raw_image_size, oTextureStock.raw_image, GL_STATIC_DRAW);

        CheckOpenGLError();
        glGenTextures(1, &id);
        if (!id) {
                log_e("failed to genereate texture");
                glDeleteBuffers(1, &tbo_id);
                return uEXT_LIBRARY_ERROR;
        }

        glBindTexture(GL_TEXTURE_BUFFER, id);
        glTexBuffer(GL_TEXTURE_BUFFER, oTextureStock.internal_format, tbo_id);
        CheckOpenGLError();

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
        glDeleteBuffers(1, &tbo_id);

        log_d("id = {}, width = {}, height = {}, format = {}, internal_format = {}",
                        id,
                        oTextureStock.width,
                        oTextureStock.height,
                        oTextureStock.format,
                        oTextureStock.internal_format);


        log_d("id = {}, is tex = {}", id, glIsTexture(id));

        gl_type = GL_TEXTURE_BUFFER;

        return uSUCCESS;
}

}

