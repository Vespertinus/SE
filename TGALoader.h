

#ifndef __TGA_LOADER_H__
#define __TGA_LOADER_H__ 1


namespace SE {

class TGALoader {

  public:
  //empty settings for this loader
  struct Settings { };
  typedef Settings TChild;

  TGALoader(const Settings & oSettings);
  ~TGALoader() throw();
  ret_code_t Load(const std::string sPath, TextureStock & oTextureStock);

};

} //namespace SE

#include <TGALoader.tcc>

#endif
