
#ifndef __FLATBUFFERS_COMPONENT_WRITER_DETAILS_H__
#define __FLATBUFFERS_COMPONENT_WRITER_DETAILS_H__

#include "ErrCode.h"
#include "Common.h"

namespace SE {
namespace TOOLS {

std::tuple<flatbuffers::Offset<SE::FlatBuffers::Component>, ret_code_t> SerializeComponent(
                const TComponent                & oComponent,
                flatbuffers::FlatBufferBuilder  & oBuilder,
                const ImportCtx                 & oCtx);

/** Build a Component(Animator) offset with an inline AnimationGraph whose states
 *  map 1:1 to oCtx.vAnimClips.  Returns a zero offset if there are no clips. */
flatbuffers::Offset<SE::FlatBuffers::Component> SerializeAnimatorComponent(
                flatbuffers::FlatBufferBuilder  & oBuilder,
                const ImportCtx                 & oCtx);

}
}
#endif


