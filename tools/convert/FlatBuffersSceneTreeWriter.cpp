#include <fstream>

#include <Logging.h>
#include <SceneTree_generated.h>
#include "FlatBuffersSceneTreeWriter.h"
#include "FlatBuffersComponentWriterDetails.h"

namespace SE {
namespace TOOLS {

//TODO check duplication in import from fbx and write only one mesh copy

using StringOffset = flatbuffers::Offset<flatbuffers::String>;
using SE::FlatBuffers::Node;
using SE::FlatBuffers::Vec3;
using SE::FlatBuffers::Component;

static std::tuple<flatbuffers::Offset<Node>, ret_code_t> SerializeNode(
                const NodeData & oNode,
                flatbuffers::FlatBufferBuilder & oBuilder) {

        std::vector<flatbuffers::Offset<Node>> vChildren;
        std::vector<flatbuffers::Offset<Component>> vComponents;

        for (auto & item : oNode.vComponents) {

                auto [offset, res] =  SerializeComponent(item, oBuilder);
                if (res != uSUCCESS) {
                        return {0, res};
                }

                vComponents.emplace_back(offset);
        }

        for (auto & item : oNode.vChildren) {
                auto [offset, res] =  SerializeNode(item, oBuilder);
                if (res != uSUCCESS) {
                        return {0, res};
                }
                vChildren.emplace_back(offset);
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

        return {
                CreateNode(oBuilder,
                          oNode.sName.empty() ? 0 : oBuilder.CreateString(oNode.sName),
                          &translation_fb,
                          &rotation_fb,
                          &scale_fb,
                          vComponents.size() ?   oBuilder.CreateVector(vComponents) : 0,
                          vChildren.size() ? oBuilder.CreateVector(vChildren) : 0,
                          oNode.sInfo.empty() ? 0 : oBuilder.CreateString(oNode.sInfo),
                          oNode.enabled),
                 uSUCCESS
        };

}

SE::ret_code_t WriteSceneTree(const std::string sPath, const NodeData & oRootNode) {


        flatbuffers::FlatBufferBuilder oBuilder(1024);

        auto [root_fb, res] = SerializeNode(oRootNode, oBuilder);
        if (res != uSUCCESS) {
                return res;
        }

        auto scene_fb = CreateSceneTree(oBuilder, root_fb);

        FinishSceneTreeBuffer(oBuilder, scene_fb);

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
