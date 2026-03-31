#ifdef SE_UI_ENABLED

#include <SDL2/SDL.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <ui/UITypes.h>
#include <StrID.h>
#include <EventManager.h>

namespace SE {

// --- UIDocumentRouter ---

UIDocumentRouter::UIDocumentRouter(Rml::ElementDocument * doc,
                                   const std::unordered_map<StrID, ValidatorFn> & validators)
        : pDoc(doc), rValidators(validators) {}

void UIDocumentRouter::ProcessEvent(Rml::Event & ev) {

        if (ev.GetId() == Rml::EventId::Click) {

                Rml::Element * pTarget = ev.GetTargetElement();
                if (!pTarget) return;

                const Rml::String data_event =
                        pTarget->GetAttribute("data-event", Rml::String{});
                if (data_event.empty()) return;

                EUIAction ev_data;
                ev_data.event_id   = StrID(data_event.c_str());
                const Rml::String elem_id = pTarget->GetId();
                ev_data.element_id = StrID(elem_id.c_str());

                GetSystem<EventManager>().TriggerEvent(ev_data);

        } else if (ev.GetId() == Rml::EventId::Submit) {

                Rml::Element * pForm = ev.GetTargetElement();
                if (!pForm) return;

                const Rml::String data_event =
                        pForm->GetAttribute("data-event", Rml::String{});
                if (data_event.empty()) return;

                EUIAction ev_data;
                ev_data.event_id   = StrID(data_event.c_str());
                ev_data.element_id = StrID(pForm->GetId().c_str());
                // string_val carries the submit button's value (may be empty)
                const Rml::String submit_val =
                        ev.GetParameter<Rml::String>("submit", Rml::String{});
                SDL_strlcpy(ev_data.string_val, submit_val.c_str(),
                            sizeof(ev_data.string_val));

                GetSystem<EventManager>().TriggerEvent(ev_data);

        } else if (ev.GetId() == Rml::EventId::Change) {

                Rml::Element * pInput = ev.GetTargetElement();
                if (!pInput) return;

                const Rml::String data_event =
                        pInput->GetAttribute("data-event", Rml::String{});
                if (data_event.empty()) return;

                std::string val(
                        ev.GetParameter<Rml::String>("value", Rml::String{}).c_str());

                const StrID elem_id(pInput->GetId().c_str());

                // Run validator if registered; reject if it returns false
                auto it = rValidators.find(elem_id);
                if (it != rValidators.end()) {
                        if (!it->second(val)) return;
                }

                EUIAction ev_data;
                ev_data.event_id   = StrID(data_event.c_str());
                ev_data.element_id = elem_id;
                SDL_strlcpy(ev_data.string_val, val.c_str(),
                            sizeof(ev_data.string_val));

                GetSystem<EventManager>().TriggerEvent(ev_data);
        }
}

void UIDocumentRouter::OnDetach(Rml::Element * /* element */) {
        // listener detached (document closed); nothing extra to do
}

// --- UIEventRouter ---

void UIEventRouter::Attach(Rml::ElementDocument * pDoc) {

        if (!pDoc || mRouters.count(pDoc)) return;
        auto router = std::make_unique<UIDocumentRouter>(pDoc, mValidators);
        pDoc->AddEventListener("click",  router.get(), true /* capture */);
        pDoc->AddEventListener("submit", router.get(), true);
        pDoc->AddEventListener("change", router.get(), true);
        mRouters.emplace(pDoc, std::move(router));
}

void UIEventRouter::Detach(Rml::ElementDocument * pDoc) {

        auto it = mRouters.find(pDoc);
        if (it == mRouters.end()) return;
        pDoc->RemoveEventListener("click",  it->second.get(), true);
        pDoc->RemoveEventListener("submit", it->second.get(), true);
        pDoc->RemoveEventListener("change", it->second.get(), true);
        mRouters.erase(it);
}

void UIEventRouter::RegisterValidator(StrID element_id, ValidatorFn fn) {

        mValidators[element_id] = std::move(fn);
}

void UIEventRouter::UnregisterValidator(StrID element_id) {

        mValidators.erase(element_id);
}

} // namespace SE

#endif // SE_UI_ENABLED
