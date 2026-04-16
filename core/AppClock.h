
#ifndef SE_APP_CLOCK_H
#define SE_APP_CLOCK_H 1

#include <cstdint>

namespace SE {

// Scaled, pausable game clock. Owned by TEngine (TCoreSystems).
// Application::Run() calls Tick() once per frame with the raw wall delta.
// All gameplay systems read Delta() for dt.
class AppClock {

        float    scaled_delta = 0.0f;
        float    raw_delta    = 0.0f;
        float    total_scaled = 0.0f;
        float    time_scale   = 1.0f;
        bool     paused       = false;
        uint64_t frame_count  = 0;

        static constexpr float kMaxRawDelta = 1.0f / 15.0f;  // 4-frame spike cap

public:

        // Feed the raw wall-clock delta for this frame. Called by Application::Run().
        void     Tick(double wall_delta);

        // Scaled game delta — use everywhere in gameplay logic.
        float    Delta()    const { return scaled_delta; }

        // Unscaled wall delta — use for UI, input, and debug overlays only.
        float    RawDelta() const { return raw_delta; }

        // Total accumulated scaled time since startup.
        float    Total()    const { return total_scaled; }

        // Frame counter. Monotonically increasing, never resets.
        uint64_t Frame()    const { return frame_count; }

        void  Pause();
        void  Resume();
        bool  IsPaused()  const { return paused; }

        // 0.0 = effectively paused; 0.5 = slow motion; 2.0 = fast forward.
        void  SetScale(float s);
        float Scale()     const { return time_scale; }
};

} // namespace SE

#ifdef SE_IMPL
#include <AppClock.tcc>
#endif

#endif
