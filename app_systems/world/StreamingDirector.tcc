
#ifdef SE_IMPL

#include <world/StreamingDirector.h>

#include <Logging.h>

#include <SceneTree_generated.h>
#include <Terrain_generated.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace SE {

namespace {

// Same cap as TSceneTree::ReadSceneFile; the worker is the streaming path's
// only file reader, so it carries its own guard.
constexpr std::streamoff kMaxSceneFileSize = 1024ll * 1024ll * 100ll;

// Read + verify a flatbuffer file off the main thread. Throws on any failure; the
// worker catches and converts to an ok=false result (no exceptions cross
// the thread boundary).
void ReadAndVerifyFile(const std::string & sPath, std::vector<char> & vOut,
                       StreamingDirector::EContentKind eKind) {

        std::ifstream oInput(sPath, std::ios::binary | std::ios::ate);
        if (!oInput.is_open()) {
                throw std::runtime_error("failed to open file: " + sPath);
        }

        const std::streamoff size = oInput.tellg();
        if (size < 0 || size > kMaxSceneFileSize) {
                throw std::runtime_error("too big scene file, file: " + sPath);
        }

        vOut.resize(static_cast<size_t>(size));
        oInput.seekg(0, std::ios::beg);
        oInput.read(vOut.data(), size);
        if (!oInput) {
                throw std::runtime_error("short read, file: " + sPath);
        }

        flatbuffers::Verifier oVerifier(reinterpret_cast<const uint8_t *>(vOut.data()),
                                        vOut.size());
        switch (eKind) {
        case StreamingDirector::EContentKind::TerrainTile:
                // .setile shares the manifest's file_identifier ("SETN"); flatc emits
                // a generated Verify*Buffer helper only for the schema root type, so
                // the tile table verifies through the generic template.
                if (!oVerifier.VerifyBuffer<SE::FlatBuffers::TerrainTileData>(SE::FlatBuffers::TerrainIdentifier())) {
                        throw std::runtime_error("failed to verify terrain tile data in: " + sPath);
                }
                break;
        case StreamingDirector::EContentKind::SceneTree:
        default:
                if (SE::FlatBuffers::VerifySceneTreeBuffer(oVerifier) != true) {
                        throw std::runtime_error("failed to verify data in: " + sPath);
                }
                break;
        }
}

} // namespace

// ---------------------------------------------------------------------------
// Planner
// ---------------------------------------------------------------------------

void StreamingDirector::Reset(const uint32_t level_count) {
        vUnloadTimers.assign(level_count, 0);
        vCooldowns.assign(level_count, 0);
        vScratchOrder.clear();
}

void StreamingDirector::NotifyFailure(const uint32_t index) {
        if (index < vCooldowns.size()) { vCooldowns[index] = kRetryCooldownFrames; }
}

float StreamingDirector::DistanceToBounds(const glm::vec3 & vPos,
                                          const glm::vec3 & vMin, const glm::vec3 & vMax) {

        const glm::vec3 vClamped = glm::max(glm::min(vPos, vMax), vMin);
        return glm::length(vClamped - vPos);
}

