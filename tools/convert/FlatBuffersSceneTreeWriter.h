
#ifndef __FLATBUFFERS_SCENE_TREE_WRITER_H__
#define __FLATBUFFERS_SCENE_TREE_WRITER_H__

#include <ErrCode.h>
#include "Common.h"

namespace SE {
namespace TOOLS {

SE::ret_code_t WriteSceneTree(const std::string sPath, const NodeData & oRootNode);

}
}
#endif
