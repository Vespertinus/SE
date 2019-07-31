
#include <fstream>
#include <GLUtil.h>

namespace SE {

ShaderComponent::ShaderComponent(const std::string & sName,
                                 const rid_t new_rid) :
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
        if (SE::FlatBuffers::VerifyShaderComponentBuffer(oVerifier) != true) {
                throw(std::runtime_error("failed to verify data in: " + sName));
        }

        Load(SE::FlatBuffers::GetShaderComponent(&vBuffer[0]));
}

ShaderComponent::ShaderComponent(const std::string & sName,
                                 const rid_t new_rid,
                                 const SE::FlatBuffers::ShaderComponent * pShader) :
        ResourceHolder(new_rid, sName),
        gl_id(0) {

        Load(pShader);
}


void ShaderComponent::Load(const SE::FlatBuffers::ShaderComponent * pShader) {

        int status;

        auto * pDependenciesFB  = pShader->dependencies();
        if (pDependenciesFB != nullptr) {
                size_t dependencies_cnt = pDependenciesFB->Length();
                vDependencies.reserve(dependencies_cnt);

                auto & oConfig = GetSystem<Config>();

                for (size_t i = 0; i < dependencies_cnt; ++i) {
                        ShaderComponent * pDependShader = CreateResource<ShaderComponent>(
                                        oConfig.sResourceDir +
                                        pDependenciesFB->Get(i)->c_str());
                        vDependencies.emplace_back(pDependShader);
                }
        }

        auto * pSource = pShader->source();
        if (pSource == nullptr || pSource->Length() == 0) {
                throw (std::runtime_error( "ShaderComponent::Load: empty source in: '" + sName));
        }
        size_t source_lines_cnt = pSource->Length();
        std::vector<const char *> vLines(source_lines_cnt);
        std::vector<int>          vLinesLength(source_lines_cnt);

        for (size_t i = 0; i < source_lines_cnt; ++i) {
                auto pLine = pSource->Get(i);
                if (pLine == nullptr) {
                        throw (std::runtime_error(
                                "ShaderComponent::Load: wrong source in: '" +
                                sName +
                                "', reason: line " +
                                std::to_string(i) +
                                " empty"));
                }
                vLines[i] = pLine->data();
                vLinesLength[i] = pLine->size();
        }

        type = pShader->type();

        gl_id = glCreateShader(
                        (type == SE::FlatBuffers::ShaderType::VERTEX) ? GL_VERTEX_SHADER :
                        (type == SE::FlatBuffers::ShaderType::FRAGMENT) ? GL_FRAGMENT_SHADER : GL_GEOMETRY_SHADER);
        if (!gl_id) {
                throw (std::runtime_error( "ShaderComponent::Load: failed to create gl shader"));
        }

        glShaderSource(gl_id, source_lines_cnt, vLines.data(), vLinesLength.data());
        glCompileShader(gl_id);

        glGetShaderiv(gl_id, GL_COMPILE_STATUS, &status);
        if (auto ret = CheckOpenGLError(); ret != uSUCCESS || !status) {
                PrintShaderInfoLog("compile shader", gl_id);

                glDeleteShader(gl_id);
                throw (std::runtime_error( "ShaderComponent::Load: failed to compile"));
        }
#ifdef DEBUG_BUILD
        else {
                PrintShaderInfoLog("compile shader", gl_id);
        }
#endif
        log_d("shader '{}' compiled from {} source lines", sName, source_lines_cnt);
}

ShaderComponent::~ShaderComponent() noexcept {

        if (gl_id) {
                glDeleteShader(gl_id);
        }
}

const std::vector <ShaderComponent *> & ShaderComponent::GetDependencies() const {
        return vDependencies;
}

uint32_t ShaderComponent::Get() const {
        return gl_id;
}

} //namespace SE
