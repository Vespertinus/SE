#ifndef __UI_WIDGET_H__
#define __UI_WIDGET_H__ 1

#ifdef SE_UI_ENABLED

#include <string>
#include <functional>
#include <glm/vec3.hpp>
#include <ui/UITypes.h>
#include <RmlUi/Core/DataModelHandle.h>

namespace Rml {
class ElementDocument;
class DataModelConstructor;
}

namespace SE {

class Event;

// Component that loads an RmlUi document and repositions it each frame by projecting
// the owning node's world-space position into screen space.
//
// Data model: a unique model name is generated per instance. The RML file must have
// a data-model attribute on its root element (any placeholder name); UIWidget replaces
// the value at load time with the generated name.
//
// The document is loaded once at construction and kept alive for the widget's lifetime.
// Enable()/Disable() show and hide it without touching the disk.
//
// Position is updated reactively: UIWidget subscribes to ECameraChanged and registers
// as a TargetTransformChanged listener on its own node. No per-frame poll is needed.
//
// Call Dirty() whenever the data backing the model's BindFunc getters has changed;
// UIWidget will not dirty the model automatically.
class UIWidget {

        TSceneTree::TSceneNodeExact * pNode;
        Rml::ElementDocument        * pDoc    {nullptr};
        std::string                   sPath;
        UILayer                       eLayer  {UILayer::HUD};
        glm::vec3                     vOffset {0.0f};

        std::string                                       sModelName;
        std::function<void(Rml::DataModelConstructor &)> fnSetup;
        Rml::DataModelHandle                              hModel;
        bool                                              position_subscribed{false};

        void UpdatePosition();
        void OnCameraChanged(const Event & oEvent);

public:
        void TargetTransformChanged(TSceneTree::TSceneNodeExact * pTargetNode);
        UIWidget(TSceneTree::TSceneNodeExact *                    pNode,
                 std::string                                       path,
                 UILayer                                           layer,
                 std::function<void(Rml::DataModelConstructor &)> setup_fn,
                 glm::vec3                                         offset = glm::vec3(0.0f));
        ~UIWidget() noexcept;

        void Enable();
        void Disable();

        // Mark all model variables dirty — call whenever the backing data has changed.
        void Dirty();

        // Shift the projected anchor in world space (e.g. +Y to float above a character).
        void SetOffset(const glm::vec3 & offset) { vOffset = offset; }

        Rml::ElementDocument * GetDocument() const { return pDoc; }

        void        Print   (const size_t) const {}
        std::string Str     () const { return "UIWidget(" + sPath + ")"; }
        void        DrawDebug() const {}
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_WIDGET_H__
