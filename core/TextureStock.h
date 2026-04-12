

#ifndef __TEXTURE_STOCK_H_
#define __TEXTURE_STOCK_H_ 1

#include <vector>
#include <cstdint>

namespace SE {

struct TextureStock {

        const uint8_t * raw_image     {};
        uint32_t        raw_image_size {};

        //GL_RGB, GL_BGRA and so on
        int             format         {};
        /** input data type GL_RGBA8, GL_R32F etc */
        int             internal_format{};

        uint32_t        width  {};
        uint32_t        height {};

        /** GL pixel data type (e.g. GL_UNSIGNED_BYTE=0x1401, GL_UNSIGNED_INT_10F_11F_11F_REV=0x8C3A).
         *  0 means "not set"; Store strategies that need a type fall back to GL_UNSIGNED_BYTE. */
        uint32_t        gl_type   { 0 };
        /** 1 for 2-D textures, 6 for cubemaps (KTX path). */
        uint32_t        num_faces { 1 };
        /** Number of mip levels present in raw_image (KTX path). */
        uint32_t        num_mips  { 1 };
        /** Owned backing buffer; raw_image may point into this (KTXLoader sets it). */
        std::vector<uint8_t> vOwnedData;
};

} //namespace SE

#endif
