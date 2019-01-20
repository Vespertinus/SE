
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
        MP::TPair<glm::uvec4,   GL_UNSIGNED_INT_VEC4>
        >;

UniformBlock::UniformBlock(ShaderProgram * pShader, const UniformUnitInfo::Type new_unit_id) : unit_id(new_unit_id) {

        pDesc = pShader->GetBlockDescriptor(unit_id);
        if (!pDesc) {
                throw(std::runtime_error(fmt::format("shader: '{}' does not contain uniform unit: '{}'",
                                                pShader->Name(),
                                                TGraphicsState::Instance().GetUniformUnitInfo(unit_id).sName)));

        }

        pBuffer = TGraphicsState::Instance().GetUniformBuffer(unit_id, pDesc->aligned_size);
        if (!pBuffer) {
                throw(std::runtime_error(fmt::format("failed to get buffer for block_size: {}, shader: '{}', unit: '{}'",
                                                pDesc->aligned_size,
                                                pShader->Name(),
                                                TGraphicsState::Instance().GetUniformUnitInfo(unit_id).sName )));
        }

        block_id = pBuffer->AllocateBlock();
}

bool UniformBlock::OwnVariable(const StrID name) const {
        return (pDesc->mVariables.find(name) != pDesc->mVariables.end());
}

ret_code_t UniformBlock::SetVariable(const StrID name, const float * pValue, const uint32_t count) {

        auto it = pDesc->mVariables.find(name);

        if (it == pDesc->mVariables.end()) {
                //TODO print atleast shader and unit name
                log_e("uniform block '{}' does not contain variable: '{}'",
                                TGraphicsState::Instance().GetUniformUnitInfo(unit_id).sName,
                                name);
                return uWRONG_INPUT_DATA;
        }

        return pBuffer->SetValue(block_id, it->second.location, pValue, sizeof(float) * count);
}

template <class TArg> ret_code_t UniformBlock::SetValueInternal(const StrID name, const TArg & val) {

        auto it = pDesc->mVariables.find(name);

        if (it == pDesc->mVariables.end()) {
                log_e("uniform block '{}' does not contain variable: '{}'",
                                TGraphicsState::Instance().GetUniformUnitInfo(unit_id).sName,
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

void UniformBlock::Apply() const {
        pBuffer->Apply(block_id, unit_id);
}


ShaderProgramState::ShaderProgramState(ShaderProgram * pNewShader) : pShader(pNewShader) {

}


ret_code_t ShaderProgramState::SetBlock(const UniformUnitInfo::Type unit_id, UniformBlock * pBlock) {

        if (!pShader->GetBlockDescriptor(unit_id)) {
                log_e("shader: '{}' does not contain block: '{}'",
                                pShader->Name(),
                                TGraphicsState::Instance().GetUniformUnitInfo(unit_id).sName);
                return uWRONG_INPUT_DATA;
        }
        mShaderBlocks.insert_or_assign(unit_id, pBlock);
        //TODO rehash

        return uSUCCESS;
}

void ShaderProgramState::Apply() const {

        TGraphicsState::Instance().SetShaderProgram(pShader);

        //TODO set textures
        //set uniform values without block

        for (auto & oItem : mShaderBlocks) {
                oItem.second->Apply();
        }
}


}
