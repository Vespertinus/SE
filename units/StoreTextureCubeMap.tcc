
namespace SE {

static constexpr std::array<std::pair<uint32_t, const char*>, 6> kCubeFaces = {{
        { GL_TEXTURE_CUBE_MAP_POSITIVE_X, "px.png" },
        { GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "nx.png" },
        { GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "py.png" },
        { GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "ny.png" },
        { GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "pz.png" },
        { GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "nz.png" },
}};

StoreTextureCubeMap::StoreTextureCubeMap(const Settings & s) : oSettings(s) {}

ret_code_t StoreTextureCubeMap::Store(const TextureStock & oTextureStock, uint32_t & id, uint32_t & gl_type) {

        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id);

        if (oTextureStock.num_faces == 6 && oTextureStock.raw_image) {
                // --- KTX cubemap: data already loaded by KTXLoader ---
                const uint8_t * ptr   = oTextureStock.raw_image;
                const uint8_t * end   = ptr + oTextureStock.raw_image_size;
                uint32_t        mip_w = oTextureStock.width;
                uint32_t        mip_h = oTextureStock.height;

                for (uint32_t mip = 0; mip < oTextureStock.num_mips; ++mip) {
                        if (ptr + 4 > end) {
                                log_e("StoreTextureCubeMap: KTX data truncated at mip {}", mip);
                                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                                glDeleteTextures(1, &id); id = 0;
                                return uWRONG_INPUT_DATA;
                        }
                        const uint32_t face_size = *reinterpret_cast<const uint32_t*>(ptr);
                        ptr += 4;

                        for (uint32_t face = 0; face < 6; ++face) {
                                if (ptr + face_size > end) {
                                        log_e("StoreTextureCubeMap: KTX data truncated at face {}", face);
                                        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                                        glDeleteTextures(1, &id); id = 0;
                                        return uWRONG_INPUT_DATA;
                                }
                                glTexImage2D(
                                        static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face),
                                        static_cast<GLint>(mip),
                                        static_cast<GLint>(oTextureStock.internal_format),
                                        static_cast<GLsizei>(mip_w),
                                        static_cast<GLsizei>(mip_h),
                                        0,
                                        static_cast<GLenum>(oTextureStock.format),
                                        static_cast<GLenum>(oTextureStock.gl_type),
                                        ptr);
                                ptr += face_size;
                                ptr += (4 - (face_size % 4)) % 4;  // KTX face padding to 4 bytes
                        }
                        mip_w = std::max(1u, mip_w / 2);
                        mip_h = std::max(1u, mip_h / 2);
                }
                if (oTextureStock.num_mips == 1)
                        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        } else if (oSettings.path_prefix.empty()) {
                // Solid-colour 1×1 cubemap
                const uint8_t r = static_cast<uint8_t>(glm::clamp(oSettings.solid_color.r * 255.f, 0.f, 255.f));
                const uint8_t g = static_cast<uint8_t>(glm::clamp(oSettings.solid_color.g * 255.f, 0.f, 255.f));
                const uint8_t b = static_cast<uint8_t>(glm::clamp(oSettings.solid_color.b * 255.f, 0.f, 255.f));
                const uint8_t pixel[3] = { r, g, b };
                for (uint32_t i = 0; i < 6; ++i) {
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<int32_t>(i), 0, GL_RGB8,
                                     1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel);
                }
        } else {
                for (auto [face, suffix] : kCubeFaces) {
                        cv::Mat img = cv::imread(oSettings.path_prefix + suffix, cv::IMREAD_COLOR);
                        if (img.empty()) {
                                log_e("StoreTextureCubeMap: failed to load '{}{}'", oSettings.path_prefix, suffix);
                                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                                glDeleteTextures(1, &id);
                                id = 0;
                                return uWRONG_INPUT_DATA;
                        }
                        if (oSettings.flip_vertically)
                                cv::flip(img, img, 0);
                        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
                        glTexImage2D(static_cast<int32_t>(face), 0, GL_RGB8, img.cols, img.rows, 0,
                                     GL_RGB, GL_UNSIGNED_BYTE, img.data);
                }
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, oSettings.min_filter);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, oSettings.mag_filter);
        // For KTX uploads with a partial mip chain, clamp the driver's view so
        // the texture is not considered incomplete (default GL_TEXTURE_MAX_LEVEL=1000).
        if (oTextureStock.num_mips > 1)
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,
                                static_cast<GLint>(oTextureStock.num_mips - 1));
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        gl_type = GL_TEXTURE_CUBE_MAP;
        return uSUCCESS;
}

} // namespace SE
