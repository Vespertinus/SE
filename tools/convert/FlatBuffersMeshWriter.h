
#ifndef __FLATBUFFERS_MESH_WRITER_H__
#define __FLATBUFFERS_MESH_WRITER_H__

#include <ErrCode.h>
#include "Common.h"

namespace SE {
namespace TOOLS {

SE::ret_code_t WriteMesh(const std::string sPath,
                         const MeshData  & oMesh);

}
}
#endif
