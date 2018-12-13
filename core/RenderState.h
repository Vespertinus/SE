
#ifndef __RENDER_STATE_H__
#define __RENDER_STATE_H__ 1

#include <GLUtil.h>
#include <Chrono.h>

namespace SE {

/*
 basic stub for render state managing

 all arguments for Set* must be alive until the end of frame
 so it is not acceptable to store local variables
 */
class RenderState {

        const glm::mat4       * pModelViewProjection;
        const glm::mat4       * pTransformMat;
        ShaderProgram         * pShader;
        uint32_t                cur_vao;
        glm::uvec2              screen_size;
        time_point <micro>      frame_start_time;
        float                   last_frame_time;
        uint32_t                active_tex_unit{};

        //TODO initialize from GL limit, store in global graphics config
        std::array<TTexture *, 16> vTextureUnits{};

        //TODO
        //variables

        public:

        RenderState();

        template <class T> ret_code_t   SetVariable(const StrID name, const T & val);
        ret_code_t                      SetTexture(const TextureUnit unit_index, TTexture * pTex);
        ret_code_t                      SetTexture(const StrID name, TTexture * pTex);
        void                            SetTransform(const glm::mat4 & oMat);
        void                            SetViewProjection(const glm::mat4 & oMat);
        void                            SetShaderProgram(ShaderProgram * pNewShader);
        void                            SetScreenSize(const uint32_t width, const uint32_t height);
        void                            SetVao(const uint32_t vao_id);
        void                            Draw(const uint32_t vao_id,
                                             const uint32_t mode,
                                             const uint32_t index_type,
                                             const uint32_t start,
                                             const uint32_t count);
        void                            DrawArrays(const uint32_t vao_id,
                                                   const uint32_t mode,
                                                   const uint32_t start,
                                                   const uint32_t count);
        void                            FrameStart();
        const glm::uvec2 &              GetScreenSize() const;
        float                           GetLastFrameTime() const;

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
