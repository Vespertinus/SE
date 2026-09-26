
#ifndef APP_NAV_CROWD_SYSTEM_H
#define APP_NAV_CROWD_SYSTEM_H 1

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <FixedClock.h>
#include <StrID.h>
#include <nav/LocalAvoidance.h>

namespace SE {

class NavAgent;
class PathFollower;
class Event;

// ---------------------------------------------------------------------------
// CrowdSystemT — the steering driver.
//
// Owns the shared crowd pipeline; local avoidance itself lives in TBackend
// (the LocalAvoidance contract). Per solver tick:
//
//   1. write-back   real body positions/velocities into the backend
//                   (external locomotion — physics owns the bodies);
//   2. followers    arrival checks, phase-bucketed repath upkeep, and the
//                   desired velocity from corridor corners (NavigationSystem
//                   primitives — dtCrowd's own path machinery stays unused);
//   3. solve        TBackend::Update (one batch — proximity solvers must see
//                   all agents in the same tick; only follower upkeep staggers);
//   4. output       adjusted velocity -> analog InputState.move_axis for
//                   CharacterMovementSystem (which honors move_axis magnitude,
//                   so slow-radius deceleration needs no movement-stack change).
//
// Ticks on EInputUpdate (fires before EUpdate, where CharacterMovementSystem
// reads InputState — same-frame consumption regardless of typelist order;
// InputMappingContext flushes at the same phase). Solver cadence is a
// FixedClock (SConfig::crowd_dt, default 30 Hz) stepped with AppClock-scaled
// delta: slow-motion slows the crowd's brain too.
//
// Enrollment: NavAgent::Enable()/Disable() post ENavAgentRegistered/
// ENavAgentUnregistering; agents with avoidance=true join the backend roster
// (with or without a PathFollower — idle agents are stable obstacles: a zero
// requested velocity clamps their solved velocity to zero). Agents whose nav
// class is unbound (world still loading) or whose pool slot was refused retry
// every tick; agents of classes whose dtNavMesh died (UnregisterAll) are
// unenrolled and retry when the class returns.
//
// Composition: after NavigationSystem (uses its queries/filters/corridors and
// carries no WorldSystem knowledge). PathFollower (TCustomComponents)
// hard-depends on this system; NavAgent does not (crowd-less apps keep plain
// nav agents).
//
// Single-writer contract: while composed, this system owns
// InputState::move_axis (and zeroes jump/sprint) of every follower node every
// solver tick — do not co-drive the same node with an AIBrain-style writer.
// ---------------------------------------------------------------------------

template <typename TBackend>
class CrowdSystemT {
public:

        struct SConfig {
                float    crowd_dt             = 1.0f / 30.0f;
                uint32_t max_agents           = 64;      // avoidance pool
                float    max_agent_radius     = 2.5f;    // backend proximity sizing
                float    default_radius       = 0.5f;    // when NavAgent.radius == 0
                float    default_height       = 2.0f;
                float    default_max_speed    = 5.0f;    // when no CharacterController sibling
                float    default_max_accel    = 20.0f;
                float    teleport_distance    = 5.0f;    // > this from the corridor => repath
                uint32_t max_repaths_per_tick = 2;       // upkeep amortization cap
                uint32_t repath_buckets       = 4;       // phase buckets for repath checks
                float    idle_speed_epsilon   = 0.05f;   // move_axis deadzone (m/s)
        };

        struct SStats {
                // per frame (reset at EPostUpdate)
                uint32_t solver_ticks = 0;
                uint32_t arrivals     = 0;
                uint32_t repaths      = 0;
                float    solver_ms    = 0.f;
                // live
                uint32_t followers    = 0;
                uint32_t avoided      = 0;   // enrolled crowd agents (with or without follower)
        };

        // Debugger-facing follower snapshot (CrowdDebugger panel)
        struct SFollowerDebug {
                std::string sName;
                uint8_t     status       = 0;    // PathStatus
                glm::vec3   goal{};
                float       dist_to_goal = 0.f;
                glm::vec3   vDesired{};
                glm::vec3   vAdjusted{};
                bool        avoided     = false;
        };

        CrowdSystemT();
        ~CrowdSystemT() noexcept;

