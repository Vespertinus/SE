
#ifndef __STORE_TEXTURE_2D_H__
#define __STORE_TEXTURE_2D_H__ 1

namespace SE {

class StoreTexture2D {

  public:
  struct Settings {
    int32_t wrap;
    int32_t min_filter,
            mag_filter;
    int32_t apply_method;
    uint8_t mipmap_level;

    Settings() : 
      wrap(GL_REPEAT), 
      min_filter(GL_LINEAR_MIPMAP_NEAREST),
      //min_filter(GL_LINEAR),
      mag_filter(GL_LINEAR),
      apply_method(GL_MODULATE),
      //apply_method(GL_REPLACE),
      mipmap_level(4) { ;; }
  };
  
  typedef Settings  TChild;

  private:
  
  const Settings  & oSettings;

  public:

  StoreTexture2D(const Settings & oNewSettings);
  ~StoreTexture2D() throw();
  ret_code_t Store(TextureStock & oTextureStock, uint32_t & id);

};

} //namespace SE

#include <StoreTexture2D.tcc>

#endif
