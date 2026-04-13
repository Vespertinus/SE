#include <opencv2/opencv.hpp>
#include <GLUtil.h>
#include <Logging.h>

namespace SE {

/** Simple reinhard tone mapping + gamma correction for HDR → LDR */
static void ToneMapAndSaveHDR(const float *buf, uint32_t w, uint32_t h, const std::string &path) {
        std::vector<uint8_t> out(w * h * 4);
        for (size_t i = 0; i < w * h; ++i) {
                for (int c = 0; c < 3; ++c) {
                        float v = buf[i * 4 + c];
                        v = v / (1.0f + v); // reinhard
                        v = std::pow(v, 1.0f / 2.2f); // gamma
                        out[i * 4 + c] = static_cast<uint8_t>(std::min(v * 255.0f, 255.0f));
                }
                out[i * 4 + 3] = 255;
        }
        cv::Mat img(h, w, CV_8UC4, out.data());
        cv::flip(img, img, 0);  // flip bottom-to-top → top-to-bottom
        cv::cvtColor(img, img, cv::COLOR_RGBA2BGRA);
        cv::imwrite(path, img);
}

bool ImageUtil::WriteTexture2D(TTexture *pTex, const std::string &path) {

        if (!pTex) {
                log_e("ImageUtil: null texture for '{}'", path);
                return false;
        }

        const uint32_t glId  = pTex->GetID();
        const auto dims      = pTex->GetDimensions();
        const uint32_t w     = dims.first;
        const uint32_t h     = dims.second;
        const uint32_t glType = pTex->Type();

        glBindTexture(GL_TEXTURE_2D, glId);

        if (glType == GL_TEXTURE_2D) {
                GLint internalFmt = 0;
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFmt);

                switch (internalFmt) {
                case GL_RGBA8: {
                        std::vector<uint8_t> buf(w * h * 4);
                        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
                        cv::Mat img(h, w, CV_8UC4, buf.data());
                        cv::flip(img, img, 0);
                        cv::cvtColor(img, img, cv::COLOR_RGBA2BGRA);
                        cv::imwrite(path, img);
                        break;
                }
                case GL_RGB8: {
                        std::vector<uint8_t> buf(w * h * 3);
                        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, buf.data());
                        std::vector<uint8_t> out(w * h * 4);
                        for (size_t i = 0; i < w * h; ++i) {
                                out[i * 4 + 0] = buf[i * 3 + 0];
                                out[i * 4 + 1] = buf[i * 3 + 1];
                                out[i * 4 + 2] = buf[i * 3 + 2];
                                out[i * 4 + 3] = 255;
                        }
                        cv::Mat img(h, w, CV_8UC4, out.data());
                        cv::flip(img, img, 0);
                        cv::cvtColor(img, img, cv::COLOR_RGBA2BGRA);
                        cv::imwrite(path, img);
                        break;
                }
                case GL_RGBA16F: {
                        std::vector<float> buf(w * h * 4);
                        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, buf.data());
                        ToneMapAndSaveHDR(buf.data(), w, h, path);
                        break;
                }
                case GL_RGB16F:
                [[fallthrough]];
                case GL_R11F_G11F_B10F: {
                        std::vector<float> buf(w * h * 3);
                        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, buf.data());
                        std::vector<float> rgba(w * h * 4);
                        for (size_t i = 0; i < w * h; ++i) {
                                for (int c = 0; c < 3; ++c) rgba[i * 4 + c] = buf[i * 3 + c];
                                rgba[i * 4 + 3] = 1.0f;
                        }
                        ToneMapAndSaveHDR(rgba.data(), w, h, path);
                        break;
                }
                case GL_DEPTH24_STENCIL8: {
                        std::vector<uint32_t> buf(w * h);
                        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, buf.data());
                        std::vector<uint8_t> out(w * h);
                        for (size_t i = 0; i < w * h; ++i) {
                                float depth = static_cast<float>(buf[i] >> 8) / 16777215.0f;
                                out[i] = static_cast<uint8_t>(std::min(depth * 255.0f, 255.0f));
                        }
                        cv::Mat img(h, w, CV_8UC1, out.data());
                        cv::flip(img, img, 0);
                        cv::imwrite(path, img);
                        break;
                }
                case GL_R8: {
                        std::vector<uint8_t> buf(w * h);
                        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, buf.data());
                        cv::Mat img(h, w, CV_8UC1, buf.data());
                        cv::flip(img, img, 0);
                        // SSAO is grayscale, expand to RGBA for visibility
                        cv::Mat out;
                        cv::cvtColor(img, out, cv::COLOR_GRAY2BGRA);
                        cv::imwrite(path, out);
                        break;
                }
                default:
                        log_w("ImageUtil: unsupported internal format {:x} for '{}'", internalFmt, path);
                        glBindTexture(GL_TEXTURE_2D, 0);
                        return false;
                }
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        log_d("ImageUtil: wrote '{}' ({}x{})", path, w, h);
        return true;
}

