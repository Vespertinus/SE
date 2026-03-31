#ifdef SE_UI_ENABLED

#include <SDL2/SDL.h>

namespace SE {

UISystemInterface::UISystemInterface() {

        pCursorDefault     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        pCursorPointer     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
        pCursorText        = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
        pCursorMove        = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
        pCursorResizeH     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
        pCursorResizeV     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
        pCursorCross       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        pCursorUnavailable = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
}

UISystemInterface::~UISystemInterface() {

        SDL_FreeCursor(pCursorDefault);
        SDL_FreeCursor(pCursorPointer);
        SDL_FreeCursor(pCursorText);
        SDL_FreeCursor(pCursorMove);
        SDL_FreeCursor(pCursorResizeH);
        SDL_FreeCursor(pCursorResizeV);
        SDL_FreeCursor(pCursorCross);
        SDL_FreeCursor(pCursorUnavailable);
}

void UISystemInterface::Tick(float dt) {

        elapsed_time += static_cast<double>(dt);
}

double UISystemInterface::GetElapsedTime() {

        return elapsed_time;
}

bool UISystemInterface::LogMessage(Rml::Log::Type type, const Rml::String & message) {

        switch (type) {
                case Rml::Log::LT_ERROR:   log_e("RmlUi: {}", message); break;
                case Rml::Log::LT_WARNING: log_w("RmlUi: {}", message); break;
                default:                   log_d("RmlUi: {}", message); break;
        }
        return true;
}

void UISystemInterface::SetMouseCursor(const Rml::String & cursor_name) {

        SDL_Cursor * pCursor = pCursorDefault;
        if      (cursor_name == "pointer")                              pCursor = pCursorPointer;
        else if (cursor_name == "text" || cursor_name == "i-beam")      pCursor = pCursorText;
        else if (cursor_name == "move")                                 pCursor = pCursorMove;
        else if (cursor_name.substr(0, 13) == "rmlui-scroll-")          pCursor = pCursorMove;
        else if (cursor_name == "resize" || cursor_name == "ew-resize") pCursor = pCursorResizeH;
        else if (cursor_name == "ns-resize")                            pCursor = pCursorResizeV;
        else if (cursor_name == "cross")                                pCursor = pCursorCross;
        else if (cursor_name == "unavailable" ||
                 cursor_name == "not-allowed")                          pCursor = pCursorUnavailable;

        if (pCursor) SDL_SetCursor(pCursor);
}

void UISystemInterface::ActivateKeyboard(Rml::Vector2f, float) {

        SDL_StartTextInput();
}

void UISystemInterface::DeactivateKeyboard() {

        SDL_StopTextInput();
}

} // namespace SE

#endif // SE_UI_ENABLED
