#ifndef __UI_SYSTEM_H__
#define __UI_SYSTEM_H__ 1

#ifdef SE_UI_ENABLED

#include <array>
#include <vector>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Types.h>
#include <ui/UITypes.h>
#include <ui/UIFileInterface.h>
#include <ui/UISystemInterface.h>
#include <ui/UIRenderInterface.h>
#include <ui/UIInputTranslator.h>
#include <ui/UIDocumentManager.h>
#include <ui/UIAnimationController.h>
#include <ui/UIScreenManager.h>
#include <ui/UIEventRouter.h>
#include <ui/UIThemeManager.h>
#include <ui/UIFontRegistry.h>
#include <ui/UILocalization.h>
#ifdef SE_UI_HOT_RELOAD
#include <ui/UIFileWatcher.h>
#endif

namespace SE {

class UISystem {

        // One bundle per named render layer (HUD → MENU → POPUP → DEBUG → SYSTEM).
        // Each bundle owns its own RmlUi context, document manager, animator and screen stack.
        struct UILayerBundle {
                Rml::Context        * pContext{nullptr};
                UIDocumentManager     oDocManager;
                UIAnimationController oAnimController;
                UIScreenManager       oScreenManager;
                UIThemeManager        oThemeManager;
        };

        UIRenderInterface     oRenderInterface;
        UIFileInterface       oFileInterface;
        UISystemInterface     oSystemInterface;
        UIEventRouter         oEventRouter;       // shared across all layers
        UIInputTranslator   * pInputTranslator{nullptr};

        UIFontRegistry                            oFontRegistry;
        UILocalization                            oLocalization;
        std::array<UILayerBundle, UI_LAYER_COUNT> vLayers;
#ifdef SE_UI_HOT_RELOAD
        UIFileWatcher                             oFileWatcher;
#endif

        void OnUpdate          (const Event & oEvent);
        void OnPostRenderUpdate(const Event & oEvent);
#ifdef DEBUG_BUILD
        int  debug_layer_idx{0};
        void OnDebugKeyDown    (const Event & oEvent);
#endif

public:
        UISystem();
        ~UISystem() noexcept;

        // Called by Application::ResizeViewport
        void SetDimensions(int width, int height);

        // Layer-aware accessors.  Default layer is MENU for backward compatibility.
        Rml::Context      * GetContext       (UILayer layer = UILayer::MENU);
        UIScreenManager   & GetScreenManager (UILayer layer = UILayer::MENU);
        UIDocumentManager & GetDocumentManager(UILayer layer = UILayer::MENU);
        UIThemeManager    & GetThemeManager  (UILayer layer = UILayer::MENU);
        UIInputTranslator & GetInputTranslator() { return *pInputTranslator; }
        UIFontRegistry    & GetFontRegistry()    { return oFontRegistry; }

        void               SetLocale      (const std::string & locale);
        UILocalization   & GetLocalization()  { return oLocalization; }
        UIEventRouter    & GetEventRouter()   { return oEventRouter; }
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_SYSTEM_H__
