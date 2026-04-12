
#include <opencv2/opencv.hpp>

namespace SE  {


Material::Material(const std::string & sName, const rid_t new_rid) :
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
        if (SE::FlatBuffers::VerifyMaterialBuffer(oVerifier) != true) {
                throw(std::runtime_error("failed to verify data in: " + sName));
        }

        Load(SE::FlatBuffers::GetMaterial(&vBuffer[0]));
}

Material::Material(
                const std::string & sName,
                const rid_t new_rid,
                const SE::FlatBuffers::Material * pMaterial) :
                        ResourceHolder(new_rid, sName) {

        Load(pMaterial);
}

Material::Material(const std::string & sName, const rid_t new_rid, H<SE::ShaderProgram> hNewShader) :
        ResourceHolder(new_rid, sName),
        hShader(hNewShader) {

        auto * pShader = GetShader();
        if (pShader && pShader->GetBlockDescriptor(UniformUnitInfo::Type::MATERIAL)) {
                pBlock = std::make_unique<UniformBlock>(pShader, UniformUnitInfo::Type::MATERIAL);
        }
}

ShaderProgram * Material::GetShader() const {

        return GetResource(hShader);
}

std::string Material::Str() const {

        return fmt::format("Material name: '{}', shader: '{}', textures cnt: {}, variables cnt: {}",
                        sName,
                        GetShader()->Name(),
                        mTextures.size(),
                        mShaderVariables.size());
}

void Material::Apply() const {

        // Apply render state: blend mode, depth, culling, etc.
        auto & gs = GetSystem<GraphicsState>();
        gs.SetBlendMode(blendMode);

        for (auto & oShaderVar : mShaderVariables) {

                std::visit([&oShaderVar](auto & var) {
                                GetSystem<GraphicsState>().SetVariable(oShaderVar.first, var);
                                },
                                oShaderVar.second);
        }
}

