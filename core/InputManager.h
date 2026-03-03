#ifndef __INPUT_MANAGER_H__
#define __INPUT_MANAGER_H__

#include <string>
#include <unordered_set>
#include <unordered_map>

#include <InputEvents.h>

namespace SE {

using Key = SDL_Keycode;

class Joystick { //TODO
        public:
};

class InputManager {

        std::unordered_set<SDL_Keycode>              sKeyDown;
        std::unordered_set<SDL_Keycode>              sKeyPress;
        std::unordered_set<SDL_Scancode>             sScancodeDown;
        std::unordered_set<SDL_Scancode>             sScancodePress;
        std::unordered_map<SDL_JoystickID, Joystick> mJoysticks;

        std::string     sInputText;

        uint32_t        mouse_button_down;
        uint32_t        mouse_button_press;

        glm::ivec2      last_mouse_pos;
        glm::ivec2      mouse_move;
        int32_t         mouse_move_wheel;

        glm::ivec2      window_size;
        uint32_t        window_id;

        bool            mouse_visible;
        bool            mouse_grabbed;
        bool            input_focus;
        bool            minimized;

        public:
        InputManager();
        ~InputManager() noexcept;

        void Init(uint32_t window_id, bool grab_mouse, bool hide_mouse);
        void HandleEvent(const SDL_Event & event);
        void Capture();
        void SetWindowExtents(int32_t w, int32_t h);

        void SetMouseVisible(bool enable);
        void SetMouseGrabbed(bool grab);
        void SetMousePos(const glm::ivec2 & pos);
        void CenterMousePos();

        bool       GetKeyDown(Key key) const;
        bool       GetKeyPress(Key key) const;
        bool       GetScancodeDown(SDL_Scancode sc) const;
        bool       GetScancodePress(SDL_Scancode sc) const;
        glm::ivec2 GetMousePos() const;
        glm::ivec2 GetMouseMove() const;
        int32_t    GetMouseWheel() const;
        bool       GetMouseButtonDown(MouseB btn) const;
        bool       GetMouseButtonPress(MouseB btn) const;
        const std::string & GetInputText() const;
        bool       HasFocus() const;
        bool       IsMinimized() const;
        bool       IsMouseVisible() const;
        bool       IsMouseGrabbed() const;
};

} // namespace SE

#endif
