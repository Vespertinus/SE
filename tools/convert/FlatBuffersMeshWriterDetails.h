
#ifndef __FLATBUFFERS_MESH_WRITER_DETAILS_H__
#define __FLATBUFFERS_MESH_WRITER_DETAILS_H__

#include "Common.h"

namespace SE {
namespace TOOLS {

flatbuffers::Offset<SE::FlatBuffers::Mesh> SerializeMesh(
                const MeshData                  & oMesh,
                flatbuffers::FlatBufferBuilder  & oBuilder);

}
}
#endif

