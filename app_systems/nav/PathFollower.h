
#ifndef APP_NAV_PATH_FOLLOWER_H
#define APP_NAV_PATH_FOLLOWER_H 1

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include <StrID.h>
#include <Component_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// PathFollowerDesc — programmatic construction (tests, tools).
// ---------------------------------------------------------------------------

struct PathFollowerDesc {
        float arrival_radius       = 0.35f;  // stop within this XZ distance of the goal
        float slow_radius          = 2.5f;   // decelerate linearly inside this radius
        float repath_interval      = 0.5f;   // seconds between automatic re-paths; 0 = never
        bool  accept_partial_paths = true;   // follow partial paths to unreachable goals
};

// ---------------------------------------------------------------------------
enum class PathStatus : uint8_t {
        Idle      = 0,   // no goal (or Stop() called)
        Following = 1,   // goal set, corridor live
        Arrived   = 2,   // reached the goal (latched until the next MoveTo)
        Failed    = 3    // pathing failed (no navmesh / unreachable / refused)
};

// Posted synchronously (TriggerEvent) by CrowdSystemT from its solver tick.
// EPathArrived drives HSM triggers / BT task completion without polling.
struct EPathArrived { TSceneTree::TSceneNodeWeak node; };
struct EPathFailed  { TSceneTree::TSceneNodeWeak node; };

// ---------------------------------------------------------------------------
// PathFollower — path-following intent on a nav agent.
//
// Data + decision surface only: MoveTo/Stop set the goal and status; the
// composed CrowdSystemT drives the corridor (NavigationSystem primitives),
// local avoidance (its backend) and the InputState output every solver tick.
// Requires a NavAgent sibling (the corridor lives on the NavAgent's
// NavigationSystem record); InputState + CharacterController siblings make the
// output actually move the body (the player movement stack, unmodified).
//
// Registration: Enable()/Disable() -> CrowdSystem Add/RemoveFollower — a
// compile-time GetSystem dependency: any app composing PathFollower must
// compose CrowdSystem (and NavigationSystem). SE_NAV_ENABLED compiles the
// registration out for nav-less apps (component stays an inert carrier).
//
// Single-writer: CrowdSystemT owns InputState::move_axis of follower nodes.
// ---------------------------------------------------------------------------

class PathFollower {
public:
        using TSerialized = FlatBuffers::PathFollower;

        PathFollower(TSceneTree::TSceneNodeExact * pNode, const PathFollowerDesc & desc);
        PathFollower(TSceneTree::TSceneNodeExact * pNode, const FlatBuffers::PathFollower * pFB);
        ~PathFollower() noexcept;

        void Enable();
        void Disable();

        // --- intent API (game code / HSM / BT tasks) ------------------------

        void MoveTo(const glm::vec3 & vGoal);
        void Stop();

        PathStatus               GetStatus() const { return eStatus; }
        bool                     HasArrived() const { return eStatus == PathStatus::Arrived; }
        const glm::vec3 &        GetGoal() const { return vGoal; }
        const PathFollowerDesc & Desc() const { return oDesc; }

        std::string Name() const { return pNode ? pNode->GetName() : std::string(); }
        std::string Str() const;

private:
        template <typename T> friend class CrowdSystemT;

        TSceneTree::TSceneNodeExact * pNode = nullptr;
        PathFollowerDesc              oDesc;
        glm::vec3                     vGoal{};
        PathStatus                    eStatus      = PathStatus::Idle;
        bool                          request_path = false;   // set by MoveTo, consumed by CrowdSystemT
};

} // namespace SE

#endif
