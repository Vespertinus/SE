#ifndef __UI_SCREEN_MANAGER_H__
#define __UI_SCREEN_MANAGER_H__ 1

#ifdef SE_UI_ENABLED

#include <string>
#include <vector>
#include <ui/UITypes.h>

namespace SE {

class UIDocumentManager;
class UIAnimationController;

class UIScreenManager {

        struct ScreenEntry {
                UIDocumentId id  {INVALID_UI_DOCUMENT};
                std::string  sPath;
        };

        std::vector<ScreenEntry>  vStack;
        std::vector<UIDocumentId> vPersistent;
        UIDocumentManager       * pDocMgr  {nullptr};
        UIAnimationController   * pAnimCtrl{nullptr};
        bool                      top_is_modal{false};

public:
        UIScreenManager() = default;

        void Init(UIDocumentManager & docMgr, UIAnimationController & animCtrl);

        UIDocumentId Push     (const std::string & path,
                               ScreenTransition transition = ScreenTransition::NONE,
                               float duration = 0.3f,
                               AnimEasing easing = AnimEasing::LINEAR);
        void         Pop      (ScreenTransition transition = ScreenTransition::NONE,
                               float duration = 0.3f,
                               AnimEasing easing = AnimEasing::LINEAR);
        void         Replace  (const std::string & path,
                               ScreenTransition transition = ScreenTransition::NONE,
                               float duration = 0.3f,
                               AnimEasing easing = AnimEasing::LINEAR);
        void         PopTo    (size_t target_size);

        // Show doc as modal (blocks input to layers below this one).
        UIDocumentId PushModal(const std::string & path,
                               ScreenTransition transition = ScreenTransition::FADE,
                               float duration = 0.3f,
                               AnimEasing easing = AnimEasing::LINEAR);

        // Persistent layer: loaded/shown once, unaffected by stack operations.
        UIDocumentId ShowPersistent(const std::string & path);
        void         HidePersistent(UIDocumentId id);

        size_t       Size()     const { return vStack.size(); }
        UIDocumentId Top()      const;
        bool         HasModal() const { return top_is_modal; }

        void Update(float dt);
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_SCREEN_MANAGER_H__
