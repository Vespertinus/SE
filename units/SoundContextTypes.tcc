
#include <SoundContextTypes.h>
#include <StrID.h>

namespace SE {

// ---------------------------------------------------------------------------
// CharacterSoundContext
// ---------------------------------------------------------------------------

float CharacterSoundContext::GetParameter(StrID param_id) const {
        static const StrID kSpeed  ("speed");
        static const StrID kWeight ("weight");
        static const StrID kWetness("wetness");
        static const StrID kImpact ("impact");
        if (param_id == kSpeed)    return speed;
        if (param_id == kWeight)   return weight;
        if (param_id == kWetness)  return wetness;
        if (param_id == kImpact)   return impact;
        return 0.0f;
}

// ---------------------------------------------------------------------------
// VehicleSoundContext
// ---------------------------------------------------------------------------

float VehicleSoundContext::GetParameter(StrID param_id) const {
        static const StrID kRpm    ("rpm_normalized");
        static const StrID kLoad   ("engine_load");
        static const StrID kLat    ("lateral_slip");
        static const StrID kLong   ("longitudinal_slip");
        static const StrID kImpact ("impact");
        if (param_id == kRpm)    return rpm_normalized;
        if (param_id == kLoad)   return engine_load;
        if (param_id == kLat)    return lateral_slip;
        if (param_id == kLong)   return longitudinal_slip;
        if (param_id == kImpact) return impact;
        return 0.0f;
}

} // namespace SE
