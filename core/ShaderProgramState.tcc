
namespace SE {

using TShaderVariableTypesMap = MP::Type2IntDict<
        MP::TPair<float,        GL_FLOAT>,
        MP::TPair<glm::vec2,    GL_FLOAT_VEC2>,
        MP::TPair<glm::vec3,    GL_FLOAT_VEC3>,
        MP::TPair<glm::vec4,    GL_FLOAT_VEC4>,
        MP::TPair<glm::mat3,    GL_FLOAT_MAT3>,
        MP::TPair<glm::mat4,    GL_FLOAT_MAT4>,
        MP::TPair<glm::uvec2,   GL_UNSIGNED_INT_VEC2>,
        MP::TPair<glm::uvec3,   GL_UNSIGNED_INT_VEC3>,
        MP::TPair<glm::uvec4,   GL_UNSIGNED_INT_VEC4>,
        MP::TPair<uint32_t,     GL_UNSIGNED_INT>,
        MP::TPair<int32_t,      GL_INT>
        >;

UniformBlock::UniformBlock(ShaderProgram * pShader, const UniformUnitInfo::Type new_unit_id) : unit_id(new_unit_id) {

        pDesc = pShader->GetBlockDescriptor(unit_id);
        if (!pDesc) {
                throw(std::runtime_error(fmt::format("shader: '{}' does not contain uniform unit: '{}'",
                                                pShader->Name(),
                                                GetSystem<GraphicsState>().GetUniformUnitInfo(unit_id).sName)));

        }

        pBuffer = GetSystem<GraphicsState>().GetUniformBuffer(unit_id, pDesc->aligned_size);
        if (!pBuffer) {
                throw(std::runtime_error(fmt::format("failed to get buffer for block_size: {}, shader: '{}', unit: '{}'",
                                                pDesc->aligned_size,
                                                pShader->Name(),
                                                GetSystem<GraphicsState>().GetUniformUnitInfo(unit_id).sName )));
        }

        block_id = pBuffer->AllocateBlock();
}

UniformBlock::~UniformBlock() noexcept {
        pBuffer->ReleaseBlock(block_id);
}

bool UniformBlock::OwnVariable(const StrID name) const {
        return (pDesc->mVariables.find(name) != pDesc->mVariables.end());
}

template <class TArg> ret_code_t UniformBlock::SetValueInternal(const StrID name, const TArg & val) {

        auto it = pDesc->mVariables.find(name);

        if (it == pDesc->mVariables.end()) {
                log_e("uniform block '{}' does not contain variable: '{}'",
                                GetSystem<GraphicsState>().GetUniformUnitInfo(unit_id).sName,
                                name);
                return uWRONG_INPUT_DATA;
        }
        if (it->second.type != TShaderVariableTypesMap::Get<TArg>::value) {
                log_e("wrong type '{}', variable '{}' expect {}",
                                typeid(TArg).name(),
                                it->second.sName,
                                it->second.type);
                return uWRONG_INPUT_DATA;
        }

        return pBuffer->SetValue(block_id, it->second.location, &val, sizeof(TArg));
}

template <class TArg> ret_code_t UniformBlock::SetArrayElementInternal(const StrID name, const uint16_t index, const TArg & val) {

        auto it = pDesc->mVariables.find(name);

        if (it == pDesc->mVariables.end()) {
                log_e("uniform block '{}' does not contain variable: '{}'",
                                GetSystem<GraphicsState>().GetUniformUnitInfo(unit_id).sName,
                                name);
                return uWRONG_INPUT_DATA;
        }
        if (it->second.type != TShaderVariableTypesMap::Get<TArg>::value) {
                log_e("wrong type '{}', variable '{}' expect {}",
                                typeid(TArg).name(),
                                it->second.sName,
                                it->second.type);
                return uWRONG_INPUT_DATA;
        }
        if (index >= it->second.array_cnt) {
                log_e("wrong index {}, variable '{}' array len: {}",
                                typeid(TArg).name(),
                                it->second.sName,
                                it->second.array_cnt);
                return uWRONG_INPUT_DATA;
        }

        return pBuffer->SetValue(
                        block_id,
                        it->second.location + index * it->second.array_stride,
                        &val,
                        sizeof(TArg));
}

ret_code_t UniformBlock::SetVariable(const StrID name, float val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const glm::vec2 & val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const glm::vec3 & val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const glm::vec4 & val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const glm::uvec2 & val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const glm::uvec3 & val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const glm::uvec4 & val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const uint32_t val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const int32_t val) {
        return SetValueInternal(name, val);
}

ret_code_t UniformBlock::SetVariable(const StrID name, const float * pValue, const uint16_t count) {

        auto it = pDesc->mVariables.find(name);

        if (it == pDesc->mVariables.end()) {
                log_e("uniform block '{}' does not contain variable: '{}'",
                                GetSystem<GraphicsState>().GetUniformUnitInfo(unit_id).sName,
                                name);
                return uWRONG_INPUT_DATA;
        }
        if (!(it->second.type == GL_FLOAT_MAT4 ||
              it->second.type == GL_FLOAT_MAT3 ||
              it->second.type == GL_FLOAT ||
              it->second.type == GL_FLOAT_VEC2 ||
              it->second.type == GL_FLOAT_VEC3 ||
              it->second.type == GL_FLOAT_VEC4) ) {

                log_e("wrong type float *, variable '{}' expect {}",
                                it->second.sName,
                                it->second.type);
                return uWRONG_INPUT_DATA;
        }

        return pBuffer->SetValue(block_id, it->second.location, pValue, count * sizeof(float));
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, float val) {
        return SetArrayElementInternal(name, index, val);
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, const glm::vec2 & val) {
        return SetArrayElementInternal(name, index, val);
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, const glm::vec3 & val) {
        return SetArrayElementInternal(name, index, val);
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, const glm::vec4 & val) {
        return SetArrayElementInternal(name, index, val);
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, const glm::uvec2 & val) {
        return SetArrayElementInternal(name, index, val);
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, const glm::uvec3 & val) {
        return SetArrayElementInternal(name, index, val);
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, const glm::uvec4 & val) {
        return SetArrayElementInternal(name, index, val);
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, const glm::mat3 & val) {
        return SetArrayElementInternal(name, index, val);
}

ret_code_t UniformBlock::SetArrayElement(const StrID name, const uint16_t index, const glm::mat4 & val) {
        return SetArrayElementInternal(name, index, val);
}

template <class TArg> ret_code_t UniformBlock::GetValueInternal (const StrID name, const TArg *& pValue) const {

        auto it = pDesc->mVariables.find(name);

        if (it == pDesc->mVariables.end()) {
                log_e("uniform block '{}' does not contain variable: '{}'",
                                GetSystem<GraphicsState>().GetUniformUnitInfo(unit_id).sName,
                                name);
                return uWRONG_INPUT_DATA;
        }
        if (it->second.type != TShaderVariableTypesMap::Get<TArg>::value) {
                log_e("wrong type '{}', variable '{}' expect {}",
                                typeid(TArg).name(),
                                it->second.sName,
                                it->second.type);
                return uWRONG_INPUT_DATA;
        }

        return pBuffer->GetValue(block_id, it->second.location, reinterpret_cast<const void *&>(pValue), sizeof(TArg));
}

template <class TArg> ret_code_t UniformBlock::GetArrayElementInternal(const StrID name, const uint16_t index, const TArg *& pValue) const {

        auto it = pDesc->mVariables.find(name);

        if (it == pDesc->mVariables.end()) {
                log_e("uniform block '{}' does not contain variable: '{}'",
                                GetSystem<GraphicsState>().GetUniformUnitInfo(unit_id).sName,
                                name);
                return uWRONG_INPUT_DATA;
        }
        if (it->second.type != TShaderVariableTypesMap::Get<TArg>::value) {
                log_e("wrong type '{}', variable '{}' expect {}",
                                typeid(TArg).name(),
                                it->second.sName,
                                it->second.type);
                return uWRONG_INPUT_DATA;
        }
        if (index >= it->second.array_cnt) {
                log_e("wrong index {}, variable '{}' array len: {}",
                                typeid(TArg).name(),
                                it->second.sName,
                                it->second.array_cnt);
                return uWRONG_INPUT_DATA;
        }

        return pBuffer->GetValue(
                        block_id,
                        it->second.location + index * it->second.array_stride,
                        reinterpret_cast<const void *&>(pValue),
                        sizeof(TArg));
}

ret_code_t UniformBlock::GetVariable(const StrID name, const float *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetVariable(const StrID name, const glm::vec2 *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetVariable(const StrID name, const glm::vec3 *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetVariable(const StrID name, const glm::vec4 *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetVariable(const StrID name, const glm::uvec2 *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetVariable(const StrID name, const glm::uvec3 *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetVariable(const StrID name, const glm::uvec4 *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetVariable(const StrID name, const glm::mat3 *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetVariable(const StrID name, const glm::mat4 *& pValue) const {
        return GetValueInternal(name, pValue);
}

ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const float *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}
ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const glm::vec2 *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}

ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const glm::vec3 *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}

ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const glm::vec4 *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}

ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const glm::uvec2 *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}

ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const glm::uvec3 *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}

ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const glm::uvec4 *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}

ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const glm::mat3 *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}

ret_code_t UniformBlock::GetArrayElement(const StrID name, const uint16_t index, const glm::mat4 *& pValue) const {
        return GetArrayElementInternal(name, index, pValue);
}


void UniformBlock::Apply() const {
        pBuffer->Apply(block_id, unit_id);
}

std::string UniformBlock::StrDump(const size_t indent) const {

        const char * sData;
        pBuffer->GetValue(block_id, 0, reinterpret_cast<const void *&>(sData), pDesc->size);

        std::string sResult = fmt::format("{:>{}} UniformBlock: id: {}, UniformUnit: {}, data hash: {}\n",
                        ">",
                        indent,
                        block_id,
                        static_cast<int32_t>(unit_id),
                        StrID(sData, pDesc->size));
        sResult += pBuffer->StrDump(indent + 2) + "\n";
        sResult += pDesc->StrDump(indent + 2);

        return sResult;
}


ShaderProgramState::ShaderProgramState(const Material * pNewMaterial) :
        pShader(pNewMaterial->GetShader()),
        pMaterial(pNewMaterial) {

}


ret_code_t ShaderProgramState::SetBlock(const UniformUnitInfo::Type unit_id, const UniformBlock * pBlock) {

        if (!pShader->GetBlockDescriptor(unit_id)) {
                log_e("shader: '{}' does not contain block: '{}'",
                                pShader->Name(),
                                GetSystem<GraphicsState>().GetUniformUnitInfo(unit_id).sName);
                return uWRONG_INPUT_DATA;
        }
        mShaderBlocks.insert_or_assign(unit_id, pBlock);
        //TODO rehash

        return uSUCCESS;
}

ret_code_t ShaderProgramState::SetTextures(const UniformUnitInfo::Type unit_id, const TexturesMap * pTextures) {
        if (unit_id > UniformUnitInfo::Type::MAX) {
                log_e("wrong unit_id: '{}'", static_cast<uint8_t>(unit_id));
                return uWRONG_INPUT_DATA;
        }

        mTextures.insert_or_assign(unit_id, pTextures);
        return uSUCCESS;
}

ret_code_t ShaderProgramState::SetTexture(const TextureUnit unit_index, TTexture * pTex) {

        if (!pShader->OwnTextureUnit(unit_index)) {
                uint32_t unit_num = static_cast<uint32_t>(unit_index);
                log_e("texture unit {} unused, shader program: '{}'",
                                unit_num,
                                pShader->Name());
                return uWRONG_INPUT_DATA;
        }

        mDefaultTextures.insert_or_assign(unit_index, pTex);
        return uSUCCESS;
}

void ShaderProgramState::Apply() const {

        auto & oGraphicsState = GetSystem<GraphicsState>();

        oGraphicsState.SetShaderProgram(pShader);

        pMaterial->Apply();

        for (auto oTexMapItem : mTextures) {
                for (auto & oTexItem : *(oTexMapItem.second)) {
                        oGraphicsState.SetTexture(oTexItem.first, oTexItem.second);
                }
        }
        for (auto & oTexItem : mDefaultTextures) {
                oGraphicsState.SetTexture(oTexItem.first, oTexItem.second);
        }

        for (auto & oItem : mShaderBlocks) {
                oItem.second->Apply();
        }
}

std::string ShaderProgramState::StrDump(const size_t indent) const {

        std::string sResult = fmt::format("{:>{}} ShaderProgramState: shader: '{}'\n", ">", indent, pShader->Name());
        sResult += pMaterial->StrDump(indent + 2) + "\n";

        sResult += fmt::format("{:>{}} ShaderBlocks cnt: {}\n", ">", indent + 2, mShaderBlocks.size());
        for (auto & oItem : mShaderBlocks) {
                sResult += fmt::format("{:>{}} UniformUnit: {}\n", ">", indent + 4, static_cast<int32_t>(oItem.first));
                sResult += oItem.second->StrDump(indent + 4) + "\n";
        }

        sResult += fmt::format("{:>{}} Textures sets cnt: {}\n", ">", indent + 2, mTextures.size());
        for (auto & oItem : mTextures) {
                for (const auto & oTexItem : *oItem.second) {
                        sResult += fmt::format("{:>{}} block set: {}, TextureUnit: {}\n",
                                        ">",
                                        indent + 4,
                                        static_cast<int32_t>(oItem.first),
                                        static_cast<int32_t>(oTexItem.first));
                        sResult += oTexItem.second->StrDump(indent + 4) + "\n";
                }
        }

        sResult += fmt::format("{:>{}} default Textures set cnt: {}\n", ">", indent + 2, mDefaultTextures.size());
        for (auto & oTexItem : mDefaultTextures) {
                sResult += fmt::format("{:>{}} TextureUnit: {}\n",
                                ">",
                                indent + 4,
                                static_cast<int32_t>(oTexItem.first));
                sResult += oTexItem.second->StrDump(indent + 4) + "\n";
        }

        return sResult;
}


}
