
#ifndef __RENDER_STATE_H__
#define __RENDER_STATE_H__ 1

#include <GLUtil.h>

namespace SE {

/*
 basic stub for render state managing

 all arguments for Set* must be alive until the end of frame
 so it is not acceptable to store local variables
 */
class RenderState {

        const glm::mat4 * pModelViewProjection;
        const glm::mat4 * pTransformMat;
        ShaderProgram   * pShader;
        uint32_t          cur_vao;
        glm::uvec2        screen_size;

        //TODO
        //texture units
        //variables

        public:

        RenderState();

        template <class T> ret_code_t   SetVariable(const StrID name, const T & val);
        ret_code_t                      SetTexture(const TextureUnit unit_index, const TTexture * pTex);
        ret_code_t                      SetTexture(const StrID name, const TTexture * pTex);
        void                            SetTransform(const glm::mat4 & oMat);
        void                            SetViewProjection(const glm::mat4 & oMat);
        void                            SetShaderProgram(ShaderProgram * pNewShader);
        void                            SetScreenSize(const uint32_t width, const uint32_t height);
        void                            Draw(const uint32_t vao_id,
                                             const uint32_t triangles_cnt,
                                             const uint32_t gl_index_type);
        void                            DrawArrays(const uint32_t vao_id,
                                                   const uint32_t mode,
                                                   const uint32_t first,
                                                   const uint32_t count);
        void                            Reset(); //TODO --> frame start

};

template <class T> ret_code_t RenderState::SetVariable(const StrID name, const T & val) {

        if (pShader) {
                return pShader->SetVariable(name, val);
        }

        log_w("shader not set, var: '{}'", name);
        return uLOGIC_ERROR;
}

typedef Loki::SingletonHolder < RenderState >   TRenderState;

} // namespace SE


#ifdef SE_IMPL
#include <RenderState.tcc>
#endif

#endif
