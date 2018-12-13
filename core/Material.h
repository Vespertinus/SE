
#ifndef __MATERIAL_H__
#define __MATERIAL_H__ 1

#include <Material_generated.h>

namespace SE {

class Material : public ResourceHolder {

        //TODO store in buffer, sizeof max TVariant item 16, but in general, 12?
        using TVariant = std::variant<float, glm::vec2, glm::vec3, glm::vec4, glm::uvec2, glm::uvec3, glm::uvec4>;

        SE::ShaderProgram                             * pShader;
        //TODO later rewrite on Uniform Buffer Object
        std::unordered_map<StrID, TVariant>             mShaderVariables;
        std::unordered_map<TextureUnit, TTexture *>           mTextures;

        void Load(const SE::FlatBuffers::Material * pMaterial);

        public:
        Material(const std::string & sName, const rid_t new_rid);
        /** create material with default values */
        Material(const std::string & sName, const rid_t new_rid, SE::ShaderProgram * pNewShader);
        Material(const std::string & sName,
                 const rid_t new_rid,
                 const SE::FlatBuffers::Material * pMaterial);
        //Material from input data (ctx\settings), like Texture

        void                            SetShader(SE::ShaderProgram * pNewShader);
        ret_code_t                      SetTexture(const StrID name, const TTexture * pTex);
        //ret_code_t                      SetTexture(const TextureUnit unit_index, const TTexture * pTex);
        template <class T> ret_code_t   SetVariable(const StrID name, const T & val);
        void                            Apply() const;

        std::string     Str() const;
};


template <class T> ret_code_t   Material::SetVariable(const StrID name, const T & val) {

        //TODO check variable type
        if (!pShader->OwnVariable(name)) {
                log_w("shader '{}' does not have variable: '{}'", pShader->Name(), name);
                return uWRONG_INPUT_DATA;
        }
        mShaderVariables.emplace(name, val);
        return uSUCCESS;
}


} //namespace SE

#ifdef SE_IMPL
#include <Material.tcc>
#endif

#endif
