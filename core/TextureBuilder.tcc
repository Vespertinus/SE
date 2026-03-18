
#ifdef SE_IMPL

#include <TextureBuilder.h>
#include <TextureStock.h>
#include <StoreTexture2D.h>

#include <algorithm>
#include <cstring>

namespace SE {

// GL format -> bytes per pixel
static uint32_t BppFromFormat(int fmt) {
        switch (fmt) {
                case 0x1908: return 4; // GL_RGBA
                case 0x1907: return 3; // GL_RGB
                case 0x1903: return 1; // GL_RED
                case 0x8227: return 2; // GL_RG
                default:     return 4;
        }
}

TextureBuilder::TextureBuilder(uint32_t w, uint32_t h,
                               int internal_fmt, int fmt)
        : width(w), height(h)
        , internal_format(internal_fmt), format(fmt)
        , bpp(BppFromFormat(fmt))
        , vPixels(w * h * bpp, 0)
{
}

uint8_t * TextureBuilder::PixelAt(uint32_t x, uint32_t y) {
        return vPixels.data() + (y * width + x) * bpp;
}

TextureBuilder & TextureBuilder::Fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        for (uint32_t y = 0; y < height; ++y)
                for (uint32_t x = 0; x < width; ++x)
                        SetPixel(x, y, r, g, b, a);
        return *this;
}

TextureBuilder & TextureBuilder::SetPixel(uint32_t x, uint32_t y,
                                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x >= width || y >= height) return *this;
        uint8_t * p = PixelAt(x, y);
        if (bpp >= 1) p[0] = r;
        if (bpp >= 2) p[1] = (bpp == 2) ? g : g; // RG: store g in ch1
        if (bpp >= 3) p[2] = b;
        if (bpp >= 4) p[3] = a;
        return *this;
}

TextureBuilder & TextureBuilder::FillRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        uint32_t x1 = std::min(x + w, width);
        uint32_t y1 = std::min(y + h, height);
        for (uint32_t py = y; py < y1; ++py)
                for (uint32_t px = x; px < x1; ++px)
                        SetPixel(px, py, r, g, b, a);
        return *this;
}

TextureBuilder & TextureBuilder::FillCheckerboard(uint8_t r0, uint8_t g0, uint8_t b0,
                                                    uint8_t r1, uint8_t g1, uint8_t b1,
                                                    uint32_t cell_size) {
        for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                        bool even = ((x / cell_size) + (y / cell_size)) % 2 == 0;
                        if (even)
                                SetPixel(x, y, r0, g0, b0);
                        else
                                SetPixel(x, y, r1, g1, b1);
                }
        }
        return *this;
}

TextureBuilder & TextureBuilder::FillGradient(uint8_t r0, uint8_t g0, uint8_t b0,
                                               uint8_t r1, uint8_t g1, uint8_t b1,
                                               bool horizontal) {
        for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                        float t = horizontal
                                ? static_cast<float>(x) / (width  > 1 ? width  - 1 : 1)
                                : static_cast<float>(y) / (height > 1 ? height - 1 : 1);
                        uint8_t r = static_cast<uint8_t>(r0 + t * (r1 - r0));
                        uint8_t g = static_cast<uint8_t>(g0 + t * (g1 - g0));
                        uint8_t b = static_cast<uint8_t>(b0 + t * (b1 - b0));
                        SetPixel(x, y, r, g, b);
                }
        }
        return *this;
}

TextureBuilder & TextureBuilder::Blur(uint32_t radius) {
        if (radius == 0) return *this;
        std::vector<uint8_t> vTmp = vPixels;

        for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                        uint32_t acc[4] = { 0, 0, 0, 0 };
                        uint32_t cnt    = 0;
                        for (uint32_t ky = (y >= radius ? y - radius : 0);
                             ky <= std::min(y + radius, height - 1); ++ky) {
                                for (uint32_t kx = (x >= radius ? x - radius : 0);
                                     kx <= std::min(x + radius, width - 1); ++kx) {
                                        const uint8_t * src = vTmp.data() + (ky * width + kx) * bpp;
                                        for (uint32_t c = 0; c < bpp; ++c) acc[c] += src[c];
                                        ++cnt;
                                }
                        }
                        uint8_t * dst = PixelAt(x, y);
                        for (uint32_t c = 0; c < bpp; ++c) dst[c] = static_cast<uint8_t>(acc[c] / cnt);
                }
        }
        return *this;
}

H<TTexture> TextureBuilder::Upload(const char * name) {
        TextureStock stock;
        stock.raw_image      = vPixels.data();
        stock.raw_image_size = static_cast<uint32_t>(vPixels.size());
        stock.format         = format;
        stock.internal_format= internal_format;
        stock.width          = width;
        stock.height         = height;

        StoreTexture2D::Settings store_settings;
        return CreateResource<TTexture>(name, stock, store_settings);
}

} // namespace SE

#endif // SE_IMPL