void Material::Load(const SE::FlatBuffers::Material * pMaterial) {

        //setup shader
        if (pMaterial->shader()->path() != nullptr) {
                hShader = CreateResource<ShaderProgram>(
                                GetSystem<Config>().sResourceDir + pMaterial->shader()->path()->c_str() );
        }
        else if (pMaterial->shader()->shader() != nullptr) {
                //TODO later
                throw(std::runtime_error("inplace shader program loading unsupported yet"));
        }
        else {
                        throw(std::runtime_error(fmt::format("wrong material shader state, path {:p}, shader {:p}, material name: '{}'",
                                                        (void *)pMaterial->shader()->path(),
                                                        (void *)pMaterial->shader()->shader(),
                                                        sName)));
        }

        auto * pShader = GetShader();

        if (pShader->GetBlockDescriptor(UniformUnitInfo::Type::MATERIAL)) {
                pBlock = std::make_unique<UniformBlock>(pShader, UniformUnitInfo::Type::MATERIAL);
        }

        //fill textures vec
        uint32_t textures_cnt = (pMaterial->textures()) ? pMaterial->textures()->Length() : 0;

        for (uint32_t i = 0; i < textures_cnt; ++i) {

                auto * pTextureHolder = pMaterial->textures()->Get(i);
                auto hTex = LoadTexture(pTextureHolder);

                if (!hTex.IsValid()) {

                        throw(std::runtime_error(fmt::format("wrong texture state, path {:p}, name {:p}, stock: {:p}, material name: '{}'",
                                                        (void *)pTextureHolder->path(),
                                                        (void *)pTextureHolder->name(),
                                                        (void *)pTextureHolder->stock(),
                                                        sName)));
                }
                mTextures.emplace(static_cast<TextureUnit>(pTextureHolder->unit()), hTex);
        }

        ret_code_t res;
        uint32_t mat_variables_cnt = (pMaterial->variables()) ? pMaterial->variables()->Length() : 0;

        for (uint32_t i = 0; i < mat_variables_cnt; ++i) {

                auto * pVar = pMaterial->variables()->Get(i);
                StrID name(pVar->name()->c_str());

                //THINK... bad union
                if (auto * pValue = pVar->vec4_val()) {
                        res = SetVariable(name, *reinterpret_cast<const glm::vec4 *>(pValue));
                }
                else if (auto * pValue = pVar->vec3_val()) {
                        res = SetVariable(name, *reinterpret_cast<const glm::vec3 *>(pValue));
                }
                else if (auto * pValue = pVar->vec2_val()) {
                        res = SetVariable(name, *reinterpret_cast<const glm::vec2 *>(pValue));
                }
                else if (auto * pValue = pVar->uvec4_val()) {
                        res = SetVariable(name, *reinterpret_cast<const glm::uvec4 *>(pValue));
                }
                else if (auto * pValue = pVar->uvec3_val()) {
                        res = SetVariable(name, *reinterpret_cast<const glm::uvec3 *>(pValue));
                }
                else if (auto * pValue = pVar->uvec2_val()) {
                        res = SetVariable(name, *reinterpret_cast<const glm::uvec2 *>(pValue));
                }
                else if (auto oValue = pVar->int_val()) {
                        res = SetVariable(name, oValue);
                }
                /* array float vec4, vec3, etc */
                else { //float
                        res = SetVariable(name, pVar->float_val());
                }

                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format("failed to set variable: '{}', material: '{}', shader '{}'",
                                                        pVar->name()->c_str(),
                                                        sName,
                                                        pShader->Name() )));

                }

                log_d("material: '{}' set variable: '{}'", sName, pVar->name()->c_str());
        }

        blendMode = static_cast<BlendMode>(pMaterial->blend_mode());

        // Default fallback textures: materials that don't provide a map for a slot used by their
        // shader get a neutral 1×1 texture so previous draw calls' bindings don't bleed through.
        // CreateResource deduplicates by name — each texture is created once and shared.

        if (pShader->OwnTextureUnit(TextureUnit::DIFFUSE) &&
            mTextures.find(TextureUnit::DIFFUSE) == mTextures.end()) {
                static const uint8_t kWhite[4] = {255, 255, 255, 255};
                TextureStock ts;
                ts.raw_image       = kWhite;
                ts.raw_image_size  = 4;
                ts.format          = GL_RGBA;
                ts.internal_format = GL_RGBA8;
                ts.width           = 1;
                ts.height          = 1;
                mTextures.emplace(TextureUnit::DIFFUSE,
                        CreateResource<TTexture>("engine/default/white_diffuse", ts,
                                StoreTexture2D::Settings(false)));
        }

        if (pShader->OwnTextureUnit(TextureUnit::NORMAL) &&
            mTextures.find(TextureUnit::NORMAL) == mTextures.end()) {
                // (128,128,255) encodes tangent-space (0,0,1) — no normal perturbation
                static const uint8_t kFlatNormal[4] = {128, 128, 255, 255};
                TextureStock ts;
                ts.raw_image       = kFlatNormal;
                ts.raw_image_size  = 4;
                ts.format          = GL_RGBA;
                ts.internal_format = GL_RGBA8;
                ts.width           = 1;
                ts.height          = 1;
                mTextures.emplace(TextureUnit::NORMAL,
                        CreateResource<TTexture>("engine/default/flat_normal", ts,
                                StoreTexture2D::Settings(false)));
        }

        if (pShader->OwnTextureUnit(TextureUnit::SPECULAR) &&
            mTextures.find(TextureUnit::SPECULAR) == mTextures.end()) {
                static const uint8_t kWhite[4] = {255, 255, 255, 255};
                TextureStock ts;
                ts.raw_image       = kWhite;
                ts.raw_image_size  = 4;
                ts.format          = GL_RGBA;
                ts.internal_format = GL_RGBA8;
                ts.width           = 1;
                ts.height          = 1;
                mTextures.emplace(TextureUnit::SPECULAR,
                        CreateResource<TTexture>("engine/default/white_specular", ts,
                                StoreTexture2D::Settings(false)));
        }

        if (pShader->OwnTextureUnit(TextureUnit::EMISSIVE) &&
            mTextures.find(TextureUnit::EMISSIVE) == mTextures.end()) {
                static const uint8_t kWhite[4] = {255, 255, 255, 255};
                TextureStock ts;
                ts.raw_image       = kWhite;
                ts.raw_image_size  = 4;
                ts.format          = GL_RGBA;
                ts.internal_format = GL_RGBA8;
                ts.width           = 1;
                ts.height          = 1;
                mTextures.emplace(TextureUnit::EMISSIVE,
                        CreateResource<TTexture>("engine/default/white_emissive", ts,
                                StoreTexture2D::Settings(false)));
        }

}

ret_code_t Material::SetTexture(const StrID name, H<TTexture> hTex) {

        auto * pShader = GetShader();
        auto oTexInfo = pShader->GetTextureInfo(name);

        if (!oTexInfo) {
                log_e("can't find texture with strid = '{}' in shader program: '{}'",
                                name,
                                pShader->Name());
                return uWRONG_INPUT_DATA;
        }

        mTextures.insert_or_assign(oTexInfo->get().unit_index, hTex);

        return uSUCCESS;
}