bool ImageUtil::WriteReadBufferToFile(uint32_t fboId, uint32_t attachment,
                                       uint32_t w, uint32_t h, const std::string &path) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId);
        glReadBuffer(attachment);

        std::vector<float> buf(w * h * 4);
        glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                     GL_RGBA, GL_FLOAT, buf.data());

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        // glReadPixels returns bottom-to-top; flip vertically
        std::vector<uint8_t> out(w * h * 4);
        for (uint32_t y = 0; y < h; ++y) {
                uint32_t srcRow = (h - 1 - y) * w;
                uint32_t dstRow = y * w;
                for (uint32_t x = 0; x < w; ++x) {
                        size_t si = (srcRow + x) * 4;
                        size_t di = (dstRow + x) * 4;
                        for (int c = 0; c < 3; ++c) {
                                float v = buf[si + c];
                                v = v / (1.0f + v); // reinhard
                                v = std::pow(v, 1.0f / 2.2f); // gamma
                                out[di + c] = static_cast<uint8_t>(std::min(v * 255.0f, 255.0f));
                        }
                        out[di + 3] = 255;
                }
        }

        cv::Mat img(h, w, CV_8UC4, out.data());
        cv::cvtColor(img, img, cv::COLOR_RGBA2BGRA);
        cv::imwrite(path, img);

        log_d("ImageUtil: read framebuffer from FBO {} to '{}' ({}x{})", fboId, path, w, h);
        return true;
}

bool ImageUtil::WriteDepthTexture(TTexture *pTex, float nearZ, float farZ, const std::string &path) {

        if (!pTex) {
                log_e("ImageUtil: null depth texture for '{}'", path);
                return false;
        }

        const uint32_t glId  = pTex->GetID();
        const auto dims      = pTex->GetDimensions();
        const uint32_t w     = dims.first;
        const uint32_t h     = dims.second;

        glBindTexture(GL_TEXTURE_2D, glId);

        std::vector<float> buf(w * h);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, buf.data());

        // Convert non-linear depth to linear view-space depth for visualization
        // d is in [0,1] range from the depth buffer
        // ndc_z = 2*d - 1 maps to [-1, 1]
        // view_z = (2 * near * far) / (near + far - ndc_z * (far - near))
        // Then visualize: linear_depth = -view_z / far (clamped to [0, 1])
        std::vector<uint8_t> out(w * h);
        for (size_t i = 0; i < w * h; ++i) {
                float d = buf[i];
                float ndc_z = 2.0f * d - 1.0f;
                float view_z = (2.0f * nearZ * farZ) / (nearZ + farZ - ndc_z * (farZ - nearZ));
                float linear = view_z / farZ;  // view_z is positive distance in view space
                linear = std::max(0.0f, std::min(1.0f, linear));
                out[i] = static_cast<uint8_t>(linear * 255.0f);
        }

        cv::Mat img(h, w, CV_8UC1, out.data());
        cv::flip(img, img, 0);
        cv::imwrite(path, img);

        glBindTexture(GL_TEXTURE_2D, 0);
        log_d("ImageUtil: wrote depth '{}' ({}x{}, near={}, far={})", path, w, h, nearZ, farZ);
        return true;
}

} // namespace SE
