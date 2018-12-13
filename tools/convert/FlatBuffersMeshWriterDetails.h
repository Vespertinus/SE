
#ifndef __FLATBUFFERS_MESH_WRITER_DETAILS_H__
#define __FLATBUFFERS_MESH_WRITER_DETAILS_H__

#include "Common.h"
#include <ErrCode.h>

namespace SE {
namespace TOOLS {

std::tuple<flatbuffers::Offset<SE::FlatBuffers::Mesh>, ret_code_t> SerializeMesh(
                const MeshData                  & oMesh,
                flatbuffers::FlatBufferBuilder  & oBuilder);

}
}
#endif

