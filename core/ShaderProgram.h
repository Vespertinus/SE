#ifndef __SHADER_PROGRAM_H__
#define __SHADER_PROGRAM_H__ 1

#include <ShaderProgram_generated.h>
#include <GLUtil.h>

namespace SE {

/*TODO move gl state manipulation into separate object
  check current state and decide when need to change it
  some kind of gl ctx, FSM like
  TODO init shader const from gl

  TODO create uniform variables storage, like buffer + ptr + type

  store used attributes list with it's types
*/

enum ShaderSystemVariables : uint32_t {

        MVPMatrix       = 0x1,
        MVMatrix        = 0x2,
        ScreenSize      = 0x4
};

struct ShaderVariable {

        std::string     sName;
        uint32_t        type;
        union {
                int             location;
                TextureUnit     unit_index;
        };

        //buffer
};

struct UniformBlockDescriptor {

        std::unordered_map<StrID, ShaderVariable> mVariables;
        uint16_t                                  size;
        uint16_t                                  aligned_size;
};


class ShaderProgram : public ResourceHolder {

        private:

        std::unordered_map<
                StrID,
                ShaderVariable>                 mVariables;
        std::unordered_map<
                StrID,
                ShaderVariable>                 mSamplers;
        std::unordered_map<
                UniformUnitInfo::Type,
                UniformBlockDescriptor>         mBlockDescriptors;
        uint32_t                                gl_id{};
        uint32_t                                used_system_variables{};
        uint16_t                                used_texture_units{};

        void Load(const FlatBuffers::ShaderProgram * pShaderProgram);
        void FillVariables(std::unordered_map<uint32_t, UniformUnitInfo::Type> & mBlockBinding);
        void FillUniformBlocks(std::unordered_map<uint32_t, UniformUnitInfo::Type> & mBlockBinding);

        public:
        ShaderProgram(const std::string & sName,
                      const rid_t         new_rid);
        ShaderProgram(const std::string & sName,
                      const rid_t         new_rid,
                      const SE::FlatBuffers::ShaderProgram * pShaderProgram);
        ~ShaderProgram() noexcept;

        void                    Use() const;
        //TODO rewrite on template version
        ret_code_t              SetVariable(const StrID name, float val);
        ret_code_t              SetVariable(const StrID name, const glm::vec2 & val);
        ret_code_t              SetVariable(const StrID name, const glm::vec3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::vec4 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec2 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::uvec4 & val);
        ret_code_t              SetVariable(const StrID name, const glm::mat3 & val);
        ret_code_t              SetVariable(const StrID name, const glm::mat4 & val);
        //and so on

        bool                    OwnVariable(const StrID name) const;
        bool                    OwnTexture(const StrID name) const; //---> TODO 2x
        bool                    OwnTextureUnit(const TextureUnit unit_index) const;
        std::optional<std::reference_wrapper<const ShaderVariable>>
                                GetTextureInfo(const StrID name) const;
        //ret_code_t              Validate() const; //check that all variables set via bitset + location, texture ???
        uint32_t                UsedSystemVariables() const;
        //TODO return shared ptr on self block
        const UniformBlockDescriptor *
                                GetBlockDescriptor(const UniformUnitInfo::Type unit_id) const;
};

} //namespace SE
#endif
