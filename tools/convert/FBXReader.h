#ifndef __FBX_READER_H__
#define __FBX_READER_H__

#include <ErrCode.h>
#include "Common.h"

namespace fbxsdk {
        class FbxManager;
}

namespace SE {
namespace TOOLS {

class FBXReader {

        fbxsdk::FbxManager * pManager;

        public:

        FBXReader();
        ~FBXReader() noexcept;

        ret_code_t ReadScene(const std::string_view sPath,
                             NodeData & oNodeData,
                             ImportCtx & oCtx);

};



}
}

#endif
