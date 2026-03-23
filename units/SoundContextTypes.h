
#ifndef __SOUND_CONTEXT_TYPES_H__
#define __SOUND_CONTEXT_TYPES_H__ 1

#include <cstdint>
#include <SoundContext.h>

namespace SE {

// ---------------------------------------------------------------------------
// SurfaceType
// ---------------------------------------------------------------------------

enum class SurfaceType : uint8_t {
        DEFAULT = 0, STONE, WOOD, METAL, DIRT, WATER, SAND, GRASS, CONCRETE, COUNT
};

static const char* const kSurfaceNames[] = {
        "default","stone","wood","metal","dirt","water","sand","grass","concrete"
};

// ---------------------------------------------------------------------------
// CharacterSoundContext
// ---------------------------------------------------------------------------

struct CharacterSoundContext : SoundEventContext {
        float       speed   = 0.0f;
        float       weight  = 1.0f;
        float       wetness = 0.0f;
        float       impact  = 0.0f;
        SurfaceType surface = SurfaceType::DEFAULT;

        // Shadows base non-virtually — static dispatch via template TCtx
        float GetParameter(StrID param_id) const;
};

// ---------------------------------------------------------------------------
// VehicleSoundContext
// ---------------------------------------------------------------------------

struct VehicleSoundContext : SoundEventContext {
        float rpm_normalized    = 0.0f;
        float engine_load       = 0.0f;
        float lateral_slip      = 0.0f;
        float longitudinal_slip = 0.0f;
        float impact            = 0.0f;

        // Shadows base non-virtually — static dispatch via template TCtx
        float GetParameter(StrID param_id) const;
};

} // namespace SE

#endif
