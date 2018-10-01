

#ifndef __TEXTURE_STOCK_H_
#define __TEXTURE_STOCK_H_ 1

namespace SE {

struct TextureStock {

        uint8_t   * raw_image;
        uint32_t    raw_image_size;

        //GL_RGB, GL_BGRA and so on
        int        format;
        /** input data type GL_RGBA8, GL_R32F etc */
        int        internal_format;

        uint32_t    width;
        uint32_t    height;
};

} //namespace SE

#endif
