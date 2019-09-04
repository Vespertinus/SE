
#include <fstream>

#include <Logging.h>
#include <MPUtil.h>
#include <Mesh_generated.h>
#include "FlatBuffersMeshWriter.h"
#include "FlatBuffersMeshWriterDetails.h"

namespace SE {
namespace TOOLS {

//TODO throw or change ret type to ret_code
std::tuple<flatbuffers::Offset<SE::FlatBuffers::Mesh>, ret_code_t> SerializeMesh(
                const MeshData & oMesh,
                flatbuffers::FlatBufferBuilder & oBuilder) {

        if (!oMesh.vShapes.size()) {
                log_w("nothing to write");
                return {0, SE::uWRONG_INPUT_DATA};
        }

        using namespace SE::FlatBuffers;

        std::vector<flatbuffers::Offset<Shape>> vFBShapes;

        for (auto & oItem : oMesh.vShapes) {

                auto min_fb     = Vec3(oItem.oBBox.Min().x, oItem.oBBox.Min().y, oItem.oBBox.Min().z);
                auto max_fb     = Vec3(oItem.oBBox.Max().x, oItem.oBBox.Max().y, oItem.oBBox.Max().z);
                auto bbox_fb    = SE::FlatBuffers::BoundingBox(min_fb, max_fb);

                auto shape_fb = CreateShape(
                                oBuilder,
                                &bbox_fb,
                                oItem.start,
                                oItem.count);

                vFBShapes.emplace_back(shape_fb);
        }

        std::vector<flatbuffers::Offset<VertexAttribute>>       vVertexAttributes;
        std::vector<flatbuffers::Offset<VertexBuffer>>          vVertexBuffers;

        auto [index_type, index_fb] = MP::Visit(oMesh.oIndex,
                        [&oBuilder](const std::vector<uint8_t> & vData) {
                        return std::make_tuple(
                                        IndexBufferU::Uint8Vector,
                                        CreateUint8Vector(oBuilder, oBuilder.CreateVector(vData)).Union()
                                        );
                        },
                        [&oBuilder](const std::vector<uint16_t> & vData) {
                        return std::make_tuple(
                                        IndexBufferU::Uint16Vector,
                                        CreateUint16Vector(oBuilder, oBuilder.CreateVector(vData)).Union()
                                        );
                        },
                        [&oBuilder](const std::vector<uint32_t> & vData) {
                        return std::make_tuple(
                                        IndexBufferU::Uint32Vector,
                                        CreateUint32Vector(oBuilder, oBuilder.CreateVector(vData)).Union()
                                        );
                        },
                        [](auto & arg) {
                        log_e("unsupported index type: '{}'", typeid(arg).name());
                        return std::make_tuple(
                                        IndexBufferU::NONE,
                                        flatbuffers::Offset<void>(0)
                                        );
                        }
        );

        if (index_type == IndexBufferU::NONE) {
                return {0, SE::uLOGIC_ERROR};
        }

        auto index_table_fb = CreateIndexBuffer(oBuilder, index_type, index_fb);

        for (auto & oVertexBuffer : oMesh.vVertexBuffers) {

                MP::Visit(oVertexBuffer.oBuffer,
                                [&oBuilder, &oVertexBuffer/*FIXME*/, &vVertexBuffers](const std::vector<float> & vData) {

                                        vVertexBuffers.emplace_back(CreateVertexBuffer(
                                                                oBuilder,
                                                                VertexBufferU::FloatVector,
                                                                CreateFloatVector(oBuilder, oBuilder.CreateVector(vData)).Union(),
                                                                oVertexBuffer.stride));
                                },
                                [&oBuilder, &oVertexBuffer, &vVertexBuffers](const std::vector<uint8_t> & vData) {
                                        vVertexBuffers.emplace_back(CreateVertexBuffer(
                                                                oBuilder,
                                                                VertexBufferU::ByteVector,
                                                                CreateByteVector(oBuilder, oBuilder.CreateVector(vData)).Union(),
                                                                oVertexBuffer.stride));
                                },
                                [&oBuilder, &oVertexBuffer, &vVertexBuffers](const std::vector<uint32_t> & vData) {
                                        vVertexBuffers.emplace_back(CreateVertexBuffer(
                                                                oBuilder,
                                                                VertexBufferU::Uint32Vector,
                                                                CreateUint32Vector(oBuilder, oBuilder.CreateVector(vData)).Union(),
                                                                oVertexBuffer.stride));
                                }
                         );
        }

        if (oMesh.vVertexBuffers.size() != vVertexBuffers.size()) {
                log_e("failed to add some vertex buffers, in cnt: {}, processed cnt: {}",
                                oMesh.vVertexBuffers.size(),
                                vVertexBuffers.size());
                return {0, SE::uLOGIC_ERROR};
        }

        for (auto & oVertexAttribute : oMesh.vAttributes) {
                vVertexAttributes.emplace_back(
                                CreateVertexAttribute(
                                        oBuilder,
                                        oBuilder.CreateString(oVertexAttribute.sName),
                                        oVertexAttribute.offset,
                                        oVertexAttribute.elem_size,
                                        oVertexAttribute.buffer_ind,
                                        oVertexAttribute.custom,
                                        static_cast<SE::FlatBuffers::AttribDestType>(
                                                oVertexAttribute.destination
                                                )
                                        )
                                );
        }

        auto min_fb     = Vec3(oMesh.oBBox.Min().x, oMesh.oBBox.Min().y, oMesh.oBBox.Min().z);
        auto max_fb     = Vec3(oMesh.oBBox.Max().x, oMesh.oBBox.Max().y, oMesh.oBBox.Max().z);
        auto bbox_fb    = SE::FlatBuffers::BoundingBox(min_fb, max_fb);

        return { CreateMesh(
                        oBuilder,
                        index_table_fb,
                        oBuilder.CreateVector(vVertexBuffers),
                        oBuilder.CreateVector(vVertexAttributes),
                        PrimitiveType::GEOM_TRIANGLES,
                        oBuilder.CreateVector(vFBShapes),
                        &bbox_fb
                        ),
               SE::uSUCCESS
        };
}


SE::ret_code_t WriteMesh(const std::string sPath, const MeshData & oMesh) {

        flatbuffers::FlatBufferBuilder oBuilder(1024);

        auto [mesh_fb, res] = SerializeMesh(oMesh, oBuilder);
        if (res != uSUCCESS) {
                return res;
        }

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