void StreamingDirector::Plan(const SParams & oParams, const SLevelSnap * pSnaps,
                             const ELevelStatus * pStatuses, const uint32_t count,
                             const SFocus * pFocuses, const uint32_t focus_count,
                             const uint32_t in_flight_reads, const bool bAdvanceTimers,
                             SPlan & out) {

        out.Clear();
        vScratchOrder.clear();

        const float band_width = oParams.unload_radius - oParams.load_radius;

        for (uint32_t i = 0; i < count; ++i) {

                const auto & snap = pSnaps[i];
                const auto   st   = pStatuses[i];

                if (snap.manual) { continue; }   // explicit game control only

                if (snap.always_loaded) {
                        // Wanted by definition; startup keeps them active (synchronous path).
                        ++out.wanted;
                        if (st == ELevelStatus::Active) { ++out.active; }
                        continue;
                }

                bool  wanted        = false;
                bool  evict_pending = true;
                float best_dist     = std::numeric_limits<float>::max();

                for (uint32_t f = 0; f < focus_count; ++f) {

                        const float dist      = DistanceToBounds(pFocuses[f].pos,
                                                                 snap.bounds_min, snap.bounds_max);
                        const float load_eff  = (pFocuses[f].radius > 0.f)
                                                ? pFocuses[f].radius
                                                : oParams.load_radius;
                        const float unload_eff = load_eff + band_width;

                        if (dist <= load_eff)   { wanted = true; }
                        if (dist <= unload_eff) { evict_pending = false; }
                        best_dist = std::min(best_dist, dist);
                }

                if (wanted) { ++out.wanted; }

                switch (st) {

                case ELevelStatus::Unloaded:
                        if (wanted && vCooldowns[i] == 0) {
                                vScratchOrder.emplace_back(best_dist, i);
                        }
                        break;

                case ELevelStatus::Loading:
                        // In-flight load no longer wanted: cancel before side effects
                        // (no events have fired, unpins are cheap).
                        if (!wanted) { out.vCancels.push_back(i); }
                        break;

                case ELevelStatus::Active:
                        ++out.active;
                        if (evict_pending) {
                                if (bAdvanceTimers) {
                                        ++vUnloadTimers[i];
                                        if (vUnloadTimers[i] >= oParams.unload_frame_delay) {
                                                out.vUnloads.push_back(i);
                                                vUnloadTimers[i] = 0;
                                        }
                                }
                                else {
                                        // Flush mode: shed immediately. Timers never
                                        // advance here, so waiting on them would stall
                                        // a synchronous transition forever.
                                        out.vUnloads.push_back(i);
                                }
                        }
                        else {
                                vUnloadTimers[i] = 0;
                        }
                        break;

                case ELevelStatus::Unloading:
                        break;   // teardown owns it until it lands in Unloaded
                }

                if (bAdvanceTimers && vCooldowns[i] > 0) { --vCooldowns[i]; }
        }

        // Deterministic request order: nearest first, slot index breaks ties.
        std::sort(vScratchOrder.begin(), vScratchOrder.end(),
                        [](const auto & a, const auto & b) {
                                if (a.first != b.first) { return a.first < b.first; }
                                return a.second < b.second;
                        });

        // In-flight cap: only room for (max_in_flight - outstanding) new reads.
        const uint32_t room = (oParams.max_in_flight > in_flight_reads)
                        ? oParams.max_in_flight - in_flight_reads : 0;
        const uint32_t n = std::min(uint32_t(vScratchOrder.size()), room);
        for (uint32_t i = 0; i < n; ++i) {
                out.vRequests.push_back(vScratchOrder[i].second);
        }
}

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------

bool StreamingDirector::DispatchRead(const uint32_t world_serial, const uint32_t ticket,
                                     const uint32_t index, const std::string & sPath,
                                     const EContentKind eKind) {

        if (!bRunning.load(std::memory_order_relaxed)) {
                if (oWorker.joinable()) { return false; }   // stopped for good
                bRunning.store(true, std::memory_order_relaxed);
                oWorker = std::thread(&StreamingDirector::WorkerFunc, this);
        }

        SReadRequest req;
        req.world_serial = world_serial;
        req.ticket       = ticket;
        req.index        = index;
        req.kind         = eKind;
        req.path         = sPath;

        if (!qRequests.TryPush(req)) { return false; }

        ++in_flight_reads;
        return true;
}

void StreamingDirector::StopWorker() {

        if (!bRunning.load(std::memory_order_relaxed)) { return; }

        bRunning.store(false, std::memory_order_relaxed);
        if (oWorker.joinable()) { oWorker.join(); }
}

void StreamingDirector::WorkerFunc() {

        using namespace std::chrono_literals;

        while (bRunning.load(std::memory_order_relaxed)) {

                SReadRequest req;
                if (!qRequests.TryPop(req)) {
                        std::this_thread::sleep_for(1ms);
                        continue;
                }

                SReadResult res;
                res.world_serial = req.world_serial;
                res.ticket       = req.ticket;
                res.index        = req.index;
                res.kind         = req.kind;

                try {
                        auto pBytes = std::make_shared<std::vector<char>>();
                        ReadAndVerifyFile(req.path, *pBytes, req.kind);
                        res.ok     = true;
                        res.pBytes = std::move(pBytes);
                }
                catch (const std::exception & ex) {
                        log_e("StreamingDirector: worker read failed for '{}': {}",
                                        req.path, ex.what());
                        res.ok = false;
                }

                // Main thread drains every frame; a full result queue only happens
                // if the main thread stalled — sleep and retry until it catches up
                // (or we are shut down).
                while (bRunning.load(std::memory_order_relaxed) && !qResults.TryPush(res)) {
                        std::this_thread::sleep_for(1ms);
                }
        }
}

} // namespace SE

#endif
