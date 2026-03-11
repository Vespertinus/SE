
#ifndef __GLTF_READER_H__
#define __GLTF_READER_H__

#include <ErrCode.h>
#include "Common.h"

namespace SE {
namespace TOOLS {

class GLTFReader {

        ResourceStash oResStash;

        public:

        ret_code_t ReadScene(const std::string & sPath, NodeData & oRoot, ImportCtx & oCtx);

};

}
}

#endif
