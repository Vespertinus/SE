#ifndef __UI_SYSTEM_INTERFACE_H__
#define __UI_SYSTEM_INTERFACE_H__ 1

#ifdef SE_UI_ENABLED

#include <SDL2/SDL.h>
#include <RmlUi/Core/SystemInterface.h>

namespace SE {

class UISystemInterface : public Rml::SystemInterface {

        double       elapsed_time{0.0};

        SDL_Cursor * pCursorDefault    {nullptr};
        SDL_Cursor * pCursorPointer    {nullptr};
        SDL_Cursor * pCursorText       {nullptr};
        SDL_Cursor * pCursorMove       {nullptr};
        SDL_Cursor * pCursorResizeH    {nullptr};
        SDL_Cursor * pCursorResizeV    {nullptr};
        SDL_Cursor * pCursorCross      {nullptr};
        SDL_Cursor * pCursorUnavailable{nullptr};

public:
        UISystemInterface();
        ~UISystemInterface();

        void   Tick(float dt);
        double GetElapsedTime() override;
        bool   LogMessage(Rml::Log::Type type, const Rml::String & message) override;
        void   SetMouseCursor (const Rml::String & cursor_name) override;
        void   ActivateKeyboard  (Rml::Vector2f caret_position, float line_height) override;
        void   DeactivateKeyboard() override;
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_SYSTEM_INTERFACE_H__
