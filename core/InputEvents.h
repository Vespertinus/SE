#ifndef INPUT_EVENTS_H
#define INPUT_EVENTS_H

#include <SDL2/SDL.h>
#include <glm/vec2.hpp>

namespace SE {

enum class MouseB : uint32_t {
        NONE   = 0,
        LEFT   = SDL_BUTTON_LMASK,
        MIDDLE = SDL_BUTTON_MMASK,
        RIGHT  = SDL_BUTTON_RMASK,
        X1     = SDL_BUTTON_X1MASK,
        X2     = SDL_BUTTON_X2MASK
};

struct EKeyDown         { SDL_Keycode key; SDL_Scancode scancode; uint16_t mod; };
struct EKeyUp           { SDL_Keycode key; SDL_Scancode scancode; uint16_t mod; };
struct ETextInput       { char text[SDL_TEXTINPUTEVENT_TEXT_SIZE]; };
struct EMouseMove       { glm::ivec2 pos; glm::ivec2 delta; };
struct EMouseButtonDown { glm::ivec2 pos; MouseB button; uint8_t clicks; };
struct EMouseButtonUp   { glm::ivec2 pos; MouseB button; };
struct EMouseWheel      { int32_t delta; };

} // namespace SE

#endif
