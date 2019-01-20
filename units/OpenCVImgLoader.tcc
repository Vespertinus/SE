
#include <opencv2/opencv.hpp>

namespace SE {

OpenCVImgLoader::OpenCVImgLoader(const Settings & oSettings) { ;; }



OpenCVImgLoader::~OpenCVImgLoader() throw() { ;; }



ret_code_t OpenCVImgLoader::Load(const std::string sPath, TextureStock & oTextureStock) {

        cv::Mat oImg = cv::imread(sPath, cv::IMREAD_UNCHANGED);
        cv::Mat oResImg;

        if (oImg.empty()) {
                log_e("OpenCV failed to load image = '{}'", sPath.c_str());
                return uREAD_FILE_ERROR;
        }

        //HACK for bad drivers and non power of two textures
        if (oImg.channels() == 3 &&
                        (!(oImg.cols && ((oImg.cols & (oImg.cols - 1)) == 0) ) ||
                         !(oImg.rows && ((oImg.rows & (oImg.rows - 1)) == 0) ) )) {

                log_w("3 channel non power of two image ('{}') size ({}, {}), convert to BGRA to workaround",
                                sPath,
                                oImg.rows,
                                oImg.cols);
                cv::Mat oBGRA;
                cv::cvtColor(oImg, oBGRA, cv::COLOR_BGR2BGRA);

                cv::flip(oBGRA, oResImg, 0);
        }
        else {
                cv::flip(oImg, oResImg, 0);
        }

        uint32_t bpp                    = oResImg.channels();
        oTextureStock.width             = oResImg.cols;
        oTextureStock.height            = oResImg.rows;

        if (bpp != 3 && bpp != 4) {
                log_e("wrong channels cnt = {}, in file '{}'", bpp, sPath.c_str());
                return uWRONG_INPUT_DATA;
        }

        oTextureStock.format            = (bpp == 4) ? GL_BGRA : GL_BGR;
        oTextureStock.internal_format   = GL_RGBA8;
        oTextureStock.raw_image         = oResImg.ptr();
        oTextureStock.raw_image_size    = oResImg.total() * oResImg.elemSize();

        vImagesData.emplace_back(std::move(oResImg));

        return uSUCCESS;
}

} //namespace SE
