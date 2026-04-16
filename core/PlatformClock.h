
#ifndef SE_PLATFORM_CLOCK_H
#define SE_PLATFORM_CLOCK_H 1

#include <chrono>

namespace SE {

// Thin wrapper over std::chrono::high_resolution_clock.
// Returns wall time as double seconds. Baseline is set on first Now() call.
// Used only by Application::Run() to compute raw frame deltas.
struct PlatformClock {

        static double Now() {
                static const auto baseline = std::chrono::high_resolution_clock::now();
                const auto cur = std::chrono::high_resolution_clock::now();
                return std::chrono::duration<double>(cur - baseline).count();
        }

        static double Resolution() {
                return static_cast<double>(std::chrono::high_resolution_clock::period::num) /
                       static_cast<double>(std::chrono::high_resolution_clock::period::den);
        }
};

} // namespace SE

#endif
