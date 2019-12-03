#ifndef __SHADER_PROGRAM_STATE_H__
#define __SHADER_PROGRAM_STATE_H__ 1


namespace SE {

class  UniformBuffer;
struct UniformBlockDescriptor;
class  Material;

class UniformBlock {

        std::shared_ptr<UniformBuffer>  pBuffer;
        const UniformBlockDescriptor  * pDesc;
        uint16_t                        block_id;
        UniformUnitInfo::Type           unit_id;

        template <class TArg> ret_code_t SetValueInternal(const StrID name, const TArg & val);
        template <class TArg> ret_code_t SetArrayElementInternal(
                        const StrID name,
                        const uint16_t index,
                        const TArg & val);
        template <class TArg> ret_code_t GetValueInternal (
                        const StrID name,
                        const TArg *& pValue) const;
        template <class TArg> ret_code_t GetArrayElementInternal(
                        const StrID name,
                        const uint16_t index,
                        const TArg *& pValue) const;

        public:

        UniformBlock(ShaderProgram * pShader, const UniformUnitInfo::Type new_unit_id);
        ~UniformBlock() noexcept;

        ret_code_t              SetVariable(const StrID name, float val);
        ret_code_t              SetVariable(const StrID name, const glm::vec2 & val);
        ret_code_t              SetVariable(const StrID name, const glm::vec3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::vec4 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec2 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec4 & val);
        ret_code_t              SetVariable(const StrID name, const glm::mat3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::mat4 & val);
        ret_code_t              SetVariable(const StrID name, const uint32_t val);
        ret_code_t              SetVariable(const StrID name, const int32_t val);
        ret_code_t              SetVariable(const StrID name, const float * pValue, const uint16_t count);
        ret_code_t              SetVariable(const StrID name, const uint32_t * pValue, const uint16_t count);

        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, float val);
        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, const glm::vec2 & val);
        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, const glm::vec3 & val);
        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, const glm::vec4 & val);
        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, const glm::uvec2 & val);
        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, const glm::uvec3 & val);
        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, const glm::uvec4 & val);
        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, const glm::mat3 & val);
        ret_code_t              SetArrayElement(const StrID name, const uint16_t index, const glm::mat4 & val);

        ret_code_t              GetVariable(const StrID name, const float *& pValue) const;
        ret_code_t              GetVariable(const StrID name, const glm::vec2 *& pValue) const;
        ret_code_t              GetVariable(const StrID name, const glm::vec3 *& pValue) const;
        ret_code_t              GetVariable(const StrID name, const glm::vec4 *& pValue) const;
        ret_code_t              GetVariable(const StrID name, const glm::uvec2 *& pValue) const;
        ret_code_t              GetVariable(const StrID name, const glm::uvec3 *& pValue) const;
        ret_code_t              GetVariable(const StrID name, const glm::uvec4 *& pValue) const;
        ret_code_t              GetVariable(const StrID name, const glm::mat3 *& pValue) const;
        ret_code_t              GetVariable(const StrID name, const glm::mat4 *& pValue) const;

        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const float *& pValue) const;
        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const glm::vec2 *& pValue) const;
        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const glm::vec3 *& pValue) const;
        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const glm::vec4 *& pValue) const;
        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const glm::uvec2 *& pValue) const;
        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const glm::uvec3 *& pValue) const;
        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const glm::uvec4 *& pValue) const;
        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const glm::mat3 *& pValue) const;
        ret_code_t              GetArrayElement(const StrID name, const uint16_t index, const glm::mat4 *& pValue) const;

        bool                    OwnVariable(const StrID name) const;
        void                    Apply() const;
        std::string             StrDump(const size_t indent) const;
};

class ShaderProgramState {

        using TexturesMap = std::unordered_map<TextureUnit, TTexture *>;

        ShaderProgram         * pShader;
        /** material also must contain render state settings (blending, depth, etc) */
        const Material        * pMaterial;//TODO remove material
        //TODO later rewrite on std::array<, MAX_UNIT(16)>
        std::unordered_map<UniformUnitInfo::Type, const UniformBlock *> mShaderBlocks;
        std::unordered_map<UniformUnitInfo::Type, const TexturesMap * > mTextures;//THINK remove mapping for each Uniform Block store only
        TexturesMap             mDefaultTextures;
        //uint64_t                hash; //from tex + uniform buf id + block_id

        public:

        ShaderProgramState(const Material * pMaterial);

        ret_code_t              SetBlock(const UniformUnitInfo::Type unit_id, const UniformBlock * pBlock);
        ret_code_t              SetTextures(const UniformUnitInfo::Type unit_id, const TexturesMap * pTextures);
        ret_code_t              SetTexture(const TextureUnit unit_index, TTexture * pTex);
        //GetBlock ?
        ret_code_t              Validate() const;
        void                    Apply() const;
        //uint64_t                GetSortKey() const;
        std::string             StrDump(const size_t indent) const;
};


} //namespace SE
#endif