        void SetConfig(const SConfig & oCfg) {
                oConfig = oCfg;
                oClock = FixedClock(oCfg.crowd_dt);
                backend_dirty = true;
        }
        const SConfig & GetConfig() const { return oConfig; }

        // --- follower registry (PathFollower::Enable/Disable) ----------------

        void AddFollower(PathFollower * pFollower);
        void RemoveFollower(PathFollower * pFollower);

        // --- solver seam (tests drive single steps) --------------------------

        void RunCrowdStep(float fDt);      // one fixed solver tick
        TBackend &       Backend()         { return oBackend; }
        const TBackend & Backend() const   { return oBackend; }

        const SStats & GetStats() const { return oStats; }
        void GetDebugFollowers(std::vector<SFollowerDebug> & vOut) const;

private:

        struct SCrowdAgent {                 // avoidance roster (per NavAgent)
                NavAgent *       pAgent = nullptr;
                int32_t          slot   = -1;   // backend slot; -1 = unavioded (retry)
                SAvoidanceParams oParams;
                // cached per agent (resolve-once; NPC components don't churn):
                CharacterController * pCC = nullptr;
                // mesh-currency cache — revalidate only when the registry
                // generation moves (RegisterClass / UnregisterAll)
                StrID     cached_class_id {};
                uint64_t  cached_mesh_gen = 0;
                bool      mesh_checked  = false;
        };

        struct SFollowerRec {
                PathFollower * pFollower    = nullptr;
                float          repath_timer = 0.f;
                // Set on a failed re-path; blocks further attempts (including
                // BT MoveTo re-issues) until it drains — an unreachable goal
                // would otherwise burn a budgeted findPath per follower per tick.
                float          fail_cooldown = 0.f;
                // cached per tick (debugger + output stage)
                glm::vec3      vDesired{};
                glm::vec3      vAdjusted{};
                float          dist_to_goal = 0.f;
                // component pointer cache (resolve-once-when-null; a follower
                // used to pay 5-6 variant scans per tick re-finding these)
                NavAgent *            pCachedAgent = nullptr;
                CharacterController * pCachedCC    = nullptr;
                InputState *          pCachedInput = nullptr;
        };

        CrowdSystemT(const CrowdSystemT &) = delete;
        CrowdSystemT & operator = (const CrowdSystemT &) = delete;

        void OnInputUpdate(const Event & oEvent);
        void OnPostUpdate(const Event & oEvent);
        void OnNavAgentRegistered(const Event & oEvent);
        void OnNavAgentUnregistering(const Event & oEvent);

        void EnsureBackend();
        void TryEnroll(SCrowdAgent & oAgent);
        void UpkeepRoster();               // retries + mesh-currency sweep
        // Steering math for one following follower: desired velocity from the
        // corridor's first corner (every tick — cheap local query), with
        // linear deceleration across the slow radius.
        void ComputeDesired(SFollowerRec & oRec, NavAgent * pAgent, const glm::vec3 & vPos);
        // Phase-gated repath checks (timer / invalid corridor / teleport);
        // performs the actual re-path (the budgeted operation). Returns true
        // when a re-path was attempted.
        bool RepathIfNeeded(SFollowerRec & oRec, NavAgent * pAgent, const glm::vec3 & vPos, bool bPhaseDue);
        // Adjusted velocity (or raw desired when unavioded) -> InputState.
        void WriteOutput(SFollowerRec & oRec);

        float ResolveMaxSpeed(SFollowerRec & oRec);   // CharacterController sibling or config default

        SConfig oConfig;
        SStats  oStats;

        TBackend oBackend;
        bool     backend_dirty = true;
        bool     backend_ready = false;

        FixedClock oClock{1.0f / 30.0f};
        uint32_t   nTickIndex = 0;

        std::vector<SCrowdAgent>                     vAgents;
        std::unordered_map<NavAgent *, uint32_t>     mAgentLookup;

        std::vector<SFollowerRec>                    vFollowers;
        std::unordered_map<PathFollower *, uint32_t> mFollowerLookup;
};

} // namespace SE

#include <nav/DetourCrowdAvoidance.h>
namespace SE { using CrowdSystem = CrowdSystemT<DetourCrowdAvoidance>; }

#endif
