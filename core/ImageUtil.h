#ifndef __IMAGE_UTIL_H__
#define __IMAGE_UTIL_H__ 1

namespace SE {

struct ImageUtil {
        /** Write a 2D GL texture to a PNG file. Handles format conversion via glGetTexImage. */
        static bool WriteTexture2D(TTexture *pTex, const std::string &path);

        /** Write depth texture to PNG with linear depth visualization. */
        static bool WriteDepthTexture(TTexture *pTex, float nearZ, float farZ, const std::string &path);

        /** Read color attachment from bound FBO via glReadPixels and write to PNG. */
        static bool WriteReadBufferToFile(uint32_t fboId, uint32_t attachment,
                                          uint32_t w, uint32_t h, const std::string &path);
};

} // namespace SE

#endif
