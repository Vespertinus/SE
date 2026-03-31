#ifdef SE_UI_ENABLED

#include <RmlUi/Core/Context.h>

namespace SE {

void UIThemeManager::ActivateTheme(const std::string & theme_name) {

        if (!pContext) return;

        if (!sActiveTheme.empty())
                pContext->ActivateTheme(sActiveTheme, false);

        sActiveTheme = theme_name;

        if (!sActiveTheme.empty())
                pContext->ActivateTheme(sActiveTheme, true);
}

} // namespace SE

#endif // SE_UI_ENABLED
