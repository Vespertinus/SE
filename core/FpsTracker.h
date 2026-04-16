
#ifndef SE_FPS_TRACKER_H
#define SE_FPS_TRACKER_H 1

#include <array>
#include <cstdint>

namespace SE {

// Rolling FPS statistics over a fixed window of raw frame deltas.
// Owned by TEngine (TCoreSystems). Updated by Application::Run() each frame.
class FpsTracker {

        static constexpr int kWindowSize = 60;

        std::array<float, kWindowSize> vSamples{};
        int  head = 0;
        bool full = false;

public:

        void  Update(float raw_delta);

        float Fps()        const;   // smoothed FPS over the rolling window
        float FrameMs()    const;   // most recent frame time in milliseconds
        float MinFrameMs() const;   // minimum frame time in the window
        float MaxFrameMs() const;   // maximum frame time in the window
};

} // namespace SE

#ifdef SE_IMPL
#include <FpsTracker.tcc>
#endif

#endif
