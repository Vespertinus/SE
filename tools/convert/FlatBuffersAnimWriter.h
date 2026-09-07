
#ifndef __FLATBUFFERS_ANIM_WRITER_H__
#define __FLATBUFFERS_ANIM_WRITER_H__

#include <ErrCode.h>
#include <flatbuffers/flatbuffers.h>
#include <AnimationClip_generated.h>
#include "Common.h"

namespace SE {
namespace TOOLS {

/** Serialize one clip into an existing builder. Reusable for inline embedding. */
std::tuple<flatbuffers::Offset<SE::FlatBuffers::AnimationClip>, SE::ret_code_t>
SerializeAnimClip(const AnimClipData & oClip, flatbuffers::FlatBufferBuilder & oBuilder);

SE::ret_code_t WriteAnimClip(const std::string & sPath, const AnimClipData & oClip);
SE::ret_code_t WriteSkeleton(const std::string & sPath, const Skeleton & oSkel);

}
}
#endif
