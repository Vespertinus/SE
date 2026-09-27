
#ifndef APP_WORLD_PORTAL_H
#define APP_WORLD_PORTAL_H 1

#include <string>

#include <Component_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// Portal — the reusable level link for doorways and transit points.
//
// While any registered streaming focus (the player, a vehicle, ...) is inside
// activate_radius of this node, the referenced Manual sub-level is staged
// through the budgeted drain (WorldSystem::RequestActivation); once every
// focus has left release_radius (> activate_radius: hysteresis band) it is
// released again. Levels the streaming director already manages are ignored —
// a portal is for game-gated interiors (Manual|NoSpawn), not for
// distance-streamed content.
//
// The hysteresis bookkeeping tolerates explicit game control: if the game
// deactivates the level while its focus is still inside the band (a crypt
// exit), the portal does not immediately re-request — it stays disarmed until
// the focus has left the release radius once.
//
// Gameplay on top (teleport on door open, transitions) stays application-side;
// the demo's Door component composes with a Portal on the same node.
// ---------------------------------------------------------------------------

struct PortalDesc {
        std::string level_path;
        float       activate_radius = 22.0f;
        float       release_radius  = 34.0f;
};

class Portal {

        TSceneTree::TSceneNodeExact * pNode = nullptr;

        std::string level_path;
        float       activate_radius = 22.0f;
        float       release_radius  = 34.0f;

        // runtime — the world may load long after this component is created,
        // so path→index resolution is deferred to the first tick
        uint32_t    level_index = UINT32_MAX;
        bool        resolved    = false;
        bool        requested   = false;   // we staged it; release is ours
        bool        subscribed  = false;

public:
        using TSerialized = FlatBuffers::Portal;

        Portal(TSceneTree::TSceneNodeExact * pNewNode, const PortalDesc & oDesc);
        Portal(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::Portal * pFB);
        ~Portal() noexcept;

        void OnUpdate(const Event & e);

        void Enable();
        void Disable();

        const std::string & GetLevelPath() const { return level_path; }
        std::string Str() const;
};

} // namespace SE

#endif
