#include <fstream>

#include <Logging.h>
#include <SceneTree_generated.h>
#include "FlatBuffersSceneTreeWriter.h"
#include "FlatBuffersMeshWriterDetails.h"

namespace SE {
namespace TOOLS {

//TODO check duplication in import from fbx and write only one mesh copy

using StringOffset = flatbuffers::Offset<flatbuffers::String>;
using SE::FlatBuffers::Node;
using SE::FlatBuffers::Mesh;
using SE::FlatBuffers::Vec3;
using SE::FlatBuffers::Entity;

static flatbuffers::Offset<Node> SerializeNode(
                const NodeData & oNode,
                flatbuffers::FlatBufferBuilder & oBuilder) {

        std::vector<flatbuffers::Offset<Node>> vChildren;
        std::vector<flatbuffers::Offset<Entity>> vEntity;

        for (auto & item : oNode.vChildren) {
                vChildren.emplace_back(SerializeNode(item, oBuilder));
        }

        for (auto & item : oNode.vEntity) {
                vEntity.emplace_back( SE::FlatBuffers::CreateEntity(
                                        oBuilder,
                                        oBuilder.CreateString(item.sName),
                                        SerializeMesh(item, oBuilder)) );
        }

        auto translation_fb = Vec3(oNode.translation.x,
                                   oNode.translation.y,
                                   oNode.translation.z);
        auto rotation_fb    = Vec3(oNode.rotation.x,
                                   oNode.rotation.y,
                                   oNode.rotation.z);
        auto scale_fb       = Vec3(oNode.scale.x,
                                   oNode.scale.y,
                                   oNode.scale.z);

        return CreateNode(oBuilder,
                          oNode.sName.empty() ? 0 : oBuilder.CreateString(oNode.sName),
                          &translation_fb,
                          &rotation_fb,
                          &scale_fb,
                          vChildren.size() ? oBuilder.CreateVector(vChildren) : 0,
                          vEntity.size() ?   oBuilder.CreateVector(vEntity) : 0,
                          oNode.sInfo.empty() ? 0 : oBuilder.CreateString(oNode.sInfo) );

}

SE::ret_code_t WriteSceneTree(const std::string sPath, const NodeData & oRootNode) {


        flatbuffers::FlatBufferBuilder oBuilder(1024);

        auto root_fb = SerializeNode(oRootNode, oBuilder);

        FinishNodeBuffer(oBuilder, root_fb);

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
