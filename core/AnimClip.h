
#ifndef __ANIM_CLIP_H__
#define __ANIM_CLIP_H__ 1

#include <string>
#include <vector>
#include <cstdint>

#include <ResourceHolder.h>
#include <StrID.h>

namespace SE::FlatBuffers { struct AnimationClip; }

namespace SE {

/**
 * AnimClip — runtime decoded animation clip asset.
 *
 * Stores per-bone curve channels and timed animation events decoded from a
 * baked .seak FlatBuffer file.
 *
 * Two constructors:
 *   Path-based:   AnimClip(sName, rid)              — loads and decodes file
 *   Inline FB:    AnimClip(sName, rid, pFB)         — decodes from live FlatBuffer
 */
class AnimClip : public ResourceHolder {

public:
        enum class Format : uint8_t {
                ConstantF32 = 0,
                StepF32     = 1,
                HermiteF32  = 2,
                LinearF32   = 3
                // Quantized16 (FlatBuffer value 3) is dequantized at load time and stored as HermiteF32
        };

        struct CurveChannel {
                uint16_t          bone_index = 0;
                uint8_t           target     = 0;   // 0-2=pos xyz, 3-6=rot xyzw, 7-9=scale xyz
                Format            format     = Format::HermiteF32;
                std::vector<float> vTimes;
                std::vector<float> vValues;
                std::vector<float> vTangents;  // empty for ConstantF32/StepF32
        };

        struct AnimEvent {
                float       time  = 0.0f;
                std::string name;   ///< original string (for logging / routing by name)
                StrID       nameID; ///< pre-hashed id (for fast equality checks)
                float       value = 0.0f;
        };

private:
        float                    duration = 0.0f;
        bool                     looping  = false;
        std::vector<CurveChannel> vChannels;
        std::vector<AnimEvent>    vEvents;   // sorted by time

        void LoadFromFB(const SE::FlatBuffers::AnimationClip* pFB);

public:
        AnimClip(const std::string& sName, rid_t rid);
        AnimClip(const std::string& sName, rid_t rid,
                 const SE::FlatBuffers::AnimationClip* pFB);

        float                           Duration()  const { return duration; }
        bool                            Looping()   const { return looping; }
        const std::vector<CurveChannel>& Channels() const { return vChannels; }
        const std::vector<AnimEvent>&    Events()   const { return vEvents; }

        std::string Str() const;
};

} // namespace SE

#endif
