#ifndef __UI_ANIMATION_CONTROLLER_H__
#define __UI_ANIMATION_CONTROLLER_H__ 1

#ifdef SE_UI_ENABLED

#include <functional>
#include <ui/UITypes.h>

namespace SE {

class UIDocumentManager;

class UIAnimationController {

        struct ActiveAnim {
                UIDocumentId          out_id      {INVALID_UI_DOCUMENT};
                UIDocumentId          in_id       {INVALID_UI_DOCUMENT};
                ScreenTransition      type        {ScreenTransition::NONE};
                AnimEasing            easing      {AnimEasing::LINEAR};
                float                 progress    {0.0f};
                float                 duration    {0.3f};
                float                 viewport_w  {0.0f};
                std::function<void()> fnOnComplete;
        };

        ActiveAnim         oAnim;
        UIDocumentManager * pDocMgr{nullptr};

        static float Ease(float t, AnimEasing easing);

public:
        UIAnimationController() = default;

        void Init(UIDocumentManager & mgr) { pDocMgr = &mgr; }

        void Start(UIDocumentId out_id, UIDocumentId in_id,
                   ScreenTransition type, float duration = 0.3f,
                   AnimEasing easing = AnimEasing::LINEAR,
                   std::function<void()> on_complete = nullptr);
        void Update(float dt);

        bool IsComplete() const { return oAnim.type == ScreenTransition::NONE; }
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_ANIMATION_CONTROLLER_H__
