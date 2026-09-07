
#ifndef APP_INPUT_STATE_H
#define APP_INPUT_STATE_H 1

#include <glm/vec2.hpp>
#include <string>

namespace SE {

// Written each frame by a player input bridge or AI controller.
// Read by CharacterMovementSystem. The movement system never calls SDL directly.
//
// move_axis: world-relative XZ plane — x=right, y=forward.
//   Typically computed from camera-relative stick input rotated by camera yaw.
//   Callers should pass a normalised vector; the system clamps magnitude to [0, 1].
struct InputState {
        InputState() = default;
        explicit InputState(TSceneTree::TSceneNodeExact*) {}

        glm::vec2 move_axis       {0.f, 0.f};
        glm::vec2 look_axis       {0.f, 0.f};  // mouse / right-stick delta (degrees)
        bool      jump_pressed    = false;      // true on the first frame of jump input
        bool      jump_held       = false;      // true while jump is held
        bool      sprint_held     = false;
        bool      crouch_held     = false;
        bool      interact_pressed = false;

        void Enable()  {}
        void Disable() {}

        std::string Str() const;
        void DrawDebug() const {}
};

inline std::string InputState::Str() const {
        return "InputState";
}

} // namespace SE

#endif
