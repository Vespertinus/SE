#ifndef __UI_TYPES_H__
#define __UI_TYPES_H__ 1

#ifdef SE_UI_ENABLED

#include <cstdint>
#include <StrID.h>

namespace SE {

using UIDocumentId = uint32_t;
static constexpr UIDocumentId INVALID_UI_DOCUMENT = 0;

enum class ScreenTransition : uint8_t {
        NONE,
        FADE,
        SLIDE_LEFT,
        SLIDE_RIGHT,
        SCALE
};

enum class AnimEasing : uint8_t {
        LINEAR,
        EASE_IN,
        EASE_OUT,
        EASE_IN_OUT
};

enum class UILayer : uint8_t {
        HUD = 0,
        MENU,
        POPUP,
        DEBUG,
        SYSTEM,
        COUNT
};
static constexpr size_t UI_LAYER_COUNT = static_cast<size_t>(UILayer::COUNT);

// Posted to EventManager when a data-event attribute fires
struct EUIAction {
        StrID  event_id;
        StrID  element_id;
        float  float_val{0.0f};
        char   string_val[64]{};
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_TYPES_H__
