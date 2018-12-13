
#ifndef __OBJ_READER_H__
#define __OBJ_READER_H__

#include "Common.h"
#include <ErrCode.h>

namespace SE {
namespace TOOLS {

SE::ret_code_t ReadOBJ(const std::string & sPath,
                       ModelData         & oModel,
                       ImportCtx & oCtx);

}
}
#endif
