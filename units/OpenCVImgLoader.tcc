
namespace SE {

OpenCVImgLoader::OpenCVImgLoader(const Settings & oSettings) { ;; }



OpenCVImgLoader::~OpenCVImgLoader() throw() { ;; }



ret_code_t OpenCVImgLoader::Load(const std::string sPath, TextureStock & oTextureStock) {

        cv::Mat oImg = cv::imread(sPath);
        
        if (oImg.empty()) {        
                fprintf(stderr, "OpenCVImgLoader::Load: OpenCV failed to load image = '%s'\n", sPath.c_str());
                return uREAD_FILE_ERROR;
        }
        
        cv::flip(oResImg, oImg, 0);

        oTextureStock.width             = oResImg.cols;
        oTextureStock.height            = oResImg.rows;
        oTextureStock.bpp               = oResImg.channels();

        if (oTextureStock.bpp != 3 && oTextureStock.bpp != 4) {
                fprintf(stderr, "OpenCVImgLoader::Load: wrong channels cnt = %u in file '%s'\n", oTextureStock.bpp, sPath.c_str());
                return uWRONG_INPUT_DATA;
        }

        oTextureStock.color_order       = (oTextureStock.bpp == 4) ? GL_BGRA : GL_BGR;
        oTextureStock.raw_image         = oResImg.ptr();
        oTextureStock.raw_image_size    = oResImg.total() * oResImg.elemSize();
        oTextureStock.compressed        = uUNCOMPRESSED_TEXTURE;

        return uSUCCESS;
}

} //namespace SE