ret_code_t Material::SetTexture(const TextureUnit unit_index, H<TTexture> hTex) {

        auto * pShader = GetShader();
        if (!pShader->OwnTextureUnit(unit_index)) {
                uint32_t unit_num = static_cast<uint32_t>(unit_index);
                log_e("texture unit {} unused, shader program: '{}'",
                                unit_num,
                                pShader->Name());
                return uWRONG_INPUT_DATA;
        }

        mTextures.insert_or_assign(unit_index, hTex);
        return uSUCCESS;
}

TTexture * Material::GetTexture(const TextureUnit unit_index) const {

        auto it = mTextures.find(unit_index);
        if (it != mTextures.end()) {
                return GetResource(it->second);
        }
        return nullptr;
}


H<TTexture> LoadTexture(const SE::FlatBuffers::TextureHolder * pTextureHolder) {

        H<TTexture> hTex;
        auto LoadTex = [](auto store_type, auto * pHolder, auto && ... args) -> H<TTexture> {

                switch (store_type) {

                        case SE::FlatBuffers::StoreSettings::NONE:
                                {
                                        return CreateResource<SE::TTexture>(args...);
                                }

                        case SE::FlatBuffers::StoreSettings::StoreTexture2D:
                                {
                                        auto * pStore = pHolder->store_as_StoreTexture2D();
                                        // FlatBuffers defaults to 0 for int fields; substitute
                                        // valid GL enums when the writer omitted explicit values.
                                        constexpr int32_t SE_GL_CLAMP_TO_EDGE       = 0x812F;
                                        constexpr int32_t SE_GL_LINEAR              = 0x2601;
                                        constexpr int32_t SE_GL_LINEAR_MIPMAP_LINEAR = 0x2703;
                                        bool    mipmap = pStore->mipmap();
                                        int32_t wrap   = pStore->wrap()       ? pStore->wrap()       : SE_GL_CLAMP_TO_EDGE;
                                        int32_t min_f  = pStore->min_filter() ? pStore->min_filter() : (mipmap ? SE_GL_LINEAR_MIPMAP_LINEAR : SE_GL_LINEAR);
                                        int32_t mag_f  = pStore->mag_filter() ? pStore->mag_filter() : SE_GL_LINEAR;
                                        StoreTexture2D::Settings oSettings(mipmap, wrap, min_f, mag_f);

                                        return CreateResource<SE::TTexture>(args..., oSettings);
                                }

                        case SE::FlatBuffers::StoreSettings::StoreTextureBuffer:
                                {
                                        return CreateResource<SE::TTexture>(args..., StoreTextureBufferObject::Settings{});
                                }
                        default:
                                log_e("unknown StoreSettings type: '{}'", SE::FlatBuffers::EnumNameStoreSettings(store_type));
                                return H<SE::TTexture>::Null();
                }
        };

        if (pTextureHolder->path() != nullptr) {
                hTex = LoadTex(
                                pTextureHolder->store_type(),
                                pTextureHolder,
                                pTextureHolder->path()->c_str());

        }
        else if (pTextureHolder->name() != nullptr &&
                 pTextureHolder->stock() != nullptr &&
                 pTextureHolder->stock()->encoded_data() != nullptr) {

                // inline_all: decode compressed texture bytes (PNG/JPG/TGA etc.) on first use
                const auto * pEncData = pTextureHolder->stock()->encoded_data();
                std::vector<uint8_t> vRaw(pEncData->Data(), pEncData->Data() + pEncData->size());
                cv::Mat oDecoded = cv::imdecode(vRaw, cv::IMREAD_UNCHANGED);
                if (oDecoded.empty()) {
                        log_e("LoadTexture: cv::imdecode failed for '{}'",
                              pTextureHolder->name()->c_str());
                        return H<TTexture>::Null();
                }
                cv::flip(oDecoded, oDecoded, 0);  // GL convention: V=0 = image bottom
                // GL constants (avoid GL header dependency in Material.tcc)
                constexpr int SE_MT_GL_BGRA  = 0x80E1;
                constexpr int SE_MT_GL_BGR   = 0x80E0;
                constexpr int SE_MT_GL_RGBA8 = 0x8058;

                uint32_t bpp = static_cast<uint32_t>(oDecoded.channels());
                TextureStock oStock{
                        oDecoded.ptr(),
                        static_cast<uint32_t>(oDecoded.total() * oDecoded.elemSize()),
                        (bpp == 4) ? SE_MT_GL_BGRA : SE_MT_GL_BGR,
                        SE_MT_GL_RGBA8,
                        static_cast<uint32_t>(oDecoded.cols),
                        static_cast<uint32_t>(oDecoded.rows)
                };

                hTex = LoadTex(
                                pTextureHolder->store_type(),
                                pTextureHolder,
                                pTextureHolder->name()->c_str(),
                                oStock);

        }
        else if (pTextureHolder->name() != nullptr && pTextureHolder->stock() != nullptr &&
                 pTextureHolder->stock()->image() != nullptr) {

                //TODO texture create from FlatBuffers::TextureStock
                TextureStock oStock{
                        pTextureHolder->stock()->image()->Data(),
                        pTextureHolder->stock()->image()->Length(),
                        pTextureHolder->stock()->format(),
                        pTextureHolder->stock()->internal_format(),
                        pTextureHolder->stock()->width(),
                        pTextureHolder->stock()->height()
                };

                hTex = LoadTex(
                                pTextureHolder->store_type(),
                                pTextureHolder,
                                pTextureHolder->name()->c_str(),
                                oStock);

        }
        else {
                log_e("wrong texture state, path {:p}, name {:p}, stock: {:p}",
                                (void *)pTextureHolder->path(),
                                (void *)pTextureHolder->name(),
                                (void *)pTextureHolder->stock() );
                return H<TTexture>::Null();
        }

        return hTex;
}

