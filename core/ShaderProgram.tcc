
#include <ShaderComponent.h>

namespace SE {

//TODO cube texture and tex arrays
static const std::unordered_map<StrID, TextureUnit> mTextureUnitMapping = {
        { "DiffuseTex",         TextureUnit::DIFFUSE },
        { "NormalTex",          TextureUnit::NORMAL },
        { "SpecularTex",        TextureUnit::SPECULAR },
        { "EnvTex",             TextureUnit::ENV },
        { "ShadowTex",          TextureUnit::SHADOW },
        { "BufferTex",          TextureUnit::BUFFER },
        { "CustomTex",          TextureUnit::CUSTOM }
};

//TODO store gl_type for type checking on load
static const std::unordered_map<StrID, uint32_t> mSystemVariables = {
        { "MVPMatrix",          ShaderSystemVariables::MVPMatrix },
        { "MVMatrix",           ShaderSystemVariables::MVMatrix },
        { "ScreenSize",         ShaderSystemVariables::ScreenSize }
};


static const std::unordered_map<StrID, UniformUnitInfo::Type> mUniformBlockMapping = {
        {"Transform",           UniformUnitInfo::Type::TRANSFORM },
        {"Material",            UniformUnitInfo::Type::MATERIAL },
        {"Camera",              UniformUnitInfo::Type::CAMERA },
        {"Animation",           UniformUnitInfo::Type::ANIMATION },
        {"Object",              UniformUnitInfo::Type::OBJECT },
        {"Lighting",            UniformUnitInfo::Type::LIGHTING },
        {"CUSTOM",              UniformUnitInfo::Type::CUSTOM },
};

static const std::unordered_map<uint32_t, uint32_t> mSamplers2Textures = {
        { GL_SAMPLER_1D,                        GL_TEXTURE_1D},
        { GL_SAMPLER_2D,                        GL_TEXTURE_2D },
        { GL_SAMPLER_3D,                        GL_TEXTURE_3D },
        { GL_SAMPLER_CUBE,                      GL_TEXTURE_CUBE_MAP },
        { GL_SAMPLER_1D_SHADOW,                 GL_TEXTURE_1D },
        { GL_SAMPLER_2D_SHADOW,                 GL_TEXTURE_2D },
        { GL_SAMPLER_1D_ARRAY,                  GL_TEXTURE_1D_ARRAY },
        { GL_SAMPLER_2D_ARRAY,                  GL_TEXTURE_2D_ARRAY },
        { GL_SAMPLER_1D_ARRAY_SHADOW,           GL_TEXTURE_1D_ARRAY },
        { GL_SAMPLER_2D_ARRAY_SHADOW,           GL_TEXTURE_2D_ARRAY },
        { GL_SAMPLER_2D_MULTISAMPLE,            GL_TEXTURE_2D_MULTISAMPLE },
        { GL_SAMPLER_2D_MULTISAMPLE_ARRAY,      GL_TEXTURE_2D_MULTISAMPLE_ARRAY },
        { GL_SAMPLER_CUBE_SHADOW,               GL_TEXTURE_CUBE_MAP },
        { GL_SAMPLER_BUFFER,                    GL_TEXTURE_BUFFER },
        { GL_SAMPLER_2D_RECT,                   GL_TEXTURE_RECTANGLE },
        { GL_SAMPLER_2D_RECT_SHADOW,            GL_TEXTURE_RECTANGLE },
};

static bool IsTexture(uint32_t gl_type) {

        bool res = false;

        switch (gl_type) {
                case GL_SAMPLER_1D:
                case GL_SAMPLER_2D:
                case GL_SAMPLER_3D:
                case GL_SAMPLER_CUBE:
                case GL_SAMPLER_1D_SHADOW:
                case GL_SAMPLER_2D_SHADOW:
                case GL_SAMPLER_1D_ARRAY:
                case GL_SAMPLER_2D_ARRAY:
                case GL_SAMPLER_1D_ARRAY_SHADOW:
                case GL_SAMPLER_2D_ARRAY_SHADOW:
                case GL_SAMPLER_2D_MULTISAMPLE:
                case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_SAMPLER_CUBE_SHADOW:
                case GL_SAMPLER_BUFFER:
                case GL_SAMPLER_2D_RECT:
                case GL_SAMPLER_2D_RECT_SHADOW:
                        res = true;
        }

        return res;
}

ShaderProgram::ShaderProgram(const std::string & sName,
                             const rid_t         new_rid) :
        ResourceHolder(new_rid, sName) {

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

        Load(SE::FlatBuffers::GetShaderProgram(&vBuffer[0]));
}

ShaderProgram::ShaderProgram(const std::string & sName,
                             const rid_t         new_rid,
                             const SE::FlatBuffers::ShaderProgram * pShaderProgram) :
        ResourceHolder(new_rid, sName) {

        Load(pShaderProgram);
}

ShaderProgram::~ShaderProgram() noexcept {

        if (gl_id) {
                glDeleteProgram(gl_id);
        }
}

void ShaderProgram::Load(const FlatBuffers::ShaderProgram * pShaderProgram) {

        uint32_t shader_cnt = 2;
        int      status;
        auto   & oConfig = GetSystem<Config>();

        auto GetShader = [&](const SE::FlatBuffers::Shader * pShaderFB) -> ShaderComponent * {
                if (pShaderFB == nullptr) { return nullptr; }

                auto * pShaderDataFB = pShaderFB->data();
                if (pShaderDataFB != nullptr) {
                        return CreateResource<ShaderComponent>(
                                        oConfig.sResourceDir + pShaderFB->name()->c_str(),
                                        pShaderDataFB);
                }

                return CreateResource<ShaderComponent>(oConfig.sResourceDir + pShaderFB->name()->c_str());
        };

        auto * pVertexShader    = GetShader(pShaderProgram->vertex());
        auto * pFragmentShader  = GetShader(pShaderProgram->fragment());
        auto * pGeometryShader  = GetShader(pShaderProgram->geometry());

        if (!pVertexShader) {
                throw (std::runtime_error( "ShaderProgram::Load: failed to get vertex shader, program name: " + sName));
        }
        if (!pFragmentShader) {
                throw (std::runtime_error( "ShaderProgram::Load: failed to get fragment shader, program name: " + sName));
        }

        gl_id = glCreateProgram();
        if (!gl_id) {
                throw (std::runtime_error( "ShaderProgram::Load: failed to create gl program, name: " + sName));
        }

        //bind attributes
        for (auto & oAttribLocation : mAttributeLocation) {
                glBindAttribLocation(gl_id, oAttribLocation.second, oAttribLocation.first.c_str());
        }
        if (CheckOpenGLError() != uSUCCESS) {

                glDeleteProgram(gl_id);
                throw (std::runtime_error( "ShaderProgram::Load: failed to bind vertex attribute location"));
        }

        //TODO hierarchical dependencies are not yet supported
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

        if (pGeometryShader != nullptr) {
                glAttachShader(gl_id, pGeometryShader->Get());
                ++shader_cnt;
                const auto & vDependencies = pGeometryShader->GetDependencies();
                for (auto * pItem : vDependencies) {
                        glAttachShader(gl_id, pItem->Get());
                        ++shader_cnt;
                }
        }

        log_d("attach {} shaders to program '{}'", shader_cnt, sName);

        glLinkProgram(gl_id);

        glDetachShader(gl_id, pVertexShader->Get());
        for (auto * pItem : vVertDependencies) {
                glDetachShader(gl_id, pItem->Get());
        }
        glDetachShader(gl_id, pFragmentShader->Get());
        for (auto * pItem : vFragDependencies) {
                glDetachShader(gl_id, pItem->Get());
        }
        if (pGeometryShader != nullptr) {
                glDetachShader(gl_id, pGeometryShader->Get());
                for (auto * pItem : pGeometryShader->GetDependencies()) {
                        glDetachShader(gl_id, pItem->Get());
                }
        }

        glGetProgramiv(gl_id, GL_LINK_STATUS, &status);
        if (auto ret = CheckOpenGLError(); ret != uSUCCESS || !status) {
                PrintProgramInfoLog("link shader program", gl_id);

                glDeleteProgram(gl_id);
                throw (std::runtime_error( "ShaderProgram::Load: failed to link"));
        }
#ifdef DEBUG_BUILD
        else {
                PrintProgramInfoLog("link shader program", gl_id);
        }
#endif
/*TEMP
        glValidateProgram(gl_id);
        glGetProgramiv(gl_id, GL_VALIDATE_STATUS, &status);
        if (auto ret = CheckOpenGLError(); ret != uSUCCESS || !status) {
                PrintProgramInfoLog("validate shader program", gl_id);

                glDeleteProgram(gl_id);
                throw (std::runtime_error( "ShaderProgram::Load: failed to validate"));
        }
*/
        glUseProgram(gl_id);

        std::unordered_map<uint32_t, UniformUnitInfo::Type> mBlockBinding;
        FillUniformBlocks(mBlockBinding);
        FillVariables(mBlockBinding);

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

ret_code_t ShaderProgram::SetVariable(const StrID name, const glm::vec4 & val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_FLOAT_VEC4) {
                log_e("wrong type 'glm::vec4', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniform4fv(it->second.location, 1, glm::value_ptr(val));
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

ret_code_t ShaderProgram::SetVariable(const StrID name, const glm::uvec2 & val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_UNSIGNED_INT_VEC2) {
                log_e("wrong type 'glm::uvec2', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniform2uiv(it->second.location, 1, glm::value_ptr(val));
        return uSUCCESS;
}

ret_code_t ShaderProgram::SetVariable(const StrID name, const glm::uvec3 & val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_UNSIGNED_INT_VEC3) {
                log_e("wrong type 'glm::uvec3', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniform3uiv(it->second.location, 1, glm::value_ptr(val));
        return uSUCCESS;
}

ret_code_t ShaderProgram::SetVariable(const StrID name, const glm::uvec4 & val) {

        auto it = mVariables.find(name);
        if (it == mVariables.end()) {
                log_e("can't find variable with strid = '{}' in shader program: '{}'", name, sName);
                return uWRONG_INPUT_DATA;
        }

        if (it->second.type != GL_UNSIGNED_INT_VEC4) {
                log_e("wrong type 'glm::uvec4', variable '{}' expect {}, in shader program: '{}'",
                                it->second.sName,
                                it->second.type,
                                sName);
                return uWRONG_INPUT_DATA;
        }
        glUniform4uiv(it->second.location, 1, glm::value_ptr(val));
        return uSUCCESS;
}

void ShaderProgram::Use() const {

        glUseProgram(gl_id);
}

bool ShaderProgram::OwnVariable(const StrID name) const {

        return (mVariables.find(name) != mVariables.end());
}

bool ShaderProgram::OwnTexture(const StrID name) const {
        return (mSamplers.find(name) != mSamplers.end());
}

bool ShaderProgram::OwnTextureUnit(const TextureUnit unit_index) const {

        return (used_texture_units & (1 << static_cast<int32_t>(unit_index))) ;
}

std::optional<std::reference_wrapper<const ShaderVariable>>
        ShaderProgram::GetTextureInfo(const StrID name) const {

        if  (auto it = mSamplers.find(name); it != mSamplers.end()) {
                return std::cref(it->second);
        }

        return std::nullopt;
}

uint32_t ShaderProgram::UsedSystemVariables() const {
        return used_system_variables;
}

const UniformBlockDescriptor * ShaderProgram::GetBlockDescriptor(const UniformUnitInfo::Type unit) const {

        if  (auto it = mBlockDescriptors.find(unit); it != mBlockDescriptors.end()) {
                return &(it->second);
        }

        return nullptr;
}

void ShaderProgram::FillVariables(std::unordered_map<uint32_t, UniformUnitInfo::Type> & mBlockBinding) {

        uint32_t uniforms_cnt = 0;
        int32_t items_cnt;//TODO array support
        int32_t name_len = 0;
        static std::array<char, 512> oNameBuffer;
        int32_t block_index;

        glGetProgramiv(gl_id, GL_ACTIVE_UNIFORMS, reinterpret_cast<int32_t *>(&uniforms_cnt));

        for (uint32_t i = 0; i < uniforms_cnt; ++i) {

                ShaderVariable oVar;
                block_index = -1;

                glGetActiveUniform(
                                gl_id,
                                i,
                                oNameBuffer.size(),
                                &name_len,
                                &items_cnt,
                                &oVar.type,
                                oNameBuffer.data());
                oVar.location = glGetUniformLocation(gl_id, oNameBuffer.data());
                if (oVar.location < 0) {

                        glGetActiveUniformsiv(
                                        gl_id,
                                        1,
                                        &i,
                                        GL_UNIFORM_BLOCK_INDEX,
                                        &block_index);

                        if (block_index < 0) {
                                glDeleteProgram(gl_id);
                                throw (std::runtime_error(
                                                        "ShaderProgram::Load: failed to get uniform '" +
                                                        std::string(oNameBuffer.data(), name_len) +
                                                        "'location = " +
                                                        std::to_string(oVar.location) +
                                                        ", may be uniform stored inside block (unsupported currently), name: " +
                                                        sName));
                        }

                        glGetActiveUniformsiv(
                                        gl_id,
                                        1,
                                        static_cast<uint32_t *>(&i),
                                        GL_UNIFORM_OFFSET,
                                        &oVar.location);
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
                        auto it2 = mSamplers2Textures.find(oVar.type);
                        if (it2 == mSamplers2Textures.end()) {
                                glDeleteProgram(gl_id);
                                throw (std::runtime_error(
                                                        "ShaderProgram::Load: unknown sampler type: " +
                                                        std::to_string(oVar.type) +
                                                        ", name: '" +
                                                        oVar.sName +
                                                        "', shader program name: " +
                                                        sName));

                        }
                        oVar.type = it2->second;

                        int32_t unit_num = static_cast<int32_t>(it->second);
                        assert(unit_num < MAX_TEXTURE_IMAGE_UNITS);

                        if (used_texture_units & (1 << unit_num) ) {
                                glDeleteProgram(gl_id);
                                throw (std::runtime_error(
                                                        "ShaderProgram::Load: texture block: " +
                                                        std::to_string(unit_num) +
                                                        "already used, name: '" +
                                                        oVar.sName +
                                                        "', shader program name: " +
                                                        sName));
                        }

                        used_texture_units |= 1 << unit_num;

                        glUniform1iv(oVar.location, 1, reinterpret_cast<const int32_t *>(&it->second));
                        oVar.unit_index = it->second;

                        log_d("add tex sampler: '{}' to shader program: '{}'", oVar.sName, sName);
                        mSamplers.emplace(key, std::move(oVar));
                }
                else { //uniform value
                        if (block_index >= 0) {

                                auto itBlock = mBlockBinding.find(block_index);
                                if (itBlock == mBlockBinding.end()) {
                                        glDeleteProgram(gl_id);
                                        throw (std::runtime_error(fmt::format(
                                                "wrong block index: {}, name: '{}', location: {},  shader program name: '{}'",
                                                block_index,
                                                oVar.sName,
                                                oVar.location,
                                                sName )));
                                }

                                auto itBlockDesc = mBlockDescriptors.find(itBlock->second);
                                if (itBlockDesc == mBlockDescriptors.end()) {
                                        glDeleteProgram(gl_id);
                                        throw (std::runtime_error(fmt::format(
                                                "wrong block descriptor: {}, name: '{}', location: {},  shader program name: '{}'",
                                                static_cast<uint8_t>(itBlock->second),
                                                oVar.sName,
                                                oVar.location,
                                                sName )));
                                }
                                log_d("add uniform: '{}' to block: {}, type: {}, size: {}, location: {}, to shader program: '{}'",
                                                oVar.sName,
                                                static_cast<uint8_t>(itBlock->second),
                                                oVar.type,
                                                items_cnt,
                                                oVar.location,
                                                sName);
                                itBlockDesc->second.mVariables.emplace(key, std::move(oVar));
                        }
                        else {
                                uint32_t mask    = 0;
                                auto itSystemVar = mSystemVariables.find(key);
                                if (itSystemVar != mSystemVariables.end()) {
                                        mask = itSystemVar->second;
                                }

                                log_d("add {} uniform: '{}', type: {}, size: {}, location: {}, to shader program: '{}'",
                                                (mask) ? "system" : "custom",
                                                oVar.sName,
                                                oVar.type,
                                                items_cnt,
                                                oVar.location,
                                                sName);

                                mVariables.emplace(key, std::move(oVar));
                                used_system_variables |= mask;
                        }
                }
        }
}

void ShaderProgram::FillUniformBlocks(std::unordered_map<uint32_t, UniformUnitInfo::Type> & mBlockBinding) {

        int32_t         uniform_blocks_cnt = 0;
        int32_t         name_len           = 0;
        uint32_t        block_index;
        int32_t         block_size;
        uint32_t        alignment = GetSystem<GraphicsConfig>().GetValue(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
        static std::array<char, 512> oNameBuffer;

        glGetProgramiv(gl_id, GL_ACTIVE_UNIFORM_BLOCKS, &uniform_blocks_cnt);

        for (int32_t i = 0; i < uniform_blocks_cnt; ++i) {

                glGetActiveUniformBlockName(
                                gl_id,
                                static_cast<uint32_t>(i),
                                oNameBuffer.size(),
                                &name_len,
                                oNameBuffer.data());

                block_index = glGetUniformBlockIndex(gl_id, oNameBuffer.data());
                StrID key(oNameBuffer.data());

                auto it = mUniformBlockMapping.find(key);
                if (it == mUniformBlockMapping.end()) {

                        glDeleteProgram(gl_id);
                        throw (std::runtime_error(fmt::format(
                                "unknown uniform block name: '{}', shader program: '{}'",
                                oNameBuffer.data(),
                                sName )));
                }

                glGetActiveUniformBlockiv(gl_id, block_index, GL_UNIFORM_BLOCK_DATA_SIZE, &block_size);

                if (!block_size) {
                        log_w("skip empty block: '{}', shader program: '{}'", oNameBuffer.data(), sName);
                        continue;
                }

                mBlockBinding[block_index] = it->second;
                glUniformBlockBinding(gl_id, block_index, static_cast<int32_t>(it->second));


                auto & oDesc = mBlockDescriptors.emplace(it->second, UniformBlockDescriptor{}).first->second;
                oDesc.size = block_size;
                oDesc.aligned_size = ((block_size + (alignment - 1)) & ~(alignment - 1));
                log_d("add uniform block descriptor: '{}' size: {}, aligned_size: {}, to shader: '{}'",
                                oNameBuffer.data(),
                                oDesc.size,
                                oDesc.aligned_size,
                                sName);
        }
}


} //namespace SE
