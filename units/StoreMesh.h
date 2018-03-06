
#ifndef __STORE_MESH_H__
#define __STORE_MESH_H__ 1

namespace SE {

class StoreMesh {

        public:
        struct Settings { };
  
        typedef Settings  TChild;

        private:
  
        const Settings  & oSettings;

        public:

        StoreMesh(const Settings & oNewSettings);
        ~StoreMesh() throw();
        ret_code_t Store(MeshStock & oMeshStock, MeshCtx & oMeshCtx);

};

} //namespace SE

#ifdef SE_IMPL
#include <StoreMesh.tcc>
#endif

#endif
