
#include <fstream>

#include <Logging.h>
#include <MPUtil.h>
#include <Mesh_generated.h>
#include "FlatBuffersMeshWriter.h"
#include "FlatBuffersMeshWriterDetails.h"

namespace SE {
namespace TOOLS {

//TODO throw or change ret type to ret_code
flatbuffers::Offset<SE::FlatBuffers::Mesh> SerializeMesh(const MeshData & oMesh,
                                        flatbuffers::FlatBufferBuilder & oBuilder) {
        if (!oMesh.vShapes.size()) {
                log_w("nothing to write");
                return SE::uWRONG_INPUT_DATA;
        }

        using namespace SE::FlatBuffers;

        std::vector<flatbuffers::Offset<Shape>> vFBShapes;

        for (auto & oItem : oMesh.vShapes) {

                auto min_fb     = Vec3(oItem.min.x, oItem.min.y, oItem.min.z);
                auto max_fb     = Vec3(oItem.max.x, oItem.max.y, oItem.max.z);

                std::vector<uint8_t>                                    vVertexBufferType;
                std::vector<flatbuffers::Offset<void> >                 vVertexBufferData;
                std::vector<flatbuffers::Offset<VertexAttribute>>       vVertexAttributes;

                auto [index_type, index_fb] = MP::Visit(oItem.oIndex,
                                [&oBuilder](const std::vector<uint8_t> & vData) {
                                        return std::make_tuple(
                                                IndexBuffer::Uint8Vector,
                                                CreateUint8Vector(oBuilder, oBuilder.CreateVector(vData)).Union()
                                                );
                                },
                                [&oBuilder](const std::vector<uint16_t> & vData) {
                                        return std::make_tuple(
                                                IndexBuffer::Uint16Vector,
                                                CreateUint16Vector(oBuilder, oBuilder.CreateVector(vData)).Union()
                                                );
                                },
                                [&oBuilder](const std::vector<uint32_t> & vData) {
                                        return std::make_tuple(
                                                IndexBuffer::Uint32Vector,
                                                CreateUint32Vector(oBuilder, oBuilder.CreateVector(vData)).Union()
                                                );
                                },
                                [](auto & arg) {
                                log_e("unsupported index type: '{}'", typeid(arg).name());
                                        return std::make_tuple(
                                                IndexBuffer::NONE,
                                                flatbuffers::Offset<void>(0)
                                                );
                                }
                );

                if (index_type == IndexBuffer::NONE) {
                        return SE::uLOGIC_ERROR;
                }

                for (auto & oVertexBuffer : oItem.vVertexBuffers) {

                        MP::Visit(oVertexBuffer,
                                  [&vVertexBufferType, &vVertexBufferData, &oBuilder](const std::vector<float> & vData) {
                                        vVertexBufferType.emplace_back(static_cast<uint8_t>(VertexBuffer::FloatVector));
                                        vVertexBufferData.emplace_back(CreateFloatVector(oBuilder, oBuilder.CreateVector(vData)).Union());
                                  },
                                  [&vVertexBufferType, &vVertexBufferData, &oBuilder](const std::vector<uint8_t> & vData) {
                                        vVertexBufferType.emplace_back(static_cast<uint8_t>(VertexBuffer::ByteVector));
                                        vVertexBufferData.emplace_back(CreateByteVector(oBuilder, oBuilder.CreateVector(vData)).Union());
                                  },
                                  [&vVertexBufferType, &vVertexBufferData, &oBuilder](const std::vector<uint32_t> & vData) {
                                        vVertexBufferType.emplace_back(static_cast<uint8_t>(VertexBuffer::Uint32Vector));
                                        vVertexBufferData.emplace_back(CreateUint32Vector(oBuilder, oBuilder.CreateVector(vData)).Union());
                                  }
                                 );
                }

                if (oItem.vVertexBuffers.size() != vVertexBufferData.size()) {
                        log_e("failed to add some vertex buffers, in cnt: {}, processed cnt: {}",
                                        oItem.vVertexBuffers.size(),
                                        vVertexBufferData.size());
                        return SE::uLOGIC_ERROR;
                }

                for (auto & oVertexAttribute : oItem.vAttributes) {
                        vVertexAttributes.emplace_back(
                                        CreateVertexAttribute(
                                                oBuilder,
                                                oBuilder.CreateString(oVertexAttribute.sName),
                                                oVertexAttribute.offset,
                                                oVertexAttribute.elem_size,
                                                oVertexAttribute.buffer_ind)
                                        );
                }


                auto shape_fb = CreateShape(
                                oBuilder,
                                oItem.sName.empty() ? 0 : oBuilder.CreateString(oItem.sName),
                                index_type,
                                index_fb,
                                oBuilder.CreateVector(vVertexBufferType),
                                oBuilder.CreateVector(vVertexBufferData),
                                oBuilder.CreateVector(vVertexAttributes),
                                oItem.triangles_cnt,
                                oItem.stride,
                                oItem.sTextureName.empty() ? 0 : oBuilder.CreateString(oItem.sTextureName),
                                &min_fb,
                                &max_fb);

                vFBShapes.emplace_back(shape_fb);
        }

        auto min_fb = Vec3(oMesh.min.x, oMesh.min.y, oMesh.min.z);
        auto max_fb = Vec3(oMesh.max.x, oMesh.max.y, oMesh.max.z);

        auto mesh_fb = CreateMesh(oBuilder,
                        oBuilder.CreateVector(vFBShapes),
                        &min_fb,
                        &max_fb
                        );

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

