
#ifndef __FLATBUFFERS_COMPONENT_WRITER_DETAILS_H__
#define __FLATBUFFERS_COMPONENT_WRITER_DETAILS_H__

#include "ErrCode.h"
#include "Common.h"

namespace SE {
namespace TOOLS {

std::tuple<flatbuffers::Offset<SE::FlatBuffers::Component>, ret_code_t> SerializeComponent(
                const TComponent                & oComponent,
                flatbuffers::FlatBufferBuilder  & oBuilder);

}
}
#endif


