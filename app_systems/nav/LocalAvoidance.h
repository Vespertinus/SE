
#ifndef APP_NAV_LOCAL_AVOIDANCE_H
#define APP_NAV_LOCAL_AVOIDANCE_H 1

#include <cstdint>

#include <glm/glm.hpp>

#include <StrID.h>

// ---------------------------------------------------------------------------
// LocalAvoidance — the compile-time local-avoidance contract.
//
// CrowdSystemT<TBackend> owns everything the pipeline shares (corridor
// following via NavigationSystem, the 30 Hz FixedClock cadence, follower
// state, InputState output) and delegates ONLY "given what every agent wants
// to do, what should each agent's velocity be" to TBackend. 
// This header is the whole dependency footprint of that swap: 
// replaces DetourCrowdAvoidance with an ORCA/flow-field backend and no movement code
// changes
//
// External locomotion: a backend NEVER owns positions. CrowdSystem writes the
// real body state through SyncAgent() before every solve and reads the
// adjusted steering velocity after it; physics (Jolt CharacterVirtual) remains
// the single authority over where agents actually are.
//
// The contract (TBackend must expose exactly these members):
//
//   bool Init(uint32_t nMaxAgents, float fMaxAgentRadius);
//       Sized once from CrowdSystem::SConfig. False => the backend is inert;
//       every Enroll refuses and all followers run unavioded.
//   void Shutdown();
//       Frees all solver state (class bindings included).
//   bool BindClass(StrID sClass, dtNavMesh * pMesh, const dtQueryFilter * pFilter);
//       Creates solver state for one agent class. Idempotent. False when
//       pMesh is null (inert nav class) or the solver refused; agents of such
//       classes run unavioded. The filter is copied at bind time (enrollment
//       snapshot; runtime SetAreaCost changes are picked up on rebind).
//   bool IsClassMeshCurrent(StrID sClass, const dtNavMesh * pMesh) const;
//       True when the class is bound to exactly this mesh. CrowdSystem probes
//       it per tick — NavigationSystem::UnregisterAll frees meshes, and the
//       backend must never outlive them.
//   void DropClass(StrID sClass);
//       Frees the class's solver state. Slots of that class become inert:
//       SetDesired/AdjustedVelocity are no-ops, Unenroll frees them.
//   int  Enroll(StrID sClass, const glm::vec3 & vPos, const SAvoidanceParams & oParams);
//       Slot id >= 0, or -1 when refused (pool full / class unbound). First
//       free slot — determinism comes from deterministic enrollment order.
//   void Unenroll(int nSlot);
//   void SyncAgent(int nSlot, const glm::vec3 & vPos, const glm::vec3 & vVel);
//       External-locomotion write-back: the solver samples this REAL state on
//       its next Update.
//   void SetDesired(int nSlot, const glm::vec3 & vDesired);
//       The pre-avoidance steering velocity (desired = from the corridor).
//   void Update(float fDt);
//       One batch solve over all enrolled agents.
//   glm::vec3 AdjustedVelocity(int nSlot);
//       Post-solve steering velocity (desired, avoidance-adjusted). Zero for
//       inert/refused slots.
//   uint32_t MaxAgents() const;  size_t ActiveAgents() const;
//       Pool size + live enrollment count (stats/debugger).
// ---------------------------------------------------------------------------

struct dtNavMesh;
struct dtQueryFilter;

namespace SE {

struct SAvoidanceParams {
        float radius    = 0.5f;   // avoidance body radius (meters)
        float height    = 2.0f;
        float max_speed = 5.0f;   // m/s — must match the real body's limits
        float max_accel = 20.0f;  // m/s^2
};

} // namespace SE

#endif
