
namespace SE  {


Material::Material(const std::string & sName, const rid_t new_rid) :
        ResourceHolder(new_rid, sName),
        pShader(nullptr) {

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

Material::Material(const std::string & sName, const rid_t new_rid, SE::ShaderProgram * pNewShader) :
        ResourceHolder(new_rid, sName),
        pShader(pNewShader) {

        if (pShader->GetBlockDescriptor(UniformUnitInfo::Type::MATERIAL) ) {

                pBlock = std::make_unique<UniformBlock>(pShader, UniformUnitInfo::Type::MATERIAL);
        }
}

std::string Material::Str() const {

        return fmt::format("Material name: '{}', shader: '{}', textures cnt: {}, variables cnt: {}",
                        sName,
                        pShader->Name(),
                        mTextures.size(),
                        mShaderVariables.size());
}

void Material::Apply() const {

        //TODO set render state for material, alpha cut, blending, depth test etc

        for (auto & oShaderVar : mShaderVariables) {

                std::visit([&oShaderVar](auto & var) {
                                GetSystem<GraphicsState>().SetVariable(oShaderVar.first, var);
                                },
                                oShaderVar.second);
        }
}
/*
void Material::SetShader(SE::ShaderProgram * pNewShader) {

        mShaderVariables.clear();
        mTextures.clear();

        pShader = pNewShader;

        //TODO fill variables list?;
        //FIXME reallocate block.. and ShaderProgramState
}*/

void Material::Load(const SE::FlatBuffers::Material * pMaterial) {

        //setup shader
        if (pMaterial->shader()->path() != nullptr) {
                pShader  = CreateResource<ShaderProgram>(
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

        if (pShader->GetBlockDescriptor(UniformUnitInfo::Type::MATERIAL) ) {

                pBlock = std::make_unique<UniformBlock>(pShader, UniformUnitInfo::Type::MATERIAL);
        }

        //fill textures vec
        uint32_t textures_cnt = (pMaterial->textures()) ? pMaterial->textures()->Length() : 0;

        for (uint32_t i = 0; i < textures_cnt; ++i) {

                auto * pTextureHolder = pMaterial->textures()->Get(i);
                auto * pTex = LoadTexture(pTextureHolder);

                if (!pTex) {

                        throw(std::runtime_error(fmt::format("wrong texture state, path {:p}, name {:p}, stock: {:p}, material name: '{}'",
                                                        (void *)pTextureHolder->path(),
                                                        (void *)pTextureHolder->name(),
                                                        (void *)pTextureHolder->stock(),
                                                        sName)));
                }
                mTextures.emplace(static_cast<TextureUnit>(pTextureHolder->unit()), pTex);
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

}

ret_code_t Material::SetTexture(const StrID name, TTexture * pTex) {

        auto oTexInfo = pShader->GetTextureInfo(name);

        if (!oTexInfo) {
                log_e("can't find texture with strid = '{}' in shader program: '{}'",
                                name,
                                pShader->Name());
                return uWRONG_INPUT_DATA;
        }

        mTextures.insert_or_assign(oTexInfo->get().unit_index, pTex);

        return uSUCCESS;
}

ret_code_t Material::SetTexture(const TextureUnit unit_index, TTexture * pTex) {

        if (!pShader->OwnTextureUnit(unit_index)) {
                uint32_t unit_num = static_cast<uint32_t>(unit_index);
                log_e("texture unit {} unused, shader program: '{}'",
                                unit_num,
                                pShader->Name());
                return uWRONG_INPUT_DATA;
        }

        mTextures.insert_or_assign(unit_index, pTex);
        return uSUCCESS;
}

TTexture * Material::GetTexture(const TextureUnit unit_index) const {

        auto it = mTextures.find(unit_index);
        if (it != mTextures.end()) {
                return it->second;
        }
        return nullptr;
}

ShaderProgram * Material::GetShader() const {

        return pShader;
}



TTexture * LoadTexture(const SE::FlatBuffers::TextureHolder * pTextureHolder) {

        TTexture * pTex;
        auto LoadTex = [](auto store_type, auto * pHolder, auto && ... args) -> TTexture * {

                switch (store_type) {

                        case SE::FlatBuffers::StoreSettings::NONE:
                                {
                                        return CreateResource<SE::TTexture>(args...);
                                }

                        case SE::FlatBuffers::StoreSettings::StoreTexture2D:
                                {
                                        auto * pStore = pHolder->store_as_StoreTexture2D();
                                        StoreTexture2D::Settings oSettings(
                                                        pStore->mipmap(),
                                                        pStore->wrap(),
                                                        pStore->min_filter(),
                                                        pStore->mag_filter());

                                        return CreateResource<SE::TTexture>(args..., oSettings);
                                }

                        case SE::FlatBuffers::StoreSettings::StoreTextureBuffer:
                                {
                                        return CreateResource<SE::TTexture>(args..., StoreTextureBufferObject::Settings{});
                                }
                        default:
                                log_e("unknown StoreSettings type: '{}'", SE::FlatBuffers::EnumNameStoreSettings(store_type));
                                return nullptr;
                }
        };

        if (pTextureHolder->path() != nullptr) {
                pTex = LoadTex(
                                pTextureHolder->store_type(),
                                pTextureHolder,
                                pTextureHolder->path()->c_str());

        }
        else if (pTextureHolder->name() != nullptr && pTextureHolder->stock() != nullptr) {

                //TODO texture create from FlatBuffers::TextureStock
                TextureStock oStock{
                        pTextureHolder->stock()->image()->Data(),
                        pTextureHolder->stock()->image()->Length(),
                        pTextureHolder->stock()->format(),
                        pTextureHolder->stock()->internal_format(),
                        pTextureHolder->stock()->width(),
                        pTextureHolder->stock()->height()
                };

                pTex = LoadTex(
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
                return nullptr;
        }

        return pTex;
}

const UniformBlock * Material::GetUniformBlock() const {

        if (pBlock) { return pBlock.get(); }
        return nullptr;
}

const Material::TexturesMap * Material::GetTextures() const {

        return &mTextures;
}

std::string Material::StrDump(const size_t indent) const {

        std::string sResult = fmt::format("{:>{}} Material: shader: '{}'\n", ">", indent, pShader->Name());
        sResult += pBlock->StrDump(indent + 2) + "\n";

        sResult += fmt::format("{:>{}} Shader variables cnt: {}\n", ">", indent + 2, mShaderVariables.size());
        for (auto & oItem : mShaderVariables) {
                sResult += fmt::format("{:{}} Name id: {}\n",
                                ">",
                                indent + 4,
                                oItem.first);
                auto cur_indent = indent + 4;

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
                sResult += fmt::format("{:{}} TextureUnit: {}\n",
                                ">",
                                indent + 4,
                                static_cast<int32_t>(oTexItem.first));
                sResult += oTexItem.second->StrDump(indent + 4) + "\n";
        }

        return sResult;
}

}

