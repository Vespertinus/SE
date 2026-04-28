
#ifndef APP_TRIGGER_VOLUME_H
#define APP_TRIGGER_VOLUME_H 1

#include <unordered_set>
#include <string_view>

#include <StrID.h>
#include <PhysicsTypes.h>
#include <Component_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// ETriggerFired — posted to EventManager when a TriggerVolume fires.
// Trivially copyable: StrID is uint64_t, BodyHandle is two uint32_t.
// Game logic: AddListener<ETriggerFired, ...> and check ev.event_id == StrID("name").
// ---------------------------------------------------------------------------

struct ETriggerFired {
        StrID      event_id;
        BodyHandle hTrigger;
        BodyHandle hOther;
};

// ---------------------------------------------------------------------------
// TriggerVolumeDesc — programmatic construction (Scene.tcc, tests).
// ---------------------------------------------------------------------------

struct TriggerVolumeDesc {
        ColliderDesc oCollider;
        std::string  on_enter_event;
        std::string  on_exit_event;
        std::string  on_stay_event;
        float        stay_interval   = 0.0f;
        bool         one_shot        = false;
        uint32_t     collision_layer = CollisionLayers::DEFAULT;
        uint32_t     collision_mask  = 0xFFFFFFFFu;
};

// ---------------------------------------------------------------------------
// TriggerVolume — sensor body component that posts named events on overlap.
//
// Authored in entity templates via TriggerVolume FlatBuffer table, or
// created programmatically via TriggerVolumeDesc.
// Lifecycle follows RigidBody: constructor creates the Jolt sensor body,
// destructor destroys it.  Transform sync uses the SceneNode listener pattern.
// ---------------------------------------------------------------------------

class TriggerVolume {

        TSceneTree::TSceneNodeExact* pNode  = nullptr;
        BodyHandle                   hBody;
        ColliderDesc                 oCollider;

        StrID  on_enter_id;
        StrID  on_exit_id;
        StrID  on_stay_id;
        float  stay_interval = 0.0f;
        bool   one_shot      = false;
        bool   enabled       = true;

        // runtime overlap tracking
        std::unordered_set<BodyHandle> sActiveOverlaps;
        float stay_timer = 0.0f;
        bool  fired      = false;   // for one_shot

public:
        using TSerialized = FlatBuffers::TriggerVolume;

        TriggerVolume(TSceneTree::TSceneNodeExact* pNode, const TriggerVolumeDesc& desc);
        TriggerVolume(TSceneTree::TSceneNodeExact* pNode, const FlatBuffers::TriggerVolume* pFB);
        ~TriggerVolume() noexcept;

        void TargetTransformChanged(TSceneTree::TSceneNodeExact* pTargetNode);

        void OnTriggerEnter(const Event& e);
        void OnTriggerExit(const Event& e);
        void OnUpdate(const Event& e);

        void Enable();
        void Disable();

        std::string Str() const;
};

} // namespace SE

#endif
