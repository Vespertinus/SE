
#ifndef BLOOM_BUFFER_H
#define BLOOM_BUFFER_H 1

#include <FrameBuffer.h>
#include <glm/vec2.hpp>

namespace SE {

class BloomBuffer {

        static constexpr int kMipLevels = 4;

        FrameBuffer oFboDown[kMipLevels];
        H<TTexture> hDownTex[kMipLevels];

        FrameBuffer oFboUp[kMipLevels - 1];
        H<TTexture> hUpTex[kMipLevels - 1];

        glm::uvec2 size        { 0 };
        bool       initialized { false };

        void Create();
        void Destroy() noexcept;

public:

        static constexpr int MipCount = kMipLevels;

        ~BloomBuffer() noexcept;

        void Resize(glm::uvec2 new_size);
        bool IsInitialized() const { return initialized; }
        glm::uvec2 MipSize(int level) const;  // size >> level  (level 0 = W/2, H/2)

        void BindDown(int level);
        void BindUp  (int level);
        void Unbind  ();

        TTexture * GetDownTex(int level) const;
        TTexture * GetUpTex  (int level) const;
        TTexture * GetBloomTex()         const { return GetUpTex(0); }
};

} // namespace SE
#endif
