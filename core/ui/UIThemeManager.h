#ifndef __UI_THEME_MANAGER_H__
#define __UI_THEME_MANAGER_H__ 1

#ifdef SE_UI_ENABLED

#include <string>

namespace Rml {
class Context;
}

namespace SE {

class UIThemeManager {

        Rml::Context * pContext{nullptr};
        std::string    sActiveTheme;

public:
        UIThemeManager() = default;

        void Init(Rml::Context * ctx) { pContext = ctx; }

        void               ActivateTheme(const std::string & theme_name);
        const std::string& GetActiveTheme() const { return sActiveTheme; }
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_THEME_MANAGER_H__
