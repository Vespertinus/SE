#ifndef __SHADER_PROGRAM_STATE_H__
#define __SHADER_PROGRAM_STATE_H__ 1


namespace SE {

struct UniformBuffer;
struct UniformBlockDescriptor;

class UniformBlock {

        std::shared_ptr<UniformBuffer>  pBuffer;
        const UniformBlockDescriptor  * pDesc;
        uint16_t                        block_id;
        UniformUnitInfo::Type           unit_id;

        template <class TArg> ret_code_t SetValueInternal(const StrID name, const TArg & val);

        public:

        UniformBlock(ShaderProgram * pShader, const UniformUnitInfo::Type new_unit_id);
        ret_code_t              SetVariable(const StrID name, float val);
        ret_code_t              SetVariable(const StrID name, const glm::vec2 & val);
        ret_code_t              SetVariable(const StrID name, const glm::vec3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::vec4 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec2 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec4 & val);
        ret_code_t              SetVariable(const StrID name, const glm::mat3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::mat4 & val);
        ret_code_t              SetVariable(const StrID name, const float * pValue, const uint32_t count);
        //TODO SetVariable for int array
        //and struct?
        bool                    OwnVariable(const StrID name) const;
        //TODO GetVariable
        void                    Apply() const;
};

class ShaderProgramState {

        ShaderProgram         * pShader;
        std::unordered_map<UniformUnitInfo::Type, UniformBlock *> mShaderBlocks;
        //textures map, per unit info? or list
        //uniform variables from material
        uint64_t                hash; //from tex + uniform buf id + block_id

        public:

        ShaderProgramState(ShaderProgram * pNewShader);

        ret_code_t              SetBlock(const UniformUnitInfo::Type unit_id, UniformBlock * pBlock);
        //GetBlock ?
        //Set Tex?
        //Set default uniform group
        ret_code_t              Validate() const;
        void                    Apply() const;
        uint64_t                GetSortKey() const;
};


} //namespace SE

#ifdef SE_IMPL
#include <ShaderProgramState.tcc>
#endif

#endif





