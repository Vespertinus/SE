
#ifndef __OBJ_LOADER_H__
#define __TOBJLOADER_H__ 1

namespace SE {

class OBJLoader {

        public:
        //empty settings for this loader
        struct Settings { };
        typedef Settings TChild;

        OBJLoader(const Settings & oSettings);
        ~OBJLoader() throw();
        ret_code_t Load(const std::string sPath, MeshStock & oMeshStock);

};

} //namespace SE

#endif
