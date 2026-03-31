#ifndef __UI_EVENT_ROUTER_H__
#define __UI_EVENT_ROUTER_H__ 1

#ifdef SE_UI_ENABLED

#include <unordered_map>
#include <functional>
#include <memory>
#include <string>
#include <RmlUi/Core/EventListener.h>
#include <StrID.h>

namespace Rml {
class ElementDocument;
class Element;
}

namespace SE {

using ValidatorFn = std::function<bool(std::string &)>;

// One instance per loaded document; registered in capture phase to intercept
// click, submit, and change events.
class UIDocumentRouter : public Rml::EventListener {

        Rml::ElementDocument                            * pDoc;
        const std::unordered_map<StrID, ValidatorFn>   & rValidators;

public:
        UIDocumentRouter(Rml::ElementDocument * doc,
                         const std::unordered_map<StrID, ValidatorFn> & validators);

        void ProcessEvent(Rml::Event & ev) override;
        void OnDetach    (Rml::Element * element) override;
};

class UIEventRouter {

        std::unordered_map<Rml::ElementDocument *,
                           std::unique_ptr<UIDocumentRouter>> mRouters;
        std::unordered_map<StrID, ValidatorFn>               mValidators;

public:

        void Attach(Rml::ElementDocument * pDoc);
        void Detach(Rml::ElementDocument * pDoc);

        // Register a validator for an element (by id attribute).
        // fn(value) → true = accept (value may be modified in-place);
        //            false = reject (EUIAction is not posted).
        void RegisterValidator  (StrID element_id, ValidatorFn fn);
        void UnregisterValidator(StrID element_id);
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_EVENT_ROUTER_H__
