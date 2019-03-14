
#ifndef __MATERIAL_H__
#define __MATERIAL_H__ 1

#include <Material_generated.h>

namespace SE {


class Material : public ResourceHolder {

        //TODO store in buffer, sizeof max TVariant item 16, but in general, 12?
        using TVariant    = std::variant<
                float,
                int32_t,
                glm::vec2,
                glm::vec3,
                glm::vec4,
                glm::uvec2,
                glm::uvec3,
                glm::uvec4>;
        using TexturesMap = std::unordered_map<TextureUnit, TTexture *>;

        SE::ShaderProgram                             * pShader;
        std::unique_ptr<UniformBlock>                   pBlock;
        std::unordered_map<StrID, TVariant>             mShaderVariables;
        //std::array<TTexture *, max texture units> vTextures; ...
        std::unordered_map<TextureUnit, TTexture *>     mTextures;

        void Load(const SE::FlatBuffers::Material * pMaterial);

        public:

        Material(const std::string & sName, const rid_t new_rid);
        /** create material with default values */
        Material(const std::string & sName, const rid_t new_rid, SE::ShaderProgram * pNewShader);
        Material(const std::string & sName,
                 const rid_t new_rid,
                 const SE::FlatBuffers::Material * pMaterial);
        //Material from input data (ctx\settings), like Texture

        //SetShader does not allow to change shader, ShaderProgramState and many other things would broken
        ret_code_t                      SetTexture(const StrID name, TTexture * pTex);
        ret_code_t                      SetTexture(const TextureUnit unit_index, TTexture * pTex);
        template <class T> ret_code_t   SetVariable(const StrID name, const T & val);
        //SetArrayData struct?
        void                            Apply() const;
        TTexture *                      GetTexture(const TextureUnit unit_index) const;
        ShaderProgram *                 GetShader() const;
        const UniformBlock *            GetUniformBlock() const;
        const TexturesMap *             GetTextures() const;

        std::string     Str() const;
};


template <class T> ret_code_t Material::SetVariable(const StrID name, const T & val) {

        //TODO check variable type
        if (pShader->OwnVariable(name)) {
                mShaderVariables.insert_or_assign(name, val);
                return uSUCCESS;
        }

        ret_code_t res = uWRONG_INPUT_DATA;
        if (!pBlock || (res = pBlock->SetVariable(name, val)) != uSUCCESS ) {
                log_w("shader '{}' does not have variable: '{}'", pShader->Name(), name);
                return res;
        }
        return uSUCCESS;
}


TTexture * LoadTexture(const SE::FlatBuffers::TextureHolder * pTexHolder);


} //namespace SE

#endif
