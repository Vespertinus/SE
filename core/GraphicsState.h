
#ifndef __GRAPHICS_STATE_H__
#define __GRAPHICS_STATE_H__ 1

#include <GLUtil.h>
#include <Chrono.h>

namespace SE {

class UniformBuffer;

struct FrameState {
        uint32_t                frame_num{0};
        float                   last_frame_time{1/60};
        //time_point <micro>      frame_start_time;
        glm::uvec2              screen_size{800, 600};
};

/*
 basic stub for render state managing

 all arguments for Set* must be alive until the end of frame
 so it is not acceptable to store local variables
 */
class GraphicsState {

        const glm::mat4       * pModelViewProjection;
        const glm::mat4       * pTransformMat;
        ShaderProgram         * pShader;
        uint32_t                cur_vao;
        FrameState              oFrame;
        time_point <micro>      frame_start_time;
        uint32_t                active_tex_unit{};
        uint32_t                active_ubo{};
        glm::vec4               vClearColor{0.0f};
        float                   clear_depth{1.0f};
        DepthFunc               depth_func{DepthFunc::LESS};
        bool                    depth_test{false};
        bool                    depth_write{true};
        bool                    color_write{true};

        //TODO initialize from GL limit, store in global graphics config
        std::array<TTexture *, 16> vTextureUnits{};
        //std::array<UniformBuffer *, 16> vUniformUnits{};
        //THINK possibly could use shader with binded but deleted ubo..
        std::array<uint32_t, 16> vUniformUnits{};
        std::array<uint32_t, 16> vUniformRanges{};

        //TODO
        //variables

        std::unordered_map<
                uint32_t,
                std::weak_ptr<UniformBuffer> >  mUniformBuffers;

        std::array<UniformUnitInfo, 16> vUniformUnitInfo = {{

                {"Transform",  1000 },
                {"Material",   100 },
                {"Camera",     10 },
                {"Animation",  100 },
                {"Object",     100 },
                {"Lighting",   10 },
                {"Err",        10 },
                {"CUSTOM",     10 }
        }};

        public:

        GraphicsState();

        template <class T> ret_code_t   SetVariable(const StrID name, const T & val);
        ret_code_t                      SetTexture(const TextureUnit unit_index, TTexture * pTex);
        ret_code_t                      SetTexture(const StrID name, TTexture * pTex);
        void                            SetTransform(const glm::mat4 & oMat);
        void                            SetViewProjection(const glm::mat4 & oMat);
        void                            SetShaderProgram(ShaderProgram * pNewShader);
        void                            SetScreenSize(const uint32_t width, const uint32_t height);
        void                            SetScreenSize(const glm::uvec2 new_screen_size);
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
        float                           FrameStart();
        const glm::uvec2 &              GetScreenSize() const;
        float                           GetLastFrameTime() const;
        const FrameState &              GetFrameState() const;
        //TODO need to reset range in binded array
        void                            UploadUniformBufferData(const uint32_t buf_id,
                                                                const uint32_t buf_size,
                                                                const void * pData);
        void                            UploadUniformBufferSubData(const uint32_t buf_id,
                                                                   const uint32_t buf_offset,
                                                                   const uint32_t block_size,
                                                                   const void * pData);
        void                            BindUniformBufferRange(const uint32_t buf_id,
                                                               const UniformUnitInfo::Type unit_id,
                                                               const uint32_t buf_offset,
                                                               const uint32_t block_size);
        std::shared_ptr<UniformBuffer>
                                        GetUniformBuffer(
                                                        const UniformUnitInfo::Type unit_id,
                                                        const uint16_t block_size);
        //void CleanUnused remove expired weak_ptr
        const UniformUnitInfo         & GetUniformUnitInfo(const UniformUnitInfo::Type unit_id) const;
        //Set UniformUnitInfo options
        void                            SetClearColor(const glm::vec4 & vColor);
        void                            SetClearColor(const float r, const float g, const float b, const float a);
        void                            SetClearDepth(const float value);

        void                            SetDepthFunc(const DepthFunc value);
        void                            SetDepthTest(const bool enable);
        void                            SetDepthMask(const bool enable);
        void                            SetColorMask(const bool enable);
        /*
        void                            Clear(const ClearBufferType flags);
        void                            SetCullMode(const CullType value);*/
        //void                            SetBlendMode(type, alpha to coverage?);

};

template <class T> ret_code_t GraphicsState::SetVariable(const StrID name, const T & val) {

        if (pShader) {
                return pShader->SetVariable(name, val);
        }

        log_w("shader not set, var: '{}'", name);
        return uLOGIC_ERROR;
}

} // namespace SE

#endif
