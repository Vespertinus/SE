
#ifndef __OBJ_LOADER_H__
#define __OBJ_LOADER_H__ 1

namespace SE {

class OBJLoader {

        public:
        //empty settings for this loader
        struct Settings {

                struct ShapeSettings {
                        /** 0 none, 1 flip h, 2 flip v */
                        uint8_t flip_tex_coords : 2;
                };

                bool skip_normals;

                std::map <std::string, ShapeSettings> mShapesOptions;
                StoreTexture2D::Settings oTex2DSettings;
        };
        typedef Settings TChild;

        private:

        Settings oSettings;

        public:

        OBJLoader(const Settings & oNewSettings);
        ~OBJLoader() throw();
        ret_code_t Load(const std::string sPath, MeshStock & oMeshStock);

};

} //namespace SE

#endif