const UniformBlock * Material::GetUniformBlock() const {

        if (pBlock) { return pBlock.get(); }
        return nullptr;
}

const Material::TexturesMap * Material::GetTextures() const {

        return &mTextures;
}

BlendMode Material::GetBlendMode() const {
        return blendMode;
}

void Material::SetBlendMode(BlendMode mode) {
        blendMode = mode;
}

std::string Material::StrDump(const size_t indent) const {

        std::string sResult = fmt::format("{:>{}} Material: shader: '{}'\n", ">", indent, GetShader()->Name());
        if (pBlock) {
                sResult += pBlock->StrDump(indent + 2) + "\n";
        }

        sResult += fmt::format("{:>{}} Shader variables cnt: {}\n", ">", indent + 2, mShaderVariables.size());
        for (auto & oItem : mShaderVariables) {
                sResult += fmt::format("{:>{}} Name id: {}\n",
                                ">",
                                indent + 4,
                                oItem.first);
                size_t cur_indent = indent + 4;

                MP::Visit(oItem.second,
                                [&sResult, cur_indent ](const float oVar) {
                                        sResult += fmt::format("{:>{}} value: {}\n", ">", cur_indent, oVar);
                                },
                                [&sResult, cur_indent](const int32_t oVar) {
                                        sResult += fmt::format("{:>{}} value: {}\n", ">", cur_indent, oVar);
                                },
                                [&sResult, cur_indent](const glm::vec2 & oVar) {
                                        sResult += fmt::format("{:>{}} value: ({}, {})\n",
                                                        ">",
                                                        cur_indent,
                                                        oVar[0],
                                                        oVar[1]);
                                },
                                [&sResult, cur_indent](const glm::vec3 & oVar) {
                                        sResult += fmt::format("{:>{}} value: ({}, {}, {})\n",
                                                        ">",
                                                        cur_indent,
                                                        oVar[0],
                                                        oVar[1],
                                                        oVar[2]);
                                },
                                [&sResult, cur_indent](const glm::vec4 & oVar) {
                                        sResult += fmt::format("{:>{}} value: ({}, {}, {}, {})\n",
                                                        ">",
                                                        cur_indent,
                                                        oVar[0],
                                                        oVar[1],
                                                        oVar[2],
                                                        oVar[3]);
                                },
                                [&sResult, cur_indent](const glm::uvec2 & oVar) {
                                        sResult += fmt::format("{:>{}} value: ({}, {})\n",
                                                        ">",
                                                        cur_indent,
                                                        oVar[0],
                                                        oVar[1]);
                                },
                                [&sResult, cur_indent](const glm::uvec3 & oVar) {
                                        sResult += fmt::format("{:>{}} value: ({}, {}, {})\n",
                                                        ">",
                                                        cur_indent,
                                                        oVar[0],
                                                        oVar[1],
                                                        oVar[2]);
                                },
                                [&sResult, cur_indent](const glm::uvec4 & oVar) {
                                        sResult += fmt::format("{:>{}} value: ({}, {}, {}, {})\n",
                                                        ">",
                                                        cur_indent,
                                                        oVar[0],
                                                        oVar[1],
                                                        oVar[2],
                                                        oVar[3]);
                                }
                );

        }

        sResult += fmt::format("{:>{}} Textures cnt: {}\n", ">", indent + 2, mTextures.size());
        for (auto & oTexItem : mTextures) {
                sResult += fmt::format("{:>{}} TextureUnit: {}\n",
                                ">",
                                indent + 4,
                                static_cast<int32_t>(oTexItem.first));
                if (auto * pTex = GetResource(oTexItem.second)) {
                        sResult += pTex->StrDump(indent + 4) + "\n";
                }
        }

        return sResult;
}

}


