
#ifndef APP_WORLD_STREAMING_DIRECTOR_H
#define APP_WORLD_STREAMING_DIRECTOR_H 1

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

#include <SPSCQueue.h>

namespace SE {

// ---------------------------------------------------------------------------
// StreamingDirector — distance-driven sub-level streaming.
//
// Two halves, one class (owned by WorldSystem by value):
//
//   Planner — a PURE decision function over value snapshots (no engine state):
//     which levels to request (sorted, deterministic), which in-flight loads to
//     cancel, which active levels to unload after the hysteresis delay, plus
//     wanted/active progress counts. Unit-testable without a scene.
//
//   Worker  — one background thread reading + verifying .sesc bytes 
//     Transport: SPSCQueue both directions (audio
//     thread / EntityManager precedent). The worker never touches resources,
//     nodes or GL; results carry staged bytes as shared_ptr (queue elements
//     must be copy-assignable).
//
// Hysteresis: a level is wanted inside load_radius of any focus and stays
// loaded until it is beyond unload_radius of EVERY focus for
// unload_frame_delay consecutive Plan calls. Distances are measured to the
// level's AABB (not its center). A focus with radius > 0 overrides the world
// load_radius; the hysteresis band width (unload - load) is preserved.
// ---------------------------------------------------------------------------

class StreamingDirector {
public:

        // WorldSystem::ELevelStatus mapping: Requested+Loading both mean "in flight".
        enum class ELevelStatus : uint8_t {
                Unloaded = 0,
                Loading,
                Active,
                Unloading,
        };

        struct SParams {
                float    load_radius       = 96.f;
                float    unload_radius     = 128.f;   // > load_radius => hysteresis band
                uint32_t unload_frame_delay = 60;
                uint32_t max_in_flight      = 8;      // outstanding worker reads
        };

        struct SLevelSnap {
                glm::vec3 bounds_min{};
                glm::vec3 bounds_max{};
                bool      manual        = false;
                bool      always_loaded = false;
        };

        struct SFocus {
                glm::vec3 pos{};
                float     radius   = 0.f;    // > 0 => overrides load_radius for this focus
                float     priority = 0.f;    // reserved (HUD display); not an ordering key yet
        };

        struct SPlan {
                std::vector<uint32_t> vRequests;   // Unloaded+wanted, sorted by (distance, index)
                std::vector<uint32_t> vCancels;    // Loading levels that left the wanted set
                std::vector<uint32_t> vUnloads;    // Active levels past the hysteresis delay
                uint32_t              wanted = 0;  // progress denominator (always + in-range)
                uint32_t              active = 0;  // progress numerator

                void Clear() {
                        vRequests.clear();
                        vCancels.clear();
                        vUnloads.clear();
                        wanted = active = 0;
                }
        };

        // --- planner -------------------------------------------------------------

        /// Re-size per-level persistent state (unload timers, failure cooldowns)
        /// for a freshly loaded world. Clears everything.
        void Reset(uint32_t level_count);

        /// One decision pass. bAdvanceTimers=false is the flush mode: no unload-timer
        /// ticks and no cooldown burns (a FlushStreaming burst must not advance
        /// frame-counted state multiple times inside one frame).
        void Plan(const SParams & oParams, const SLevelSnap * pSnaps, const ELevelStatus * pStatuses,
                  uint32_t count, const SFocus * pFocuses, uint32_t focus_count,
                  uint32_t in_flight_reads, bool bAdvanceTimers, SPlan & out);

        /// A level whose read or dependency load failed will not be re-requested
        /// for kRetryCooldownFrames Plan calls (no retry/log storms).
        void NotifyFailure(uint32_t index);

        // --- worker ---------------------------------------------------------------

        /// Content type of a read: the worker verifies by kind (SceneTree buffers via
        /// VerifySceneTreeBuffer, terrain tiles via TerrainTileData). Extends
        /// the director to page terrain tiles through the same worker thread.
        enum class EContentKind : uint8_t {
                SceneTree = 0,
                TerrainTile,
        };

        struct SReadRequest {
                uint32_t     world_serial = 0;   // stale-world guard
                uint32_t     ticket       = 0;   // stale-result guard (per dispatch)
                uint32_t     index        = 0;
                EContentKind kind         = EContentKind::SceneTree;
                std::string  path;
        };

        struct SReadResult {
                uint32_t                             world_serial = 0;
                uint32_t                             ticket       = 0;
                uint32_t                             index        = 0;
                EContentKind                         kind         = EContentKind::SceneTree;
                bool                                 ok           = false;
                std::shared_ptr<std::vector<char>>   pBytes;
        };

        /// Queue a file read (starts the worker lazily). False: worker stopped
        /// or request queue full (caller retries next tick).
        bool DispatchRead(uint32_t world_serial, uint32_t ticket, uint32_t index,
                          const std::string & sPath,
                          EContentKind kind = EContentKind::SceneTree);
        bool TryPopResult(SReadResult & out) {
                const bool bOk = qResults.TryPop(out);
                if (bOk && in_flight_reads > 0) { --in_flight_reads; }
                return bOk;
        }

        /// Requests dispatched but not yet consumed by the main thread.
        uint32_t GetInFlightCount() const { return in_flight_reads; }

        void StopWorker();
        ~StreamingDirector() noexcept { StopWorker(); }

        static constexpr uint32_t kQueueCapacity      = 64;   // power of two (SPSCQueue)
        static constexpr uint32_t kMaxInFlightReads   = 8;    // outstanding worker reads
        static constexpr uint32_t kRetryCooldownFrames = 300; // ~5 s at 60 fps

        static float DistanceToBounds(const glm::vec3 & vPos,
                                      const glm::vec3 & vMin, const glm::vec3 & vMax);

private:

        void WorkerFunc();

        std::atomic<bool>          bRunning{ false };
        std::thread                oWorker;

        SPSCQueue<SReadRequest, kQueueCapacity> qRequests;
        SPSCQueue<SReadResult, kQueueCapacity>  qResults;

        uint32_t                   in_flight_reads = 0;   // main-thread accounting

        // planner-persistent per-level state
        std::vector<uint32_t>      vUnloadTimers;
        std::vector<uint32_t>      vCooldowns;
        std::vector<std::pair<float, uint32_t>> vScratchOrder;   // (distance, index)
};

} // namespace SE

#endif
