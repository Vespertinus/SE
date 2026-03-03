
#include <SDL2/SDL.h>
#include <Logging.h>

namespace SE {

InputManager::InputManager() :
        mouse_button_down(0),
        mouse_button_press(0),
        last_mouse_pos(0, 0),
        mouse_move(0, 0),
        mouse_move_wheel(0),
        window_size(0, 0),
        window_id(0),
        mouse_visible(true),
        mouse_grabbed(false),
        input_focus(true),
        minimized(false) {
}

InputManager::~InputManager() noexcept { }

void InputManager::Init(uint32_t new_window_id, bool grab_mouse, bool hide_mouse) {

        window_id     = new_window_id;
        mouse_visible = !hide_mouse;
        mouse_grabbed = grab_mouse;

        SDL_ShowCursor(hide_mouse ? SDL_DISABLE : SDL_ENABLE);

        if (grab_mouse) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
        }
}

void InputManager::HandleEvent(const SDL_Event & event) {

        auto & oEventManager = GetSystem<EventManager>();

        switch (event.type) {

                case SDL_KEYDOWN:
                        if (event.key.repeat == 0) {
                                SDL_Keycode  key      = event.key.keysym.sym;
                                SDL_Scancode scancode = event.key.keysym.scancode;
                                uint16_t     mod      = static_cast<uint16_t>(event.key.keysym.mod);
                                sKeyDown.insert(key);
                                sKeyPress.insert(key);
                                sScancodeDown.insert(scancode);
                                sScancodePress.insert(scancode);
                                oEventManager.TriggerEvent(EKeyDown{key, scancode, mod});
                        }
                        break;

                case SDL_KEYUP: {
                        SDL_Keycode  key      = event.key.keysym.sym;
                        SDL_Scancode scancode = event.key.keysym.scancode;
                        uint16_t     mod      = static_cast<uint16_t>(event.key.keysym.mod);
                        sKeyDown.erase(key);
                        sScancodeDown.erase(scancode);
                        oEventManager.TriggerEvent(EKeyUp{key, scancode, mod});
                        break;
                }

                case SDL_TEXTINPUT:
                        sInputText += event.text.text;
                        {
                                ETextInput e{};
                                SDL_strlcpy(e.text, event.text.text, SE::TEXT_INPUT_SIZE);
                                oEventManager.TriggerEvent(e);
                        }
                        break;

                case SDL_MOUSEMOTION: {
                        glm::ivec2 pos  {event.motion.x,    event.motion.y};
                        glm::ivec2 delta{event.motion.xrel, event.motion.yrel};
                        last_mouse_pos = pos;
                        mouse_move    += delta;
                        oEventManager.TriggerEvent(EMouseMove{pos, delta});
                        break;
                }

                case SDL_MOUSEBUTTONDOWN: {
                        uint32_t   mask   = SDL_BUTTON(event.button.button);
                        glm::ivec2 pos    {event.button.x, event.button.y};
                        mouse_button_down  |= mask;
                        mouse_button_press |= mask;
                        oEventManager.TriggerEvent(EMouseButtonDown{pos, MouseB(mask), event.button.clicks});
                        break;
                }

                case SDL_MOUSEBUTTONUP: {
                        uint32_t   mask = SDL_BUTTON(event.button.button);
                        glm::ivec2 pos  {event.button.x, event.button.y};
                        mouse_button_down &= ~mask;
                        oEventManager.TriggerEvent(EMouseButtonUp{pos, MouseB(mask)});
                        break;
                }

                case SDL_MOUSEWHEEL: {
                        int32_t delta = event.wheel.y;
                        mouse_move_wheel += delta;
                        oEventManager.TriggerEvent(EMouseWheel{delta});
                        break;
                }

                case SDL_WINDOWEVENT:
                        if (event.window.windowID != window_id) { break; }
                        switch (event.window.event) {
                                case SDL_WINDOWEVENT_FOCUS_GAINED:
                                        input_focus = true;
                                        break;
                                case SDL_WINDOWEVENT_FOCUS_LOST:
                                        input_focus = false;
                                        sKeyDown.clear();
                                        sScancodeDown.clear();
                                        mouse_button_down = 0;
                                        break;
                                case SDL_WINDOWEVENT_MINIMIZED:
                                        minimized = true;
                                        break;
                                case SDL_WINDOWEVENT_RESTORED:
                                        minimized = false;
                                        break;
                        }
                        break;

                default:
                        break;
        }
}

void InputManager::Capture() {

        sKeyPress.clear();
        sScancodePress.clear();
        mouse_button_press = 0;
        mouse_move         = {0, 0};
        mouse_move_wheel   = 0;
        sInputText.clear();
}

void InputManager::SetWindowExtents(int32_t w, int32_t h) {

        window_size = {w, h};
}

void InputManager::SetMouseVisible(bool enable) {

        mouse_visible = enable;
        SDL_ShowCursor(enable ? SDL_ENABLE : SDL_DISABLE);
}

void InputManager::SetMouseGrabbed(bool grab) {

        mouse_grabbed = grab;
        SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE);
}

void InputManager::SetMousePos(const glm::ivec2 & pos) {

        SDL_WarpMouseInWindow(SDL_GetWindowFromID(window_id), pos.x, pos.y);
        last_mouse_pos = pos;
}

void InputManager::CenterMousePos() {

        SetMousePos(window_size / 2);
}

bool InputManager::GetKeyDown(Key key) const {

        return sKeyDown.count(key) > 0;
}

bool InputManager::GetKeyPress(Key key) const {

        return sKeyPress.count(key) > 0;
}

bool InputManager::GetScancodeDown(Scancode sc) const {

        return sScancodeDown.count(sc) > 0;
}

bool InputManager::GetScancodePress(Scancode sc) const {

        return sScancodePress.count(sc) > 0;
}

glm::ivec2 InputManager::GetMousePos() const {

        return last_mouse_pos;
}

glm::ivec2 InputManager::GetMouseMove() const {

        return mouse_move;
}

int32_t InputManager::GetMouseWheel() const {

        return mouse_move_wheel;
}

bool InputManager::GetMouseButtonDown(MouseB btn) const {

        return (mouse_button_down & static_cast<uint32_t>(btn)) != 0;
}

bool InputManager::GetMouseButtonPress(MouseB btn) const {

        return (mouse_button_press & static_cast<uint32_t>(btn)) != 0;
}

const std::string & InputManager::GetInputText() const {

        return sInputText;
}

bool InputManager::HasFocus() const {

        return input_focus;
}

bool InputManager::IsMinimized() const {

        return minimized;
}

bool InputManager::IsMouseVisible() const {

        return mouse_visible;
}

bool InputManager::IsMouseGrabbed() const {

        return mouse_grabbed;
}

} // namespace SE
