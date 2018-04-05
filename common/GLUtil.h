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
#define PrintProgramInfoLog(msg, gl_id)  PrintGLProgramInfoLog(msg, gl_id, __FILE__, __LINE__)
#define PrintShaderInfoLog(msg, gl_id)  PrintGLShaderInfoLog(msg, gl_id, __FILE__, __LINE__)

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

static inline void PrintGLProgramInfoLog(std::string_view sMsg, const uint32_t gl_id, const char * file, const int line) {

                int length = 0;
                int res_length;
                glGetProgramiv(gl_id, GL_INFO_LOG_LENGTH, &length);
                if (length > 0) {
                        std::string sOutput;
                        sOutput.resize(length);
                        glGetProgramInfoLog(gl_id, length, &res_length, sOutput.data());
                        log_e("{}: infolog: ({}:{})\n{}", sMsg, file, line, sOutput);
                }
}

static inline void PrintGLShaderInfoLog(std::string_view sMsg, const uint32_t gl_id, const char * file, const int line) {

                int length = 0;
                int res_length;
                glGetShaderiv(gl_id, GL_INFO_LOG_LENGTH, &length);
                if (length > 0) {
                        std::string sOutput;
                        sOutput.resize(length);
                        glGetShaderInfoLog(gl_id, length, &res_length, sOutput.data());
                        log_e("{}: infolog: ({}:{})\n{}", sMsg, file, line, sOutput);
                }
}

static const float col_epsilon = 1e-10;

static inline void RGB2HCV(const glm::vec3 & RGBColor, glm::vec3 & HCVColor) {

        glm::vec4 P = (RGBColor.g < RGBColor.b) ?
                glm::vec4(RGBColor.b, RGBColor.g, -1.0, 2.0/3.0) :
                glm::vec4(RGBColor.g, RGBColor.b, 0.0, -1.0/3.0);

        glm::vec4 Q = (RGBColor.r < P.x) ?
                glm::vec4(P.x, P.y, P.w, RGBColor.r) :
                glm::vec4(RGBColor.r, P.y, P.z, P.x);
        float C = Q.x - std::min(Q.w, Q.y);
        float H = abs((Q.w - Q.y) / (6 * C + col_epsilon) + Q.z);
        HCVColor = glm::vec3(H, C, Q.x);
}


static inline void RGb2HSL(const glm::vec3 & RGBColor, glm::vec3 & HSLColor) {

        glm::vec3 HCVColor;
        RGB2HCV(RGBColor, HCVColor);
        float L  = HCVColor.z - HCVColor.y * 0.5;
        float Saturation = HCVColor.y / (1 - abs(L * 2 - 1) + col_epsilon);
        HSLColor = glm::vec3(HCVColor.x, Saturation, L);
}

static inline void PrintGLInfo() {

        int val;
        int gl_major, gl_minor;
        
        glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
        glGetIntegerv(GL_MINOR_VERSION, &gl_minor);
        auto * pVendor = glGetString(GL_VENDOR);
        auto * pRenderer = glGetString(GL_RENDERER);

        log_d("OpenGL version: {}.{}, vendor: '{}', renderer: '{}'", gl_major, gl_minor, pVendor, pRenderer);

        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &val);
        log_d("GL_MAX_VERTEX_ATTRIBS: {}", val);
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &val);
        log_d("GL_MAX_UNIFORM_BUFFER_BINDINGS: {}", val);
        
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &val);
        log_d("GL_MAX_VERTEX_UNIFORM_BLOCKS: {}", val);
        glGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_BLOCKS, &val);
        log_d("GL_MAX_GEOMETRY_UNIFORM_BLOCKS: {}", val);
        glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &val);
        log_d("GL_MAX_FRAGMENT_UNIFORM_BLOCKS: {}", val);
        
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &val);
        log_d("GL_MAX_UNIFORM_BLOCK_SIZE: {}", val);

        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &val);
        log_d("GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: {}", val);
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &val);
        log_d("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: {}", val);

        glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &val);
        log_d("GL_MAX_TEXTURE_BUFFER_SIZE: {}", val);

        glGetIntegerv(GL_NUM_EXTENSIONS, &val);
        log_d("{} extensions supported", val);

        for (auto i = 0; i < val; ++i) {
                log_d("'{}'", glGetStringi(GL_EXTENSIONS, i));
        }
}

}
#endif
