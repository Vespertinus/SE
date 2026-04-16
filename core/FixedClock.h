
#ifndef SE_FIXED_CLOCK_H
#define SE_FIXED_CLOCK_H 1

namespace SE {

// Fixed-timestep accumulator utility. Not a TEngine system; used internally
// by PhysicsSystem (and any other system that needs a fixed-rate update loop).
//
// Usage per frame:
//   while (fixed_clock.Step(game_dt)) { RunFixedStep(); }
//   float alpha = fixed_clock.Alpha();   // interpolation blend [0, 1]
//
// game_dt is consumed on the first Step() call; subsequent calls in the same
// while-loop drain the accumulator without re-adding game_dt.
class FixedClock {

        float fixed_dt   = 1.0f / 60.0f;
        float accumulator = 0.0f;
        float alpha       = 0.0f;
        bool  fed         = false;

public:

        explicit FixedClock(float fixed_dt = 1.0f / 60.0f);

        bool  Step(float game_delta);

        float Dt()    const { return fixed_dt; }
        float Alpha() const { return alpha; }

        // Reset accumulator (e.g. when physics is paused/resumed).
        void  Reset();
};

} // namespace SE

#ifdef SE_IMPL
#include <FixedClock.tcc>
#endif

#endif
