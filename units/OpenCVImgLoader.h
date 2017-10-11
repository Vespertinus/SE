
#ifndef __OPEN_CV_IM_LOADER_H__
#define __OPEN_CV_IM_LOADER_H__ 1

#include <opencv2/opencv.hpp>

namespace SE {

class OpenCVImgLoader {

        std::vector <cv::Mat> vImagesData;
        
        public:
        //empty settings for this loader
        struct Settings { };
        typedef Settings TChild;

        OpenCVImgLoader(const Settings & oSettings);
        ~OpenCVImgLoader() throw();
        ret_code_t Load(const std::string sPath, TextureStock & oTextureStock);
};

} //namespace SE

#include <OpenCVImgLoader.tcc>

#endif
