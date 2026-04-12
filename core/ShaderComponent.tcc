
#include <fstream>
#include <GLUtil.h>

namespace SE {

/** Shared file-loading helper: reads file into caller's buffer, verifies, returns FB pointer */
static const SE::FlatBuffers::ShaderComponent *
LoadShaderComponentFromFile(const std::string & sName, std::vector<char> & vBuffer) {

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

        vBuffer.resize(file_size);
        log_d("LoadShaderComponentFromFile: shader file: '{}', buffer size: {}", sName, file_size);

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

        return SE::FlatBuffers::GetShaderComponent(&vBuffer[0]);
}

/** Helper: resolve effective shader type considering parent_type override */
inline SE::FlatBuffers::ShaderType
ResolveShaderType(SE::FlatBuffers::ShaderType fb_type, int8_t parent_type) {
        if (parent_type != -1) {
                return static_cast<SE::FlatBuffers::ShaderType>(parent_type);
        }
        return fb_type;
}

/** Helper: map FlatBuffers ShaderType to GL shader type enum */
inline uint32_t ShaderTypeToGL(SE::FlatBuffers::ShaderType t) {
        switch (t) {
                case SE::FlatBuffers::ShaderType::VERTEX:   return GL_VERTEX_SHADER;
                case SE::FlatBuffers::ShaderType::FRAGMENT: return GL_FRAGMENT_SHADER;
                case SE::FlatBuffers::ShaderType::GEOMETRY: return GL_GEOMETRY_SHADER;
                case SE::FlatBuffers::ShaderType::COMPUTE:  return GL_COMPUTE_SHADER;
                default:                                    return GL_GEOMETRY_SHADER;
        }
}

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

ShaderComponent::ShaderComponent(const std::string & sName,
                                 const rid_t new_rid) :
        ResourceHolder(new_rid, sName),
        gl_id(0) {

        std::vector<char> vBuffer;
        Load(LoadShaderComponentFromFile(sName, vBuffer));
}

ShaderComponent::ShaderComponent(const std::string & sName,
                                 const rid_t new_rid,
                                 const SE::FlatBuffers::ShaderComponent * pShader) :
        ResourceHolder(new_rid, sName),
        gl_id(0) {

        Load(pShader);
}

ShaderComponent::ShaderComponent(const std::string & sName,
                                 const rid_t new_rid,
                                 int8_t parent_type) :
        ResourceHolder(new_rid, sName + ":" + std::to_string(parent_type)),
        gl_id(0) {

        std::vector<char> vBuffer;
        Load(LoadShaderComponentFromFile(sName, vBuffer), parent_type);
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

void ShaderComponent::Load(const SE::FlatBuffers::ShaderComponent * pShader, int8_t parent_type) {

        int status;

        auto & oConfig = GetSystem<Config>();

        // --- Process dependencies (glAttachShader for separate shader objects) ---
        auto * pDependenciesFB  = pShader->dependencies();
        if (pDependenciesFB != nullptr) {
                size_t dependencies_cnt = pDependenciesFB->Length();
                vDependencies.reserve(dependencies_cnt);

                for (size_t i = 0; i < dependencies_cnt; ++i) {
                        ShaderComponent * pDependShader = CreateRawResource<ShaderComponent>(
                                        oConfig.sResourceDir +
                                        pDependenciesFB->Get(i)->c_str(),
                                        static_cast<int8_t>(ResolveShaderType(pShader->type(), parent_type)));
                        vDependencies.emplace_back(pDependShader);
                }
        }

        // --- Collect source lines: header → includes → main source ---
        // All source text is copied into owned strings to avoid dangling pointers
        // from temporary FlatBuffers buffers.
        std::vector<std::string> vOwnedLines;
        std::vector<const char *> vLines;
        std::vector<int>          vLinesLength;

        // 1. Header lines (e.g., #version directive) — must come first
        auto * pHeaderPaths = pShader->header();
        if (pHeaderPaths != nullptr) {
                for (size_t i = 0; i < pHeaderPaths->Length(); ++i) {
                        auto pLine = pHeaderPaths->Get(i);
                        if (pLine == nullptr) {
                                throw (std::runtime_error(
                                        "ShaderComponent::Load: empty header line " +
                                        std::to_string(i) + " in: '" + sName + "'"));
                        }
                        vOwnedLines.emplace_back(pLine->data(), pLine->size());
                        vLines.push_back(vOwnedLines.back().data());
                        vLinesLength.push_back(vOwnedLines.back().size());
                }
        }

        // 2. Include sources
        auto * pIncludePaths = pShader->include();
        if (pIncludePaths != nullptr) {

                std::vector<char> vIncBuffer;

                for (size_t i = 0; i < pIncludePaths->Length(); ++i) {
                        std::string incPath = oConfig.sResourceDir + pIncludePaths->Get(i)->c_str();

                        // Read and parse included .sesl file
                        vIncBuffer.clear();
                        const SE::FlatBuffers::ShaderComponent * pIncFB =
                                LoadShaderComponentFromFile(incPath, vIncBuffer);

                        auto * pIncSource = pIncFB->source();
                        if (pIncSource == nullptr || pIncSource->Length() == 0) {
                                throw (std::runtime_error(
                                        "ShaderComponent::Load: empty source in include file: '" +
                                        incPath + "'"));
                        }

                        for (size_t j = 0; j < pIncSource->Length(); ++j) {
                                auto pLine = pIncSource->Get(j);
                                if (pLine == nullptr) {
                                        throw (std::runtime_error(
                                                "ShaderComponent::Load: wrong source in include file: '" +
                                                incPath + "', line " + std::to_string(j) + " empty"));
                                }
                                // Copy into owned string (pLine->data() is in temporary vIncBuffer)
                                vOwnedLines.emplace_back(pLine->data(), pLine->size());
                                vLines.push_back(vOwnedLines.back().data());
                                vLinesLength.push_back(vOwnedLines.back().size());
                        }
                }
        }

        // 3. Main source lines
        auto * pSource = pShader->source();
        if (pSource == nullptr || pSource->Length() == 0) {
                throw (std::runtime_error( "ShaderComponent::Load: empty source in: '" + sName));
        }
        const size_t mainLineCount = pSource->Length();
        vOwnedLines.reserve(vOwnedLines.size() + mainLineCount);
        vLines.reserve(vLines.size() + mainLineCount);
        vLinesLength.reserve(vLinesLength.size() + mainLineCount);

        for (size_t i = 0; i < mainLineCount; ++i) {
                auto pLine = pSource->Get(i);
                if (pLine == nullptr) {
                        throw (std::runtime_error(
                                "ShaderComponent::Load: wrong source in: '" +
                                sName +
                                "', reason: line " +
                                std::to_string(i) +
                                " empty"));
                }
                vOwnedLines.emplace_back(pLine->data(), pLine->size());
                vLines.push_back(vOwnedLines.back().data());
                vLinesLength.push_back(vOwnedLines.back().size());
        }

        size_t totalLines = vLines.size();

        type = ResolveShaderType(pShader->type(), parent_type);

        /*
        if (auto ret = CheckOpenGLError(); ret != uSUCCESS || !status) {
                throw (std::runtime_error( "ShaderComponent::Load: gl check failed"));
        }*/

        gl_id = glCreateShader(ShaderTypeToGL(type));
        if (!gl_id) {
                throw (std::runtime_error( "ShaderComponent::Load: failed to create gl shader"));
        }

        glShaderSource(gl_id, static_cast<GLsizei>(totalLines), vLines.data(), vLinesLength.data());
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
        log_d("shader '{}' compiled from {} source lines ({} header, {} included)", sName, totalLines,
                        (pHeaderPaths ? pHeaderPaths->Length() : 0),
                        (pIncludePaths ? pIncludePaths->Length() : 0));
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
