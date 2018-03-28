
#include <ShaderComponent.h>

namespace SE {

//TODO cube texture and tex arrays
static const std::unordered_map<StrID, TextureUnit> mTextureUnitMapping = {
        { "DiffuseTex",         TextureUnit::DIFFUSE },
        { "NormalTex",          TextureUnit::NORMAL },
        { "SpecularTex",        TextureUnit::SPECULAR },
        { "EnvTex",             TextureUnit::ENV },
        { "ShadowTex",          TextureUnit::SHADOW },
        { "CustomTex",          TextureUnit::CUSTOM }
};

static bool IsTexture(uint32_t gl_type) {

        bool res = false;

        switch (gl_type) {

                case GL_TEXTURE_1D:
                case GL_TEXTURE_2D:
                case GL_TEXTURE_3D:
                case GL_TEXTURE_CUBE_MAP:
                case GL_TEXTURE_RECTANGLE:
                case GL_TEXTURE_1D_ARRAY:
                case GL_TEXTURE_2D_ARRAY:
                case GL_TEXTURE_CUBE_MAP_ARRAY:
                case GL_TEXTURE_BUFFER:
                case GL_TEXTURE_2D_MULTISAMPLE:
                case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
                        res = true;
        }

        return res;
}

ShaderProgram::ShaderProgram(const std::string & sName,
                             const rid_t         new_rid,
                             const Settings    & oSettings) :
        ResourceHolder(new_rid, sName),
        gl_id(0) {

        static const size_t max_file_size = 1024 * 1024 * 10;

        auto file_size = boost::filesystem::file_size(sName);
        if (file_size > max_file_size) {
                throw(std::runtime_error(
                                        "too big file size, allowed max = " +
                                        std::to_string(max_file_size) +
                                        ", got " +
                                        std::to_string(file_size) +
                                        " bytes"));
        }

        std::vector<char> vBuffer(file_size);
        log_d("buffer size: {}", vBuffer.size());

        //TODO rewrite on os wrappers that in linux case call mmap
        {
                std::ifstream oInput(sName, std::ios::binary | std::ios::in);
                if(!oInput.is_open()) {
                        throw(std::runtime_error("failed to open file: " + sName));
                }
                oInput.read(&vBuffer[0], file_size);
        }
        flatbuffers::Verifier oVerifier(reinterpret_cast<uint8_t *>(&vBuffer[0]), file_size);
        if (SE::FlatBuffers::VerifyShaderProgramBuffer(oVerifier) != true) {
                throw(std::runtime_error("failed to verify data in: " + sName));
        }

        Load(SE::FlatBuffers::GetShaderProgram(&vBuffer[0]), oSettings);
}

ShaderProgram::ShaderProgram(const std::string & sName,
                             const rid_t         new_rid,
                             const SE::FlatBuffers::ShaderProgram * pShaderProgram,
                             const Settings   & oSettings) :
        ResourceHolder(new_rid, sName),
        gl_id(0) {

        Load(pShaderProgram, oSettings);
}

ShaderProgram::~ShaderProgram() noexcept {

        if (gl_id) {
                glDeleteProgram(gl_id);
        }
}

void ShaderProgram::Load(const FlatBuffers::ShaderProgram * pShaderProgram, const Settings & oSettings) {

        uint32_t shader_cnt = 2;
        int      status;

        auto GetShader = [&](const SE::FlatBuffers::Shader * pShaderFB) -> ShaderComponent * {
                auto * pShaderDataFB = pShaderFB->data();
                if (pShaderDataFB != nullptr) {
                        return CreateResource<ShaderComponent>(
                                        oSettings.sShadersDir + pShaderFB->name()->c_str(),
                                        pShaderDataFB,
                                        ShaderComponent::Settings{ oSettings.sShadersDir } );
                }

                return CreateResource<ShaderComponent>(
                                oSettings.sShadersDir + pShaderFB->name()->c_str() ,
                                ShaderComponent::Settings{ oSettings.sShadersDir } );
        };

        auto * pVertexShader    = GetShader(pShaderProgram->vertex());
        auto * pFragmentShader  = GetShader(pShaderProgram->fragment());

        gl_id = glCreateProgram();
        if (!gl_id) {
                throw (std::runtime_error( "ShaderProgram::Load: failed to create gl program, name: " + sName));
        }

        glAttachShader(gl_id, pVertexShader->Get());
        const auto & vVertDependencies = pVertexShader->GetDependencies();
        for (auto * pItem : vVertDependencies) {
                glAttachShader(gl_id, pItem->Get());
                ++shader_cnt;
        }
        glAttachShader(gl_id, pFragmentShader->Get());
        const auto & vFragDependencies = pFragmentShader->GetDependencies();
        for (auto * pItem : vFragDependencies) {
                glAttachShader(gl_id, pItem->Get());
                ++shader_cnt;
        }
        log_d("attach {} shaders to program '{}'", shader_cnt, sName);

        glLinkProgram(gl_id);

        glGetProgramiv(gl_id, GL_LINK_STATUS, &status);
        if (auto ret = CheckOpenGLError(); ret != uSUCCESS || !status) {
                PrintInfoLog("link shader program", gl_id);

                glDeleteProgram(gl_id);
                throw (std::runtime_error( "ShaderProgram::Load: failed to link"));
        }

        glValidateProgram(gl_id);
        glGetProgramiv(gl_id, GL_VALIDATE_STATUS, &status);
        if (auto ret = CheckOpenGLError(); ret != uSUCCESS || !status) {
                PrintInfoLog("validate shader program", gl_id);

                glDeleteProgram(gl_id);
                throw (std::runtime_error( "ShaderProgram::Load: failed to validate"));
        }

        glUseProgram(gl_id);

        //TODO configure uniform buffers
        int32_t uniforms_cnt = 0;
        int32_t items_cnt;
        int32_t name_len = 0;
        static std::array<char, 512> oNameBuffer;

        glGetProgramiv(gl_id, GL_ACTIVE_UNIFORMS, &uniforms_cnt);

        for (int32_t i = 0; i < uniforms_cnt; ++i) {
                ShaderVariable oVar;
                glGetActiveUniform(
                                gl_id,
                                static_cast<uint32_t>(i),
                                oNameBuffer.size(),
                                &name_len,
                                &items_cnt,
                                &oVar.type,
                                oNameBuffer.data());
                oVar.location = glGetUniformLocation(gl_id, oNameBuffer.data());
                if (oVar.location < 0) {
                        glDeleteProgram(gl_id);
                        throw (std::runtime_error(
                                                "ShaderProgram::Load: failed to get uniform '" +
                                                std::string(oNameBuffer.data(), name_len) +
                                                "'location = " +
                                                std::to_string(oVar.location) +
                                                ", may be uniform stored inside block (unsupported currently), name: " +
                                                sName));
                }

                oVar.sName = std::string_view(oNameBuffer.data(), name_len);
                StrID key(oVar.sName);

                if (IsTexture(oVar.type)) {

                        auto it = mTextureUnitMapping.find(key);
                        if (it == mTextureUnitMapping.end()) {

                                glDeleteProgram(gl_id);
                                throw (std::runtime_error(
                                                        "ShaderProgram::Load: unknown texture variable name: '" +
                                                        oVar.sName +
                                                        "', can't find proper texture unit, shader program name: " +
                                                        sName));
                        }

                        glUniform1iv(oVar.location, 1, reinterpret_cast<const int32_t *>(&it->second));
                        oVar.unit_index = it->second;

                        log_d("add tex sampler: '{}' to shader program: '{}'", oVar.sName, sName);
                        mSamplers.emplace(key, std::move(oVar));
                }
                else {
                        log_d("add uniform: '{}', type: {}, size: {}, to shader program: '{}'",
                                        oVar.sName,
                                        oVar.type,
                                        items_cnt,
                                        sName);
                        mVariables.emplace(key, std::move(oVar));
                }

        }

        CheckOpenGLError();
        log_d("shader program: '{}' link done", sName);

        glUseProgram(0);
}

ret_code_t ShaderProgram::SetVariable(const StrID name, float val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_FLOAT) {
                log_e("wrong type 'float', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniform1fv(it->second.location, 1, &val);
        return uSUCCESS;
}


ret_code_t ShaderProgram::SetVariable(const StrID name, const glm::vec2 & val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_FLOAT_VEC2) {
                log_e("wrong type 'glm::vec2', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniform2fv(it->second.location, 1, glm::value_ptr(val));
        return uSUCCESS;
}

ret_code_t ShaderProgram::SetVariable(const StrID name, const glm::vec3 & val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_FLOAT_VEC3) {
                log_e("wrong type 'glm::vec3', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniform3fv(it->second.location, 1, glm::value_ptr(val));
        return uSUCCESS;
}

ret_code_t ShaderProgram::SetVariable(const StrID name, const glm::mat3 & val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_FLOAT_MAT3) {
                log_e("wrong type 'glm::mat3', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniformMatrix3fv(it->second.location, 1, GL_FALSE, glm::value_ptr(val));
        return uSUCCESS;
}

ret_code_t ShaderProgram::SetVariable(const StrID name, const glm::mat4 & val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_FLOAT_MAT4) {
                log_e("wrong type 'glm::mat4', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniformMatrix4fv(it->second.location, 1, GL_FALSE, glm::value_ptr(val));
        return uSUCCESS;
}

void ShaderProgram::Use() const {

        glUseProgram(gl_id);
}

bool ShaderProgram::HasVariable(const StrID name) const {

        return (mVariables.find(name) != mVariables.end());
}

ret_code_t ShaderProgram::SetTexture(const TextureUnit unit_index, const TTexture * pTex) {

        int32_t unit_num = static_cast<int32_t>(unit_index);

        if (unit_num >= MAX_TEXTURE_IMAGE_UNITS) {
                log_e("too big unit index = {}, max allowed = {}, shader program: '{}'",
                                unit_num,
                                (const uint8_t)MAX_TEXTURE_IMAGE_UNITS,
                                sName);
                return uWRONG_INPUT_DATA;
        }

        glActiveTexture(GL_TEXTURE0 + unit_num);
        glBindTexture(pTex->Type(), pTex->GetID());

        return uSUCCESS;
}

ret_code_t ShaderProgram::SetTexture(const StrID name, const TTexture * pTex) {

        auto it = mSamplers.find(name);
        if (it == mSamplers.end()) {
                log_e("can't find texture with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != pTex->Type()) {
                log_e("wrong type '{}', sampler '{}' expect {}, in shader program: '{}'",
                                pTex->Type(),
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }

        glActiveTexture(GL_TEXTURE0 + static_cast<int32_t>(it->second.unit_index) );
        glBindTexture(pTex->Type(), pTex->GetID());

        return uSUCCESS;
}

} //namespace SE
