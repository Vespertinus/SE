
#ifndef __PHYSICS_DEBUG_CONTROLLER_H__
#define __PHYSICS_DEBUG_CONTROLLER_H__

namespace SE {

// Keyboard controls:
//   P  — toggle physics pause
//   N  — advance one fixed step (only when paused)
class PhysicsDebugController {

        bool paused { false };

        void OnKeyDown(const Event & oEvent);

public:
        PhysicsDebugController();
        ~PhysicsDebugController() noexcept;
        void SetPaused(bool paused);
};

} // namespace SE

#endif
