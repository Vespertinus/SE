
namespace SE {

GraphicsConfig::GraphicsConfig() {

        glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
        glGetIntegerv(GL_MINOR_VERSION, &gl_minor);

        sVendor         = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        sRenderer       = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        auto * pGLSLVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);

        if (sVendor.empty() || sRenderer.empty()) {
                throw(std::runtime_error("failed to get OpenGL vendor or renderer"));
        }

        log_d("OpenGL version: {}.{}, vendor: '{}', renderer: '{}', glsl version: '{}'", gl_major, gl_minor, sVendor, sRenderer, pGLSLVersion);

        FillVariables();

        int32_t ext_num = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &ext_num);
        log_d("{} extensions supported", ext_num);

        for (auto i = 0; i < ext_num; ++i) {
                mExtensions.insert(reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, i)));
        }
}

#define GET_GL_VAR(name) \
        glGetIntegerv(name, &val); \
        mVariables.emplace(name, val); \
        log_d_clean(#name": {}", val);

void GraphicsConfig::FillVariables() {

        int32_t val;

        GET_GL_VAR(GL_MAX_VERTEX_ATTRIBS);
        GET_GL_VAR(GL_MAX_UNIFORM_BUFFER_BINDINGS);
        GET_GL_VAR(GL_MAX_VERTEX_UNIFORM_BLOCKS);
        GET_GL_VAR(GL_MAX_GEOMETRY_UNIFORM_BLOCKS);
        GET_GL_VAR(GL_MAX_FRAGMENT_UNIFORM_BLOCKS);
        GET_GL_VAR(GL_MAX_UNIFORM_BLOCK_SIZE);
        GET_GL_VAR(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
        GET_GL_VAR(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS);
        GET_GL_VAR(GL_MAX_TEXTURE_IMAGE_UNITS);
        GET_GL_VAR(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
        GET_GL_VAR(GL_MAX_TEXTURE_BUFFER_SIZE);
        GET_GL_VAR(GL_MAX_TEXTURE_SIZE);
        GET_GL_VAR(GL_MAX_RECTANGLE_TEXTURE_SIZE);
        GET_GL_VAR(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
        GET_GL_VAR(GL_MAX_UNIFORM_LOCATIONS);
        GET_GL_VAR(GL_MAX_RENDERBUFFER_SIZE);
        //OpenGL 4
        //GET_GL_VAR(GL_MAX_FRAMEBUFFER_WIDTH);
        //GET_GL_VAR(GL_MAX_FRAMEBUFFER_HEIGHT);
        GET_GL_VAR(GL_MAX_ELEMENTS_INDICES);
        GET_GL_VAR(GL_MAX_ELEMENTS_VERTICES);
        GET_GL_VAR(GL_MAX_3D_TEXTURE_SIZE);

        //TODO set GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT to atleast 16 bytes
        //must be power of two
}
#undef GET_GL_VAR

void GraphicsConfig::PrintExtensions() const {

        log_d_clean("{} extensions supported", mExtensions.size());
        for (auto & sExt : mExtensions) {
                log_d_clean("'{}'", sExt);
        }
}

void GraphicsConfig::PrintConfiguration() const {

        log_d_clean("OpenGL configuration:");

        //TODO store variables names
        for (auto & oItem : mVariables) {
                log_d_clean("{}: {}", oItem.first, oItem.second);
        }
}

void GraphicsConfig::PrintGLInfo() const {

        PrintConfiguration();
        PrintExtensions();
}

uint32_t GraphicsConfig::GetValue(const uint32_t key) const {

        auto it = mVariables.find(key);
        if (it == mVariables.end()) {
                log_w("unknown GL variable: {}", key);
                return -1;
        }
        return it->second;
}

bool GraphicsConfig::CheckExtension(const std::string & sExt) const {

        if (mExtensions.count(sExt) == 1) {

                return true;
        }
        return false;
}

bool GraphicsConfig::VersionSupport(const int32_t major, const int32_t minor) const {

        if ((major < gl_major) || (major == gl_major && minor <= gl_minor)) {
                return true;
        }
        return false;
}


}
