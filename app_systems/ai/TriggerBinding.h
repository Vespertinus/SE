
#ifndef APP_AI_TRIGGER_BINDING_H
#define APP_AI_TRIGGER_BINDING_H 1

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <StrID.h>
#include <ai/BlackboardAsset.h>

namespace SE::FlatBuffers { struct TriggerBinding; }

namespace SE {

// ---------------------------------------------------------------------------
// TriggerBinding — authored trigger→blackboard writes as inert component data.
//
// Place on the same node as a TriggerVolume; BlackboardSystem listens to
// ETriggerFired, matches bindings by event_id and writes into the global
// blackboard or the trigger node's own Blackboard component 
// (keeps TriggerVolume free of AI typelist dependencies).
//
// Matching by event_id covers enter/exit/stay uniformly. No lifecycle — the
// component is pure data; nothing to Enable/Disable.
// ---------------------------------------------------------------------------
class TriggerBinding {

public:
        using TSerialized = FlatBuffers::TriggerBinding;

        // One authored write. The value is type-tagged (must match the target
        // key's schema type — checked at dispatch, one-shot warn, no write).
        struct SDef {
                StrID       event_id;
                std::string s_event;         // debug display
                bool        global_target = true;
                StrID       key;
                std::string s_key;           // debug display / warn messages

                bool        has_value      = false;
                EBBType     value_type     = EBBType::Bool;
                float       f_value        = 0.f;
                bool        b_value        = false;
                int32_t     i_value        = 0;
                glm::vec3   v_value        {};
                bool        write_position = false;   // write the entering actor's position instead

                mutable bool warned = false;          // one-shot warn per binding
        };

        struct Desc {
                std::vector<SDef> bindings;
        };

        TriggerBinding(TSceneTree::TSceneNodeExact* pNode, const Desc& desc);
        TriggerBinding(TSceneTree::TSceneNodeExact* pNode, const FlatBuffers::TriggerBinding* pFB);

        // SceneNode's component lifecycle hooks — TriggerBinding is pure data,
        // nothing to enable; the hooks exist for the CreateComponent contract.
        void Enable() {}
        void Disable() {}

        const std::vector<SDef>& GetBindings() const { return vBindings; }
        std::vector<SDef>&       GetBindings()       { return vBindings; }   // warn-once flags

        std::string Str() const;

private:
        std::vector<SDef> vBindings;
};

} // namespace SE

#endif
