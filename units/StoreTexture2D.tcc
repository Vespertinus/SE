
namespace SE {


StoreTexture2D::StoreTexture2D(const Settings & oNewSettings) : oSettings(oNewSettings) { ;; }



StoreTexture2D::~StoreTexture2D() throw() { 
  
        //gl free tex id 
}



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

        uint32_t _id;

        glGenTextures(1, &_id);

        glBindTexture(GL_TEXTURE_2D, _id);

        id = _id;

        fprintf(stderr, "StoreTexture2D::Store: id = %u, width = %u, height = %u, bpp = %u\n",
                        id,
                        oTextureStock.width,
                        oTextureStock.height,
                        oTextureStock.bpp);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, oSettings.wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, oSettings.wrap);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, oSettings.min_filter);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, oSettings.mag_filter);

//        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, oSettings.mipmap_enabled); 

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, oSettings.apply_method);
        
/*        glTexImage2D(GL_TEXTURE_2D, 
                        0, 
                        GL_RGBA8,//oTextureStock.color_order, 
                        oTextureStock.width, 
                        oTextureStock.height,
                        0, 
                        GL_RGB8,//oTextureStock.color_order,
                        GL_UNSIGNED_BYTE, 
                        oTextureStock.raw_image);
*/        
/*
        if (oSettings.mipmap_enabled) {
                glGenerateMipmap(GL_TEXTURE_2D);
        }
*/
        ///* 
           //if (oSettings.mipmap_level) {

           gluBuild2DMipmaps(GL_TEXTURE_2D, 
           4, //oSettings.mipmap_level, 
           oTextureStock.width, 
           oTextureStock.height,
           oTextureStock.color_order,
           //(oTextureStock.bpp == 3) ? GL_RGB : GL_RGBA, 
           GL_UNSIGNED_BYTE, 
           oTextureStock.raw_image);
           //}
        //*/

        fprintf(stderr, "StoreTexture2D::Store: is tex = %u\n", glIsTexture(_id));

        return ret_code;
}


} // namespace SE
