
#ifndef __SOUND_CONTEXT_H__
#define __SOUND_CONTEXT_H__ 1

#include <AudioTypes.h>
#include <StrID.h>

namespace SE {

class SoundEmitter;  // forward

// ---------------------------------------------------------------------------
// SoundEventContext — spatial anchor + optional parameter bag
// No virtual methods — domain subclasses shadow GetParameter non-virtually.
// ---------------------------------------------------------------------------

struct SoundEventContext {
        SoundEmitter* pEmitter        = nullptr;
        glm::vec3     vPosition       = {};
        glm::vec3     vVelocity       = {};
        float         volume_override = -1.0f;   // < 0 = no override
        float         pitch_override  = -1.0f;   // < 0 = no override
        MixBusId      bus_override    = MixBusId::COUNT; // COUNT = no override

        // Non-virtual default — returns 0 for any unknown parameter
        float GetParameter(StrID /*param_id*/) const { return 0.0f; }
};

} // namespace SE

#endif
