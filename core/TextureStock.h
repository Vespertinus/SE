

#ifndef __TEXTURE_STOCK_H_
#define __TEXTURE_STOCK_H_ 1

namespace SE {

const uint8_t uUNCOMPRESSED_TEXTURE   = 0;
const uint8_t uDXT3_TEXTURE           = 3;
//TODO other types;

struct TextureStock {

        uint8_t   * raw_image;
        uint32_t    raw_image_size;

        uint16_t    width;
        uint16_t    height;

        /** bytes per pixel */
        uint8_t     bpp;

        //TODO compressed texture stuff
        uint8_t     compressed;


        //GL_RGB, GL_BGRA and so on
        int        color_order;

        ~TextureStock() throw() {
                if (raw_image) { delete[] raw_image; }
        }
};

} //namespace SE

#endif
