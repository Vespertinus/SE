
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

        
        cv::flip(oImg, oResImg, 0);

        oTextureStock.bpp               = oImg.channels();
        oTextureStock.width             = oResImg.cols;
        oTextureStock.height            = oResImg.rows;

        if (oTextureStock.bpp != 3 && oTextureStock.bpp != 4) {
                log_e("wrong channels cnt = {}, in file '{}'", oTextureStock.bpp, sPath.c_str());
                return uWRONG_INPUT_DATA;
        }

        oTextureStock.color_order       = (oTextureStock.bpp == 4) ? GL_BGRA : GL_BGR;
        oTextureStock.raw_image         = oResImg.ptr();
        oTextureStock.raw_image_size    = oResImg.total() * oResImg.elemSize();
        oTextureStock.compressed        = uUNCOMPRESSED_TEXTURE;

        vImagesData.emplace_back(std::move(oResImg));

        return uSUCCESS;
}

} //namespace SE
