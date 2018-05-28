

namespace SE {


TGALoader::TGALoader(const Settings & oSettings) { ;; }



TGALoader::~TGALoader() throw() { ;; }



ret_code_t TGALoader::Load(const std::string sPath, TextureStock & oTextureStock) {

        ret_code_t  ret_code = uSUCCESS;

        static uint8_t	targa_magic[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint8_t         file_magic[12];
        uint8_t         header[6];
        uint32_t	swap;
        std::vector <uint8_t> vImageData;

        FILE * oImageFile = fopen (sPath.c_str(), "rb");

        if (oImageFile == NULL)	{
                log_e("can't open file = '{}', reason = {}", sPath.c_str(), strerror(errno));
                return uREAD_FILE_ERROR;
        }

        if (
                        (fread (file_magic, 1, sizeof (file_magic), oImageFile) != sizeof (file_magic) ) ||
                        (memcmp (targa_magic, file_magic, sizeof (targa_magic)) != 0 ) ||
                        (fread (header, 1, sizeof (header), oImageFile) != sizeof (header) ))	{

                log_e("wrong data in file = '{}'", sPath.c_str());
                fclose (oImageFile);
                return uREAD_FILE_ERROR;
        }

        oTextureStock.width   = header [1] * 256 + header [0];
        oTextureStock.height  = header [3] * 256 + header [2];

        if (oTextureStock.width <= 0 || oTextureStock.height <= 0 || (header [4] != 24 && header [4] != 32)) {
                fclose (oImageFile);
                log_e("wrong header, width = {}, height = {}, bpp = {}",
                                oTextureStock.width,
                                oTextureStock.height,
                                header[4]);
                return uREAD_FILE_ERROR;
        }

        oTextureStock.bpp             = header [4] / 8;
        oTextureStock.color_order     = (oTextureStock.bpp == 4) ? GL_RGBA : GL_RGB;
        oTextureStock.raw_image       = 0;
        oTextureStock.raw_image_size  = oTextureStock.width * oTextureStock.height * oTextureStock.bpp;

        try {
                vImageData.resize(oTextureStock.raw_image_size);
        }
        catch (std::exception & ex) {
                log_e("failed to allocate {} bytes for image ({}) data, reason: '{}'",
                                oTextureStock.raw_image_size,
                                sPath.c_str(),
                                ex.what());
                fclose (oImageFile);
                return uMEMORY_ALLOCATION_ERROR;

        }
        catch(...) {
                log_e("failed to allocate {} bytes for image ({}) data",
                                oTextureStock.raw_image_size,
                                sPath.c_str() );
                fclose (oImageFile);
                return uMEMORY_ALLOCATION_ERROR;
        }


        if (fread (&vImageData[0], 1, oTextureStock.raw_image_size, oImageFile) != oTextureStock.raw_image_size) {


                log_e("can't read {} bytes, reason = {}", oTextureStock.raw_image_size, strerror(errno));
                fclose (oImageFile);
                return uREAD_FILE_ERROR;
        }

        for (uint32_t index = 0; index < oTextureStock.raw_image_size; index += oTextureStock.bpp)	{
                swap                    = vImageData [index];
                vImageData [index]      = vImageData [index + 2];
                vImageData [index + 2]  = swap;
        }

        oTextureStock.compressed = uUNCOMPRESSED_TEXTURE;
        oTextureStock.raw_image  = &vImageData[0];

        fclose (oImageFile);

        vImagesData.emplace_back(std::move(vImageData));

        return ret_code;
}




} //namespace SE
