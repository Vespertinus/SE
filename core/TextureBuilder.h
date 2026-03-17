
#ifndef __TEXTURE_BUILDER_H__
#define __TEXTURE_BUILDER_H__ 1

#include <vector>
#include <cstdint>
#include <ResourceHandle.h>

namespace SE {

class TextureBuilder {

        uint32_t             width, height;
        int                  internal_format;
        int                  format;
        uint32_t             bpp;            // bytes per pixel
        std::vector<uint8_t> vPixels;

        uint8_t * PixelAt(uint32_t x, uint32_t y);

public:
        // internal_format defaults to GL_RGBA8 (0x8058), format to GL_RGBA (0x1908)
        TextureBuilder(uint32_t width, uint32_t height,
                       int internal_format = 0x8058 /*GL_RGBA8*/,
                       int format          = 0x1908 /*GL_RGBA*/);

        TextureBuilder & Fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        TextureBuilder & SetPixel(uint32_t x, uint32_t y,
                                  uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        TextureBuilder & FillRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                  uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        TextureBuilder & FillCheckerboard(uint8_t r0, uint8_t g0, uint8_t b0,
                                          uint8_t r1, uint8_t g1, uint8_t b1,
                                          uint32_t cell_size = 8);
        TextureBuilder & FillGradient(uint8_t r0, uint8_t g0, uint8_t b0,
                                      uint8_t r1, uint8_t g1, uint8_t b1,
                                      bool horizontal = true);
        TextureBuilder & Blur(uint32_t radius);

        H<TTexture> Upload(const char * name);
};

} // namespace SE

#endif
