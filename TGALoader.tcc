

namespace SE {

 
TGALoader::TGALoader(const Settings & oSettings) { ;; }



TGALoader::~TGALoader() throw() { ;; }



ret_code_t TGALoader::Load(const std::string sPath, TextureStock & oTextureStock) {

  ret_code_t  ret_code = uSUCCESS;

  static uint8_t	targa_magic[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint8_t         file_magic[12];
  uint8_t         header[6];
  uint32_t	      swap;
  
  FILE * oImageFile = fopen (sPath.c_str(), "rb");

  if (oImageFile == NULL)	{
    fprintf(stderr, "TGALoader::Load: can't open file = '%s', reason = %s", sPath.c_str(), strerror(errno));
    return uREAD_FILE_ERROR;
  }

  if (
      (fread (file_magic, 1, sizeof (file_magic), oImageFile) != sizeof (file_magic) ) ||
      (memcmp (targa_magic, file_magic, sizeof (targa_magic)) != 0 ) ||
      (fread (header, 1, sizeof (header), oImageFile) != sizeof (header) ))	{

    fprintf(stderr, "TGALoader::Load: wrong data in file = '%s'", sPath.c_str());
    fclose (oImageFile);
    return uREAD_FILE_ERROR;
  }

  oTextureStock.width   = header [1] * 256 + header [0];
  oTextureStock.height  = header [3] * 256 + header [2];

  if (oTextureStock.width <= 0 || oTextureStock.height <= 0 || (header [4] != 24 && header [4] != 32)) {
    fclose (oImageFile);
    fprintf(stderr, "TGALoader::Load: wromg header, width = %u, height = %u, bpp = %u\n",
        oTextureStock.width,
        oTextureStock.height,
        header[4]);
    return uREAD_FILE_ERROR;
  }

  oTextureStock.bpp             = header [4] / 8;

  oTextureStock.raw_image_size  = oTextureStock.width * oTextureStock.height * oTextureStock.bpp;

  oTextureStock.raw_image = new uint8_t [oTextureStock.raw_image_size];
  if (oTextureStock.raw_image == 0 || (uint32_t) fread (oTextureStock.raw_image, 1, oTextureStock.raw_image_size, oImageFile) != oTextureStock.raw_image_size) {

    if (oTextureStock.raw_image != NULL)	{
      delete[] (oTextureStock.raw_image);
      oTextureStock.raw_image = 0;

      fprintf(stderr, "TGALoader::Load: can't read %u bytes, reason = %s\n", oTextureStock.raw_image_size, strerror(errno));
      ret_code = uREAD_FILE_ERROR;
    }
    else {
      
      fprintf(stderr, "TGALoader::Load: can't allocate %u bytes\n", oTextureStock.raw_image_size);
      ret_code = uMEMORY_ALLOCATION_ERROR;
    }
    fclose (oImageFile);

    return ret_code;
  }

  for (uint32_t index = 0; index < oTextureStock.raw_image_size; index += oTextureStock.bpp)	{ 
    swap                                = oTextureStock.raw_image [index];
    oTextureStock.raw_image [index]     = oTextureStock.raw_image [index + 2];
    oTextureStock.raw_image [index + 2] = swap;
  }

  oTextureStock.compressed = uUNCOMPRESSED_TEXTURE;
  
  fclose (oImageFile);

  return ret_code;
} 




} //namespace SE
