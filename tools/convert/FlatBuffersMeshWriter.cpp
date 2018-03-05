
#include <fstream>

#include <Logging.h>
#include <Mesh_generated.h>
#include "FlatBuffersMeshWriter.h"
#include "FlatBuffersMeshWriterDetails.h"

namespace SE {
namespace TOOLS {

flatbuffers::Offset<SE::FlatBuffers::Mesh> SerializeMesh(const MeshData & oMesh,
                                        flatbuffers::FlatBufferBuilder & oBuilder) {
        if (!oMesh.vShapes.size()) {
                log_w("nothing to write");
                return SE::uWRONG_INPUT_DATA;
        }

        std::vector<flatbuffers::Offset<SE::FlatBuffers::Shape>> vFBShapes;

        for (auto & oItem : oMesh.vShapes) {

                auto min_fb     = SE::FlatBuffers::Vec3(oItem.min.x, oItem.min.y, oItem.min.z);
                auto max_fb     = SE::FlatBuffers::Vec3(oItem.max.x, oItem.max.y, oItem.max.z);

                auto shape_fb = SE::FlatBuffers::CreateShape(oBuilder,
                                oItem.sName.empty() ? 0 : oBuilder.CreateString(oItem.sName),
                                oBuilder.CreateVector(oItem.vVertices),
                                oItem.triangles_cnt,
                                oItem.sTextureName.empty() ? 0 : oBuilder.CreateString(oItem.sTextureName),
                                &min_fb,
                                &max_fb);

                vFBShapes.emplace_back(shape_fb);
        }

        auto min_fb = SE::FlatBuffers::Vec3(oMesh.min.x, oMesh.min.y, oMesh.min.z);
        auto max_fb = SE::FlatBuffers::Vec3(oMesh.max.x, oMesh.max.y, oMesh.max.z);

        auto mesh_fb = SE::FlatBuffers::CreateMesh(oBuilder,
                        oBuilder.CreateVector(vFBShapes),
                        &min_fb,
                        &max_fb,
                        oMesh.skip_normals);

        return mesh_fb;
}


SE::ret_code_t WriteMesh(const std::string sPath, const MeshData & oMesh) {

        flatbuffers::FlatBufferBuilder oBuilder(1024);

        auto mesh_fb = SerializeMesh(oMesh, oBuilder);

        FinishMeshBuffer(oBuilder, mesh_fb);

        log_d("flatbuffers build done, size {}", oBuilder.GetSize());

        try {
                std::ofstream out(sPath, std::ios::binary);
                out.write(reinterpret_cast<char *>(oBuilder.GetBufferPointer()), oBuilder.GetSize());
        }
        catch (std::exception & ex) {
                log_e("failed to write '{}', reason: '{}'", sPath, ex.what());
                return SE::uWRITE_FILE_ERROR;
        }
        catch(...) {
                log_e("failed to write '{}', unknown exception catched", sPath);
                return SE::uWRITE_FILE_ERROR;
        }

        log_i("write '{}' done, result size = {}", sPath, oBuilder.GetSize() );

        return SE::uSUCCESS;
}


}
}

