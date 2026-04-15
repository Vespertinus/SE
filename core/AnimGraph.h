
#ifndef __ANIM_GRAPH_H__
#define __ANIM_GRAPH_H__ 1

#include <string>
#include <vector>
#include <cstdint>

#include <ResourceHolder.h>

namespace SE::FlatBuffers { struct AnimationGraph; }

namespace SE {

/**
 * AnimGraph — animation graph definition resource.
 *
 * Stores the raw FlatBuffer bytes for an animation state machine definition.
 * The runtime state machine (AnimGraphInstance) is a separate object and
 * holds a pointer back to this resource's parsed data.
 *
 * Two constructors:
 *   Path-based:   AnimGraph(sName, rid)       — loads file into rawData_
 *   Inline FB:    AnimGraph(sName, rid, pFB)  — copies data into rawData_ (caller
 *                                               does not need to keep buffer alive)
 */
class AnimGraph : public ResourceHolder {

        std::vector<uint8_t>                  vRawData;
        const SE::FlatBuffers::AnimationGraph* pFB = nullptr;

public:
        AnimGraph(const std::string& sName, rid_t rid);
        AnimGraph(const std::string& sName, rid_t rid,
                  const SE::FlatBuffers::AnimationGraph* pSrcFB);

        const SE::FlatBuffers::AnimationGraph* GetFB() const { return pFB; }

        std::string Str() const;
};

} // namespace SE

#endif
