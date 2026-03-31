#ifndef INPUT_EVENTS_H
#define INPUT_EVENTS_H

#include <cstdint>
#include <cstddef>
#include <glm/vec2.hpp>

namespace SE {

using Key      = int32_t;
using Scancode = int32_t;
using Keymod   = uint16_t;

static constexpr size_t TEXT_INPUT_SIZE = 32;

enum class MouseB : uint32_t {
        NONE   = 0,
        LEFT   = 0x01,
        MIDDLE = 0x02,
        RIGHT  = 0x04,
        X1     = 0x08,
        X2     = 0x10
};

enum class GamepadButton : uint8_t {
        A = 0, B, X, Y,
        DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT,
        START, BACK,
        LEFT_SHOULDER, RIGHT_SHOULDER,
        LEFT_STICK, RIGHT_STICK,
        COUNT
};

struct EKeyDown          { Key key; Scancode scancode; Keymod mod; };
struct EKeyUp            { Key key; Scancode scancode; Keymod mod; };
struct ETextInput        { char text[TEXT_INPUT_SIZE]; };
struct EMouseMove        { glm::ivec2 pos; glm::ivec2 delta; };
struct EMouseButtonDown  { glm::ivec2 pos; MouseB button; uint8_t clicks; };
struct EMouseButtonUp    { glm::ivec2 pos; MouseB button; };
struct EMouseWheel       { int32_t delta; };
struct EGamepadButtonDown { int32_t device_id; GamepadButton button; };
struct EGamepadButtonUp   { int32_t device_id; GamepadButton button; };
// axis_id: 0=LeftX, 1=LeftY, 2=RightX, 3=RightY, 4=TrigL, 5=TrigR
struct EGamepadAxis       { int32_t device_id; uint8_t axis_id; int16_t value; };

} // namespace SE

#endif
