
#ifndef APP_INPUT_MAPPING_CONTEXT_H
#define APP_INPUT_MAPPING_CONTEXT_H 1

#include <InputState.h>
#include <InputEvents.h>
#include <glm/vec2.hpp>
#include <array>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <string>

namespace SE {

// Which raw input source drives a binding.
enum class InputSource : uint8_t {
    KEY_DOWN,              // keyboard key — true while held
    KEY_PRESS,             // keyboard key — true on first frame only
    SCANCODE_DOWN,
    SCANCODE_PRESS,
    MOUSE_AXIS_X,          // per-frame accumulated mouse delta X (pixels × scale)
    MOUSE_AXIS_Y,
    MOUSE_BUTTON_DOWN,
    MOUSE_BUTTON_PRESS,
    GAMEPAD_BUTTON_DOWN,
    GAMEPAD_BUTTON_PRESS,
    GAMEPAD_AXIS,          // 0=LeftX, 1=LeftY, 2=RightX, 3=RightY, 4=TrigL, 5=TrigR
};

// Which field of InputState this binding writes to.
enum class InputAction : uint8_t {
    MOVE_AXIS_X,
    MOVE_AXIS_Y,
    LOOK_AXIS_X,
    LOOK_AXIS_Y,
    JUMP_PRESSED,
    JUMP_HELD,
    SPRINT_HELD,
    CROUCH_HELD,
    INTERACT_PRESSED,
};

// One binding: (source, code) → (action, scale).
// Multiple bindings may share the same action; their scaled values are summed
// and the result clamped to [-1, 1] for axes, or OR-ed for boolean actions.
//
// code meaning by source:
//   KEY_DOWN/PRESS        → SDL keycode (Key / int32_t)
//   SCANCODE_DOWN/PRESS   → SDL scancode (Scancode / int32_t)
//   MOUSE_BUTTON_DOWN/PRESS → MouseB bitmask bit (cast to int32_t)
//   GAMEPAD_BUTTON_DOWN/PRESS → GamepadButton index (0–14)
//   GAMEPAD_AXIS          → axis_id (0–5)
//   MOUSE_AXIS_X/Y        → code unused
struct InputBinding {
    InputSource source = InputSource::KEY_DOWN;
    InputAction action = InputAction::MOVE_AXIS_X;
    float       scale  = 1.f;
    int32_t     code   = 0;
};

// Self-contained component that translates raw InputManager events into an
// InputState sibling on the same entity. Add alongside InputState; no extra
// system in TCustomSystems is needed.
//
// Subscribes to EKeyDown/Up, EMouseMove, EGamepadAxis, EGamepadButtonDown/Up
// as events arrive, then flushes to InputState in OnInputUpdate (EInputUpdate).
//
// Rotating move_axis by camera yaw should be done here (in the application's
// OnInputUpdate override or via a dedicated binding post-process hook).
class InputMappingContext {
public:
        explicit InputMappingContext(TSceneTree::TSceneNodeExact* pOwner);
        ~InputMappingContext() noexcept;

        std::vector<InputBinding> vBindings;

        void Enable()  {}
        void Disable() {}

        std::string Str()       const { return "InputMappingContext"; }
        void        DrawDebug() const {}

private:
        void OnKeyDown(const Event& e);
        void OnKeyUp(const Event& e);
        void OnMouseMove(const Event& e);
        void OnMouseButtonDown(const Event& e);
        void OnMouseButtonUp(const Event& e);
        void OnGamepadAxis(const Event& e);
        void OnGamepadButtonDown(const Event& e);
        void OnGamepadButtonUp(const Event& e);
        void OnInputUpdate(const Event& e);

        TSceneTree::TSceneNodeExact* pNode;

        // Per-frame accumulated mouse delta (reset after each flush)
        glm::ivec2 mouse_delta_acc {0, 0};

        // Held and one-frame-pressed sets for keyboard
        std::unordered_set<Key>      sHeldKeys;
        std::unordered_set<Key>      sPressedKeys;
        std::unordered_set<Scancode> sHeldScancodes;
        std::unordered_set<Scancode> sPressedScancodes;

        // Mouse buttons — SDL bitmask
        uint32_t held_mouse_buttons    = 0;
        uint32_t pressed_mouse_buttons = 0;

        // Gamepad — latest axis values (normalized [-1, 1]) and button state
        std::array<float, 6>  gamepad_axes           {};
        std::array<bool, static_cast<size_t>(GamepadButton::COUNT)> gamepad_buttons_held    {};
        std::array<bool, static_cast<size_t>(GamepadButton::COUNT)> gamepad_buttons_pressed {};
};

} // namespace SE

#endif
