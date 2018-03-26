#ifndef __GL_UTIL_H__
#define __GL_UTIL_H__ 1

namespace SE {

enum class TextureUnit : int32_t {
        DIFFUSE         = 0,
        NORMAL          = 1,
        SPECULAR        = 2,
        ENV             = 3,
        SHADOW          = 4,
        CUSTOM          = 7,
        //DEPTH
        //etc
        //MAX_TEXTURE_IMAGE_UNITS = 16
};



#define CheckOpenGLError() CheckGLError(__FILE__, __LINE__)

static inline ret_code_t CheckGLError (const char * file, const int line) {

        GLenum gl_err = glGetError();
        ret_code_t res = uSUCCESS;
        while (gl_err != GL_NO_ERROR) {
                log_e_clean("got OpenGL error: '{}' ({}:{})", gl_err, file, line);
                res = uEXT_LIBRARY_ERROR;
                gl_err = glGetError();
        }
        return res;
}

}
#endif
