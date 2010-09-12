
namespace SE {


StoreTexture2D::StoreTexture2D(const Settings & oNewSettings) : oSettings(oNewSettings) { ;; }



StoreTexture2D::~StoreTexture2D() throw() { ;; }



ret_code_t StoreTexture2D::Store(TextureStock & oTextureStock, uint32_t & id) {

  ret_code_t ret_code = uSUCCESS;

  if (!oTextureStock.raw_image || !oTextureStock.raw_image_size) {
  
    fprintf(stderr, "StoreTexture2D::Store: empty image data\n");
    return uWRONG_INPUT_DATA;
  }
  
  if (!oTextureStock.width || !oTextureStock.height) {
  
    fprintf(stderr, "StoreTexture2D::Store: wrong image dimensions, width = %u, height = %u\n", 
                                                                                                oTextureStock.width,
                                                                                                oTextureStock.height);
    return uWRONG_INPUT_DATA;
  }

  glGenTextures(1, &id);
  
  glBindTexture(GL_TEXTURE_2D, id);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, oSettings.wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, oSettings.wrap);
  
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, oSettings.min_filter);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, oSettings.mag_filter);
  
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oSettings.apply_method);
/* 
  if (oSettings.mipmap_level) {

    gluBuild2DMipmaps(GL_TEXTURE_2D, 
                      oSettings.mipmap_level, 
                      oTextureStock.width, 
                      oTextureStock.height,
                      (oTextureStock.bpp == 3) ? GL_RGB : GL_RGBA, 
                      GL_UNSIGNED_BYTE, 
                      oTextureStock.raw_image);
  }
*/
  return ret_code;
}


} // namespace SE
