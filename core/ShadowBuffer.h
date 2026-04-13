
#ifndef SHADOW_BUFFER_H
#define SHADOW_BUFFER_H 1

#include <GLUtil.h>
#include <FrameBuffer.h>

namespace SE {

class ShadowBuffer {

        static constexpr uint32_t SHADOW_MAP_SIZE = 2048;

        FrameBuffer      oFbo;
        H<TTexture> hDepthTex;

        void Create();
        void Destroy() noexcept;

public:

        ~ShadowBuffer() noexcept;
        void Init();

        void Bind();
        void Unbind();

        TTexture * GetDepthTex() const;
        static constexpr uint32_t Size() { return SHADOW_MAP_SIZE; }
};

} // namespace SE

#endif
